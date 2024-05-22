#include <bb_spi_lcd.h>
#include <bb_rtc.h>
#include <bb_scd41.h>
#include "humidity.h"
#include "temp_bw.h"

//#define BIG_LCD

#ifdef BIG_LCD
#define LCD DISPLAY_CYD_543
#define SDA_PIN 17
#define SCL_PIN 18
#define ICON_X1 60
#define ICON_X2 230
#define TEMP_X 106
#define CO2_X 268
#define FONT Roboto_Thin100pt7b
#define FONT_GLYPHS Roboto_Thin100pt7bGlyphs
#include "Roboto_Thin100pt7b.h"
#include "DejaVu_Sans_Mono_Bold_48.h"
#define SMALL_FONT DejaVu_Sans_Mono_Bold_48
const int iStartY = 160;
#else
#define LCD DISPLAY_CYD
#define SDA_PIN 27
#define SCL_PIN 22
#define ICON_X1 40
#define ICON_X2 154
#define TEMP_X 80
#define CO2_X 190
#define FONT Roboto_Thin66pt7b
#define FONT_GLYPHS Roboto_Thin66pt7bGlyphs
#include "Roboto_Thin66pt7b.h"
#include "DejaVu_Sans_Mono_Bold_28.h"
#define SMALL_FONT DejaVu_Sans_Mono_Bold_28
const int iStartY = 104;
#endif

SCD41 mySensor;
BBRTC rtc;
BB_SPI_LCD lcd;
bool bHasCO2 = false;
int iCharWidth, iColonWidth;
int iStartX, iStarty;
uint16_t usColor;
struct tm myTime;

#define NIGHT_COLOR TFT_YELLOW

typedef struct button_tag
{
  int x, y, w, h;
  uint16_t usColor;
  const char *label;
} BUTTON;
uint32_t ButtonState(BUTTON *pButtons, int iButtonCount);

#define BUTTON_COUNT 2
int iDigitPos[6];

#ifdef BIG_LCD
BUTTON buttons[BUTTON_COUNT] = {
  {0, 240, 200, 30, TFT_RED, "Set Time"},
  {280, 240, 200, 30, TFT_RED, "Night Mode"}
};
#else
BUTTON buttons[BUTTON_COUNT] = {
  {0, 210, 140, 30, TFT_RED, "Set Time"},
  {180, 210, 140, 30, TFT_RED, "Night Mode"}
};
#endif

void lightSleep(uint64_t time_in_ms)
{
  esp_sleep_enable_timer_wakeup(time_in_ms * 1000L);
  esp_light_sleep_start();
} /* lightSleep() */

void SetTime(void)
{
char szTime[32];
TOUCHINFO ti;
uint32_t u32 = 0;
int iTouch, iOldTouch = -1;

    rtc.getTime(&myTime); // Read the current time from the RTC into our time structure
    sprintf(szTime, "%02d:%02d", myTime.tm_hour, myTime.tm_min);
    lcd.setFreeFont(&FONT);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.drawString(szTime, iStartX, iStartY); // erase old character

    while (u32 != 1) { // loop until the user presses "Set Time" again
      u32 = ButtonState(buttons, BUTTON_COUNT);
      lcd.rtReadTouch(&ti);
      iTouch = -1;
      if (ti.count >= 1 && ti.y[0] < iStartY) {
        if (ti.x[0] > iDigitPos[0] && ti.x[0] < iDigitPos[2]) { // change hour
           iTouch = ((ti.x[0] - iDigitPos[0])/iCharWidth);
          // Serial.printf("x = %d, digit = %d\n", ti.x[0], iTouch);
        } else if (ti.x[0] > iDigitPos[3] && ti.x[0] < (iDigitPos[4]+iCharWidth)) { // minutes
           iTouch = 3 + ((ti.x[0] - iDigitPos[3])/iCharWidth);
          // Serial.printf("x = %d, digit = %d\n", ti.x[0], iTouch);
        }
      }
      if (iOldTouch == -1 && iTouch != -1) { // update
         switch (iTouch) {
          case 0: // hours 10's
             myTime.tm_hour += 10;
             if (myTime.tm_hour > 23) { myTime.tm_hour -= 24;}
             break;
          case 1: // hours 1's
             myTime.tm_hour++;
             if (myTime.tm_hour > 23) { myTime.tm_hour -= 24;}
             break;
          case 3: // minutes 10's
             myTime.tm_min += 10;
             if (myTime.tm_min > 59) { myTime.tm_min -= 60;}
             break;
          case 4: // minutes 1's
             myTime.tm_min++;
             if (myTime.tm_min > 59) { myTime.tm_min -= 60;}
             break;
         }
         lcd.fillRect(0, 0, lcd.width(), iStartY+20, TFT_BLACK);
         sprintf(szTime, "%02d:%02d", myTime.tm_hour, myTime.tm_min);
         lcd.setTextColor(TFT_CYAN, TFT_BLACK);
         lcd.drawString(szTime, iStartX, iStartY); // erase old character
      }
      iOldTouch = iTouch;
      delay(50); // don't let the user touch things too quickly
    } // while (1)
    rtc.setTime(&myTime); // update the new info    
} /* SetTime() */

