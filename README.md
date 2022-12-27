# esp32 components
## buttons
Key buttons implementation based on Freertos task polling method.

## drivers
### LTC6804-2
Li-ion Lifepo4 battery monitor/BMS

## encoder
Encoder driver supporting rotary encoders for menu control.

## include
### FreeRTOS helping header
Helps expand delay, delay_ms, delay_us macros

### log helping header
Implements INFO, WARN and ERROR macros for printf-like logging.

## OTA
Over-the-air firmware update

## Remote logger
Retargets the output logging to remote server.

## Stepper
Stepper motor driver task based.

## Wifi
Wifi ethernet driver. Connects to wifi, helps opening TCP sockets.

### Note
All sources adopted for ESP IDF 5.0