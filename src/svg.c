#include "svg.h"
#define NANOSVG_IMPLEMENTATION
#include "HandmadeMath.h"
#include "nanosvg.h"
#include "sokol_gfx.h"
#include "sokol_gp.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SVG_FLATNESS_PX 0.25f
#define SVG_MAX_FLAT_POINTS 8192
#define SVG_MAX_EDGES 16384
#define SVG_MAX_FILL_TRIS 32768
#define SVG_MAX_STROKE_TRIS 32768

#ifndef NSVG_PI
#define NSVG_PI 3.14159265358979323846f
#endif

typedef struct {
  uint8_t *memory;
  size_t size;
  size_t used;
} svg_arena_t;

static svg_arena_t g_svg_arena = {0};

static void *svg_arena_alloc(size_t size) {
  size = (size + 15) & ~15;

  if (g_svg_arena.used + size > g_svg_arena.size) {
    fprintf(stderr, "SVG arena out of memory! Requested %zu, available %zu\n",
            size, g_svg_arena.size - g_svg_arena.used);
    return NULL;
  }

  void *ptr = g_svg_arena.memory + g_svg_arena.used;
  g_svg_arena.used += size;
  return ptr;
}

static void svg_arena_reset(void) { g_svg_arena.used = 0; }

typedef struct {
  float minx, miny, maxx, maxy;
} svg_bbox_t;

typedef struct {
  HMM_Vec2 pts[SVG_MAX_FLAT_POINTS];
  int count;
  svg_bbox_t bb;
} svg_flat_poly_t;

typedef struct {
  float x0, y0, x1, y1;
  int dir;
} svg_edge_t;

typedef struct {
  svg_edge_t edges[SVG_MAX_EDGES];
  int nedges;
  sgp_triangle tris[SVG_MAX_FILL_TRIS];
  int ntris;
} svg_edge_fill_t;

typedef struct {
  sgp_triangle tris[SVG_MAX_STROKE_TRIS];
  int count;
} svg_tri_stroke_t;

static inline svg_bbox_t svg_bbox_init(void) {
  svg_bbox_t b = {+FLT_MAX, +FLT_MAX, -FLT_MAX, -FLT_MAX};
  return b;
}

static inline void svg_bbox_add(svg_bbox_t *b, HMM_Vec2 p) {
  if (p.X < b->minx)
    b->minx = p.X;
  if (p.Y < b->miny)
    b->miny = p.Y;
  if (p.X > b->maxx)
    b->maxx = p.X;
  if (p.Y > b->maxy)
    b->maxy = p.Y;
}

static inline HMM_Vec2 svg_v2(float x, float y) { return HMM_V2(x, y); }

static inline sgp_point svg_to_sgp_point(HMM_Vec2 p) {
  sgp_point q;
  q.x = p.X;
  q.y = p.Y;
  return q;
}

static inline float svg_clampf(float x, float a, float b) {
  return x < a ? a : (x > b ? b : x);
}

static inline float svg_absf(float x) { return x < 0 ? -x : x; }

static inline void svg_set_color_u32(uint32_t rgba) {
  float r = ((rgba >> 24) & 0xFF) / 255.0f;
  float g = ((rgba >> 16) & 0xFF) / 255.0f;
  float b = ((rgba >> 8) & 0xFF) / 255.0f;
  float a = ((rgba) & 0xFF) / 255.0f;
  sgp_set_color(r, g, b, a);
}

static inline uint32_t svg_nsvg_color_to_rgba(uint32_t abgr, float opacity) {

  uint32_t rr = (abgr) & 0xFF;
  uint32_t gg = (abgr >> 8) & 0xFF;
  uint32_t bb = (abgr >> 16) & 0xFF;
  uint32_t aa = (abgr >> 24) & 0xFF;
  float a = (aa / 255.0f) * opacity;
  return (rr << 24) | (gg << 16) | (bb << 8) | ((uint32_t)(a * 255.0f + 0.5f));
}

