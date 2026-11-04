#pragma once
#include <lvgl.h>
#include "custom_icon_masks.h"
// Small LVGL offscreen renderer: does not own LCD or touch. Existing dirty-tile
// presenter remains in charge, so icons never cause full LCD redraws.
static void drawCustomIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color,CustomIcon icon) {
 const int w=customWidths[(int)icon],h=customHeights[(int)icon];
 static lv_obj_t *canvas=nullptr;
 alignas(4) static uint8_t pixels[40*40*4];
 if(!canvas){
   lv_init();
   lv_display_t *d=lv_display_create(40,40);
   if(!d)return;
   lv_display_enable_invalidation(d,false);
   canvas=lv_canvas_create(lv_screen_active());
   if(!canvas)return;
   lv_canvas_set_buffer(canvas,pixels,40,40,LV_COLOR_FORMAT_ARGB8888);
 }
 // Cache the most recent rasterization; consecutive matching icons need no
 // new LVGL draw task. Storage is fixed and reused, not allocated each frame.
 static int lastIcon=-1;static uint16_t lastColor=0;
 if(lastIcon!=(int)icon || lastColor!=color){
 // LVGL uses the antialiased SVG-derived alpha mask, not replacement geometry.
 lv_image_dsc_t image={};
 image.header.magic=LV_IMAGE_HEADER_MAGIC;
 image.header.cf=LV_COLOR_FORMAT_A8;
 image.header.w=w;image.header.h=h;image.header.stride=w;
 image.data_size=w*h;image.data=customMasks[(int)icon];
 lv_canvas_fill_bg(canvas,lv_color_black(),LV_OPA_TRANSP);
 lv_layer_t layer;lv_canvas_init_layer(canvas,&layer);
 lv_draw_image_dsc_t dsc;lv_draw_image_dsc_init(&dsc);
 dsc.src=&image;dsc.recolor_opa=LV_OPA_COVER;
 dsc.recolor=lv_color_make(((color>>11)&31)*255/31,((color>>5)&63)*255/63,(color&31)*255/31);
 lv_area_t area={0,0,w-1,h-1};lv_draw_image(&layer,&dsc,&area);
 lv_canvas_finish_layer(canvas,&layer);
 lastIcon=(int)icon;lastColor=color;
 }
 for(int r=0;r<h;r++)for(int c=0;c<w;c++){
   const uint8_t *p=pixels+4*(r*40+c);int a=p[3];if(!a)continue;
   uint16_t bg=display.backgroundPixel(x+c,y+r);
   int red=(p[2]*a+((bg>>11)*255/31)*(255-a)+127)/255;
   int green=(p[1]*a+(((bg>>5)&63)*255/63)*(255-a)+127)/255;
   int blue=(p[0]*a+((bg&31)*255/31)*(255-a)+127)/255;
   gfx.drawPixel(x+c,y+r,((red>>3)<<11)|((green>>2)<<5)|(blue>>3));
 }
}
