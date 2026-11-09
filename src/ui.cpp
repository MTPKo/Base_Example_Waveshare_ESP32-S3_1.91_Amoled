#include "ui.h"
#include "qspi_display.h"
#include "touch_face.h"
#include <Arduino.h>
#include <lvgl.h>

// External touch_face instance (defined in main.cpp)
extern Touch_face touch_face;


// Button colors (RGB hex values)
static uint32_t colors[] = {
    0xff0000, 0x00ff00, 0x0000ff, // Red, Green, Blue
    0xffff00, 0x000000, 0x00ffff, // Yellow, Center (Off), Cyan
    0xff00db, 0xffffff, 0xffa500  // Pink (Magenta-ish), White, Orange
};

// Button labels
static const char *labels[] = {"Red",  "Green", "Blue",  "Yellow", "Off",
                               "Cyan", "Pink",  "White", "Orange"};

void ui_init() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Create a container for the grid
  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, LCD_WIDTH, LCD_HEIGHT);
  lv_obj_center(cont);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 5, 0);
  lv_obj_set_layout(cont, LV_LAYOUT_GRID);

  // Define grid columns and rows (3x3)
  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};
  static int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                              LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);

  // Enable gridnav on the container for 2D navigation
  lv_gridnav_add(cont, LV_GRIDNAV_CTRL_ROLLOVER);

  // Create buttons in grid
  for (int i = 0; i < 9; i++) {
    int row = i / 3;
    int col = i % 3;

    lv_obj_t *btn = lv_button_create(cont);
    lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                         LV_GRID_ALIGN_STRETCH, row, 1);

    // Button background color
    uint32_t color = colors[i];
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 8, 0);

    // Keep button background color when focused
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_STATE_FOCUSED);

    // ADDED: Make focus more visible with internal border
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 6,
                                  LV_STATE_FOCUSED); // 6px internal border
    lv_obj_set_style_border_color(btn, lv_palette_main(LV_PALETTE_GREY),
                                  LV_STATE_FOCUSED); // Medium Gray
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_FULL, LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_STATE_FOCUSED);

    // Create label
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, labels[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);

    // Special styling for "Off" button (dark background)
    if (i == 4) {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), LV_STATE_FOCUSED);
      lv_obj_set_style_text_color(label, lv_color_white(), 0);
    } else {
      lv_obj_set_style_text_color(label, lv_color_black(), 0);
    }

    
  }

  // Create group and associate with input device
  lv_group_t *g = lv_group_create();
  lv_group_add_obj(g, cont);

  // Associate group with keypad input device
  lv_indev_t *indev = NULL;
  while ((indev = lv_indev_get_next(indev)) != NULL) {
    if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
      lv_indev_set_group(indev, g);
      Serial.println("Keypad input device found and linked to group");
      break;
    }
  }

  // Set as default group
  lv_group_set_default(g);

  // Focus the first button
  lv_obj_t *first_btn = lv_obj_get_child(cont, 0);
  if (first_btn) {
    Serial.println("Focusing first button");
    lv_group_focus_obj(first_btn);
  } else {
    Serial.println("WARNING: Could not find first button to focus!");
  }
}
