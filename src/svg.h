#ifndef SVG_H
#define SVG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NSVGimage NSVGimage;

typedef struct {
  NSVGimage *image;
  uint32_t fill_color;
  uint32_t stroke_color;
  float opacity_override;
} svg_element_t;

void svg_init(int sample_count);

void svg_cleanup(void);

void svg_begin_draw(int width, int height);

void svg_end_draw(void);

NSVGimage *svg_load_file(const char *filename, float dpi);

NSVGimage *svg_parse(char *data, float dpi);

void svg_free(NSVGimage *image);

svg_element_t svg_create_element(NSVGimage *image, uint32_t fill_color,
                                 uint32_t stroke_color);

void svg_render_element(const svg_element_t *element, float x, float y,
                        float width, float height, float dpi_scale);

#endif