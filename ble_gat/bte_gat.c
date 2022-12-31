#include "esp_bt.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "log.h"
#include <errno.h>
#include <string.h>

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* 2 for ADV, 2 for TX, 2 for RX => 2+2+2=6 */
#define GATTS_NUM_HANDLES			6
#define DEVICE_NAME				"CAGE"

static uint8_t adv_config_done = 0;
#define adv_config_flag				(1 << 0)
#define scan_rsp_config_flag			(1 << 1)

uint8_t raw_adv_data[] = {	/* DEN4DOGS */
		0x05,			/* LEN */
		0x09,			/* Complete Local Name */
		0x43, 0x41, 0x47, 0x45, /* 'CAGE' */
		0x02,			/* LEN */
		0x01,			/* Flags */
		0x05,			/* List of 32-bit UUIDs available */
		0x10,			/* LEN */
		0xFF,			/* Manufacturer Specific Data */
		0x59, 0x00, 0x43, 0x41, 0x47, 0x45,
		0x20,			/* space */
		'0', '0', '0', '0', '0', '0',	/* SN */
		0x20,			/* space */
		'F',			/* Status */
};

static uint8_t raw_scan_rsp_data[] = {
		0x11,			/* LEN */
		0x07,
		/* Complete list of 128-bit UUIDs available */
		0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE9,
		0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x11, 0x40, 0x6E,
};

static esp_ble_adv_params_t adv_params = {
	.adv_int_min        = 0x20,
	.adv_int_max        = 0x40,
	.adv_type           = ADV_TYPE_IND,
	.own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
	.channel_map        = ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static struct gatts_profile_inst {
	esp_gatts_cb_t gatts_cb;
	uint16_t gatts_if;
	uint16_t app_id;
	uint16_t conn_id;
	esp_gatt_if_t interface;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t tx_char_handle;
	uint16_t rx_char_handle;
	esp_bt_uuid_t tx_char_uuid;
	esp_bt_uuid_t rx_char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
} service_attr = {
	.gatts_cb = gatts_profile_event_handler,
	.gatts_if = ESP_GATT_IF_NONE,
	.interface = 0,
	.service_id = {
		.is_primary = true,
		.id.inst_id = 0x00,
		.id.uuid.len = ESP_UUID_LEN_128,
		.id.uuid.uuid.uuid128 = {
			0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
			0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,
		}, /* Nordic UART service */
	},
	.tx_char_uuid = {
		.len = ESP_UUID_LEN_128,
		.uuid.uuid128 = {
			0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
			0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e,
		}, /* Nordic TX Characteristic */
	},
	.rx_char_uuid = {
		.len = ESP_UUID_LEN_128,
		.uuid.uuid128 = {
			0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
			0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e,
		}, /* Nordic RX Characteristic */
	},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param)
{
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
		adv_config_done &= (~adv_config_flag);
		if (adv_config_done==0)
			esp_ble_gap_start_advertising(&adv_params);
		break;

	case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
		adv_config_done &= (~scan_rsp_config_flag);
		if (adv_config_done==0)
			esp_ble_gap_start_advertising(&adv_params);
		break;

	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) 
			ERROR("Advertising start failed");
		break;

	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
			ERROR("Advertising stop failed");
		break;

	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		break;
	default:
		ERROR("undefined event: %u", event);
		break;
	}
}

