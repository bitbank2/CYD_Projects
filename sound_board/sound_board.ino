//
// Sound board for ESP32-S3 and ESP32
// requires Arduino ESP32 support 3.0.0 or higher
// Written by Larry Bank
// Copyright (c) 2024 BitBank Software, Inc.
//

// Define the board you're using here (only 1)
//
// Define this macro to configure for the JC4827W543
//#define CYD_543
// Define this macro to configure for the ESP32-2432022 (cap touch)
//#define CYD_22C
// Define this macro for the WT32_SC01_Plus board
//#define CYD_WT32
// Define this macro for the original CYD
#define CYD
// Define this macro for the 2-USB 2.8" CYD
//#define CYD_2USB

#include <bb_spi_lcd.h>
// 11Khz 8-bit WAV files
#include "doh.h"
#include "sorry_dave.h"
#include "pacman_death.h"
#include "looking.h"

const uint8_t *sound_data[4] = {doh, pacman_death, looking, sorry_dave};
size_t sound_len[4] = {sizeof(doh), sizeof(pacman_death), sizeof(looking), sizeof(sorry_dave)};

#ifdef CYD
#define ESP32_OLD
#define LCD DISPLAY_CYD
#endif

#ifdef CYD_2USB
#define ESP32_OLD
#define LCD DISPLAY_CYD_2USB
#endif

#ifdef CYD_543
#define LCD DISPLAY_CYD_543
#define EXAMPLE_STD_BCLK    GPIO_NUM_42     // I2S bit clock io number
#define EXAMPLE_STD_WS      GPIO_NUM_2      // I2S word select io number
#define EXAMPLE_STD_DOUT    GPIO_NUM_41     // I2S data out io number
#endif

#ifdef CYD_22C
#define CAP_TOUCH
#define ESP32_OLD
#define LCD DISPLAY_CYD_22C
#define AUDIO_PIN 26
#define TOUCH_SDA 21
#define TOUCH_SCL 22
#define TOUCH_INT 23
#define TOUCH_RST -1
#endif

#ifdef CYD_WT32
#define CAP_TOUCH
// WT32-SC01 Plus
#define LCD DISPLAY_WT32_SC01_PLUS
#define EXAMPLE_STD_BCLK    GPIO_NUM_36     // I2S bit clock io number
#define EXAMPLE_STD_WS      GPIO_NUM_35     // I2S word select io number
#define EXAMPLE_STD_DOUT    GPIO_NUM_37     // I2S data out io number
#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT 7
#define TOUCH_RST -1
#endif

#ifdef CAP_TOUCH
#include <bb_captouch.h>
BBCapTouch bbct;
#endif

BB_SPI_LCD lcd;
int iWidth, iHeight;

#ifdef ESP32_OLD
#include "driver/i2s.h"
#else
#include "driver/i2s_std.h"
static i2s_chan_handle_t                tx_chan;        // I2S tx channel handler
#endif

#define EXAMPLE_STD_DIN     I2S_GPIO_UNUSED     // I2S data in io number

#define EXAMPLE_BUFF_SIZE               2048

#ifdef ESP32_OLD
int vol_shift = 5;
#else
int vol_shift = 4;
#endif

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

#ifdef ESP32_OLD
   i2s_write(I2S_NUM_0, &sample16, 2, &bytes_written, portMAX_DELAY);
   i2s_write(I2S_NUM_0, &sample16, 2, &bytes_written, portMAX_DELAY);
#else
   //  return i2s_write_bytes((i2s_port_t)i2s_num, (const char *)&sample, sizeof(uint8_t), 100);
   i2s_channel_write(tx_chan, &sample16, 2, &bytes_written, 100);
#endif
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
           //Serial.println("HEADER_RIFF");
        }
        break;
        case HEADER_FMT:
           memcpy(&wavProps, pSound, sizeof(wavProperties_t));
           pSound += sizeof(wavProperties_t);
           state = HEADER_DATA;
           i2s_set_sample_rates(I2S_NUM_0, wavProps.sampleRate); 
           //Serial.printf("Setting sample rate to %d\n", wavProps.sampleRate);
        break;
        case HEADER_DATA:
        uint32_t chunkId, chunkSize;
        memcpy(&chunkId, pSound, 4);
        pSound += 4;
        if(chunkId == CCCC('d', 'a', 't', 'a')){
          //Serial.println("HEADER_DATA");
        }
        memcpy(&chunkSize, pSound, 4);
        pSound += 4;
        //Serial.println("prepare data");
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
  //Serial.println("done!");
} /* PlaySound() */

