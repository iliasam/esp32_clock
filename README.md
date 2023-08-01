# esp32_clock
ESP32-based home clock
  
Features: 
* Displaying current time at big LED clock
* Time is periodically updated from SNTP server
* Date/Time are also stored in external RTC with its battery, so time is available instantly after power on, even if WIFI is not available
* Displaying current date at OLED
* Displaying current temperature and 6/12h weather forecast using Yandex.Weather data
* Displaying current outside temperature from a wireless sensor (the same as this devise do: https://github.com/iliasam/EinkTemperatureMonitor ) 
* Displaying measured CO2 value and drawing CO2 graph
* Brightness of the OLED and LED is controlled automatically by internal light sensor.
  
Used parts:
* ESP-WROOM-32 module
* HC-12 radio module
* MH-Z19B CO2 sensor
* Photoresistor
* 2 buttons
* 4 7-segments 1″ LED displays + MAX7219
* WEX012864 2.7″ OLED
* 5V AC-DC power supply module
  
Developed in Visual Studio Code and ESP-IDF framework.  
  
Photo of the device:
![Alt text](clock1.jpg?raw=true "Image")  
  
Displaying CO2 graph:
![Alt text](clock2.jpg?raw=true "Image")  
