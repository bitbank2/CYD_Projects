//
// This example shows how to play an animated GIF file
// from the built-in uSD slot of the CYD
// 
#include <bb_spi_lcd.h>  // https://github.com/bitbank2/bb_spi_lcd.git
#include <AnimatedGIF.h> // https://github.com/bitbank2/AnimatedGIF.git
#include <SPI.h>
#include <SD.h>

uint8_t *pFrameBuffer, *pTurboBuffer;
bool bDMA;

AnimatedGIF gif;
BB_SPI_LCD lcd;
SPIClass SD_SPI;
int iOffX, iOffY;

// GPIO pin assignments specifically for the CYD 2USB model
// check your model if different
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

// Change this to the file you would like to display
#define gif_filename "/earth_128x128.gif"
//
// Draw callback from GIF decoder
//
// called once for each line of the current frame
// MCUs with very little RAM would have to test for disposal methods, transparent pixels
// and translate the 8-bit pixels through the palette to generate the final output.
// The code for MCUs with enough RAM is much simpler because the AnimatedGIF library can
// generate "cooked" pixels that are ready to send to the display
//
void GIFDraw(GIFDRAW *pDraw)
{
  if (pDraw->y == 0) { // set the memory window when the first line is rendered
    lcd.setAddrWindow(iOffX + pDraw->iX, iOffY + pDraw->iY, pDraw->iWidth, pDraw->iHeight);
  }
  // For all other lines, just push the pixels to the display
  lcd.pushPixels((uint16_t *)pDraw->pPixels, pDraw->iWidth, (bDMA) ? DRAW_TO_LCD | DRAW_WITH_DMA : DRAW_TO_LCD);
} /* GIFDraw() */

File file;

// Callback functions to access a file on the SD card
void *myOpen(const char *filename, int32_t *size) {
  file = SD.open(filename);
  if (file) {
    *size = file.size();
  }
  return (void *)&file;
}

void myClose(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
}

int32_t myRead(GIFFILE *pFile, uint8_t *buffer, int32_t length) {
//  File *f = (File *)pFile->fHandle;
//  return f->read(buffer, length);
    int32_t iBytesRead;
    iBytesRead = length;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < length)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(buffer, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}

int32_t mySeek(GIFFILE *pFile, int32_t iPosition)
{
//  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
//  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

void setup() {
  gif.begin(BIG_ENDIAN_PIXELS);
  lcd.begin(DISPLAY_CYD_2USB);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.setFont(FONT_8x8);
  lcd.setCursor(0, 0);
  lcd.println("GIF from SD Test");
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SD_SPI, 10000000)) { // Faster than 10MHz seems to fail on the CYDs
    lcd.println("Card Mount Failed");
    while (1) {};
  } else {
    lcd.println("Card Mount Succeeded");
  }
} /* setup() */

void loop() {
  int w, h, iFPS1, iFPS2;
   long l;
  if (gif.open(gif_filename, myOpen, myClose, myRead, mySeek, GIFDraw)) {
    w = gif.getCanvasWidth();
    h = gif.getCanvasHeight();
    iOffX = (lcd.width() - w)/2;
    iOffY = (lcd.height() - h)/2;
    lcd.printf("Canvas size: %dx%d\n", w, h);
    lcd.printf("Loop count: %d\n", gif.getLoopCount());
    delay(3000);
    lcd.fillScreen(TFT_BLACK);
 //   Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", w, h);
#ifdef ARDUINO_ARCH_ESP32
    pFrameBuffer = (uint8_t *)heap_caps_malloc(w*(h+2), MALLOC_CAP_8BIT);
#else
    pFrameBuffer = (uint8_t *)malloc(w*(h+2));
#endif
  while (1) {
    int iFrame = 0;
      gif.setDrawType(GIF_DRAW_COOKED); // we want the library to generate ready-made pixels
      gif.setFrameBuf(pFrameBuffer);
      bDMA = false;
      l = millis();
      for (int i=0; i<4; i++) { // run it 4 times
        while (gif.playFrame(false, NULL)) {
            iFrame++;
        }
        gif.reset();
      } // for i
      l = millis() - l;
      iFPS1 = ((iFrame*1000)/l);
      // test with DMA enabled
      bDMA = true;
      iFrame = 0;
      l = millis();
      for (int i=0; i<4; i++) { // run it 4 times
        while (gif.playFrame(false, NULL)) {
            iFrame++;
        }
        gif.reset();
      } // for i
      l = millis() - l;
      iFPS2 = ((iFrame*1000)/l);
      lcd.setCursor(0,0);
      if (lcd.width() < 240) {
        lcd.setFont(FONT_8x8);
      } else {
        lcd.setFont(FONT_12x16);
      }
      lcd.printf("W/O DMA: %d fps\n", iFPS1);
      lcd.printf("W/  DMA: %d fps\n", iFPS2);
      delay(10000);
      lcd.fillScreen(TFT_BLACK);
  } // while (1)
  } else { // gif open
     lcd.print("Error opening GIF file");
     while (1) {};
  }
} /* loop() */
