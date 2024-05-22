//
// Sound board for ESP32-S3
// requires Arduino ESP32 support 3.0.0-rc1 or higher
//
#include <bb_spi_lcd.h>
#include "driver/i2s_std.h"
// 11Khz 8-bit WAV files
#include "doh.h"
#include "sorry_dave.h"
#include "pacman_death.h"
#include "looking.h"

const uint8_t *sound_data[4] = {doh, pacman_death, looking, sorry_dave};
size_t sound_len[4] = {sizeof(doh), sizeof(pacman_death), sizeof(looking), sizeof(sorry_dave)};

//
// Tested on WT32_SC01_Plus and JC4827W543
//
// Define this macro to configure for the JC4827W543
#define CYD_543

#ifdef CYD_543
// WT32-SC01 Plus
#define LCD DISPLAY_CYD_543
#define EXAMPLE_STD_BCLK    GPIO_NUM_42     // I2S bit clock io number
#define EXAMPLE_STD_WS      GPIO_NUM_2      // I2S word select io number
#define EXAMPLE_STD_DOUT    GPIO_NUM_41     // I2S data out io number
#else
// WT32-SC01 Plus
#define LCD DISPLAY_WT32_SC01_PLUS
#define EXAMPLE_STD_BCLK    GPIO_NUM_36     // I2S bit clock io number
#define EXAMPLE_STD_WS      GPIO_NUM_35     // I2S word select io number
#define EXAMPLE_STD_DOUT    GPIO_NUM_37     // I2S data out io number
#include <bb_captouch.h>
BBCapTouch bbct;
#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT 7
#define TOUCH_RST -1
#endif

BB_SPI_LCD lcd;
int iWidth, iHeight;
// CYD_543
//#define LCD DISPLAY_CYD_543
//#define EXAMPLE_STD_BCLK    GPIO_NUM_42     // I2S bit clock io number
//#define EXAMPLE_STD_WS      GPIO_NUM_2     // I2S word select io number
//#define EXAMPLE_STD_DOUT    GPIO_NUM_41     // I2S data out io number

#define EXAMPLE_STD_DIN     I2S_GPIO_UNUSED     // I2S data in io number

#define EXAMPLE_BUFF_SIZE               2048

static i2s_chan_handle_t                tx_chan;        // I2S tx channel handler
int vol_shift = 4;

#define CCCC(c1, c2, c3, c4)    ((c4 << 24) | (c3 << 16) | (c2 << 8) | c1)

/* these are data structures to process wav file */
typedef enum headerState_e {
    HEADER_RIFF, HEADER_FMT, HEADER_DATA, DATA
} headerState_t;

typedef struct wavRiff_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
} wavRiff_t;
typedef struct wavProperties_s {
    uint32_t chunkID;
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} wavProperties_t;
headerState_t state = HEADER_RIFF;
wavProperties_t wavProps;

/* write sample data to I2S */
void i2s_write_sample_nb(uint8_t sample){
  uint16_t sample16 = (sample << vol_shift);
  size_t bytes_written;

   //  return i2s_write_bytes((i2s_port_t)i2s_num, (const char *)&sample, sizeof(uint8_t), 100);
   i2s_channel_write(tx_chan, &sample16, 2, &bytes_written, 100);
}

void PlaySound(const uint8_t *pSound, int iLen)
{
int c = 0;
int n;
  /* Play wav file */
const uint8_t *pEnd = &pSound[iLen];

  state = HEADER_RIFF;

  while (pSound < pEnd) {
      switch(state){
        case HEADER_RIFF:
        wavRiff_t wavRiff;
        /* these are function to process wav file */
        memcpy(&wavRiff, pSound, sizeof(wavRiff_t));
        pSound += sizeof(wavRiff_t);
        if(wavRiff.chunkID == CCCC('R', 'I', 'F', 'F') && wavRiff.format == CCCC('W', 'A', 'V', 'E')){
            state = HEADER_FMT;
            Serial.println("HEADER_RIFF");
        }
        break;
        case HEADER_FMT:
           memcpy(&wavProps, pSound, sizeof(wavProperties_t));
           pSound += sizeof(wavProperties_t);
           state = HEADER_DATA;
        break;
        case HEADER_DATA:
        uint32_t chunkId, chunkSize;
        memcpy(&chunkId, pSound, 4);
        pSound += 4;
        if(chunkId == CCCC('d', 'a', 't', 'a')){
          Serial.println("HEADER_DATA");
        }
        memcpy(&chunkSize, pSound, 4);
        pSound += 4;
        Serial.println("prepare data");
        state = DATA;
        /* after processing wav file, it is time to process music data */
        break;
        case DATA:
        uint8_t data;
        data = *pSound++;
        i2s_write_sample_nb(data);
        break;
      }
    }
  Serial.println("done!");
} /* PlaySound() */

void i2sInit(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(11000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = EXAMPLE_STD_BCLK,
            .ws   = EXAMPLE_STD_WS,
            .dout = EXAMPLE_STD_DOUT,
            .din  = EXAMPLE_STD_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    /* Before write data, start the tx channel first */
}
void setup(void)
{
  int x, y;
  //Serial.begin(115200);
  lcd.begin(LCD);
  iWidth = lcd.width();
  iHeight = lcd.height();
#ifdef CYD_543
  lcd.rtInit();
#else
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  bbct.setOrientation(270, 320, 480); // 270 degrees, native width, native height
#endif
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(FONT_12x16);
  y = (iHeight-40)/2;
  lcd.fillRoundRect(20, 20, (iWidth-40)/2, y, 20, TFT_WHITE);
  lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  lcd.setCursor(110, 20 + (y-16)/2);
  lcd.print("Doh!");
  lcd.fillRoundRect(40+(iWidth-40)/2, 20, (iWidth-40)/2, y, 20, TFT_YELLOW);
  lcd.setTextColor(TFT_BLACK, TFT_YELLOW);
  lcd.setCursor(300, 20 + (y-16)/2);
  lcd.print("PacMan Death");
  lcd.fillRoundRect(20, 40+(iHeight-40)/2, (iWidth-40)/2, y, 20, TFT_CYAN);
  lcd.setTextColor(TFT_BLACK, TFT_CYAN);
  lcd.setCursor(92, (y-16)/2 + 40 + (iHeight-40)/2);
  lcd.print("Bogart");
  lcd.fillRoundRect(40+(iWidth-40)/2, 40+(iHeight-40)/2, (iWidth-40)/2, y, 20, TFT_MAGENTA);
  lcd.setTextColor(TFT_BLACK, TFT_MAGENTA);
  lcd.setCursor(312, (y-16)/2 + 40 + (iHeight-40)/2);
  lcd.print("Sorry Dave");
  i2sInit();
}

void loop(void)
{
  TOUCHINFO ti;
  int i;

#ifdef CYD_543
  if (lcd.rtReadTouch(&ti) && ti.count >= 1) { // a touch event
#else
  if (bbct.getSamples(&ti) && ti.count >= 1) { // a touch event
#endif
     i = (ti.x[0] / (iWidth/2));
     if (ti.y[0] > (iHeight/2)) i += 2;
     i2s_channel_enable(tx_chan);
     PlaySound(sound_data[i], sound_len[i]);
     i2s_channel_disable(tx_chan);
  }
}
