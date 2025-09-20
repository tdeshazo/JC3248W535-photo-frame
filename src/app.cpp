#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>

#include <lvgl.h>
#include "esp_bsp.h"
#include "lv_port.h"
#include "display.h"

#define LVGL_PORT_ROTATION_DEGREE 270 // 0 / 90 / 180 / 270
#define SDCARD_CS 10
#define SDCARD_MISO 13
#define SDCARD_MOSI 11
#define SDCARD_SCK 12
SPIClass sdSPI(HSPI);

/* ---- Slideshow state ---- */
static std::vector<String> jpgList; // S:/pic/… paths
static size_t current = 0;
static lv_obj_t *img_widget = nullptr;
static lv_timer_t *slide_timer = nullptr; // Timer to advance slides

/* -------------------------------------------------------------------------*/
static bool is_jpg(const char *name)
{
    String s(name);
    s.toLowerCase();
    return s.endsWith(".jpg") || s.endsWith(".jpeg");
}

static void scan_sd_for_jpg()
{
    Serial.println("Scanning for directory...");
    File dir = SD.open("/pic");
    if (!dir)
    {
        Serial.println("pic not found");
        return;
    }

    File f;
    while ((f = dir.openNextFile()))
    {
        if (!f.isDirectory() && is_jpg(f.name()))
        {
            // prepend drive letter so LVGL can open it directly
            jpgList.push_back(String("S:") + String(f.path()));
        }
        f.close();
    }
    Serial.printf("Found %d JPGs\n", jpgList.size());
}

/* ---- LVGL timer: advance slide ---- */
static void next_slide(lv_timer_t *)
{
    if (jpgList.empty())
        return;
    current = (current + 1) % jpgList.size();
    lv_img_set_src(img_widget, jpgList[current].c_str());
}

static void prev_slide()
{
    if (jpgList.empty()) return;
    current = (current + jpgList.size() - 1) % jpgList.size();   // wrap‑around
    lv_img_set_src(img_widget, jpgList[current].c_str());
}
/* ---- Arduino setup ---- */
void setup()
{
    Serial.begin(115200);
    delay(500);

    /* --- Init display exactly like the BSP example ---- */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate = LV_DISP_ROT_270,
#else
        .rotate = LV_DISP_ROT_NONE,
#endif
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    /* ---- Mount SD ---- */
    sdSPI.begin(SDCARD_SCK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);

    if (!SD.begin(SDCARD_CS, sdSPI))
    {
        Serial.println("SD mount failed!");
        while (true)
            delay(1000);
    }

    Serial.println("SD mounted!");

    /* ---- Register FS driver & scan card ---- */
    lv_fs_fatfs_init();
    scan_sd_for_jpg();

    /* ---- Build GUI ---- */
    bsp_display_lock(0);

    img_widget = lv_img_create(lv_scr_act());
    lv_obj_center(img_widget);
    
    /* ---- Screen gesture ---- */
    lv_obj_add_event_cb(lv_scr_act(), [](lv_event_t * e){
        /* Portable across 8.3.x */
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if(dir == LV_DIR_RIGHT)      next_slide(nullptr);
        else if(dir == LV_DIR_LEFT)  prev_slide();
        else                         return;      // ignore up/down taps

        lv_timer_reset(slide_timer);             // restart 10‑s timer
    },
    LV_EVENT_GESTURE, nullptr);

    if (!jpgList.empty())
        lv_img_set_src(img_widget, jpgList.front().c_str());

    lv_img_cache_set_size(1); // keep one decoded frame

    // lv_timer_create(next_slide, 5000, nullptr);
    slide_timer = lv_timer_create(next_slide, 10000, nullptr);

    bsp_display_unlock();
}

/* ---------- Arduino loop ---------------------------------------------------*/
void loop() {}