static inline uint32_t svg_choose_fill_rgba(const svg_element_t *el,
                                            const NSVGshape *sh) {
  uint32_t rgba = 0;

  if (el->fill_color != 0) {
    rgba = el->fill_color;
  } else if (sh->fill.type == NSVG_PAINT_COLOR) {
    rgba = svg_nsvg_color_to_rgba(sh->fill.color, sh->opacity);
  } else {
    return 0;
  }

  if (el->opacity_override < 1.0f) {
    uint8_t current_alpha = (rgba & 0xFF);
    uint8_t new_alpha = (uint8_t)(current_alpha * el->opacity_override);
    rgba = (rgba & 0xFFFFFF00) | new_alpha;
  }

  return rgba;
}

static inline uint32_t svg_choose_stroke_rgba(const svg_element_t *el,
                                              const NSVGshape *sh) {
  uint32_t rgba = 0;

  if (el->stroke_color != 0) {
    rgba = el->stroke_color;
  } else if (sh->stroke.type == NSVG_PAINT_COLOR) {
    rgba = svg_nsvg_color_to_rgba(sh->stroke.color, sh->opacity);
  } else {
    return 0;
  }

  if (el->opacity_override < 1.0f) {
    uint8_t current_alpha = (rgba & 0xFF);
    uint8_t new_alpha = (uint8_t)(current_alpha * el->opacity_override);
    rgba = (rgba & 0xFFFFFF00) | new_alpha;
  }
  return rgba;
}

static void svg_flatten_cubic_bez(HMM_Vec2 p1, HMM_Vec2 p2, HMM_Vec2 p3,
                                  HMM_Vec2 p4, svg_flat_poly_t *out, float tol,
                                  int level) {
  if (level > 10) {
    if (out->count < SVG_MAX_FLAT_POINTS) {
      out->pts[out->count++] = p4;
      svg_bbox_add(&out->bb, p4);
    }
    return;
  }

  float x12 = (p1.X + p2.X) * 0.5f;
  float y12 = (p1.Y + p2.Y) * 0.5f;
  float x23 = (p2.X + p3.X) * 0.5f;
  float y23 = (p2.Y + p3.Y) * 0.5f;
  float x34 = (p3.X + p4.X) * 0.5f;
  float y34 = (p3.Y + p4.Y) * 0.5f;
  float x123 = (x12 + x23) * 0.5f;
  float y123 = (y12 + y23) * 0.5f;

  float dx = p4.X - p1.X;
  float dy = p4.Y - p1.Y;
  float d2 = svg_absf((p2.X - p4.X) * dy - (p2.Y - p4.Y) * dx);
  float d3 = svg_absf((p3.X - p4.X) * dy - (p3.Y - p4.Y) * dx);

  if ((d2 + d3) * (d2 + d3) < tol * (dx * dx + dy * dy)) {
    if (out->count < SVG_MAX_FLAT_POINTS) {
      out->pts[out->count++] = p4;
      svg_bbox_add(&out->bb, p4);
    }
    return;
  }

  float x234 = (x23 + x34) * 0.5f;
  float y234 = (y23 + y34) * 0.5f;
  float x1234 = (x123 + x234) * 0.5f;
  float y1234 = (y123 + y234) * 0.5f;

  svg_flatten_cubic_bez(p1, svg_v2(x12, y12), svg_v2(x123, y123),
                        svg_v2(x1234, y1234), out, tol, level + 1);
  svg_flatten_cubic_bez(svg_v2(x1234, y1234), svg_v2(x234, y234),
                        svg_v2(x34, y34), p4, out, tol, level + 1);
}

