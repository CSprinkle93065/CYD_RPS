/**
 * @file theme.cpp
 * @brief Theme helpers for CYD_RPS v0.2.0.
 *
 * Background is pure black (#000000), primary text is white (#FFFFFF), and the
 * accent color for pressed/active states is dark gray (#404040). Buttons render
 * with a subtle white border on black; the pressed state uses the dark-gray
 * accent.
 */

#include "theme.h"
#include "hal_config.h"

namespace ui_theme {

lv_color_t bg_black(void)      { return lv_color_hex(0x000000); }
lv_color_t text_primary(void)  { return lv_color_white(); }
lv_color_t text_secondary(void){ return lv_color_hex(0xAAAAAA); }
lv_color_t accent_grey(void)   { return lv_color_hex(0x404040); }
lv_color_t win_green(void)     { return lv_color_hex(0x00CC44); }
lv_color_t lose_red(void)      { return lv_color_hex(0xFF3333); }

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
    lv_obj_set_style_bg_color(screen, bg_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

void style_title_label(lv_obj_t* label, const char* text)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, pick_font(24), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

void style_body_label(lv_obj_t* label, const char* text)
{
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, pick_font(16), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

void style_status_label(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, pick_font(14), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_secondary(), LV_PART_MAIN);
}

void style_device_id_label(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_obj_set_style_text_font(label, pick_font(14), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_secondary(), LV_PART_MAIN);
}

void style_score_label(lv_obj_t* label)
{
    lv_label_set_text(label, "W:0 L:0 D:0");
    lv_obj_set_style_text_font(label, pick_font(14), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

void style_error_title(lv_obj_t* label)
{
    lv_label_set_text(label, "Error");
    lv_obj_set_style_text_font(label, pick_font(24), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lose_red(), LV_PART_MAIN);
}

void style_error_msg(lv_obj_t* label)
{
    lv_label_set_text(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 200);
    lv_obj_set_style_text_font(label, pick_font(14), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
}

static void style_button_common(lv_obj_t* btn, lv_coord_t radius)
{
    lv_obj_set_style_bg_color(btn, bg_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, text_primary(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, radius, LV_PART_MAIN);

    // Pressed / active state uses the dark-gray accent.
    lv_obj_set_style_bg_color(btn, accent_grey(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
}

void style_button_primary(lv_obj_t* btn, lv_obj_t* label, const char* text)
{
    style_button_common(btn, 8);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, pick_font(16), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
    lv_obj_center(label);
}

void style_button_secondary(lv_obj_t* btn, lv_obj_t* label, const char* text)
{
    style_button_common(btn, 8);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, pick_font(16), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
    lv_obj_center(label);
}

void style_home_button(lv_obj_t* btn, lv_obj_t* label)
{
    lv_obj_set_size(btn, 30, 30);
    style_button_common(btn, 4);
    lv_label_set_text(label, "\u2302"); // Unicode home icon
    lv_obj_set_style_text_font(label, pick_font(16), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_primary(), LV_PART_MAIN);
    lv_obj_center(label);
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
    lv_obj_set_style_text_font(label, pick_font(32), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

} // namespace ui_theme
