//
// ESP32 CYD (Cheap-Yellow-Display) Weather Info
// written by Larry Bank
// Copyright (c) 2024 BitBank Software, Inc.
//
// Define the display type used and the rest of the code should "just work"

#define LCD DISPLAY_CYD_2USB

#include <NTPClient.h>           //https://github.com/taranais/NTPClient
#include <WiFi.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
ESP32Time rtc(0);
HTTPClient http;
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <bb_spi_lcd.h>
#include "Roboto_Black_50.h"
#include "Roboto_Black_28.h"
#include "Roboto_Black_40.h"
#include "Roboto_Black_20.h"
#include "Roboto_25.h"
#include "Roboto_Thin66pt7b.h"
// black and white graphics
#include "humidity_4bpp.h"
#include "sunrise_4bpp.h"
#include "sunset_4bpp.h"
#include "wind_4bpp.h"
#include "temp_4bpp.h"
#include "rain_4bpp.h"
#include "uv_icon_4bpp.h"
#include "hand_4bpp.h"

//#define LOG_TO_SERIAL
#define TZ_OFFSET 3600

BB_SPI_LCD lcd;
// Define NTP Client to get time
struct tm myTime;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
char url[]="https://wttr.in/?format=j1";
int iWind, rel_humid, temp, feels_temp, mintemp, maxtemp, cc_icon; 
String sSunrise, sSunset, updated;
int iTemp[16], iHumidity[16], iWeatherCode[16]; // hourly conditions
uint8_t uvIndex[8];
int iRainChance[16];
int iDigitPos[6]; // clock digit positions
int iCharWidth, iColonWidth;
int iStartX, iStarty;
#define FONT Roboto_Thin66pt7b
#define FONT_GLYPHS Roboto_Thin66pt7bGlyphs
uint16_t usColor = TFT_GREEN; // time color
const int iStartY = 222;

