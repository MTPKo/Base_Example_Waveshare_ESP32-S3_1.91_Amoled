#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/*====================
   MEMORY SETTINGS
 *====================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64 * 1024U)

/*====================
   STDLIB - REQUIRED FOR LVGL 9
 *====================*/
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/*====================
   DISPLAY SETTINGS
 *====================*/
#define LV_DPI_DEF 130

/*====================
   RENDERERS - REQUIRED FOR LVGL 9
 *====================*/
#define LV_USE_DRAW_SW 1

/*====================
   OPTIMIZATION
 *====================*/
#define LV_USE_DRAW_ARM2D_SYNC 0
#define LV_USE_NATIVE_HELIUM_ASM 0

/*====================
   FONT SETTINGS
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGETS
 *====================*/
#define LV_USE_BTN 1
#define LV_USE_LABEL 1

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_GRID 1

/*====================
   EXTRAS
 *====================*/
#define LV_USE_GRIDNAV 1

/*====================
   GROUPS - REQUIRED FOR INPUT
 *====================*/
#define LV_USE_GROUP 1

/*====================
   HAL SETTINGS
 *====================*/
// IMPORTANT: LV_TICK_CUSTOM_SYS_TIME_EXPR is deprecated in LVGL 9
// Use lv_tick_set_cb() in your code instead
#define LV_TICK_CUSTOM 1

/*====================
   LOG
 *====================*/
#define LV_USE_LOG 0

/*====================
   DEBUG - OPTIONAL
 *====================*/
// Uncomment these for debugging:
// #define LV_USE_LOG 1
// #define LV_LOG_LEVEL LV_LOG_LEVEL_TRACE

/*====================
   OTHERS
 *====================*/
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0

#endif /* LV_CONF_H */