static char *ret_buf;
static void callback(char *buf, void *user)
{
	(void)user;
	int len = strlen(buf);

	buf[len++] = '\n';
	buf[len] = 0;

	ret_buf = buf;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	esp_err_t res;

	switch (event) {
	case ESP_GATTS_REG_EVT:
		res = esp_ble_gap_set_device_name(DEVICE_NAME);
		if (res) {
			ERROR("set device name failed: %s",
				esp_err_to_name(res));
			break;
		}

		res = esp_ble_gap_config_adv_data_raw(raw_adv_data,
			sizeof(raw_adv_data));
		if (res) {
			ERROR("config raw adv data failed: %s",
				esp_err_to_name(res));
			break;
		}

		adv_config_done |= adv_config_flag;

		res = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data,
			sizeof(raw_scan_rsp_data));
		if (res) {
			ERROR("config raw scan rsp data failed: %s",
				esp_err_to_name(res));
			break;
		}

		adv_config_done |= scan_rsp_config_flag;

		esp_ble_gatts_create_service(gatts_if, &service_attr.service_id,
			GATTS_NUM_HANDLES);
		break;

	case ESP_GATTS_READ_EVT:
		ERROR("Read event is not supported");
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_REQ_NOT_SUPPORTED,
			NULL);
		break;

	case ESP_GATTS_WRITE_EVT:
		if (!param->write.is_prep && param->write.len > 2) {
			/* null terminate */
			param->write.value[param->write.len] = 0;

			/* execute application command */
			//handle_cmd((char *)param->write.value, callback, 0);
			INFO("Bluetooth: %s => %s",
				(char *)param->write.value, ret_buf);
			esp_ble_gatts_send_indicate(gatts_if,
				param->write.conn_id,
				service_attr.tx_char_handle,
				strlen(ret_buf),
				(uint8_t *)ret_buf, false);
			free(ret_buf);
		}

		if (param->write.need_rsp && !param->write.is_prep)
			esp_ble_gatts_send_response(gatts_if,
				param->write.conn_id,
				param->write.trans_id,
				ESP_GATT_OK, NULL);
		break;

	case ESP_GATTS_EXEC_WRITE_EVT:
		esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
			param->write.trans_id, ESP_GATT_OK, NULL);
		break;

	case ESP_GATTS_MTU_EVT:
		break;

	case ESP_GATTS_UNREG_EVT:
		break;

	case ESP_GATTS_CREATE_EVT:
		service_attr.service_handle = param->create.service_handle;

		esp_ble_gatts_start_service(service_attr.service_handle);

		res = esp_ble_gatts_add_char(service_attr.service_handle,
			&service_attr.rx_char_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			ESP_GATT_CHAR_PROP_BIT_WRITE | \
			ESP_GATT_CHAR_PROP_BIT_WRITE_NR, NULL, NULL);
		if (res) {
			ERROR("Failed to create rx characteristics: %s",
				esp_err_to_name(res));
			break;
		}

		res = esp_ble_gatts_add_char(service_attr.service_handle,
			&service_attr.tx_char_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
		if (res) {
			ERROR("Failed to create tx characteristics: %s",
				esp_err_to_name(res));
			break;
		}

		break;

	case ESP_GATTS_ADD_INCL_SRVC_EVT:
		break;

	case ESP_GATTS_ADD_CHAR_EVT: {
		uint16_t length = 0;
		const uint8_t *prf_char;

		service_attr.tx_char_handle = param->add_char.attr_handle;
		service_attr.descr_uuid.len = ESP_UUID_LEN_16;
		service_attr.descr_uuid.uuid.uuid16 = \
			ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

		res = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,
			&length, &prf_char);
		if (res == ESP_FAIL)
			ERROR("ILLEGAL HANDLE: %s", esp_err_to_name(res));

		res = esp_ble_gatts_add_char_descr(service_attr.service_handle,
			&service_attr.descr_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
		if (res)
			ERROR("add char descr failed: %s",
				esp_err_to_name(res));
		break;
	}

	case ESP_GATTS_ADD_CHAR_DESCR_EVT:
		service_attr.descr_handle = param->add_char_descr.attr_handle;
		break;

	case ESP_GATTS_DELETE_EVT:
		break;

	case ESP_GATTS_START_EVT:
		break;

	case ESP_GATTS_STOP_EVT:
		break;

	case ESP_GATTS_CONNECT_EVT: {
		esp_ble_conn_update_params_t conn_params = { 0 };
		memcpy(conn_params.bda, param->connect.remote_bda,
			sizeof(esp_bd_addr_t));
		conn_params.latency = 0;
		conn_params.max_int = 0x20; /* max_int = 0x20*1.25ms = 40ms */
		conn_params.min_int = 0x10; /* min_int = 0x10*1.25ms = 20ms */
		conn_params.timeout = 400; /* timeout = 400*10ms = 4000ms */
		INFO("Client %02X:%02X:%02X:%02X:%02X:%02X connected",
			param->connect.remote_bda[0],
			param->connect.remote_bda[1],
			param->connect.remote_bda[2],
			param->connect.remote_bda[3],
			param->connect.remote_bda[4],
			param->connect.remote_bda[5]);
		service_attr.conn_id = param->connect.conn_id;

		esp_ble_gap_update_conn_params(&conn_params);

		/* save the current interface */
		service_attr.interface = gatts_if;
		break;
	}

	case ESP_GATTS_DISCONNECT_EVT:
		esp_ble_gap_start_advertising(&adv_params);
		service_attr.interface = 0;
		INFO("Bluetooth disconnected");
		break;

	case ESP_GATTS_CONF_EVT:
		break;

	case ESP_GATTS_OPEN_EVT:
	case ESP_GATTS_CANCEL_OPEN_EVT:
	case ESP_GATTS_CLOSE_EVT:
	case ESP_GATTS_LISTEN_EVT:
	case ESP_GATTS_CONGEST_EVT:
	default:
		break;
	}
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
	/* If event is register event, store the gatts_if for each profile */
	if (event == ESP_GATTS_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK)
			service_attr.gatts_if = gatts_if;
		else
			return;
	}

	/* If the gatts_if equal to profile A, call profile A cb handler,
	* so here call each profile's callback */
	if (gatts_if == ESP_GATT_IF_NONE || gatts_if == service_attr.gatts_if)
		if (service_attr.gatts_cb)
			service_attr.gatts_cb(event, gatts_if, param);
}

void close_connection(void *user)
{
	(void)user;

	if (service_attr.interface)
		esp_ble_gatts_close(service_attr.interface,
			service_attr.conn_id);
}

int ble_gat_init(void)
{
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	esp_err_t res;

	res = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
	if (res) {
		ERROR("Bluetooth memory release failed: %s",
			esp_err_to_name(res));
		return res;
	}

	res = esp_bt_controller_init(&bt_cfg);
	if (res) {
		ERROR("Bluetooth init error: %s", esp_err_to_name(res));
		return res;
	}

	res = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (res) {
		ERROR("Bluetooth controller enable failed: %s",
			esp_err_to_name(res));
		return res;
	}

	res = esp_bluedroid_init();
	if (res) {
		ERROR("init bluetooth failed: %s", esp_err_to_name(res));
		return res;
	}

	res = esp_bluedroid_enable();
	if (res) {
		ERROR("enable bluetooth failed: %s", esp_err_to_name(res));
		return res;
	}

	res = esp_ble_gatts_register_callback(gatts_event_handler);
	if (res) {
		ERROR("gatts register error: %s", esp_err_to_name(res));
		return res;
	}

	res = esp_ble_gap_register_callback(gap_event_handler);
	if (res) {
		ERROR("gatts register error: %s", esp_err_to_name(res));
		return res;
	}

	res = esp_ble_gatts_app_register(0);
	if (res) {
		ERROR("gatts app register error: %s", esp_err_to_name(res));
		return res;
	}

	esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
	if (local_mtu_ret)
		ERROR("set local  MTU failed, error code = %x", local_mtu_ret);

	return 0;
}
