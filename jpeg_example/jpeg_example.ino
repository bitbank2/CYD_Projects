//
// JPEG display example
// written by Larry Bank (bitbank@pobox.com)
// July 17, 2024
//
// This example demonstrates how to decode and display JPEG images
// on the CYD LCD. This is the easiest scenario to use - the JPEG
// data is compiled into the program FLASH memory and appears as
// a single binary blob. It is certainly possible to provide the
// JPEG data from SD card or Web or other sources by providing
// the open, close, seek and read callback functions
// See the Wiki for more info: https://github.com/bitbank2/JPEGDEC/wiki
//
#include <bb_spi_lcd.h>
#include <JPEGDEC.h>
#include "zebra.h"

// Static instances of the 2 classes
// These can be allocated dynamically too (e.g. *jpg = new JPEGDEC(); )
JPEGDEC jpg;
BB_SPI_LCD lcd;
//
// Draw callback from JPEG decoder
//
// called multiple times with groups of MCUs (minimum coded units)
// these are 8x8, 8x16, 16x8 or 16x16 blocks of pixels depending on the 
// color subsampling option of the JPEG image
// Each call can have a large group (e.g. 128x16 pixels)
//
int JPEGDraw(JPEGDRAW *pDraw)
{
  lcd.setAddrWindow(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  lcd.pushPixels((uint16_t *)pDraw->pPixels, pDraw->iWidth * pDraw->iHeight);
  return 1;
} /* JPEGDraw() */

void setup() {
  int xoff, yoff;
  lcd.begin(DISPLAY_CYD_543); // set this to the correct display type for your board
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.setFont(FONT_12x16);
  lcd.println("JPEG Decoder Example");
  lcd.println("Decode and center on LCD");
  delay(3000);
  lcd.fillScreen(TFT_BLACK);
  if (jpg.openFLASH((uint8_t *)zebra, sizeof(zebra), JPEGDraw)) { // pass the data and its size
      jpg.setPixelType(RGB565_BIG_ENDIAN); // bb_spi_lcd uses big-endian RGB565 pixels
      // if the image is smaller than the LCD dimensions, center it
      xoff = (lcd.width() - jpg.getWidth())/2;
      yoff = (lcd.height() - jpg.getHeight())/2;
      jpg.decode(xoff,yoff,0); // center the image and no extra options (e.g. scaling)
  }      
} /* setup() */

void loop() {
} /* loop() */