const char *szMonths[] = {"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
const int iMonthLens[] = {31, 28, 31, 30, 31, 30, 31, 30, 30, 31, 30, 31};
const char *szDays[] = {"S", "M", "T", "W", "T", "F", "S"};
void drawCalendar(struct tm *pTime, const GFXfont *pFont, int x, int y)
{
int i, j, k, dx, dy; // calculate delta of each cell
uint16_t u16LineClr, u16CellClr, u16TextClr;
int tx, ty, iDay, iDeltaX;
int iMonth, iStartDay;
int16_t x1, y1;
uint16_t w, w2, h, h2;

   iMonth = pTime->tm_mon;
   iStartDay = pTime->tm_wday - ((pTime->tm_mday-1) % 7);
   if (iStartDay < 0) iStartDay += 7;
   
   u16LineClr = TFT_RED;
   u16CellClr = TFT_BLACK;
   u16TextClr = TFT_WHITE;

   lcd.setFreeFont(pFont);
   lcd.getTextBounds("00", 0, 0, &x1, &y1, &w, &h);
   dx = w+4; // width of each cell, not including the surrounding lines
   dy = (h*5)/4; // height of each cell, not including the surrounding lines

   // draw the month name area
   lcd.setTextColor(u16TextClr, TFT_BLACK);
   lcd.drawRect(x,y,(dx*7)+8,dy+2, u16LineClr);
   lcd.fillRect(x+1,y+1,(dx*7)+6,dy-1, u16CellClr);
   lcd.getTextBounds(szMonths[iMonth], 0, 0, &x1, &y1, &w2, &h2);
   iDeltaX = 1 + ((dx*7+6) - w2)/2;
   lcd.setCursor(x+iDeltaX, y + h2 + (dy-h2)/2);
   lcd.print(szMonths[iMonth]);
   
   // draw the grid and cells
   k = (iStartDay + iMonthLens[iMonth] > 35) ? 8:7;
   for (j=1; j<k; j++) { // y
     for (i=0; i<7; i++) { // x
        lcd.drawRect(x+(i*(dx+1)),y+(j*(dy+1)),dx+2,dy+2, u16LineClr);
     }
   } // for j
   // draw the day names
   ty = y + dy + 1;
   tx = x + 1;
   for (i=0; i<7; i++) {
      lcd.setCursor(tx + (dx-w/2)/2, ty + h + (dy-h)/2);
      lcd.print(szDays[i]);
      tx += dx+1;
   }
   // draw the days of the month
   ty = y + 1 + (dy + 1)*2;
   tx = x + 1 + (iStartDay * (dx+1));
   iDay = iStartDay;
   for (i=1; i<=iMonthLens[iMonth]; i++) {
      uint16_t u16Clr;
      iDeltaX = (i < 10) ? (dx-w/2)/2 : (dx-w)/2;
      u16Clr = (i == pTime->tm_mday) ? u16TextClr : u16CellClr;
      lcd.fillRect(tx,ty,dx,dy, u16Clr);
      u16Clr = (i != pTime->tm_mday) ? u16TextClr : u16CellClr;
      lcd.setTextColor(u16Clr, TFT_BLACK);
      lcd.setCursor(tx + iDeltaX, ty + h + (dy-h)/2);
      lcd.print(i,DEC);
      tx += dx+1;
      iDay++;
      if (iDay == 7) { // next line
        iDay = 0;
        tx = x + 1;
        ty += dy+1;
      }
   }
} /* drawCalendar() */

// Draw the small bar graph of the rain probabilities
// Uses black for < 50% and red for >= 50%
void showRain(int x, int y, int cx, int cy, const char *szLabel, int *pValues)
{
  int i, j, tx = (cx-2)/8;
    lcd.drawRect(x, y, cx, cy, TFT_WHITE);
    for (i=0; i<8; i++) { // 8 values of rain probability for today and tomorrow
      j = (pValues[i]*(cy-2))/100; // height of probability for today
      lcd.fillRect(x+1+(i*tx), y+1+(cy-2-j), tx, j, (pValues[i]<50) ? TFT_BLACK : TFT_RED);
    }
    // add label
    lcd.setFont(FONT_8x8);
    lcd.setCursor(x+(cx/2)-4*strlen(szLabel),y-8);
    lcd.print(szLabel);
} /* showRain() */
//
// Display the current weather conditions
// Returns the amount of time to sleep until the next reading
//
void DisplayWeather(void)
{
  char *s, szTemp[64];
  int i, j;

  lcd.fillScreen(TFT_BLACK);
  // show the update time+date
  lcd.setFont(FONT_8x8);
  lcd.setCursor(0,lcd.height()-8);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.print("Last Update: ");
  lcd.print(updated.c_str());
//  Serial.println(szTemp);
  if (lcd.width() < 200) { // super small layout (FUTURE?)
     // display sunrise+sunset times first
    lcd.drawBMP((uint8_t *)sunrise_4bpp,0,0);
    lcd.drawBMP((uint8_t *)sunset_4bpp,0,38);
    lcd.drawBMP((uint8_t *)humidity_4bpp,0,76);
    lcd.drawBMP((uint8_t *)wind_4bpp,0,114);
    lcd.drawBMP((uint8_t *)temp_4bpp,116,38);
    //obd.loadBMP((uint8_t *)hand,220,40,TFT_BLACK, TFT_RED);
    //obd.loadBMP((uint8_t *)rain,72,78,TFT_BLACK, TFT_RED);
    lcd.drawBMP((uint8_t *)uv_icon_4bpp,128,0);
    strcpy(szTemp, sSunrise.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(48,16);
    lcd.setFont(FONT_12x16);
    lcd.print(s);
    strcpy(szTemp, sSunset.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(48,56);
    lcd.print(s);
    lcd.display();
    return;
  } else if (lcd.width() < 480) { // medium display layout
    // display sunrise+sunset times first
    lcd.drawBMP((uint8_t *)sunrise_4bpp,0,0);
    lcd.drawBMP((uint8_t *)sunset_4bpp,0,40);
    lcd.drawBMP((uint8_t *)humidity_4bpp,0,78);
    lcd.drawBMP((uint8_t *)wind_4bpp,108,0);
    lcd.drawBMP((uint8_t *)temp_4bpp,108,40);
    lcd.drawBMP((uint8_t *)rain_4bpp,72,78);
    lcd.drawBMP((uint8_t *)uv_icon_4bpp,232,0);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    strcpy(szTemp, sSunrise.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(48,28);
    //obd.setFont(FONT_12x16);
    lcd.setFreeFont(&Roboto_25);
    lcd.print(s);
    strcpy(szTemp, sSunset.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(48,68);
    lcd.print(s);
    lcd.setCursor(32,106);
    lcd.print(rel_humid, DEC);
    lcd.setCursor(148,28);
    lcd.print(iWind, DEC);
    lcd.print(" kph");
    lcd.setCursor(140,60);
    sprintf(szTemp, "%dC", temp);
    lcd.print(szTemp);
    sprintf(szTemp, "(%d/%d) (feels like)", mintemp, maxtemp);
    lcd.setFont(FONT_8x8); // squeeze in this info in a smaller font
    lcd.setCursor(136,64);
    lcd.print(szTemp);
    lcd.setCursor(234,60);
    //lcd.setFont(FONT_12x16);
    lcd.setFreeFont(&Roboto_25);
    lcd.print(feels_temp, DEC);
    lcd.print("C");
    j = 0; // uv max index
    for (int i=0; i<8; i++) {
      if (uvIndex[i] > j) j = uvIndex[i];
    }
    lcd.setCursor(272,28);
    lcd.printf("%d",j); // show max UV index
    // Show rain chance as a bar graphs for today and tomorrow
    showRain(120, 89, 82, 27, "today", iRainChance);
    showRain(208, 89, 82, 27, "tomorrow", &iRainChance[8]);
    return;
  } else { // 4.2" display
    drawCalendar(&myTime, &Roboto_Black_20, 90, 148);
    // display sunrise+sunset times first
    lcd.drawBMP((uint8_t *)sunrise_4bpp,8,0);
    lcd.drawBMP((uint8_t *)sunset_4bpp,8,48);
    lcd.drawBMP((uint8_t *)wind_4bpp,180,0);
    lcd.drawBMP((uint8_t *)humidity_4bpp,180,48);
    lcd.drawBMP((uint8_t *)uv_icon_4bpp,292,48);
    lcd.drawBMP((uint8_t *)temp_4bpp,8,96);
    lcd.drawBMP((uint8_t *)hand_4bpp,300,96);
    lcd.drawBMP((uint8_t *)rain_4bpp,24,144);
    lcd.drawBMP((uint8_t *)rain_4bpp,332,144);
    // Show rain chance as a bar graphs for today and tomorrow
    showRain(8, 200, 74, 62, "today", iRainChance);
    showRain(316, 200, 74, 62, "tomorrow", &iRainChance[8]);
    strcpy(szTemp, sSunrise.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(60,36);
    lcd.setFreeFont(&Roboto_Black_40);
    lcd.print(s);
    strcpy(szTemp, sSunset.c_str());
    szTemp[5] = 0; // don't need the AM/PM part
    s = szTemp; if (s[0] == '0') s++; // skip leading 0
    lcd.setCursor(60,80);
    lcd.print(s);
    lcd.setCursor(220,80);
    lcd.print(rel_humid, DEC);
    lcd.setCursor(228,36);
    lcd.print(iWind, DEC);
    lcd.print(" kph");
    lcd.setCursor(40,132);
    sprintf(szTemp, "%dC (%d/%d)", temp, mintemp, maxtemp);
    lcd.print(szTemp);
    lcd.setCursor(340, 132);
    lcd.print(feels_temp, DEC);
    j = 0; // uv max index
    for (int i=0; i<8; i++) { // find min/max rain chance for today
//      if (iRainChance[i] < iRMin) iRMin = iRainChance[i];
//      if (iRainChance[i] > iRMax) iRMax = iRainChance[i];
      if (uvIndex[i] > j) j = uvIndex[i];
    }
    lcd.setFreeFont(&Roboto_Black_40);
    lcd.setCursor(340,80);
    lcd.print(j,DEC); // show max UV index
    return;
  }
  #ifdef BOGUS
  obd.setFreeFont(&Roboto_Black_20);
  obd.setCursor(0,20);
  sprintf(szTemp, "Updated: %s", updated.c_str());
  obd.print(szTemp);
  sprintf(szTemp, "Sunrise %s", sSunrise.c_str());
  obd.setFreeFont(&Roboto_Black_28);
  obd.setCursor(0,52);
  obd.println(szTemp);
  sprintf(szTemp, "Sunset %s", sSunset.c_str());
  obd.print(szTemp);

// Show current conditions icon
    j = 0;
    while (WeatherToIcon[j*2] != 0 && WeatherToIcon[j*2] != cc_icon) {
      j++; 
    }
    if (WeatherToIcon[j*2] != 0) { // found it
      iIcon = WeatherToIcon[j*2+1];
      Serial.printf("Weather icon number = %d\n", iIcon);
      if (tiff.openTIFF((uint8_t *)weather_icons, (int)sizeof(weather_icons), TIFFDraw))
      {
      int rc, x, y;
        Serial.println("TIFF opened successfully");
        x = iIcon % 7; // 7 per row
        y = iIcon / 7;
        tiff.setDrawParameters(1.0f, TIFF_PIXEL_1BPP, 50 + (x *128), 40 + (y * 118), 128, 118, NULL);
        rc = tiff.decode();
        tiff.close();
        if (rc == TIFF_SUCCESS) { // draw it
          Serial.println("Successfully decoded TIFF");
          obd.drawSprite(ucIcon, 128, 118, 16, 270, 0, 1);
        }
      } // if TIFF opened
    } // valid weather icon

// Show min/max temp
   sprintf(szTemp, "%dc", temp);
   obd.setFreeFont(&Roboto_Black_50);
   obd.setCursor(285, 155);
   obd.print(szTemp);
  obd.setFreeFont(&Roboto_Black_20);
  obd.setCursor(260,178);
   sprintf(szTemp, "RH %d%%", rel_humid);
   obd.print(szTemp);
  sprintf(szTemp, "Wind %d kph", iWind);
  obd.setCursor(260,196);
  obd.print(szTemp);
   
   sprintf(szTemp, "Min: %dc", mintemp);
   obd.setFreeFont(&Roboto_Black_50);
   obd.setCursor(0, 130);
   obd.print(szTemp);
   sprintf(szTemp, "Max: %dc", maxtemp);
   obd.setCursor(0,180);
   obd.print(szTemp);
// Show hourly chance of rain for today & tomorrow
   obd.setFreeFont(&Roboto_Black_20);
   obd.setCursor(0,210);
   obd.print("Today's chance of rain...");
   obd.setCursor(0,266);
   obd.print("Tomorrow's chance of rain...");
  for (i=0; i<8; i++) {
    sprintf(szTemp, "%02d:00", i*3);
    obd.setFont(FONT_8x8);
    obd.setCursor(i*50,216);
    obd.print(szTemp);
    obd.setCursor(i*50,272);
    obd.print(szTemp);
    obd.setFreeFont(&Roboto_Black_20);
    sprintf(szTemp, "%d%%", iRainChance[i]);
    obd.setCursor(8+i*50,242);
    obd.print(szTemp);
    sprintf(szTemp, "%d%%", iRainChance[i+8]);
    obd.setCursor(8+i*50,298);
    obd.print(szTemp);
  }

  obd.display();
#endif // BOGUS
} /* DisplayWeather() */

//
// Request the latest weather info as JSON from wttr.in
// Info is usually updated every 3 hours
//
int GetWeather(int *iSleepTime)
{
char szTemp[64];
int i, iHour, httpCode = -1;

   *iSleepTime = (3 * 60 * 60 * 1000); // assume 3 hours

#ifdef ARDUINO_ARCH_ESP32
   http.begin(url);
   httpCode = http.GET();  //send GET request
#else // WiFiNINA
   String payload = String("");
// https://wttr.in/?format=j1
  if (http.connectSSL("wttr.in", 443)) {
#ifdef LOG_TO_SERIAL
     Serial.println("Connected to weather server");
#endif
    http.println("GET /?format=j2 HTTP/1.1");
    http.println("Host: wttr.in");
    http.println("Connection: close");
    http.println();
    while (http.connected()) {
      if (http.available()) {
    //    char c = http.read();
    //    payload.concat(c);
        payload = http.readString();
      } else {
        delay(100); // wait for data to arrive
      }
    } // while connected
    http.stop();
    if (payload.length() > 1000) {
       httpCode = 200; // count that as a success
       i = payload.indexOf("{"); // look for starting curly brace (skip over HTTP info)
       payload.remove(0, i); // remove those leading characters
       Serial.print(payload);
    }
  } else {
#ifdef LOG_TO_SERIAL
     Serial.println("failed to connect to weather server");
#endif
  }
#endif

   if (httpCode != 200) {
#ifdef LOG_TO_SERIAL
     Serial.print("Error on HTTP request: ");
     Serial.println(httpCode);
#endif
#ifdef ARDUINO_ARCH_ESP32
     http.end();
#endif
     return 0;
   } else {
#ifdef ARDUINO_ARCH_ESP32
     String payload = http.getString();
     http.end();
//     WiFi.disconnect(true);
//     WiFi.mode(WIFI_OFF);
//     esp_wifi_deinit(); // free memory used by WiFi
#endif // ESP32

#ifdef LOG_TO_SERIAL
     sprintf(szTemp, "Received %d bytes from server\n", payload.length());
     Serial.print(szTemp);
#endif
//     StaticJsonDocument<80000> doc;
     DynamicJsonDocument doc(26000); // hopefully this is enough to capture the data; latest request returns 48k of text
#ifdef LOG_TO_SERIAL
     sprintf(szTemp, "JsonDocument::capacity() = %d\n", (int)doc.capacity());
     Serial.print(szTemp);
#endif
     DeserializationError err = deserializeJson(doc, payload);
     doc.shrinkToFit();
     if (err) {
#ifdef LOG_TO_SERIAL
       Serial.print("deserialization error ");
       Serial.println(err.c_str());
#endif
       return 0;
     }
#ifdef LOG_TO_SERIAL
     Serial.println("Deserialization succeeded!");
#endif
     JsonArray cca = doc["current_condition"].as<JsonArray>();
     JsonObject current_condition = cca[0];
//     JsonArray wd = current_condition["weatherDesc"].as<JsonArray>();
//     JsonObject weatherdesc = wd[0];
     rel_humid = current_condition["humidity"];
     feels_temp = current_condition["FeelsLikeC"];
     updated = String((const char *)current_condition["localObsDateTime"]); // last update time
     // If the update occurs late in the evening, set the sleep time to longer since we don't need to update
     // the display in the middle of the night
     strcpy(szTemp, updated.c_str());
     i = strlen(szTemp); // format is "2022-08-18 08:55 AM"
     iHour = atoi(&szTemp[i-8]);
     if (szTemp[i-2] == 'P' && iHour > 6) {
        i = 6 + (24-myTime.tm_hour); // hours to sleep until 6AM
        *iSleepTime = i * 60 * 60 * 1000; // sleep until 6AM
#ifdef LOG_TO_SERIAL
        Serial.println("late evening report; sleep until 6AM");
#endif
     }
     temp = current_condition["temp_C"];
     iWind = current_condition["windspeedKmph"];
 //    desc = String((const char *)weatherdesc["value"]);
     cc_icon = current_condition["weatherCode"]; 
     JsonArray wa = doc["weather"].as<JsonArray>();
     JsonObject weather = wa[0];
     JsonArray aa = weather["astronomy"].as<JsonArray>();
     #ifdef ARDUINO_ARCH_ESP32
     JsonArray ha = weather["hourly"].as<JsonArray>();
     #endif
     JsonObject astronomy = aa[0];
     sSunrise = String((const char *)astronomy["sunrise"]);
     sSunset = String((const char *)astronomy["sunset"]);
     mintemp = weather["mintempC"];
     maxtemp = weather["maxtempC"];
     #ifdef ARDUINO_ARCH_ESP32
     // get hourly info for today
     for (int i=0; i<8; i++) {
       iTemp[i] = ha[i]["tempC"];
       uvIndex[i] = (uint8_t)ha[i]["uvIndex"];
       iHumidity[i] = ha[i]["humidity"];
       iWeatherCode[i] = ha[i]["weatherCode"];
       iRainChance[i] = ha[i]["chanceofrain"];
     }
     // get hourly info for tomorrow
     weather = wa[1];
     ha = weather["hourly"].as<JsonArray>();
     for (int i=0; i<8; i++) {
       iTemp[i+8] = ha[i]["tempC"];
       iHumidity[i+8] = ha[i]["humidity"];
       iWeatherCode[i+8] = ha[i]["weatherCode"];
       iRainChance[i+8] = ha[i]["chanceofrain"];
     }
     #else // less capable WiFiNINA version
     memset(iRainChance, 0, sizeof(iRainChance));
     memset(uvIndex, 0, sizeof(uvIndex));
     uvIndex[0] = weather["uvIndex"];
     #endif
#ifdef LOG_TO_SERIAL
//     Serial.printf("Humidity = %d%%, temp = %dC, Conditions: %s\n", rel_humid, temp, desc.c_str());
     sprintf(szTemp, "sunrise: %s, sunset: %s, mintemp = %d, maxtemp = %d\n", sSunrise.c_str(), sSunset.c_str(), mintemp, maxtemp);
     Serial.print(szTemp);
#endif
     return 1;
   }
} /* GetWeather() */

void GetInternetTime()
{
// Initialize a NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(TZ_OFFSET);  //My timezone
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  unsigned long epochTime = timeClient.getEpochTime();
  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  memcpy(&myTime, ptm, sizeof(myTime)); // get the current time struct into a local copy
  rtc.setTime(epochTime); // set the ESP32's internal RTC to the correct time
//  Serial.printf("Current time: %02d:%02d:%02d\n", myTime.tm_hour, myTime.tm_min, myTime.tm_sec);
  timeClient.end(); // don't need it any more
} /* GetInternetTime() */

bool ConnectToInternet(void)
{
// Local instance of WiFiManager
WiFiManager wm;
bool res;

  //wm.resetSettings(); // DEBUG

  // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "E_Paper_AP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result
    wm.setEnableConfigPortal(false); // we want to know if connecting fails to inform user of what they're expected to do
    wm.setConfigPortalTimeout(60); // keep active for 1 minute
    res = wm.autoConnect("CYD_Weather"); // not password protected
    if (!res) { // failed to connect or no saved credentials, start the config portal
      lcd.fillScreen(TFT_BLACK);
      lcd.setFont(FONT_12x16);
      lcd.println("WiFi connection failed");
      lcd.println("or missing credentials");
      lcd.println("Use CYD_Weather/PW123456");
      lcd.println("and navigate to:");
      lcd.println("http://192.168.4.1");
      lcd.println("To configure future");
      lcd.println("automatic connections");
      lcd.print("Will timeout in 60 sec");
      res = wm.startConfigPortal("CYD_Weather", "PW123456");
      if (!res) {
          lcd.fillScreen(TFT_BLACK);
          lcd.println("Config timed out");
          lcd.println("Press reset to");
          lcd.println("try again");
          lcd.println("Shutting down...");
          delay(2000000000); // sleep forever
      }
    } else {
      lcd.println("WiFi Connected!");
#ifdef LOG_TO_SERIAL
      Serial.println("WiFi Connected!");
#endif
      return true;
    }
   return false;
} /* ConnectToInternet() */

void setup() {
//  int iTimeout;
lcd.begin(LCD);
lcd.fillScreen(TFT_BLACK);
lcd.setFont(FONT_12x16);
lcd.setTextColor(TFT_GREEN, TFT_BLACK);
lcd.println("Starting WiFi Manager...");
#ifdef LOG_TO_SERIAL
  Serial.begin(115200);
#endif // LOG_TO_SERIAL

// Prepare positions of clock digits
  iCharWidth = FONT_GLYPHS['0' - ' '].xAdvance;
  iColonWidth = FONT_GLYPHS[':' - ' '].xAdvance;
  iStartX = (lcd.width() - (4*iCharWidth + iColonWidth))/2;
  iDigitPos[0] = iStartX;
  iDigitPos[1] = iStartX+iCharWidth;
  iDigitPos[2] = iStartX+iCharWidth*2;
  iDigitPos[3] = iStartX+iColonWidth + iCharWidth*2;
  iDigitPos[4] = iDigitPos[3] + iCharWidth;
  iDigitPos[5] = lcd.width();

} /* setup() */

void lightSleep(uint64_t time_in_ms)
{
#ifdef ARDUINO_ARCH_ESP32
  esp_sleep_enable_timer_wakeup(time_in_ms * 1000L);
  esp_light_sleep_start();
#else
  delay(time_in_ms);
#endif
}

void deepSleep(uint64_t time_in_ms)
{
#ifdef ARDUINO_ARCH_ESP32
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF); // hibernation mode - only RTC powered
//  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
//  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL,         ESP_PD_OPTION_OFF);
  esp_sleep_enable_timer_wakeup(time_in_ms * 1000L);
  esp_deep_sleep_start();
#else
  delay(time_in_ms);
#endif
}

void DisplayTime(void)
{
char szTemp[2], szTime[32];
static char szOld[32] = "       ";
int i, iHour, iMin, iSec;
struct tm *ptm;
unsigned long epochTime;

  iHour = rtc.getHour();
  iMin = rtc.getMinute();
  iSec = rtc.getSecond();
#ifdef TWELVE_HOUR
    if (iHour > 12) iHour -= 12;
    else if (iHour == 0) iHour = 12;
#endif
    sprintf(szTime, "%02d:%02d", iHour, iMin);
    if (iSec & 0x1) { // flash the colon
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
} /* DisplayTime() */

void loop() {
int iSleepTime;
int i;

  if (ConnectToInternet() && GetWeather(&iSleepTime)) {
    GetInternetTime(); // update the internal RTC with accurate time
    DisplayWeather();
    for (i=0; i<3600; i++) { // update weather every hour and time every second
      DisplayTime();
      delay(1000);
    }
  }
  delay(30000);
} /* loop() */