void i2sInit(void)
{
#ifdef ESP32_OLD
i2s_config_t i2s_config = {
     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
     .sample_rate = 11000,
     .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, /* the DAC module will only take the 8bits from MSB */
     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
     .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_I2S_MSB,
     .intr_alloc_flags = 0, // default interrupt priority
     .dma_buf_count = 8,
     .dma_buf_len = 64,
     .use_apll = 1
    };
        //initialize i2s with configurations above
        i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        i2s_set_pin(I2S_NUM_0, NULL);
        //set sample rates of i2s to sample rate of wav file
        i2s_set_sample_rates(I2S_NUM_0, 11000); 
#else
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
#endif
}
void setup(void)
{
  int x, y;
#ifdef ESP32_OLD
  //Serial.begin(115200);
#endif
  lcd.begin(LCD);
  iWidth = lcd.width();
  iHeight = lcd.height();
#ifdef CAP_TOUCH
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  bbct.setOrientation(270, iHeight, iWidth); // 270 degrees, native width, native height
#else
  lcd.rtInit();
#endif

  lcd.fillScreen(TFT_BLACK);
  if (lcd.width() <= 320) {
    lcd.setFont(FONT_8x8);
  } else { // big display
    lcd.setFont(FONT_12x16);
  }
  y = (iHeight-40)/2;
  lcd.fillRoundRect(20, 20, (iWidth-40)/2, y, 20, TFT_WHITE);
  lcd.setTextColor(TFT_BLACK, TFT_WHITE);
  if (lcd.width() <= 320) {
    lcd.setCursor(76, 20 + (y-16)/2);
  } else {
    lcd.setCursor(110, 20 + (y-16)/2);
  }
  lcd.print("Doh!");
  lcd.fillRoundRect(40+(iWidth-40)/2, 20, (iWidth-40)/2, y, 20, TFT_YELLOW);
  lcd.setTextColor(TFT_BLACK, TFT_YELLOW);
  if (lcd.width() <= 320) {
    lcd.setCursor(200, 20 + (y-16)/2);
  } else {
    lcd.setCursor(300, 20 + (y-16)/2);
  }
  lcd.print("PacMan Death");
  lcd.fillRoundRect(20, 40+(iHeight-40)/2, (iWidth-40)/2, y, 20, TFT_CYAN);
  lcd.setTextColor(TFT_BLACK, TFT_CYAN);
 if (lcd.width() <= 320) {
   lcd.setCursor(68, (y-16)/2 + 40 + (iHeight-40)/2);
 } else {
   lcd.setCursor(92, (y-16)/2 + 40 + (iHeight-40)/2);
 }
  lcd.print("Bogart");
  lcd.fillRoundRect(40+(iWidth-40)/2, 40+(iHeight-40)/2, (iWidth-40)/2, y, 20, TFT_MAGENTA);
  lcd.setTextColor(TFT_BLACK, TFT_MAGENTA);
 if (lcd.width() <= 320) {
    lcd.setCursor(212, (y-16)/2 + 40 + (iHeight-40)/2);
 } else {
    lcd.setCursor(312, (y-16)/2 + 40 + (iHeight-40)/2);
 }
  lcd.print("Sorry Dave");
  i2sInit();
}

void loop(void)
{
  TOUCHINFO ti;
  int i;

#ifdef CAP_TOUCH
  if (bbct.getSamples(&ti) && ti.count >= 1) { // a touch event
#else
  if (lcd.rtReadTouch(&ti) && ti.count >= 1) { // a touch event
#endif
     i = (ti.x[0] / (iWidth/2));
     if (ti.y[0] > (iHeight/2)) i += 2;
     //Serial.printf("Playing sound %d\n", i);
#ifdef ESP32_OLD
     i2s_start(I2S_NUM_0);
     PlaySound(sound_data[i], sound_len[i]);
     i2s_stop(I2S_NUM_0); // if I don't stop the I2S output, it makes buzzing/thumping noises continuously
#else
     i2s_channel_enable(tx_chan);
     PlaySound(sound_data[i], sound_len[i]);
     i2s_channel_disable(tx_chan);
#endif
  }
}