void DrawButton(BUTTON *pButton)
{
int iLen;
  lcd.setFont(FONT_12x16);
  iLen = strlen(pButton->label) * 12;
  lcd.fillRoundRect(pButton->x, pButton->y, pButton->w, pButton->h, 8, pButton->usColor);
  lcd.setTextColor(TFT_BLACK, pButton->usColor);
  lcd.drawString(pButton->label, pButton->x + (pButton->w - iLen)/2, pButton->y + (pButton->h-16)/2);
} /* DrawButton() */

uint32_t ButtonState(BUTTON *pButtons, int iButtonCount)
{
  TOUCHINFO ti;
  int i;
  static uint32_t u32OldButtons = 0;
  uint32_t u32, u32Buttons = 0;

  lcd.rtReadTouch(&ti);
  if (ti.count >= 1) {
      for (i=0; i<iButtonCount; i++) {
        if (ti.x[0] >= pButtons[i].x && ti.x[0] < pButtons[i].x + pButtons[i].w &&
            ti.y[0] >= pButtons[i].y && ti.y[0] < pButtons[i].y + pButtons[i].h) {
              u32Buttons |= (1 << i); // mark this button as being pressed
        }
      } // for i
  } // if touch event
  u32 = (u32Buttons ^ u32OldButtons) & u32Buttons; // newly pressed buttons return true
  u32OldButtons = u32Buttons;
  return u32;
} /* ButtonState() */

void setup()
{
 // Serial.begin(115200);
  rtc.init(SDA_PIN, SCL_PIN, 0);
  if (rtc.getType() == RTC_RV3032) {
    rtc.setVBackup(true); // turn on the charge pump for a capacitor to hold the time during power down
  }
  lcd.begin(LCD);
  lcd.rtInit(); // start the resistive touch
  lcd.fillScreen(TFT_BLACK);
  iCharWidth = FONT_GLYPHS['0' - ' '].xAdvance;
  iColonWidth = FONT_GLYPHS[':' - ' '].xAdvance;
  iStartX = (lcd.width() - (4*iCharWidth + iColonWidth))/2;
  iDigitPos[0] = iStartX;
  iDigitPos[1] = iStartX+iCharWidth;
  iDigitPos[2] = iStartX+iCharWidth*2;
  iDigitPos[3] = iStartX+iColonWidth + iCharWidth*2;
  iDigitPos[4] = iDigitPos[3] + iCharWidth;
  iDigitPos[5] = lcd.width();

  if (mySensor.init(SDA_PIN, SCL_PIN, 1, 100000) == SCD41_SUCCESS) {
    // Serial.println("Found SCD41 sensor!");
     mySensor.start(); // start sampling mode
     bHasCO2 = true;
  }
} /* setup() */

