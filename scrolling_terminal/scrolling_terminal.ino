//
// Hardware Scrolling Terminal
// written by Larry Bank
//
// This sketch implements a simple serial terminal
// and uses hardware scrolling of the LCD when the text
// reaches the bottom line.
//
#include <bb_spi_lcd.h>
BB_SPI_LCD lcd;
int iCol, iRow, iCols, iRows;
int iScroll = 0;

//#define CYD_543

#ifdef CYD_543
#define LCD DISPLAY_CYD_543
#define FONT FONT_12x16
#define CHAR_WIDTH 12
#define CHAR_HEIGHT 16
#else // original CYD
#define LCD DISPLAY_CYD
#define ORIENTATION 0
#define FONT FONT_8x8
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#endif

void ScrollOneLine(void)
{
   // Erase rectangle where the new text will be written
   lcd.setCursor(iCol * CHAR_WIDTH, iScroll); // new text will be drawn here
   lcd.fillRect(0, iScroll, lcd.width(), CHAR_HEIGHT, TFT_BLACK);
   iScroll += CHAR_HEIGHT;
   iScroll %= lcd.height();
   lcd.setScrollPosition(iScroll);
} /* ScrollOneLine() */

void setup() {
  lcd.begin(LCD);
#ifdef ORIENTATION
  lcd.setRotation(ORIENTATION);
#endif
  lcd.fillScreen(TFT_BLACK);
  iCols = lcd.width() / CHAR_WIDTH;
  iRows = lcd.height() / CHAR_HEIGHT;
  iCol = 0; iRow = 4;
  lcd.setFont(FONT); // use a large font
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.println("Scrolling Serial Terminal Demo");
  lcd.println("by Larry Bank");
  lcd.println("Waiting for serial connection");
  lcd.println("Config: 115200 baud, 8N1, CRLF");
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  Serial.begin(115200);
}

void loop() {
  char c, cTemp[4];
    cTemp[1] = 0; // used for 1 character "strings"

    while (1) {
      if (!Serial.available()) continue;
      c = Serial.read(); // read a single byte from the serial port
      if (c >= ' ') { // printable character
         if (iCol == iCols-1) { // next line
            if (iRow < iRows-1) {
              iRow++;
              lcd.print("\n");
            } else {
              ScrollOneLine();
            }
            iCol = 0;
         }
         cTemp[0] = c;
         lcd.print(cTemp);
         iCol++;
      } else { // control characters
        if (c == 0xd) {
          iCol = 0; // carriage return
        } else if (c == 0xa) { // line feed
          if (iRow < iRows-1) {
            lcd.print("\n");
            iRow++;
          } else {
            ScrollOneLine();
          }
        } else if (c == 0x8) { // backspace
           if (iCol) {
             iCol--;
             lcd.setCursor(iCol * CHAR_WIDTH, -1);
           }
        }
      }
    } // while
} /* loop() */
