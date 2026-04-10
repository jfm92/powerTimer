#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#include "powerTimer.h"
#include "Wire.h"

#define TFT_CS    11
#define TFT_DC    10
#define TFT_MOSI   6
#define TFT_SCLK   5
#define TFT_RST    7
#define TFT_BL    -1

#define SDA_PIN 22
#define SCL_PIN 23
#define POWER_OFF_PIN 3

#define SCREEN_W 320
#define SCREEN_H 206
#define BUF_LINES 30

#define DISPLAY_ON_TIME 5 //Secs
#define TIME_TO_SLEEP  60 //Secs

#define POWER_TIMER_ON

#ifdef POWER_TIMER_ON
  powerTimer powerTimer;
#endif

Adafruit_BME680 bme(&Wire); 

static lv_color_t draw_buf_data[SCREEN_W * BUF_LINES];
static lv_display_t *display;

static lv_obj_t *lbl_temp_val;
static lv_obj_t *lbl_hum_val;
static lv_obj_t *lbl_status;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    TFT_DC,    // DC
    TFT_CS,    // CS
    TFT_SCLK,  // SCLK
    TFT_MOSI,  // MOSI
    -1       // Not MISO
);

Arduino_ST7789 *tft = new Arduino_ST7789(bus, TFT_RST, 1, true,
                                          SCREEN_H , SCREEN_W,
                                          0, 0);

// ─── Flush ──────────────────────────────────────────────────
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    tft->draw16bitRGBBitmap(area->x1 , area->y1+20, (uint16_t *)px_map,
                            area->x2 - area->x1 + 1,
                            area->y2 - area->y1 + 1);
    lv_display_flush_ready(disp);
}

// ─── Meas Card ───────────────────────────────────────────
void create_card(lv_obj_t   *parent,
                 int         x,
                 const char *titulo,
                 const char *unidad,
                 lv_color_t  color,
                 lv_obj_t  **out_label)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 148, 150);
    lv_obj_align(card, LV_ALIGN_LEFT_MID, x, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0d1b2a), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Tittle
    lv_obj_t *lbl_titulo = lv_label_create(card);
    lv_label_set_text(lbl_titulo, titulo);
    lv_obj_set_style_text_font(lbl_titulo, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_titulo, color, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(lbl_titulo, 2, LV_PART_MAIN);
    lv_obj_align(lbl_titulo, LV_ALIGN_TOP_MID, 0, 12);

    // Separator
    lv_obj_t *sep = lv_obj_create(card);
    lv_obj_set_size(sep, 110, 1);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_bg_color(sep, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);

    // Value
    lv_obj_t *lbl_val = lv_label_create(card);
    lv_label_set_text(lbl_val, "--");
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(lbl_val, LV_ALIGN_CENTER, 0, 6);
    *out_label = lbl_val;

    // Unit
    lv_obj_t *lbl_unit = lv_label_create(card);
    lv_label_set_text(lbl_unit, unidad);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(lbl_unit, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// ─── Main UI ───────────────────────────────────────────
void create_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x060d17), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Temperature card — left
    create_card(scr,
                8,
                "TEMPERATURE",
                "\xC2\xB0""C",
                lv_color_hex(0xff6b6b),
                &lbl_temp_val);

    // Humidity card — rigth
    create_card(scr,
                164,
                "HUMIDITY",
                "%",
                lv_color_hex(0x00d4ff),
                &lbl_hum_val);
}

// ─── Update sensor values───────────────────────────────────────
void update_sensors(float temp, float hum) {
    char buf[12];

    snprintf(buf, sizeof(buf), "%.1f", temp);
    lv_label_set_text(lbl_temp_val, buf);

    snprintf(buf, sizeof(buf), "%.1f", hum);
    lv_label_set_text(lbl_hum_val, buf);
}

void setup() {
    Serial.begin(115200);

    //I2C Init
    Wire.begin(SDA_PIN,SCL_PIN);

    #ifdef POWER_TIMER_ON
      if(!powerTimer.begin(POWER_OFF_PIN)){
          Serial.println("powerTimer Intialization failed.");
          while (1);
      }
    #endif

    if (!bme.begin()) {
      Serial.println("Could not find a valid BME680 sensor, check wiring!");
      #ifdef POWER_TIMER_ON
        powerTimer.powerOff();
      #else
        while(1);
      #endif
    }

    //Sensor Config
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms

    //Sensor measurement
    if (!bme.performReading()) {
      Serial.println("Failed to perform reading :(");
      #ifdef POWER_TIMER_ON
        powerTimer.powerOff();
      #else
        while(1);
      #endif
    }

    float temperature = bme.temperature;
    float humidity = bme.humidity;

    tft->begin();
    
    // UI Initialization
    lv_init();

    display = lv_display_create(SCREEN_W, SCREEN_H);

    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_buffers(display, draw_buf_data, NULL,
                           sizeof(draw_buf_data),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    create_ui();
    
    update_sensors(temperature, humidity);
}

void loop() {
    lv_timer_handler();

    static uint32_t last = 0;
    if (millis() - last > DISPLAY_ON_TIME*1000) {
        last = millis();
        
        #ifdef POWER_TIMER_ON
          if(powerTimer.readAlarmInterruptFlag()){
              Serial.println("Device wake by alarm interrupt.");
              powerTimer.clearAlarmInterruptFlag();
          }

          //Reset RTC
          powerTimer.clearInterrupts();
          if(powerTimer.setTime(0,0,0,1,1,1,2025)){
              Serial.println("Error setting date.");
              powerTimer.powerOff();
          }

          powerTimer.enableAlarmInterrupt(1, 0, 1, false);
          Serial.println("Alarm interrupt enable in 1 minute.");

          powerTimer.powerOff();
        #else
          esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000ULL);
          Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
          esp_deep_sleep_start();
        #endif

    }

    delay(5);
}