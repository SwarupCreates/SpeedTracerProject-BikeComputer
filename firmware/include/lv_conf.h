/**
 * @file lv_conf.h
 * SpeedTracer Cycling Computer - LVGL 9 Configuration
 * Optimized for Waveshare ESP32-S3-Touch-LCD-2.8 (ST7789 240x320 + CST3530 Touch)
 * Target Hardware: ESP32-S3 (240MHz, 8MB Octal PSRAM, 16MB Flash)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
/* Color depth: 16 (RGB565) */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color. Needed if the display driver expects big-endian */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS (PSRAM)
 *=========================*/
/* Use standard C library malloc/free with ESP32 PSRAM heap */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (192 * 1024U)          /* 192 KB internal/PSRAM heap for LVGL objects */
#define LV_MEM_POOL_EXPAND_SIZE 0

#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/*====================
   HAL SETTINGS
 *====================*/
/* Default display refresh period in milliseconds (16ms = ~60 FPS) */
#define LV_DEF_REFR_PERIOD 16

/* Input device read period in milliseconds */
#define LV_DEF_INDEV_READ_PERIOD 20

/* Use custom tick source: Arduino millis() */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*====================
   DRAWING ENGINE
 *====================*/
/* Software drawing engine */
#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW
    #define LV_DRAW_SW_DRAW_UNIT_CNT 1
    #define LV_DRAW_SW_COMPLEX 1
    #define LV_DRAW_SW_SHADOW_CACHE_SIZE 0
    #define LV_DRAW_SW_CIRCLE_CACHE_SIZE 4
#endif

/* Vector Graphics & SVG (ThorVG Engine) */
#define LV_USE_VECTOR_GRAPHIC 1
#define LV_USE_THORVG_INTERNAL 1
#define LV_USE_SVG 1
#define LV_USE_SVG_ANIMATION 0

/*====================
   FEATURE CONFIGURATION
 *====================*/
/* Logging */
#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF 1
#endif

/* Performance Monitor (FPS & CPU usage overlay) */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* Animations */
#define LV_USE_ANIM 1

/*====================
   FONT USAGE
 *====================*/
/* Built-in Montserrat fonts for sleek Garmin/Wahoo style typography */
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1

/* Set default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable subpixel anti-aliasing and compressed fonts */
#define LV_USE_FONT_SUBPX 1
#define LV_USE_FONT_COMPRESSED 1
#define LV_FONT_FMT_TXT_LARGE 1

/*====================
   WIDGETS & LAYOUTS
 *====================*/
/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Core Widgets */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 0
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 1
#define LV_USE_WIN 0

#endif /* LV_CONF_H */