void loop()
{
char szTemp[2], szTime[32], szOld[32];
uint8_t ucTemp[256];
int i, iTick;
uint32_t u32Buttons;
bool bNightMode = false;
int H, T, CO2, iOldH, iOldT, iOldCO2;

strcpy(szOld, "     ");

for (i=0; i<BUTTON_COUNT; i++) {
   DrawButton(&buttons[i]);
}
while (1) {
    usColor = (bNightMode) ? NIGHT_COLOR: TFT_GREEN;
    rtc.getTime(&myTime); // Read the current time from the RTC into our time structure
    sprintf(szTime, "%02d:%02d", myTime.tm_hour, myTime.tm_min);
    if ((iTick & 0x1f) < 0x10) { // flash the colon
        szTime[2] = ' ';
    }
    if (strcmp(szTime, szOld)) { // digit(s) changed, redraw them to minimize flicker
      szTemp[1] = 0;
      lcd.setFreeFont(&FONT);
      for (i=0; i<5; i++) {
         if (szTime[i] != szOld[i]) {
            szTemp[0] = szOld[i];
            lcd.setTextColor(TFT_BLACK, TFT_BLACK+1);
            lcd.drawString(szTemp, iDigitPos[i], iStartY); // erase old character
            // draw new character
            if (i == 0 && szTime[0] == '0') continue; // skip leading 0 for hours
            lcd.setTextColor(usColor, TFT_BLACK);
            szTemp[0] = szTime[i];
            lcd.drawString(szTemp, iDigitPos[i], iStartY);
         } // if needs redraw
      } // for i
      strcpy(szOld, szTime);
    }
    delay(33);
    iTick++;
    if (bHasCO2 && (iTick & 0x3f) == 0x3f) {
      // update environmental data
      for (i=0; i<4*41; i++) {
         ucTemp[i] = 255 - humidity[0x92 + i]; // inverted
      }
      lcd.drawPattern(ucTemp, 4, ICON_X1, iStartY+30, 32, 41, (bNightMode) ? NIGHT_COLOR : TFT_YELLOW, 31);
      for (i=0; i<4*40; i++) {
         ucTemp[i] = 255 - temp_bw[0x92 + i]; // inverted
      }
      lcd.drawPattern(ucTemp, 4, ICON_X2, iStartY+30, 26, 40, (bNightMode) ? NIGHT_COLOR : TFT_GREEN, 31);
      mySensor.getSample();
      H = mySensor.humidity();
      T = mySensor.temperature();
      CO2 = mySensor.co2();
      lcd.setFreeFont(&SMALL_FONT);
      if (H != iOldH) {
         iOldH = H;
         lcd.fillRect(0, iStartY+28, ICON_X1, 50, TFT_BLACK);
         lcd.setCursor(0, iStartY+68);
         lcd.setTextColor((bNightMode) ? NIGHT_COLOR : TFT_YELLOW, TFT_BLACK);
         lcd.print(H);
      }
      if (T != iOldT) {
         iOldT = T;
         lcd.fillRect(TEMP_X, iStartY+28, ICON_X2 - TEMP_X , 50, TFT_BLACK);
         lcd.setCursor(TEMP_X, iStartY+68);
         lcd.setTextColor((bNightMode) ? NIGHT_COLOR : TFT_GREEN, TFT_BLACK);
         lcd.printf("%d.%d", T/10, T % 10);
      }
      if (iOldCO2 != CO2) {
         iOldCO2 = CO2;
         lcd.fillRect(CO2_X, iStartY+28, lcd.width() - CO2_X, 50, TFT_BLACK);
         lcd.setCursor(CO2_X, iStartY+68);
         lcd.setTextColor((bNightMode) ? NIGHT_COLOR : TFT_CYAN, TFT_BLACK);
         lcd.printf("%dppm", CO2);
      }
    }
    u32Buttons = ButtonState(buttons, BUTTON_COUNT);
    if (u32Buttons & 2) { // night mode toggle
       bNightMode = !bNightMode;
       iOldH = iOldT = iOldCO2 = 0; // force immediate repaint
       strcpy(szOld, "     "); // force a redraw
       lcd.setBrightness((bNightMode)? 2 : 255);
    } else if (u32Buttons & 1) { // set time
       SetTime();
       strcpy(szOld, "     "); // force a redraw
    }
//    lightSleep(1000); // update the time once every second
  }
} /* loop() */
