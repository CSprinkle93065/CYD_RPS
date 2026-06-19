/**
 * @file touch_mapping.cpp
 * @brief XPT2046 raw ADC to screen-pixel mapping for CYD_RPS.
 *
 * Implements LL-035: raw ADC (0–4095) is mapped to screen pixels (0–239 ×
 * 0–319 in portrait) and clamped to display bounds. All touch events are
 * logged to serial for debugging.
 */

#include "touch_mapping.h"
#include "hal_config.h"

#include <Arduino.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>

static XPT2046_Touchscreen* s_touchscreen = nullptr;

static int16_t clamp(int16_t v, int16_t min, int16_t max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

void touch_map_adc_to_screen(uint16_t raw_x, uint16_t raw_y,
                             int16_t* screen_x, int16_t* screen_y)
{
    if (screen_x == nullptr || screen_y == nullptr) {
        return;
    }

    int16_t x = map(static_cast<int32_t>(raw_x),
                    TOUCH_MIN_X_RAW, TOUCH_MAX_X_RAW,
                    0, DISP_WIDTH - 1);
    int16_t y = map(static_cast<int32_t>(raw_y),
                    TOUCH_MIN_Y_RAW, TOUCH_MAX_Y_RAW,
                    0, DISP_HEIGHT - 1);

    *screen_x = clamp(x, 0, DISP_WIDTH - 1);
    *screen_y = clamp(y, 0, DISP_HEIGHT - 1);
}

void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    (void)drv;

    if (s_touchscreen == nullptr) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (s_touchscreen->touched()) {
        TS_Point p = s_touchscreen->getPoint();

        int16_t x = 0;
        int16_t y = 0;
        touch_map_adc_to_screen(p.x, p.y, &x, &y);

        data->point.x = static_cast<lv_coord_t>(x);
        data->point.y = static_cast<lv_coord_t>(y);
        data->state = LV_INDEV_STATE_PRESSED;

        Serial.printf("TOUCH raw=(%d,%d) screen=(%d,%d) z=%d\n",
                      p.x, p.y, x, y, p.z);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void touch_register_xpt2046(XPT2046_Touchscreen* touchscreen)
{
    s_touchscreen = touchscreen;

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}
