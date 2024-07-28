#include <bb_spi_lcd.h>
#include <AnimatedGIF.h>
#define GIF_NAME hyperspace
//#include "../test_images/nostromo.h"
//#include "thisisfine_128x128.h"
//#include "homer_car_240x135.h"
#include "hyperspace.h"
//#include "bart_396x222.h"
//#include "earth_128x128.h"

uint8_t *pFrameBuffer, *pTurboBuffer;
bool bDMA;
//#define T_S3_LONG
//#define T_S3_PRO
//#define CYD_28R
//#define CYD_35C
//#define CYD_128C
//#define CYD_28C
//#define CYD_24R
//#define CYD_24C
//#define CYD_22C
//#define CYD_543R
#define CYD_543C
//#define CYD_SC01

#ifdef CYD_SC01
#define TOUCH_CAPACITIVE
#define LCD DISPLAY_WT32_SC01_PLUS
#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT 7
#define TOUCH_RST -1
#endif

#ifdef CYD_22C
//#define TOUCH_CAPACITIVE
#define LCD DISPLAY_D1_R32_ILI9341
//#define LCD DISPLAY_CYD_22C
#define TOUCH_SDA -1
#define TOUCH_SCL -1
//#define TOUCH_SDA 21
//#define TOUCH_SCL 22
#define TOUCH_INT -1
#define TOUCH_RST -1
#endif

#ifdef CYD_543C
#define TOUCH_CAPACITIVE
#define LCD DISPLAY_CYD_543
#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38
#endif

#ifdef CYD_543R
#define TOUCH_RESISTIVE
#define LCD DISPLAY_CYD_543
#define TOUCH_MISO 13
#define TOUCH_MOSI 11
#define TOUCH_CLK 12
#define TOUCH_CS 38
#endif

#ifdef CYD_24R
#define TOUCH_RESISTIVE
#define LCD DISPLAY_CYD_24R
#endif

#ifdef CYD_28R
#define TOUCH_RESISTIVE
#define LCD DISPLAY_CYD
#endif

#ifdef T_S3_LONG
#define TOUCH_CAPACITIVE
#define LCD DISPLAY_T_DISPLAY_S3_LONG
#define TOUCH_SDA -1
#define TOUCH_SCL -1
#define TOUCH_INT -1
#define TOUCH_RST -1
#endif

#ifdef T_S3_PRO
#define TOUCH_CAPACITIVE
#define LCD DISPLAY_T_DISPLAY_S3_PRO
#define TOUCH_SDA 5
#define TOUCH_SCL 6
#define TOUCH_INT 21
#define TOUCH_RST 13
#endif

#ifdef CYD_35C
#define TOUCH_CAPACITIVE
#define LCD DISPLAY_CYD_35
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#endif

#if defined (CYD_28C) || defined (CYD_24C)
// These defines are for a low cost 2.8" ESP32 LCD board with the GT911 touch controller
#define TOUCH_CAPACITIVE
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#define LCD DISPLAY_CYD_28C
#endif

#ifdef CYD_128C
// These defines are for a low cost 1.28" ESP32-C3 round LCD board with the CST816D touch controller
#define TOUCH_CAPACITIVE
#define TOUCH_SDA 4
#define TOUCH_SCL 5
#define TOUCH_INT 0
#define TOUCH_RST 1
#define QWIIC_SDA 21
#define QWIIC_SCL 20
#define LCD DISPLAY_CYD_128
#endif
AnimatedGIF gif;
#ifdef TOUCH_CAPACITIVE
#include <bb_captouch.h>
BBCapTouch bbct;
#endif
BB_SPI_LCD lcd;
int iOffX, iOffY;
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

void setup() {
  Serial.begin(115200);
//  delay(3000);
  Serial.println("Starting");
  gif.begin(BIG_ENDIAN_PIXELS);
  lcd.begin(LCD);
  Serial.println("LCD started");
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.setFont(FONT_8x8);
  //lcd.setCursor(0, 0);
  lcd.println("GIF + Touch Test");
  lcd.println("Touch to pause/unpause");
  Serial.println("GIF + Touch Test");
#ifdef TOUCH_CAPACITIVE
   bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
#endif
#ifdef TOUCH_RESISTIVE
   lcd.rtInit();
#endif
} /* setup() */

void CheckTouch(void)
{
TOUCHINFO ti;
#ifdef TOUCH_CAPACITIVE    
      if (bbct.getSamples(&ti) && ti.count >= 1) { // a touch event
          delay(500);
          bbct.getSamples(&ti); // get release event
          while (!bbct.getSamples(&ti)) {};
          delay(50);
          bbct.getSamples(&ti); // get release event
      }
#endif
#ifdef TOUCH_RESISTIVE
      if (lcd.rtReadTouch(&ti) && ti.count >= 1) {
          delay(500);
          while (!lcd.rtReadTouch(&ti)) {};
          delay(50);
      } // while 1
#endif
} /* CheckTouch() */

void loop() {
  int w, h, iFPS1, iFPS2;
   long l;
  if (gif.open((uint8_t *)GIF_NAME, sizeof(GIF_NAME), GIFDraw)) {
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
//        CheckTouch();
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
//        CheckTouch();
            iFrame++;
        }
        gif.reset();
      } // for i
      l = millis() - l;
      iFPS2 = ((iFrame*1000)/l);
      lcd.setCursor(0,0);
      lcd.setFont(FONT_12x16);
      lcd.printf("Without DMA: %d fps\n", iFPS1);
      lcd.printf("With DMA: %d fps\n", iFPS2);
      delay(10000);
      lcd.fillScreen(TFT_BLACK);
  } // while (1)
  } // gif open
} /* loop() */
