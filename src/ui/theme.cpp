/**
 * @file theme.cpp
 * @brief Theme helpers for CYD_RPS screens.
 */

#include "theme.h"

namespace ui_theme {

lv_color_t bg_dark(void)      { return lv_color_hex(0x1a1a1a); }
lv_color_t text_primary(void) { return lv_color_white(); }
lv_color_t text_secondary(void){ return lv_color_hex(0xaaaaaa); }
lv_color_t accent_blue(void)  { return lv_color_hex(0x007acc); }
lv_color_t accent_grey(void)  { return lv_color_hex(0x666666); }
lv_color_t win_green(void)    { return lv_color_hex(0x00cc44); }
lv_color_t lose_red(void)     { return lv_color_hex(0xff3333); }

static const lv_font_t* pick_font(uint8_t size)
{
    switch (size) {
        case 14: return &lv_font_montserrat_14;
        case 16: return &lv_font_montserrat_16;
        case 20: return &lv_font_montserrat_20;
        case 24: return &lv_font_montserrat_24;
        case 32: return &lv_font_montserrat_32;
        default: return &lv_font_montserrat_14;
    }
}

void apply_screen_bg(lv_obj_t* screen)
{
    lv_obj_set_style_bg_color(screen, bg_dark(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

void style_title_label(lv_obj_t* label, const char* text, uint8_t font_size)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, pick_font(font_size), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}

void style_body_label(lv_obj_t* label, const char* text)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

void style_status_label(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_secondary(), LV_PART_MAIN);
}

void style_timeout_label(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_secondary(), LV_PART_MAIN);
}

void style_error_title(lv_obj_t* label)
{
    lv_label_set_text(label, "Error");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lose_red(), LV_PART_MAIN);
}

void style_error_msg(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 200);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

static void style_button(lv_obj_t* btn, lv_obj_t* label, const char* text, lv_color_t bg)
{
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_center(label);
}

void style_button_primary(lv_obj_t* btn, lv_obj_t* label, const char* text)
{
    style_button(btn, label, text, accent_blue());
}

void style_button_grey(lv_obj_t* btn, lv_obj_t* label, const char* text)
{
    style_button(btn, label, text, accent_grey());
}

void style_outcome_label(lv_obj_t* label, uint8_t outcome)
{
    // outcome_t: OUTCOME_NONE=0, OUTCOME_WIN, OUTCOME_LOSE, OUTCOME_DRAW
    lv_color_t color = text_primary();
    const char* text = "?";
    switch (outcome) {
        case 1: text = "WIN";  color = win_green(); break;
        case 2: text = "LOSE"; color = lose_red();  break;
        case 3: text = "DRAW"; color = text_primary(); break;
    }
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

} // namespace ui_theme
