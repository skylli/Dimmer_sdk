<!--
 * @Author: your name
 * @Date: 2020-02-22 15:40:21
 * @LastEditTime: 2021-02-04 16:06:02
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \mqtt_example\README.md
-->
[[source code ]](./main)

# Dimmer Sdk

## Hardware requirements

Nodemcu(esp32) board or Dimmer PCB and USB to TTL Serial Converter Adapter Module.

## Build the compilation environment
 As we use ESP-IDF of version v3.2.2, so please read [ESP-IDF guides](https://docs.espressif.com/projects/esp-idf/en/v3.2.2/get-started/index.html#) and [ESP-MDF](https://docs.espressif.com/projects/esp-mdf/en/latest/get-started/)  to build you compilation environment.
 
    notice: ESP-IDF is the Freertos for esp32 and ESP-MDF is the mesh sdk for esp32.

## source file overview
```
 -components
    |--- button       ; Key processing, including long press and short press 
    |--- light_device ; All the Dimmer function incloud factoroy rest and command respond from the mesh.
    |--- thincloud    ; Communication with tc cloud
    |--- utilis       ; Functions for small functions like byte print and malloc.
    |--- mesh_mqtt_handle ; Esp mqtt, But we use Aws mqtt, so we haven't use any of this components.
 - mqtt_example.c     ; The main function. 

```
## Compile and debug step by step 

###  set you serial port 
in the sdk_path/sdkconfig file and change this to you fash and debug port.
```
CONFIG_ESPTOOLPY_PORT="COM4"
```
### Buring and monitor 
```shell
cmd sdk_path
make flash monitor 
```
    "make flash monitor" will flash the firmware to the esp32 and open serial port to print the log.

### Source code framework description

1) Funtiono `light_device_init` will init all the device's function after init will start `app_loop` task which will handle all the event link key press/schedule update/command handle and respond from Tc.

2) Once the mesh is build `event_loop_cb` function will handle wifi and mesh event. When we get `MDF_EVENT_MWIFI_ROOT_GOT_IP` event, which mean esp32 have connect to TC, we will call `tc_client_creat` function to connnect TC.  More info please [refer to this](https://docs.espressif.com/projects/esp-mdf/en/latest/api-reference/mwifi/index.html)
3) `_tc_loop` Funtion will build mqtt connect to Tc and reconnect.

