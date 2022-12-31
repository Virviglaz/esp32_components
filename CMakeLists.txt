idf_component_register(
SRC_DIRS
#	"drivers/LTC6804-2"
#	"include"
#	"ota"
#	"remote_logger"
#	"wifi"
#	"buttons"
#	"stepper"
#	"ble_gat"
INCLUDE_DIRS
#	"drivers/LTC6804-2"
	"include"
#	"ota"
#	"remote_logger"
#	"wifi"
#	"buttons"
#	"stepper"
#	"encoder"
#	"ble_gat"
PRIV_REQUIRES
#	nvs_flash
#	esp_timer
#	esp_netif
#	esp_wifi
#	app_update
#	lwip
	bt
REQUIRES
#	driver
)
