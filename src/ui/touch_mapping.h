/**
 * @file touch_mapping.h
 * @brief Raw XPT2046 ADC to screen-pixel mapping and LVGL input registration.
 */

#ifndef TOUCH_MAPPING_H
#define TOUCH_MAPPING_H

#include <lvgl.h>
#include <stdint.h>

class XPT2046_Touchscreen;

/**
 * @brief Map raw XPT2046 ADC values to screen pixels.
 *
 * @param raw_x      Raw X ADC value (0–4095).
 * @param raw_y      Raw Y ADC value (0–4095).
 * @param screen_x   Output screen X coordinate, clamped to [0, DISP_WIDTH-1].
 * @param screen_y   Output screen Y coordinate, clamped to [0, DISP_HEIGHT-1].
 */
void touch_map_adc_to_screen(uint16_t raw_x, uint16_t raw_y,
                             int16_t* screen_x, int16_t* screen_y);

/**
 * @brief LVGL pointer input device read callback.
 *
 * Reads the registered touchscreen, maps raw coordinates, logs them, and
 * reports pressed/released state.
 */
void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data);

/**
 * @brief Register an initialized XPT2046 touchscreen as the LVGL pointer device.
 */
void touch_register_xpt2046(XPT2046_Touchscreen* touchscreen);

#endif // TOUCH_MAPPING_H
