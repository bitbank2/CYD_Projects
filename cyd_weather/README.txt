CYD Weather Project
-------------------
This program reads the current weather data from wttr.in as JSON and parses
the values into useful info to display. The code is currently in a very early
alpha stage because it was originally written for e-paper displays and I'm
porting it for use on various sized LCDs.

The quantity of font/image data requires that you set the ESP32 program partition
size larger than the default. Try "no-OTA" or "large app".

