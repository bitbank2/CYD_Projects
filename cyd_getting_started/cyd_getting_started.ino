//
// Simple example of display and touch on CYDs (Cheap Yellow Displays)
//
// written by Larry Bank
// Copyright (c) 2024 BitBank Software, Inc.
//
#include <bb_spi_lcd.h>
BB_SPI_LCD lcd;

void setup() {
  lcd.begin(DISPLAY_CYD); // initialize the LCD in landscape mode
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(FONT_12x16);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.println("CYD LCD & Touch Test");
  lcd.rtInit(); // initialize the resistive touch controller
} /* setup() */

void loop() {
TOUCHINFO ti;
bool bPrinted = false;

  while (1) {
     lcd.rtReadTouch(&ti);
     if (ti.count > 0) {
        lcd.setCursor(0,16);
        lcd.printf("Touch at: %d,%d    ", ti.x[0], ti.y[0]);
        bPrinted = true;
     } else {
        if (bPrinted) { // erase old info
           bPrinted = false;
           lcd.setCursor(0,16);
           lcd.print("                      "); // erase the line
        }
     }
  }
} /* loop() */