static void svg_path_flatten(const NSVGpath *path, float tol,
                             svg_flat_poly_t *out) {
  out->count = 0;
  out->bb = svg_bbox_init();
  if (path->npts < 1)
    return;

  HMM_Vec2 p0 = svg_v2(path->pts[0], path->pts[1]);
  out->pts[out->count++] = p0;
  svg_bbox_add(&out->bb, p0);

  for (int i = 0; i < path->npts - 1; i += 3) {
    float *p = &path->pts[i * 2];
    HMM_Vec2 p1 = svg_v2(p[0], p[1]);
    HMM_Vec2 p2 = svg_v2(p[2], p[3]);
    HMM_Vec2 p3 = svg_v2(p[4], p[5]);
    HMM_Vec2 p4 = svg_v2(p[6], p[7]);
    svg_flatten_cubic_bez(p1, p2, p3, p4, out, tol, 0);
  }

  if (path->closed && out->count > 1) {
    HMM_Vec2 first = out->pts[0];
    HMM_Vec2 last = out->pts[out->count - 1];
    if (svg_absf(first.X - last.X) > 1e-6f ||
        svg_absf(first.Y - last.Y) > 1e-6f) {
      if (out->count < SVG_MAX_FLAT_POINTS) {
        out->pts[out->count++] = first;
        svg_bbox_add(&out->bb, first);
      }
    }
  }
}

static void svg_add_edge(svg_edge_fill_t *ef, float x0, float y0, float x1,
                         float y1) {
  if (ef->nedges >= SVG_MAX_EDGES)
    return;
  if (svg_absf(y0 - y1) < 1e-6f)
    return;

  svg_edge_t *e = &ef->edges[ef->nedges++];
  if (y0 < y1) {
    e->x0 = x0;
    e->y0 = y0;
    e->x1 = x1;
    e->y1 = y1;
    e->dir = 1;
  } else {
    e->x0 = x1;
    e->y0 = y1;
    e->x1 = x0;
    e->y1 = y0;
    e->dir = -1;
  }
}

static int svg_cmp_edge(const void *a, const void *b) {
  const svg_edge_t *ea = (const svg_edge_t *)a;
  const svg_edge_t *eb = (const svg_edge_t *)b;
  if (ea->y0 < eb->y0)
    return -1;
  if (ea->y0 > eb->y0)
    return 1;
  return 0;
}

static void svg_edges_to_triangles(svg_edge_fill_t *ef, int fillRule) {
  if (ef->nedges == 0)
    return;

  qsort(ef->edges, ef->nedges, sizeof(svg_edge_t), svg_cmp_edge);

  float ymin = ef->edges[0].y0;
  float ymax = ymin;
  for (int i = 0; i < ef->nedges; i++) {
    if (ef->edges[i].y1 > ymax)
      ymax = ef->edges[i].y1;
  }

  float step = 1.0f;
  ef->ntris = 0;

  for (float y = ymin; y < ymax && ef->ntris < SVG_MAX_FILL_TRIS - 2;
       y += step) {
    float y_next = HMM_MIN(y + step, ymax);

    typedef struct {
      float x;
      int dir;
    } isect_t;
    isect_t isects[256];
    int n_isects = 0;

    float y_mid = (y + y_next) * 0.5f;

    for (int i = 0; i < ef->nedges && n_isects < 256; i++) {
      svg_edge_t *e = &ef->edges[i];

      if (e->y0 <= y_mid && y_mid < e->y1) {
        float t = (y_mid - e->y0) / (e->y1 - e->y0);
        isects[n_isects].x = e->x0 + t * (e->x1 - e->x0);
        isects[n_isects].dir = e->dir;
        n_isects++;
      }
    }

    for (int i = 0; i < n_isects - 1; i++) {
      for (int j = i + 1; j < n_isects; j++) {
        if (isects[i].x > isects[j].x) {
          isect_t tmp = isects[i];
          isects[i] = isects[j];
          isects[j] = tmp;
        }
      }
    }

    if (fillRule == NSVG_FILLRULE_EVENODD) {
      for (int i = 0; i < n_isects - 1; i += 2) {
        if (ef->ntris < SVG_MAX_FILL_TRIS - 2) {
          float x0 = isects[i].x;
          float x1 = isects[i + 1].x;

          HMM_Vec2 a = svg_v2(x0, y);
          HMM_Vec2 b = svg_v2(x1, y);
          HMM_Vec2 c = svg_v2(x1, y_next);
          HMM_Vec2 d = svg_v2(x0, y_next);

          ef->tris[ef->ntris].a = svg_to_sgp_point(a);
          ef->tris[ef->ntris].b = svg_to_sgp_point(b);
          ef->tris[ef->ntris].c = svg_to_sgp_point(c);
          ef->ntris++;

          ef->tris[ef->ntris].a = svg_to_sgp_point(a);
          ef->tris[ef->ntris].b = svg_to_sgp_point(c);
          ef->tris[ef->ntris].c = svg_to_sgp_point(d);
          ef->ntris++;
        }
      }
    } else {
      int winding = 0;
      for (int i = 0; i < n_isects - 1; i++) {
        winding += isects[i].dir;

        if (winding != 0 && ef->ntris < SVG_MAX_FILL_TRIS - 2) {
          float x0 = isects[i].x;
          float x1 = isects[i + 1].x;

          HMM_Vec2 a = svg_v2(x0, y);
          HMM_Vec2 b = svg_v2(x1, y);
          HMM_Vec2 c = svg_v2(x1, y_next);
          HMM_Vec2 d = svg_v2(x0, y_next);

          ef->tris[ef->ntris].a = svg_to_sgp_point(a);
          ef->tris[ef->ntris].b = svg_to_sgp_point(b);
          ef->tris[ef->ntris].c = svg_to_sgp_point(c);
          ef->ntris++;

          ef->tris[ef->ntris].a = svg_to_sgp_point(a);
          ef->tris[ef->ntris].b = svg_to_sgp_point(c);
          ef->tris[ef->ntris].c = svg_to_sgp_point(d);
          ef->ntris++;
        }
      }
    }
  }
}

