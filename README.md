# FriskBy for CC3200 Launchpad running Energia

A general framework for reading sensors and sending data to the [friskby server](https://github.com/FriskByBergen/friskby), using [Energia](energia.nu) on the [TI CC3200 Launchpad](http://www.ti.com/tool/cc3200-launchxl).

Currently the on-board die-temp sensor and the [Sharp GP2Y1010AU0F](http://www.dfrobot.com/index.php?route=product/product&product_id=867) dust-sensor has been implemented. The Adafruit_TMP006 library is needed for the temp-sensor, but this should be included in energia by default.

## TODO
* SSL
* Timestamp (Will not be necessary if the friskby server adds this automatically [#15](https://github.com/FriskByBergen/friskby/issues/15))
* Optimize for lower power consumption (check out sleepseconds(), suspend() etc.)

## Getting started
1. Get Energia + cc3200 up and running following the guides http://energia.nu/pin-maps/guide_cc3200launchpad/ and http://energia.nu/cc3200guide/
2. Clone this repo and open it in energia 
  * Set correct serial port in Tools - Serial Port. 
  * Select Board Launchpad without EMT: Tools - Board - Launcpad w/cc3200
3. Update configuration-section in code, e.g. Wi-FI SSID & password
4. Build and upload (Ctrl+M is handy as it also brings up port monitor) - readings and messages sent to server should appear in port monitor (baud rate 115200)

### Hardware and connections
* [TI CC3200 Launchpad](http://www.ti.com/tool/cc3200-launchxl)
* [Sharp GP2Y1010AU0F](http://www.dfrobot.com/index.php?route=product/product&product_id=867)
  * Connect to Dust sensor adapter
* [Dust sensor adapter](http://www.dfrobot.com/index.php?route=product/product&product_id=1063)
  * Input:
    * VCC connected to 5V
    * D connected to Pin 61 (Digital out)
  * Output:
    * VCC connected to 5V
    * GND and A to voltage divider
* [Voltage divider](http://www.dfrobot.com/index.php?route=product/product&product_id=90)
  * Connected to Pin 59 (ADC channel 2)

### Adding more sensors
See configuration-section on sensors.
To add a new sensor: 
* Create a new sensor-struct, add it to the listOfSensors array, and increase NUM_SENSORS. 
* Implement initSensor() and readSensor(). Note! readSensor() must populate valueStr\[\] (the reading formatted as a string)
* Framework will do the rest.

## Misc.
* ADC max input is 1.43 V (http://e2e.ti.com/support/wireless_connectivity/f/968/p/452997/1679745), dust sensor may deliver more than this so a voltage divider is needed.
* Do not use ADC Channel 0 (pin 57) because of bug, see http://e2e.ti.com/support/wireless_connectivity/f/968/p/452997/1679745
* Dust sensor will output 0.5 V pr 0.1 mg/m3 (https://www.sparkfun.com/products/9689), and the max output at no dust is 1.5 V (https://www.sparkfun.com/datasheets/Sensors/gp2y1010au_e.pdf)
