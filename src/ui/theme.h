/**
 * @file theme.h
 * @brief Theme helpers for CYD_RPS screens.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

namespace ui_theme {

// Palette
lv_color_t bg_dark(void);
lv_color_t text_primary(void);
lv_color_t text_secondary(void);
lv_color_t accent_blue(void);
lv_color_t accent_grey(void);
lv_color_t win_green(void);
lv_color_t lose_red(void);

// Screen / widget styling
void apply_screen_bg(lv_obj_t* screen);
void style_title_label(lv_obj_t* label, const char* text, uint8_t font_size);
void style_body_label(lv_obj_t* label, const char* text);
void style_status_label(lv_obj_t* label);
void style_timeout_label(lv_obj_t* label);
void style_error_title(lv_obj_t* label);
void style_error_msg(lv_obj_t* label);
void style_button_primary(lv_obj_t* btn, lv_obj_t* label, const char* text);
void style_button_grey(lv_obj_t* btn, lv_obj_t* label, const char* text);
void style_outcome_label(lv_obj_t* label, uint8_t outcome);

} // namespace ui_theme

#endif // UI_THEME_H
