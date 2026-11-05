#pragma once

#include "lvgl.h"

namespace esp_brookesia::apps::hardware_ui {

static constexpr lv_coord_t HEADER_HEIGHT = 72;
static constexpr lv_coord_t HORIZONTAL_MARGIN = 28;
static constexpr lv_coord_t CONTENT_TOP_GAP = 16;
static constexpr lv_coord_t CONTENT_BOTTOM_MARGIN = 18;

static constexpr uint32_t COLOR_BACKGROUND = 0x000000;
static constexpr uint32_t COLOR_SURFACE = 0x242426;
static constexpr uint32_t COLOR_SURFACE_ALT = 0x303034;
static constexpr uint32_t COLOR_BORDER = 0x3A3A3C;
static constexpr uint32_t COLOR_PRIMARY_TEXT = 0xFFFFFF;
static constexpr uint32_t COLOR_SECONDARY_TEXT = 0xA7A7AD;
static constexpr uint32_t COLOR_ACCENT = 0x00BFFF;
static constexpr uint32_t COLOR_SUCCESS = 0x32D583;
static constexpr uint32_t COLOR_WARNING = 0xFDB022;
static constexpr uint32_t COLOR_ERROR = 0xF97066;

inline lv_obj_t *create_page(const char *title, lv_obj_t **content_out)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *page = lv_obj_create(screen);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_center(page);
    lv_obj_set_style_bg_color(page, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(page);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, lv_pct(100), HEADER_HEIGHT);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_hex(COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, HORIZONTAL_MARGIN, 0);

    lv_obj_update_layout(page);
    lv_coord_t width = lv_obj_get_width(page) - HORIZONTAL_MARGIN * 2;
    lv_coord_t height = lv_obj_get_height(page) - HEADER_HEIGHT - CONTENT_TOP_GAP - CONTENT_BOTTOM_MARGIN;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    lv_obj_t *content = lv_obj_create(page);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, width, height);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, HEADER_HEIGHT + CONTENT_TOP_GAP);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    if (content_out) {
        *content_out = content;
    }
    return page;
}

inline lv_obj_t *create_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_style_radius(panel, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

inline lv_obj_t *create_icon_button(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 48, 48);
    lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(COLOR_SURFACE_ALT), LV_PART_MAIN);

    lv_obj_t *icon = lv_label_create(button);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(icon, lv_color_hex(COLOR_PRIMARY_TEXT), LV_PART_MAIN);
    lv_obj_center(icon);
    return button;
}

inline void style_secondary_label(lv_obj_t *label, const lv_font_t *font = &lv_font_montserrat_18)
{
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_SECONDARY_TEXT), LV_PART_MAIN);
}

} // namespace esp_brookesia::apps::hardware_ui