static inline void svg_push_tri(svg_tri_stroke_t *tb, HMM_Vec2 a, HMM_Vec2 b,
                                HMM_Vec2 c) {
  if (tb->count < SVG_MAX_STROKE_TRIS) {
    tb->tris[tb->count].a = svg_to_sgp_point(a);
    tb->tris[tb->count].b = svg_to_sgp_point(b);
    tb->tris[tb->count].c = svg_to_sgp_point(c);
    tb->count++;
  }
}

static inline void svg_push_quad(svg_tri_stroke_t *tb, HMM_Vec2 a, HMM_Vec2 b,
                                 HMM_Vec2 c, HMM_Vec2 d) {
  svg_push_tri(tb, a, b, c);
  svg_push_tri(tb, a, c, d);
}

static HMM_Vec2 svg_normal_of(HMM_Vec2 a, HMM_Vec2 b) {
  HMM_Vec2 d = HMM_SubV2(b, a);
  float L2 = HMM_DotV2(d, d);
  if (L2 <= 1e-20f)
    return HMM_V2(0, 0);
  HMM_Vec2 n = HMM_V2(-d.Y, d.X);
  return HMM_MulV2F(n, HMM_InvSqrtF(L2));
}

static void svg_fan_arc(svg_tri_stroke_t *tb, HMM_Vec2 p, HMM_Vec2 n0,
                        HMM_Vec2 n1, float r, int side, float tol) {
  float dot = svg_clampf(HMM_DotV2(n0, n1), -1.0f, 1.0f);
  float theta = acosf(dot);
  if (theta < 1e-4f)
    return;

  float da = acosf(r / (r + tol)) * 2.0f;
  int segs = (int)ceilf(theta / da);
  if (segs < 2)
    segs = 2;
  if (segs > 64)
    segs = 64;

  float ang = theta / (float)segs;
  if (side < 0)
    ang = -ang;

  HMM_Vec2 prev = HMM_AddV2(p, HMM_MulV2F(n0, r));

  for (int i = 1; i <= segs; i++) {
    float t = ang * i;
    float s = sinf(t), c = cosf(t);
    HMM_Vec2 nextN = HMM_V2(c * n0.X - s * n0.Y, s * n0.X + c * n0.Y);
    if (side < 0)
      nextN = HMM_V2(nextN.X, -nextN.Y);

    HMM_Vec2 cur = HMM_AddV2(p, HMM_MulV2F(nextN, r));
    svg_push_tri(tb, p, prev, cur);
    prev = cur;
  }
}

