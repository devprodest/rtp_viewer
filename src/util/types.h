#pragma once

#include <stdint.h>

//----------------------------------------------------------------------------------------------------------------------

typedef struct point_s
{
    int x;
    int y;
} point_s;

typedef struct fpoint_s
{
    float x;
    float y;
} fpoint_s;


typedef struct frect_s
{
    float x0;
    float y0;
    float x1;
    float y1;
} frect_s;

typedef struct rect_s
{
    int x0;
    int y0;
    int x1;
    int y1;
} rect_s;


typedef point_s image_size_s;
typedef fpoint_s fimage_size_s;


#define rect_convert(type, rect)   ((type) {.x0 = (rect).x0, .y0 = (rect).y0, .x1 = (rect).x1, .y1 = (rect).y1})
#define point_convert(type, point) ((type) {.x = (point).x, .y0 = (point).y})
#define size_convert(type, size)   ((type) {.x = (size).x, .y0 = (size).y})

//----------------------------------------------------------------------------------------------------------------------

typedef struct u8color_rgba_s
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} u8color_rgba_s;

typedef struct color_rgba_s
{
    int r;
    int g;
    int b;
    int a;
} color_rgba_s;

typedef struct fcolor_rgba_s
{
    float r;
    float g;
    float b;
    float a;
} fcolor_rgba_s;

typedef struct u8color_rgb_s
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} u8color_rgb_s;

typedef struct color_rgb_s
{
    int r;
    int g;
    int b;
} color_rgb_s;

typedef struct fcolor_rgb_s
{
    float r;
    float g;
    float b;
} fcolor_rgb_s;

#define rgba_convert(type, rgba)      ((type) {.r = (rgba).r, .g = (rgba).g, .b = (rgba).b, .a = (rgba).a})
#define rgb_convert(type, rgb)        ((type) {.r = (rgb).r, .g = (rgb).g, .b = (rgb).b})
#define to_rgba_convert(type, rgb, alpha) ((type) {.r = (rgb).r, .g = (rgb).g, .b = (rgb).b, .a = (alpha)})
