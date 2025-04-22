
//
// ESP32 IP camera stream viewer
//
// Written by Larry Bank (bitbank@pobox.com)
// April 17, 2025
// Some unsecured web cameras can provide a continuous stream of JPEG images.
// This program is able to read from their streams and display the images
// using the ESP32 HTTPClient and my JPEGDEC library
//
// Copyright 2025 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
#include <bb_spi_lcd.h>
#include <JPEGDEC.h>
#include <WiFi.h>
#include <HTTPClient.h>
BB_SPI_LCD lcd;
JPEGDEC jpg;
HTTPClient http;
//
// We can easily fit a decent quality 320x240 JPEG in 32k
// and the older ESP32 can allocate 32k without running out of heap space
//
#define BUFFER_SIZE 32768
uint8_t *pBuf;
// Change these to access your local access point
const char *SSID = "your_ssid";
const char *PW = "your_password";
// This is a sample URL - a dog care area somewhere in the Eastern USA
const char *url = "http://173.165.152.129:8011/axis-cgi/mjpg/video.cgi?resolution=320x240&compression=60";
// 10 seconds WIFI connection timeout (1/2 second intervals)
#define MAX_TIMEOUT 20
// Define this to show the connection/download status in the serial terminal
#define SHOW_DIAGNOSTICS
// forward references
void ShowFrame(uint8_t *pFrame, int iSize);
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
  lcd.pushPixels((uint16_t *)pDraw->pPixels, pDraw->iWidth * pDraw->iHeight, DRAW_TO_LCD | DRAW_WITH_DMA);
  return 1;
} /* JPEGDraw() */

//
// Open an HTTP IP camera stream (JPEG only) with the given URL
// This code will stay here, receiving images until the stream disconnects
//
void StreamVideo(const char *url)
{
int i, j, avail, httpCode;
uint8_t c;
int iCount, iPayloadSize;
long l;
bool bFound;
WiFiClient * stream;
char szTemp[256]; // to hold the http head we might need to parse manually
#ifdef SHOW_DIAGNOSTICS
    Serial.printf("Sending GET request to %s\n", url);
#endif
    http.begin(url);
    httpCode = http.GET();  //send GET request
    if (httpCode != 200) {
#ifdef SHOW_DIAGNOSTICS
        Serial.print("Error on HTTP request: ");
        Serial.println(httpCode);
#endif
        http.end();
        return;
    } else {
#ifdef SHOW_DIAGNOSTICS
        Serial.println("GET request succeeded (return code = 200)");
#endif
        stream = http.getStreamPtr();
        while (http.connected()) { // keep reading the stream as long as we stay connecte
            i = j = 0;
            bFound = false;
            iPayloadSize = -1;
            vTaskDelay(2); // allow a little time for data to arrive
find_header:
            while (i < sizeof(szTemp) && !bFound) {
              c = stream->read();
              if (c == 0xff) {
                 c = stream->peek();
                 if (c == 0xd8) {
                     bFound = true; // found the start of the JPEG data
                 } else { // we got out of sync, sift through the data until we get to an HTTP header
                     i = 0;
                     while (http.connected() && i == 0) {
                        c = stream->read();
                        if (c == 0xff && stream->peek() == 0xd9) { // found the end of a frame
                           i = 1; // break out of this loop
                           stream->read(); // get rid of the 0xd9
                        }
                     }
#ifdef SHOW_DIAGNOSTICS
                     Serial.println("successfully resync'd with the feed");
#endif
                     i = 0;
                 }
              } else {
                szTemp[i++] = c;
              }
            }
#ifdef SHOW_DIAGNOSTICS
            Serial.printf("Got http headers %d\n", i);
#endif
            for (j=0; j<i-16; j++) {
              if (memcmp(&szTemp[j], "Content-Length", 14) == 0) {
                // we get the info we need
                iPayloadSize = atoi(&szTemp[j+16]);
#ifdef SHOW_DIAGNOSTICS
                Serial.printf("Payload size: %d\n", iPayloadSize);
#endif
                j = i;
              }
            }
            if (iPayloadSize == -1) { // didn't get the info we need, keep looking
               i = 0;
               goto find_header;
            }
            pBuf[0] = 0xff; // store the JPEG data here
            iCount = 1;
            l = millis();
            // Allow 60 seconds to receive the compressed data
            while (iCount < iPayloadSize && (millis() - l) < 60000) {
               if (avail = stream->available() > 0) {
                   if (iCount + avail > iPayloadSize) avail = iPayloadSize - iCount; // only read the JPEG payload here
                   stream->readBytes(&pBuf[iCount], avail);
                   iCount += avail;
#ifdef SHOW_DIAGNOSTICS
                   if ((iCount & 0xffff) == 0) { // show progress with a "." every 64k
                      Serial.print(".");
                   }
#endif
               } else {
                   vTaskDelay(2); // allow time for data to receive
               }
            } // while receiving the JPEG data
#ifdef SHOW_DIAGNOSTICS
            l = millis() - l;
            Serial.printf("\nReceived all data in %d ms\n", (int)l);
#endif
            ShowFrame(pBuf, iPayloadSize); // display the image
         } // while (connected)
    } // server returned 200
} /* StreamVideo() */

