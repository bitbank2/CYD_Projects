//
// Example sketch demonstrating how to create a RAM (memory) drawing
// surface with bb_spi_lcd. Other display libraries can call this a 'Sprite''
// This memory can be provided by you or the library can allocate it.
// With bb_spi_lcd you will work with the main class instance for your physical LCD
// and additional class instances for each 'sprite' drawing surface. To draw these
// sprites onto the LCD, you can use the pushPixels() or pushImage() methods and
// pass the memory pointer and the width of the source drawing surface.
//
// Written by Larry Bank (bitbank@pobox.com) 7/28/2024
// Copyright (c) 2024 BitBank Software, Inc.
// 
#include <bb_spi_lcd.h>
#include "DejaVu_Sans_Bold_10.h"

uint16_t u16VirtualBuffer[88 * 48]; // holds a small block of pixels
 
BB_SPI_LCD lcd, virt; // Two instances of the class - one for the physical LCD and the other for drawing into RAM

void setup() {
  Serial.begin(115200);

  lcd.begin(DISPLAY_CYD_24R); // Initialize the display; make sure to set this correctly for your hardware type
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_GREEN);
  lcd.setFont(FONT_12x16); // use the built-in 12x16 font (really stretched+smoothed from 6x8)
  lcd.println("Virtual (sprite) Example"); // the println and printf methods are available on the ESP32
  lcd.println("This text is directly");
  lcd.println("drawn on the LCD class");
  virt.createVirtual(88, 48, u16VirtualBuffer); // width, height, optional buffer you provide
  virt.fillScreen(TFT_BLACK);
  virt.fillCircle(20, 14, 14, 0x2e0); // x, y, radius, color
  virt.fillCircle(60, 12, 12, TFT_BLUE);
  virt.fillCircle(40, 32, 16, TFT_RED);
  virt.setFreeFont(&DejaVu_Sans_Bold_10); // pass the pointer to the Arduino_GFX font data
  virt.setTextColor(TFT_YELLOW, TFT_YELLOW); // This will make the background color transparent (GFX font only on RAM drawing surface)
  virt.setCursor(0, 12); // Arduino_GFX fonts start from the baseline (y), not top
  virt.println("This");
  virt.println("is a");
  virt.println("sprite");
  for (int i=0; i<4; i++) {
    lcd.pushImage(i*64, 64 + (i*40), 88, 48, u16VirtualBuffer); // draw 4 copies of the sprite across the LCD
  }
}

void loop() {
}
