# ESPHome Configs
This is a repo of all my ESPHome configurations with details about some of my ESPHome related projects. 
The ESPHome project makes it incredibly easy to use ESP8266/ESP32 microcontrollers in Smart Home or Home Automation projects. With a simple YAML based configuration to configure sensors ESPHome makes it super easy to get started and manages the build and deployment of the software that is deployed onto the ESP8266/ESP32 devices.

In the [Wiki](https://github.com/jcallaghan/esphome-config/wiki) you will also find links to useful resources and examples of code and config I have found helpful.


## Devices
I have a number of ESPHome devices around my Smart Home and I've to provide a description and readme for each of these devices to share how I built the device and what I use it for. I also use these readme to document any special components I've used with the device and any specific build steps I followed. 

| Config | Description | Readme |
|--------|-------------|--------|
| [esph_area_kitchen.yaml](/esph_area_kitchen.yaml)| IR integration with hob/stove, used for area specific sensors and to obtain data from local Xiaomi temperature sensors via BLE.| ~~[Readme](/readme/esph_area_kitchen.md)~~|
| [esph_bedroom.yaml](/esph_bedroom.yaml) | This ESP32 board connects via two separate GPO pins two pressure pads that are placed under my mattress for presence detection. This is incredibly useful to trigger automations at bed time or in the morning when getting up. |[Readme](/readme/esph_bedroom.md)|
| [esph_ground_water.yaml](/esph_ground_water) | ESP32 with an ultrasonic distance sensor fitted inside a plastic tube. The tube is fixed to my basement wall and monitors the depth of water down there. I place a table-tennis ball inside the plastic tube which floats as water rises. This provides us with a constant view of how bad the flooding is in the local area.|~~[Readme](/readme/esph_ground_water.md)~~|
| [esph_hot_tub_energy.yaml](/esph_hot_tub_energy.yaml) | I use a Sonoff POWR2 to monitor the energy my hot tub uses while also providing a remote power switch to turn the hot tub power on and off.|~~[Readme](/readme/esph_hot_tub_energy.md)~~|
| [esph_area_upstairs.yaml](/esph_area_upstairs.yaml) | I use this ESP32 board to read bluetooth data such as the temperature from a Xiaomi LYWSD02 clock. |~~[Readme](/readme/esph_area_upstairs.md)~~|


## Patterns & Practices

### Substitutions
Typically I leverage three substitutions in my configs. These ensure I have a common hostname, readable and friendly name for entities and a useful description displayed in the ESPHome dashboard for each of my devices.
```yaml
substitutions:
  device_name: <hostname all lowercase with underscores as spaces i.e living_room>
  friendly_name: <usually CAML case version of the hostname i.e Living Room>
  device_description: <useful description of the config and or project>
```
To ensure all my ESPHome nodes have a consistent hostname I use the following when defining my ESPHome config. You can see from the code block below I prefix all my nodes with ```esph_``` and I include the ```{$device_description}``` to define the device comment.
```yaml
esphome:
  name: "esph_${device_name}"
  comment: ${device_description}
  platform: ESP32
  board: pico32
```
To ensure my entities are easy to find once the node has been added to Home Assistant I use the following format when defining my entity names.
```yaml
name: "ESPH ${friendly_name} <name of the entity (CAML case)>"
```
Here is an example of a binary_sensor where you can see the ESPH prefix on the sensor name. 
```yaml
binary_sensor:
  - platform: gpio
    pin: 
      number: GPIO4
      mode: INPUT_PULLUP
      inverted: True      
    name: "ESPH ${friendly_name} Bed Left Occupied"
```
This results in all the entities from this node are returned when searching for an entity in Home Assistant with a text search of ```esph_bedroom*```. This also means I don't need to rename the entity or change the display name of each entity when the devices is added to Home Assistant.

### Onboarding and Build process
1. When I want to run ESPHome on a new device I first determine the most appropriate device to use. 
1. I then create a new config where I define the components I want to use for the project.
1. Once I have established a config I will compile if via the ESPHome Dashboard and download it.
1. I then use ESPHome Flasher to upload the firmware to the ESP device.
1. Note that specific drivers or in some cases (particularly M5 Stick and M5 Atom) require an earlier version of the ESPHome Flasher tool. See device readme or Wiki for further information.
1. Once the device in running I make futher config edits and update OTA until I am happy with the project. 
1. I leverage the device webpage and logs in my testing. Once everything is working fine I then add the device to Home Assistant.#

## Configuration Structure

The configuration has been simplified.

- `devices/` contains complete device profiles including board definitions.
- `packages/base.yaml` contains the shared functionality used by nearly every node (WiFi, API, OTA, logger, web server, status sensors, uptime, time sync and restart switch).
- `components/` now only contains reusable parts such as sensors, fonts, colors and helper snippets.
- The `boards/` directory has been removed because it added an extra layer of indirection with very little benefit.

Example:

```yaml
substitutions:
  device_name: device_name
  friendly_name: Device Name
  device_description: Useful description

packages:
  device: !include devices/esp32_pico.yaml
```

Device profiles now directly define their ESP32/ESP8266 board using modern ESPHome syntax:

```yaml
esp32:
  board: pico32
```

This reduces the number of files substantially while keeping the reusable parts modular.