//
// Connect to a WiFi access point with the given ssid and password
// returns TRUE for success, FALSE for failure
//
bool ConnectWiFi(const char *ssid, const char *password) {
int iTimeout = 0;
   // Connect to wifi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to wifi");
    while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && iTimeout < MAX_TIMEOUT) {
      delay(500); // allow up to 10 seconds to connect
      iTimeout++;
      Serial.print(".");
    }
    if (iTimeout == MAX_TIMEOUT) {
      return false;
    }
  return true;
} /* ConnectWiFi() */
//
// Display the current JPEG image
//
void ShowFrame(uint8_t *pFrame, int iSize)
{
int w, h, xoff, yoff;
int iFlags = 0;

    if (jpg.openRAM(pFrame, iSize, JPEGDraw)) { // pass the data and its size
        jpg.setPixelType(RGB565_BIG_ENDIAN); // bb_spi_lcd uses big-endian RGB565 pixels
        // if the image is smaller than the LCD dimensions, center it
        w = jpg.getWidth();
        h = jpg.getHeight();
#ifdef SHOW_DIAGNOSTICS
        Serial.printf("JPEG opened, w=%d, h=%d\n", w, h);
#endif
        if (w > lcd.width()) { // scale it down
            iFlags = JPEG_SCALE_HALF;
            w /= 2; h /= 2;
            if (w > lcd.width()) { // still too big
                iFlags = JPEG_SCALE_QUARTER;
                w /= 2; h /= 2;
            }
        }
        xoff = (lcd.width() - w)/2;
        yoff = (lcd.height() - h)/2;
        if (jpg.decode(xoff,yoff,iFlags)) { // center the image and use the scaling option we selected
           Serial.println("Successfully decoded");
        } else {
           Serial.printf("Last error: %d\n", jpg.getLastError());
        }
  } else { // successfully opened the jpeg
      Serial.printf("JPEG failed, data: 0x%02x,0x%02x\n", pFrame[0], pFrame[1]);
  }
} /* ShowFrame() */

void setup() {
#ifdef SHOW_DIAGNOSTICS
  Serial.begin(115200);
#endif
// Set this to one of the more than 40 supported display types
  lcd.begin(DISPLAY_CYD_543);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_GREEN);
  lcd.setFont(FONT_12x16);
  pBuf = (uint8_t *)malloc(BUFFER_SIZE);
  if (!pBuf) {
    lcd.setTextColor(TFT_RED);
    lcd.println("Memory alloc error!");
    lcd.println("Halted");
    while (1) {
      vTaskDelay(1); // halt here
    }
  }
  lcd.print("Traffic/Web cam Viewer");
} /* setup() */

void loop() {

  while (1) {
    // Re-connect if we lose the connection
      if (WiFi.status() != WL_CONNECTED) { // try to connect
          if (!ConnectWiFi(SSID, PW)) {
              lcd.fillScreen(TFT_BLACK);
              lcd.setTextColor(TFT_RED);
              lcd.println("WiFi failed, halting...");
              lcd.println("Power off/reset");
              lcd.println("To try again");
              while (1) {
                  vTaskDelay(1);
              }
          } // if connection failed
          lcd.println(" - Connected!");
      } // if need to connect
      // Stream JPEG frames from the url, reconnects (goes around the loop again) if disconnected
      StreamVideo(url); // if it returns, it means that there was a stream error, so re-open it
  } // while (1)
} /* loop() */