static void svg_stroke_tessellate(const HMM_Vec2 *p, int n, HMM_Bool closed,
                                  float width, int join, int cap,
                                  float miterLimit, float tol,
                                  svg_tri_stroke_t *out) {
  if (n < 2 || width <= 0.0f) {
    out->count = 0;
    return;
  }
  out->count = 0;
  const float hw = 0.5f * width;

  for (int i = 0; i < n - 1; i++) {
    HMM_Vec2 a = p[i], b = p[i + 1];
    HMM_Vec2 nrm = svg_normal_of(a, b);
    if (HMM_DotV2(nrm, nrm) < 1e-8f)
      continue;

    HMM_Vec2 aL = HMM_AddV2(a, HMM_MulV2F(nrm, hw));
    HMM_Vec2 aR = HMM_SubV2(a, HMM_MulV2F(nrm, hw));
    HMM_Vec2 bL = HMM_AddV2(b, HMM_MulV2F(nrm, hw));
    HMM_Vec2 bR = HMM_SubV2(b, HMM_MulV2F(nrm, hw));
    svg_push_quad(out, aL, bL, bR, aR);
    if (out->count >= SVG_MAX_STROKE_TRIS)
      return;
  }

  for (int i = 1; i < n - 1; i++) {
    HMM_Vec2 a = p[i - 1], b = p[i], c = p[i + 1];
    HMM_Vec2 dn0 = HMM_NormV2(HMM_SubV2(b, a));
    HMM_Vec2 dn1 = HMM_NormV2(HMM_SubV2(c, b));
    if (HMM_DotV2(dn0, dn0) < 1e-8f || HMM_DotV2(dn1, dn1) < 1e-8f)
      continue;

    HMM_Vec2 n0 = HMM_V2(-dn0.Y, dn0.X);
    HMM_Vec2 n1 = HMM_V2(-dn1.Y, dn1.X);

    float cross = dn0.X * dn1.Y - dn0.Y * dn1.X;
    float dot = HMM_DotV2(dn0, dn1);
    int left_turn = (cross > 0.0f) ? 1 : -1;

    HMM_Bool did_join = 0;
    if (join == NSVG_JOIN_MITER && svg_absf(1.0f + dot) > 1e-6f) {
      HMM_Vec2 m = HMM_AddV2(n0, n1);
      float mlen2 = HMM_DotV2(m, m);
      if (mlen2 > 1e-8f) {
        HMM_Vec2 mN = HMM_MulV2F(m, HMM_InvSqrtF(mlen2));
        HMM_Vec2 outerN = (left_turn > 0) ? n1 : HMM_MulV2F(n1, -1.0f);
        float denom = HMM_DotV2(mN, outerN);
        if (svg_absf(denom) > 1e-6f) {
          float ml = hw / denom;
          if (svg_absf(ml) <= miterLimit * hw) {
            HMM_Vec2 outer = HMM_AddV2(b, HMM_MulV2F(mN, ml));
            HMM_Vec2 tip0 =
                HMM_AddV2(b, HMM_MulV2F(n0, left_turn > 0 ? hw : -hw));
            HMM_Vec2 tip1 =
                HMM_AddV2(b, HMM_MulV2F(n1, left_turn > 0 ? hw : -hw));
            if (left_turn > 0)
              svg_push_tri(out, tip0, outer, tip1);
            else
              svg_push_tri(out, tip1, outer, tip0);
            did_join = 1;
          }
        }
      }
    }
    if (!did_join && join == NSVG_JOIN_BEVEL) {
      HMM_Vec2 tip0 = HMM_AddV2(b, HMM_MulV2F(n0, left_turn > 0 ? hw : -hw));
      HMM_Vec2 tip1 = HMM_AddV2(b, HMM_MulV2F(n1, left_turn > 0 ? hw : -hw));
      svg_push_tri(out, b, tip0, tip1);
    } else if (!did_join && join == NSVG_JOIN_ROUND) {
      HMM_Vec2 start = (left_turn > 0) ? n0 : HMM_MulV2F(n0, -1.0f);
      HMM_Vec2 end = (left_turn > 0) ? n1 : HMM_MulV2F(n1, -1.0f);
      svg_fan_arc(out, b, start, end, hw, left_turn, tol);
    }

    if (out->count >= SVG_MAX_STROKE_TRIS)
      return;
  }

  if (!closed && cap != NSVG_CAP_BUTT) {

    {
      HMM_Vec2 a = p[0], b = p[1];
      HMM_Vec2 dir = HMM_NormV2(HMM_SubV2(b, a));
      HMM_Vec2 nrm = HMM_V2(-dir.Y, dir.X);

      if (cap == NSVG_CAP_SQUARE) {
        HMM_Vec2 back = HMM_SubV2(a, HMM_MulV2F(dir, hw));
        HMM_Vec2 bL = HMM_AddV2(back, HMM_MulV2F(nrm, hw));
        HMM_Vec2 bR = HMM_SubV2(back, HMM_MulV2F(nrm, hw));
        HMM_Vec2 aL = HMM_AddV2(a, HMM_MulV2F(nrm, hw));
        HMM_Vec2 aR = HMM_SubV2(a, HMM_MulV2F(nrm, hw));
        svg_push_quad(out, bL, aL, aR, bR);
      } else if (cap == NSVG_CAP_ROUND) {
        svg_fan_arc(out, a, nrm, HMM_MulV2F(nrm, -1.0f), hw, -1, tol);
      }
    }

    {
      HMM_Vec2 a = p[n - 2], b = p[n - 1];
      HMM_Vec2 dir = HMM_NormV2(HMM_SubV2(b, a));
      HMM_Vec2 nrm = HMM_V2(-dir.Y, dir.X);

      if (cap == NSVG_CAP_SQUARE) {
        HMM_Vec2 fwd = HMM_AddV2(b, HMM_MulV2F(dir, hw));
        HMM_Vec2 bL = HMM_AddV2(b, HMM_MulV2F(nrm, hw));
        HMM_Vec2 bR = HMM_SubV2(b, HMM_MulV2F(nrm, hw));
        HMM_Vec2 cL = HMM_AddV2(fwd, HMM_MulV2F(nrm, hw));
        HMM_Vec2 cR = HMM_SubV2(fwd, HMM_MulV2F(nrm, hw));
        svg_push_quad(out, bL, cL, cR, bR);
      } else if (cap == NSVG_CAP_ROUND) {
        svg_fan_arc(out, b, HMM_MulV2F(nrm, -1.0f), nrm, hw, -1, tol);
      }
    }
  }
}

