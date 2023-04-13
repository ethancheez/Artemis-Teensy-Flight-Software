# Artemis Flight Software (Teensy 4.1)
This repository contains the flight software for the Artemis Teensy 4.1 microcontroller.

## Current Functionality:
* Deployment sequence 
  * Antenna deploys
  * Satellite begins beaconing 
* Ping/Pong command
  * Verify communication with sattelite
* Turn on/off RPi command
  * Safely shuts down RPi and turns off RPi switch 
  * Will not turn on RPi if battery levels are too low, unless given overide command
* Turn on/off voltage switches on the PDU
* Send telem (temp, current, imu, and gps) data
  * Periodically sends data automatically
  * Can manually request data as well 
* Receive take photo command and forwards to RPi
* If battery temp is low heaters are automatically turned on/off

## TODO: 
* Add encryption 
* WDT
* Add PDU switch status to beacons 
* ADCS
* Add Radio Temperature
* Add GPS time to beacons

## Flight Software Architetcture 

![Flight Software Architetcture](/image/FSWArchitecture.png)