void svg_init(int sample_count) {
  sgp_desc sgpdesc = {0};
  sgpdesc.sample_count = sample_count;
  sgp_setup(&sgpdesc);
  if (!sgp_is_valid()) {
    fprintf(stderr, "Failed to create Sokol GP context: %s\n",
            sgp_get_error_message(sgp_get_last_error()));
    exit(-1);
  }
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);

  g_svg_arena.size = 2 * 1024 * 1024;
  g_svg_arena.memory = calloc(1, g_svg_arena.size);
  g_svg_arena.used = 0;

  if (!g_svg_arena.memory) {
    fprintf(stderr, "Failed to allocate SVG arena memory\n");
    exit(-1);
  }
}

void svg_cleanup(void) {
  sgp_shutdown();

  if (g_svg_arena.memory) {
    free(g_svg_arena.memory);
    g_svg_arena.memory = NULL;
    g_svg_arena.size = 0;
    g_svg_arena.used = 0;
  }
}

void svg_begin_draw(int width, int height) {
  sgp_begin(width, height);
  sgp_viewport(0, 0, width, height);
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);
}

void svg_end_draw(void) {
  sgp_flush();
  sgp_end();
}

NSVGimage *svg_load_file(const char *filename, float dpi) {
  return nsvgParseFromFile(filename, "px", dpi);
}

NSVGimage *svg_parse(char *data, float dpi) {
  return nsvgParse(data, "px", dpi);
}

void svg_free(NSVGimage *image) {
  if (image) {
    nsvgDelete(image);
  }
}

svg_element_t svg_create_element(NSVGimage *image, uint32_t fill_color,
                                 uint32_t stroke_color) {
  svg_element_t element;
  element.image = image;
  element.fill_color = fill_color;
  element.stroke_color = stroke_color;
  element.opacity_override = 1.0f;
  return element;
}

void svg_render_element(const svg_element_t *element, float x, float y,
                        float width, float height, float dpi_scale) {
  if (!element || !element->image)
    return;

  svg_arena_reset();

  svg_edge_fill_t *ef =
      (svg_edge_fill_t *)svg_arena_alloc(sizeof(svg_edge_fill_t));
  svg_flat_poly_t *fp =
      (svg_flat_poly_t *)svg_arena_alloc(sizeof(svg_flat_poly_t));
  svg_tri_stroke_t *sb =
      (svg_tri_stroke_t *)svg_arena_alloc(sizeof(svg_tri_stroke_t));

  if (!ef || !fp || !sb) {
    fprintf(stderr, "Failed to allocate from SVG arena\n");
    return;
  }

  const float x_px = x * dpi_scale;
  const float y_px = y * dpi_scale;
  const float sx = (width * dpi_scale) / element->image->width;
  const float sy = (height * dpi_scale) / element->image->height;

  const float smax = (sx > sy) ? sx : sy;
  const float tol_svg =
      (smax > 0.0f) ? (SVG_FLATNESS_PX / smax) : SVG_FLATNESS_PX;
  const float tol = tol_svg * tol_svg * 0.25f;

  sgp_push_transform();
  sgp_translate(x_px, y_px);
  sgp_scale(sx, sy);
  sgp_set_blend_mode(SGP_BLENDMODE_BLEND);

  for (NSVGshape *shape = element->image->shapes; shape; shape = shape->next) {
    if (!(shape->flags & NSVG_FLAGS_VISIBLE))
      continue;

    uint32_t fill_rgba = svg_choose_fill_rgba(element, shape);
    if (fill_rgba && shape->paths) {
      ef->nedges = 0;
      ef->ntris = 0;
      for (NSVGpath *path = shape->paths; path; path = path->next) {
        if (path->npts < 4)
          continue;

        fp->count = 0;
        svg_path_flatten(path, tol, fp);

        for (int j = 0, k = fp->count - 1; j < fp->count; k = j++) {
          svg_add_edge(ef, fp->pts[k].X, fp->pts[k].Y, fp->pts[j].X,
                       fp->pts[j].Y);
        }
      }

      svg_edges_to_triangles(ef, shape->fillRule);

      if (ef->ntris > 0) {
        svg_set_color_u32(fill_rgba);
        sgp_draw_filled_triangles(ef->tris, (uint32_t)ef->ntris);
      }
    }

    uint32_t stroke_rgba = svg_choose_stroke_rgba(element, shape);
    float sw = shape->strokeWidth;
    if (stroke_rgba && sw > 0.0f && shape->paths) {
      svg_set_color_u32(stroke_rgba);

      for (NSVGpath *path = shape->paths; path; path = path->next) {
        if (path->npts < 4)
          continue;

        fp->count = 0;
        svg_path_flatten(path, tol, fp);

        if (fp->count >= 2) {
          sb->count = 0;
          svg_stroke_tessellate(
              fp->pts, fp->count, path->closed, sw, (int)shape->strokeLineJoin,
              (int)shape->strokeLineCap,
              shape->miterLimit > 0.0f ? shape->miterLimit : 4.0f, tol_svg, sb);
          if (sb->count > 0) {
            sgp_draw_filled_triangles(sb->tris, (uint32_t)sb->count);
          }
        }
      }
    }
  }

  sgp_pop_transform();
}