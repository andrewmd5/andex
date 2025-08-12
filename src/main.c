#include <stddef.h>
#include <stdint.h>
#if defined(__APPLE__) && !defined(SOKOL_METAL)
#define SOKOL_METAL
#elif defined(_WIN32) && !defined(SOKOL_D3D11)
#define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__) && !defined(SOKOL_GLES2)
#define SOKOL_GLES2
#elif !defined(SOKOL_GLCORE33) && !defined(SOKOL_METAL) &&                     \
    !defined(SOKOL_D3D11) && !defined(SOKOL_GLES2)
#define SOKOL_GLCORE33
#endif

#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_gp.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "svg.h"

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define SOKOL_GL_IMPL
#include "util/sokol_gl.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define FONS_VERTEX_COUNT 128
#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"
#define SOKOL_FONTSTASH_IMPL
#include "util/sokol_fontstash.h"

#include "shaders.h"

#define SOKOL_CLAY_IMPL
#include "sokol_clay.h"

#include "buffer.h"
#include "editor.h"
#include "files.h"
#include "resources.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define APP_MIN_WIDTH 1280
#define APP_MIN_HEIGHT 720
#if !defined(__EMSCRIPTEN__)
extern void app_make_compact_window(bool show_controls);
extern void app_set_minimum_window_size(float width, float height);
#endif

#define BOTTOM_BAR_HEIGHT 68.0f
#define SIDEBAR_WIDTH 220.0f

typedef struct {
  char id[64];
  char date[32];
  char filename[256];
  char preview[64];
  time_t timestamp;
  bool is_selected;
} FileEntry;

typedef struct {
  FileEntry *entries;
  size_t count;
  size_t capacity;
  int selected_index;
} FileHistory;

typedef struct {
  Clay_Color background;
  Clay_Color sidebar_bg;
  Clay_Color text_primary;
  Clay_Color text_secondary;
  Clay_Color border;
  Clay_Color selection;
  Clay_Color hover;
} Theme;

static const Theme LIGHT_THEME = {
    .background = {248, 248, 248, 255},
    .sidebar_bg = {245, 245, 245, 255},
    .text_primary = {0, 0, 0, 255},
    .text_secondary = {128, 128, 128, 255},
    .border = {230, 230, 230, 255},
    .selection = {100, 149, 237, 80},
    .hover = {240, 240, 240, 255},
};

static const Theme DARK_THEME = {
    .background = {40, 40, 40, 255},
    .sidebar_bg = {30, 30, 30, 255},
    .text_primary = {230, 230, 230, 255},
    .text_secondary = {153, 153, 153, 255},
    .border = {60, 60, 60, 255},
    .selection = {100, 149, 237, 80},
    .hover = {50, 50, 50, 255},
};

static const Clay_String EMPTY_STRING = CLAY_STRING(" ");

static const int font_sizes[] = {22, 24, 26, 28, 30, 32, 34};
static const int num_font_sizes = sizeof(font_sizes) / sizeof(font_sizes[0]);
static Clay_String font_size_strings[] = {
    CLAY_STRING("22px"), CLAY_STRING("24px"), CLAY_STRING("26px"),
    CLAY_STRING("28px"), CLAY_STRING("30px"), CLAY_STRING("32px"),
    CLAY_STRING("34px")};

static const Clay_String WELCOME_MESSAGES[] = {
    CLAY_STRING("Start typing to begin..."),
    CLAY_STRING("Begin writing"),
    CLAY_STRING("Pick a thought and go"),
    CLAY_STRING("What's on your mind"),
    CLAY_STRING("Just start"),
    CLAY_STRING("Type your first thought"),
    CLAY_STRING("Start with one sentence"),
    CLAY_STRING("Just say it")};

static const int WELCOME_MESSAGES_COUNT =
    sizeof(WELCOME_MESSAGES) / sizeof(WELCOME_MESSAGES[0]);

static const Clay_String *selected_welcome_message = NULL;

static const Clay_String *get_welcome_message() {
  if (selected_welcome_message == NULL) {
    int index = rand() % WELCOME_MESSAGES_COUNT;
    selected_welcome_message = &WELCOME_MESSAGES[index];
  }
  return selected_welcome_message;
}

typedef struct {
  sg_pipeline pip;
  sg_bindings bind;
  globals_t uniforms;
  float cursor_x;
  float cursor_y;
  float cursor_height;
} CursorState;

typedef struct {
  float x, y;
  float r, g, b, a;
} SelectionVertex;

typedef struct {
  sg_pipeline pip;
  sg_buffer vbuf;
  sg_buffer ibuf;
  SelectionVertex *vertices;
  uint16_t *indices;
  int capacity_quads;
} SelectionState;

typedef enum {
  APP_WINDOW_NORMAL,
  APP_WINDOW_FULLSCREEN,
  APP_WINDOW_MINIMIZED
} AppWindowState;

typedef struct {
  bool initialized;
  Theme theme;
  bool dark_mode;
  bool lol;

  int font_size_index;
  int current_font_index;

  float screen_width;
  float screen_height;

  TextEditor editor;
  char current_filename[256];
  char documents_path[512];

  bool show_sidebar;
  FileHistory history;
  int hovered_entry_index;

  int timer_seconds;
  bool timer_running;
  uint64_t last_timer_update;
  char timer_string[6];
  float bottom_nav_opacity;
  bool bottom_nav_hovering;
  uint64_t bottom_nav_fade_time;

  uint64_t last_save_time;
  bool needs_save;

  CursorState cursor;
  SelectionState selection;
  void *current_line_tag;

  uint64_t start_time;
  AppWindowState window_state;

  struct {
    sclay_image images[RES_IMG_COUNT];
    sclay_font_t fonts[RES_FONT_COUNT];
    svg_element_t icons[RES_ICON_COUNT];
  } gfx;
} AppState;

static AppState *g_app = NULL;

typedef struct {
    int index;
} DeleteEntryContext;


static sclay_image make_image_from_embed(const EmbeddedBlob *blob) {
  int w = 0, h = 0, comp = 0;
  stbi_uc *pixels =
      stbi_load_from_memory(blob->data, (int)blob->size, &w, &h, &comp, 4);
  if (!pixels) {
    fprintf(stderr, "stb_image failed to load %s\n", blob->name);
    exit(1);
  }

  size_t img_size = w * h * 4;

  sg_image img = sg_make_image(&(sg_image_desc){
      .width = w,
      .height = h,
      .pixel_format = SG_PIXELFORMAT_RGBA8,
      .data.subimage[0][0] = {.ptr = pixels, .size = img_size},
      .label = blob->name,
  });

  stbi_image_free(pixels);

  if (blob->as.image.uv) {
    sg_sampler sampler =
        sg_make_sampler(&(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR,
                                           .mag_filter = SG_FILTER_LINEAR,
                                           .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                                           .wrap_v = SG_WRAP_CLAMP_TO_EDGE});

    sclay_image sclayImg = sclay_make_image_region(
        img, sampler, blob->as.image.uv->u0, blob->as.image.uv->v0,
        blob->as.image.uv->u1, blob->as.image.uv->v1);
    return sclayImg;
  }

  return sclay_make_image(img, sg_make_sampler(&(sg_sampler_desc){
                                   .min_filter = SG_FILTER_LINEAR,
                                   .mag_filter = SG_FILTER_LINEAR,
                               }));
}

sclay_font_t get_current_font() {
  return g_app->gfx.fonts[g_app->current_font_index];
}

static const void* get_current_window() {
#if defined(__APPLE__)
  return sapp_macos_get_window();
#elif defined(_WIN32)
  return sapp_win32_get_hwnd();
#elif defined(__linux__)
  return sapp_x11_get_window();
#else 
  return NULL;
#endif
}

static FileEntry create_new_entry() {
  FileEntry entry = {0};

  snprintf(entry.id, sizeof(entry.id), "%08x-%04x-%04x-%04x-%012llx", rand(),
           rand() & 0xffff, rand() & 0xffff, rand() & 0xffff,
           ((uint64_t)rand() << 32) | rand());

  time_t now = time(NULL);
  entry.timestamp = now;

  struct tm *tm_info = localtime(&now);
  strftime(entry.date, sizeof(entry.date), "%b %d", tm_info);

  char date_str[64];
  strftime(date_str, sizeof(date_str), "%Y-%m-%d-%H-%M-%S", tm_info);
  snprintf(entry.filename, sizeof(entry.filename), "[%s]-[%s].md", entry.id,
           date_str);

  entry.preview[0] = '\0';
  entry.is_selected = false;

  return entry;
}

static void update_preview_text(FileEntry *entry, const char *content) {
  char cleaned[256] = {0};
  int cleaned_idx = 0;
  bool last_was_space = false;

  for (int i = 0; content[i] && cleaned_idx < 255; i++) {
    if (content[i] == '\n' || content[i] == '\r') {
      if (!last_was_space && cleaned_idx > 0) {
        cleaned[cleaned_idx++] = ' ';
        last_was_space = true;
      }
    } else if (content[i] == ' ' || content[i] == '\t') {
      if (!last_was_space && cleaned_idx > 0) {
        cleaned[cleaned_idx++] = ' ';
        last_was_space = true;
      }
    } else {
      cleaned[cleaned_idx++] = content[i];
      last_was_space = false;
    }
  }

  if (cleaned_idx > 16) {
    strncpy(entry->preview, cleaned, 16);
    strcat(entry->preview, "...");
  } else {
    strcpy(entry->preview, cleaned);
  }
}

static void save_current_entry() {
  if (!g_app->needs_save)
    return;

  char filepath[768];
  snprintf(filepath, sizeof(filepath), "%s/%s", g_app->documents_path,
           g_app->current_filename);

  size_t len = char_buffer_len(&g_app->editor.chars);
  char *text = malloc(len + 1);
  if (text) {
    char_buffer_to_buffer(&g_app->editor.chars, text, len + 1);

    if (files_write_file(filepath, text, len)) {
      for (size_t i = 0; i < g_app->history.count; i++) {
        if (strcmp(g_app->history.entries[i].filename,
                   g_app->current_filename) == 0) {
          update_preview_text(&g_app->history.entries[i], text);
          break;
        }
      }
      printf("Saved entry to: %s\n", filepath);
      g_app->needs_save = false;
    }

    free(text);
  }
}

static void load_entry(FileEntry *entry) {
  save_current_entry();

  char filepath[768];
  snprintf(filepath, sizeof(filepath), "%s/%s", g_app->documents_path,
           entry->filename);

  char *content = NULL;
  if (files_read_file(filepath, &content, NULL)) {
    text_editor_clear(&g_app->editor);
    text_editor_insert(&g_app->editor, content, strlen(content));
    text_editor_move_to_pos(&g_app->editor, 0);
    free(content);
  } else {
    text_editor_clear(&g_app->editor);
  }

  strcpy(g_app->current_filename, entry->filename);
  g_app->needs_save = false;
}

static int compare_entries(const void *a, const void *b) {
  const FileEntry *ea = (const FileEntry *)a;
  const FileEntry *eb = (const FileEntry *)b;
  return (eb->timestamp > ea->timestamp) - (eb->timestamp < ea->timestamp);
}

static void load_existing_entries() {
  FileList list = {0};
  if (!files_list_directory(g_app->documents_path, &list))
    return;

  g_app->history.count = 0;

  for (size_t i = 0; i < list.count; i++) {
    FileMetadata *file = &list.items[i];

    if (file->is_directory || !strstr(file->name, ".md"))
      continue;

    if (g_app->history.count >= g_app->history.capacity) {
      g_app->history.capacity *= 2;
      g_app->history.entries = realloc(
          g_app->history.entries, g_app->history.capacity * sizeof(FileEntry));
    }

    FileEntry *entry = &g_app->history.entries[g_app->history.count];
    memset(entry, 0, sizeof(FileEntry));

    char *uuid_start = strchr(file->name, '[');
    char *uuid_end = strchr(file->name, ']');
    if (uuid_start && uuid_end) {
      size_t uuid_len = uuid_end - uuid_start - 1;
      strncpy(entry->id, uuid_start + 1, uuid_len);
      entry->id[uuid_len] = '\0';
    }

    char *date_start = strrchr(file->name, '[');
    char *date_end = strrchr(file->name, ']');
    if (date_start && date_end && date_start != uuid_start) {
      char date_str[32];
      size_t date_len = date_end - date_start - 1;
      strncpy(date_str, date_start + 1, date_len);
      date_str[date_len] = '\0';

      struct tm tm = {0};
      sscanf(date_str, "%d-%d-%d-%d-%d-%d", &tm.tm_year, &tm.tm_mon,
             &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
      tm.tm_year -= 1900;
      tm.tm_mon -= 1;
      entry->timestamp = mktime(&tm);

      struct tm *tm_info = localtime(&entry->timestamp);
      strftime(entry->date, sizeof(entry->date), "%b %d", tm_info);
    }

    strcpy(entry->filename, file->name);

    char *preview_content = NULL;
    if (files_read_file(file->path, &preview_content, NULL)) {
      update_preview_text(entry, preview_content);
      free(preview_content);
    }

    g_app->history.count++;
  }

  files_free_list(&list);

  if (g_app->history.count > 0) {
    qsort(g_app->history.entries, g_app->history.count, sizeof(FileEntry),
          compare_entries);
  }

  if (g_app->history.count == 0) {
    FileEntry new_entry = create_new_entry();
    if (g_app->history.count >= g_app->history.capacity) {
      g_app->history.capacity *= 2;
      g_app->history.entries = realloc(
          g_app->history.entries, g_app->history.capacity * sizeof(FileEntry));
    }
    g_app->history.entries[0] = new_entry;
    g_app->history.count = 1;
    load_entry(&g_app->history.entries[0]);
    g_app->needs_save = true;
  } else {
    time_t now = time(NULL);
    struct tm *today = localtime(&now);
    struct tm *latest = localtime(&g_app->history.entries[0].timestamp);

    if (today->tm_year != latest->tm_year || today->tm_mon != latest->tm_mon ||
        today->tm_mday != latest->tm_mday ||
        strlen(g_app->history.entries[0].preview) > 0) {

      FileEntry new_entry = create_new_entry();

      if (g_app->history.count >= g_app->history.capacity) {
        g_app->history.capacity *= 2;
        g_app->history.entries =
            realloc(g_app->history.entries,
                    g_app->history.capacity * sizeof(FileEntry));
      }

      memmove(&g_app->history.entries[1], &g_app->history.entries[0],
              g_app->history.count * sizeof(FileEntry));
      g_app->history.entries[0] = new_entry;
      g_app->history.count++;

      load_entry(&g_app->history.entries[0]);
    } else {
      load_entry(&g_app->history.entries[0]);
    }
  }
}

static void export_entry(int index) {
  if (index < 0 || index >= (int)g_app->history.count)
    return;

  FileEntry *entry = &g_app->history.entries[index];
  char filepath[768];
  snprintf(filepath, sizeof(filepath), "%s/%s", g_app->documents_path,
           entry->filename);
           printf("Exporting entry to: %s\n", filepath);

  
  char suggested_name[256];
  int j = 0;
  for (int i = 0; entry->preview[i] && j < 250; i++) {
    char c = entry->preview[i];
    if (c == ' ') {
      suggested_name[j++] = '-';
    } else if (c != '/' && c != '\\' && c != ':' && c != '*' && c != '?' &&
               c != '"' && c != '<' && c != '>' && c != '|' && c != '\n' &&
               c != '\r' && c != '\t' && c != '.') {
      suggested_name[j++] = c;
    }
  }
  suggested_name[j] = '\0';
  strcat(suggested_name, ".md");

  files_move_file(filepath, suggested_name,  get_current_window());
}

static void on_entry_deleted(bool success, void *user_data) {
    DeleteEntryContext *ctx = (DeleteEntryContext *)user_data;
    
    if (success) {
        int index = ctx->index;
        
        // Check if this was the selected entry before removing
        bool was_selected = g_app->history.entries[index].is_selected;
        
        // Remove entry from history array
        if (index < (int)g_app->history.count - 1) {
            memmove(&g_app->history.entries[index], 
                    &g_app->history.entries[index + 1],
                    (g_app->history.count - index - 1) * sizeof(FileEntry));
        }
        g_app->history.count--;
        
        // Handle selection if needed
        if (was_selected) {
            if (g_app->history.count > 0) {
                load_entry(&g_app->history.entries[0]);
            } else {
                FileEntry new_entry = create_new_entry();
                g_app->history.entries[0] = new_entry;
                g_app->history.count = 1;
                load_entry(&g_app->history.entries[0]);
            }
        }
    }
    free(ctx);
}

static void delete_entry(int index) {
    if (index < 0 || index >= (int)g_app->history.count)
        return;
    
    FileEntry *entry = &g_app->history.entries[index];
    char filepath[768];
    snprintf(filepath, sizeof(filepath), "%s/%s", g_app->documents_path,
             entry->filename);
    
    // Create context for the callback
    DeleteEntryContext *ctx = malloc(sizeof(DeleteEntryContext));
    if (!ctx) return;
    ctx->index = index;

    // Call files_delete_confirm with our callback
    files_delete_confirm(filepath, on_entry_deleted, get_current_window(), ctx);
}

static Theme *get_current_theme() {
  return g_app->dark_mode ? (Theme *)&DARK_THEME : (Theme *)&LIGHT_THEME;
}

static Clay_Color get_background_color() {
  return get_current_theme()->background;
}

static Clay_Color get_sidebar_bg_color() {
  return get_current_theme()->sidebar_bg;
}

static Clay_Color get_text_color() { return get_current_theme()->text_primary; }

static Clay_Color get_secondary_text_color() {
  return get_current_theme()->text_secondary;
}

static Clay_Color get_ui_text_color() {
  return Clay_Hovered() ? get_text_color() : get_secondary_text_color();
}

static Clay_Color get_border_color() { return get_current_theme()->border; }

static Clay_Color get_selection_color() {
  return get_current_theme()->selection;
}

static Clay_Color get_hover_color() { return get_current_theme()->hover; }

static void timer_init(int seconds) {
  g_app->timer_seconds = seconds;
  g_app->timer_running = false;
  g_app->last_timer_update = stm_now();

  int minutes = g_app->timer_seconds / 60;
  int secs = g_app->timer_seconds % 60;
  snprintf(g_app->timer_string, sizeof(g_app->timer_string), "%d:%02d", minutes,
           secs);
}

static uint64_t timer_update(void) {
  uint64_t now = stm_now();

  if (g_app->timer_running &&
      stm_sec(stm_diff(now, g_app->last_timer_update)) >= 1.0) {
    g_app->timer_seconds--;
    if (g_app->timer_seconds <= 0) {
      g_app->timer_seconds = 0;
      g_app->timer_running = false;

      if (!g_app->bottom_nav_hovering) {
        g_app->bottom_nav_opacity = 1.0f;
        g_app->bottom_nav_fade_time = now;
      }
    }
    g_app->last_timer_update = now;
    int minutes = g_app->timer_seconds / 60;
    int seconds = g_app->timer_seconds % 60;
    snprintf(g_app->timer_string, sizeof(g_app->timer_string), "%d:%02d",
             minutes, seconds);
  }
  return now;
}

static void timer_reset(void) { timer_init(900); }

static void timer_toggle(void) {
  g_app->timer_running = !g_app->timer_running;
  if (g_app->timer_running) {
    g_app->last_timer_update = stm_now();
    if (!g_app->bottom_nav_hovering) {
      g_app->bottom_nav_fade_time = stm_now();
    }
  } else {
    g_app->bottom_nav_opacity = 1.0f;
  }
}

static size_t caret_byte_from_xy(TextEditor *editor,
                                 Clay_RenderCommandArray commands,
                                 float mouse_x_screen, float mouse_y_screen) {
  float dpi = sapp_dpi_scale();
  float mx = mouse_x_screen / dpi;
  float my = mouse_y_screen / dpi + editor->scroll_y;

  size_t line_count = editor->render_line_count;
  if (line_count == 0)
    return 0;

  int target_line = -1;
  float first_top = 0.0f, last_bottom = 0.0f;
  for (size_t i = 0; i < line_count; i++) {
    Clay_ElementId line_id = CLAY_IDI("EditorLine", i);
    Clay_ElementData line_data = Clay_GetElementData(line_id);
    if (i == 0)
      first_top = line_data.boundingBox.y;
    if (i == line_count - 1)
      last_bottom = line_data.boundingBox.y + line_data.boundingBox.height;

    if (my >= line_data.boundingBox.y &&
        my <= line_data.boundingBox.y + line_data.boundingBox.height) {
      target_line = (int)i;
      break;
    }
  }
  if (target_line < 0) {
    target_line = (my < first_top) ? 0 : (int)line_count - 1;
  }

  size_t byte_prefix = 0;
  for (int i = 0; i < target_line; i++) {
    byte_prefix += strlen(editor->render_lines[i]) + 1;
  }

  const char *line_chars = editor->render_lines[target_line];
  size_t line_len = strlen(line_chars);
  if (line_len == 0) {
    return byte_prefix;
  }

  Clay_RenderCommand *best_cmd = NULL;
  const Clay_TextRenderData *best_td = NULL;
  float best_y_dist = FLT_MAX;

  for (int i = 0; i < commands.length; i++) {
    Clay_RenderCommand *cmd = &commands.internalArray[i];
    if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT)
      continue;

    const Clay_TextRenderData *td = &cmd->renderData.text;
    if (td->stringContents.baseChars != line_chars)
      continue;

    float top = cmd->boundingBox.y;
    float bot = top + cmd->boundingBox.height;

    float ydist = 0.0f;
    if (my < top)
      ydist = top - my;
    else if (my > bot)
      ydist = my - bot;
    else
      ydist = 0.0f;

    if (ydist < best_y_dist) {
      best_y_dist = ydist;
      best_cmd = cmd;
      best_td = td;
    }
  }

  if (!best_cmd || !best_td || best_td->caret.caretCount <= 0) {
    return byte_prefix + line_len;
  }

  int best_ci = 0;
  float best_dx = FLT_MAX;
  for (int ci = 0; ci < best_td->caret.caretCount; ci++) {
    float cx = best_cmd->boundingBox.x + best_td->caret.prefixX[ci];
    float dx = fabsf(mx - cx);
    if (dx < best_dx) {
      best_dx = dx;
      best_ci = ci;
    }
  }

  int in_line_byte = best_td->caret.byteOffsets[best_ci];
  if (in_line_byte < 0)
    in_line_byte = 0;
  if (in_line_byte > (int)line_len)
    in_line_byte = (int)line_len;

  return byte_prefix + (size_t)in_line_byte;
}

static void handle_char_input(const sapp_event *ev) {
    if (ev->type != SAPP_EVENTTYPE_CHAR)
        return;

    if ((ev->modifiers & SAPP_MODIFIER_CTRL) ||
        (ev->modifiers & SAPP_MODIFIER_SUPER) ||
        (ev->modifiers & SAPP_MODIFIER_ALT)) {
        return;
    }

    uint32_t c = ev->char_code;
    

    if (c < 32 || c == 127) return;
    
    char utf8[5] = {0};
    size_t len = 0;

    if (c < 0x80) {
        utf8[0] = (char)c;
        len = 1;
    } else if (c < 0x800) {
        utf8[0] = 0xC0 | (c >> 6);
        utf8[1] = 0x80 | (c & 0x3F);
        len = 2;
    } else if (c < 0x10000) {
        utf8[0] = 0xE0 | (c >> 12);
        utf8[1] = 0x80 | ((c >> 6) & 0x3F);
        utf8[2] = 0x80 | (c & 0x3F);
        len = 3;
    } else if (c < 0x110000) {
        utf8[0] = 0xF0 | (c >> 18);
        utf8[1] = 0x80 | ((c >> 12) & 0x3F);
        utf8[2] = 0x80 | ((c >> 6) & 0x3F);
        utf8[3] = 0x80 | (c & 0x3F);
        len = 4;
    }

    if (len > 0) {
        text_editor_insert(&g_app->editor, utf8, len);
        g_app->needs_save = true;
    }
}

static void handle_key_input(const sapp_event *ev) {
  if (ev->type != SAPP_EVENTTYPE_KEY_DOWN)
    return;

  TextEditor *editor = &g_app->editor;

  bool shift = (ev->modifiers & SAPP_MODIFIER_SHIFT) != 0;
  bool ctrl = (ev->modifiers & SAPP_MODIFIER_CTRL) != 0;
  bool cmd = (ev->modifiers & SAPP_MODIFIER_SUPER) != 0;
  bool mod = ctrl || cmd;

  size_t old_pos = editor->cursor.byte_pos;

  if (mod) {
    switch (ev->key_code) {
    case SAPP_KEYCODE_A:
      text_editor_set_selection(editor, 0, char_buffer_len(&editor->chars));
      return;

    case SAPP_KEYCODE_C:
      if (editor->has_selection) {
        char *text = text_editor_get_selection(editor);
        if (text) {
          sapp_set_clipboard_string(text);
          free(text);
        }
      }
      return;

    case SAPP_KEYCODE_X:
      if (editor->has_selection) {
        char *text = text_editor_get_selection(editor);
        if (text) {
          sapp_set_clipboard_string(text);
          free(text);
          text_editor_delete_selection(editor);
          g_app->needs_save = true;
        }
      }
      return;

    case SAPP_KEYCODE_V: {
      const char *clipboard = sapp_get_clipboard_string();
      if (clipboard && strlen(clipboard) > 0) {
        text_editor_insert(editor, clipboard, strlen(clipboard));
        g_app->needs_save = true;
      }
      return;
    }

    case SAPP_KEYCODE_Z:
      if (shift) {
        text_editor_redo(editor);
      } else {
        text_editor_undo(editor);
      }
      text_editor_clear_selection(editor);
      g_app->needs_save = true;
      return;

    case SAPP_KEYCODE_Y:
      text_editor_redo(editor);
      text_editor_clear_selection(editor);
      g_app->needs_save = true;
      return;
    }
  }

  bool is_nav_key = false;

  switch (ev->key_code) {
  case SAPP_KEYCODE_LEFT:
    is_nav_key = true;
    if (mod) {
      text_editor_move_word_left(editor);
    } else {
      text_editor_move_left(editor);
    }
    break;

  case SAPP_KEYCODE_RIGHT:
    is_nav_key = true;
    if (mod) {
      text_editor_move_word_right(editor);
    } else {
      text_editor_move_right(editor);
    }
    break;

  case SAPP_KEYCODE_UP:
    is_nav_key = true;
    text_editor_move_up(editor);
    break;

  case SAPP_KEYCODE_DOWN:
    is_nav_key = true;
    text_editor_move_down(editor);
    break;

  case SAPP_KEYCODE_HOME:
    is_nav_key = true;
    text_editor_move_home(editor);
    break;

  case SAPP_KEYCODE_END:
    is_nav_key = true;
    text_editor_move_end(editor);
    break;

  case SAPP_KEYCODE_PAGE_UP:
    is_nav_key = true;
    for (int i = 0; i < 20; i++) {
      text_editor_move_up(editor);
    }
    break;

  case SAPP_KEYCODE_PAGE_DOWN:
    is_nav_key = true;
    for (int i = 0; i < 20; i++) {
      text_editor_move_down(editor);
    }
    break;

  case SAPP_KEYCODE_BACKSPACE:
    if (mod) {
      text_editor_delete_word_backward(editor);
    } else {
      text_editor_delete_backward(editor);
    }
    g_app->needs_save = true;
    break;

  case SAPP_KEYCODE_DELETE:
    if (mod) {
      text_editor_delete_word_forward(editor);
    } else {
      text_editor_delete_forward(editor);
    }
    g_app->needs_save = true;
    break;

  case SAPP_KEYCODE_ENTER:
  case SAPP_KEYCODE_KP_ENTER:
    text_editor_insert(editor, "\n", 1);
    g_app->needs_save = true;
    break;

  case SAPP_KEYCODE_TAB:
    text_editor_insert(editor, "    ", 4);
    g_app->needs_save = true;
    break;

  default:
    break;
  }

  if (is_nav_key) {
    if (shift) {
      if (!editor->has_selection) {
        editor->has_selection = true;
        editor->sel_start = old_pos;
        editor->sel_end = editor->cursor.byte_pos;
      } else {
        editor->sel_end = editor->cursor.byte_pos;
      }
    } else {
      text_editor_clear_selection(editor);
    }
  }
}

static void handle_mouse_input(const sapp_event *ev) {
  TextEditor *editor = &g_app->editor;

  if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN &&
      ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
    editor->mouse.mouse_down = true;
    editor->mouse.dragging = false;
    editor->mouse.mouse_x = ev->mouse_x;
    editor->mouse.mouse_y = ev->mouse_y;
  } else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP &&
             ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
    editor->mouse.mouse_down = false;
    editor->mouse.dragging = false;
  } else if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
    editor->mouse.mouse_x = ev->mouse_x;
    editor->mouse.mouse_y = ev->mouse_y;
    if (editor->mouse.mouse_down) {
      editor->mouse.dragging = true;
    }
  } else if (ev->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
    editor->target_scroll_y -= ev->scroll_y * 40.0f;
    if (editor->target_scroll_y < 0)
      editor->target_scroll_y = 0;
  }
}

void HandleFontSelect(Clay_ElementId elementId, Clay_PointerData pointerData,
                      intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    g_app->current_font_index = (ResourceFontId)userData;
    text_editor_move_to_line_col(&g_app->editor, 0, 0);
  }
}

void HandleFontSizeChange(Clay_ElementId elementId,
                          Clay_PointerData pointerData, intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    g_app->font_size_index = (g_app->font_size_index + 1) % num_font_sizes;
  }
}

void HandleFullscreenToggle(Clay_ElementId elementId,
                            Clay_PointerData pointerData, intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    g_app->window_state = (g_app->window_state == APP_WINDOW_FULLSCREEN)
                              ? APP_WINDOW_NORMAL
                              : APP_WINDOW_FULLSCREEN;
    sapp_toggle_fullscreen();
  }
}

void HandleLulCow(Clay_ElementId elementId, Clay_PointerData pointerData,
                  intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    g_app->lol = !g_app->lol;
  }
}

void HandleTimerToggle(Clay_ElementId elementId, Clay_PointerData pointerData,
                       intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    static uint64_t last_click = 0;
    uint64_t now = stm_now();

    if (stm_ms(stm_diff(now, last_click)) < 300) {
      timer_reset();
      last_click = 0;
    } else {
      timer_toggle();
      last_click = now;
    }
  }
}

void HandleNewEntry(Clay_ElementId elementId, Clay_PointerData pointerData,
                    intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    save_current_entry();

    FileEntry new_entry = create_new_entry();

    if (g_app->history.count >= g_app->history.capacity) {
      g_app->history.capacity *= 2;
      g_app->history.entries = realloc(
          g_app->history.entries, g_app->history.capacity * sizeof(FileEntry));
    }

    memmove(&g_app->history.entries[1], &g_app->history.entries[0],
            g_app->history.count * sizeof(FileEntry));
    g_app->history.entries[0] = new_entry;
    g_app->history.count++;

    load_entry(&g_app->history.entries[0]);
  }
}

void HandleSidebarToggle(Clay_ElementId elementId, Clay_PointerData pointerData,
                         intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    g_app->show_sidebar = !g_app->show_sidebar;
  }
}

void HandleEntrySelect(Clay_ElementId elementId, Clay_PointerData pointerData,
                       intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    int index = (int)userData;
    if (index >= 0 && index < (int)g_app->history.count) {
      load_entry(&g_app->history.entries[index]);
      text_editor_clear_selection(&g_app->editor);
    }
  }
}

void HandleEntryExport(Clay_ElementId elementId, Clay_PointerData pointerData,
                       intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    int index = (int)userData;
    if (index >= 0 && index < (int)g_app->history.count) {
      export_entry(index);
    }
  }
}

void HandleEntryDelete(Clay_ElementId elementId, Clay_PointerData pointerData,
                       intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    int index = (int)userData;
    delete_entry(index);
  }
}

Clay_ElementDeclaration IconButtonStyle(bool hovered, float size,
                                        Clay_ElementId id) {
  return (Clay_ElementDeclaration){
      .id = id,
      .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}};
}

Clay_ElementDeclaration IconStyle(svg_element_t *icon, bool hovered,
                                  uint32_t normal_color, uint32_t hover_color,
                                  float_t opacity) {
  uint32_t color = hovered ? hover_color : normal_color;
  icon->fill_color = color;
  icon->stroke_color = color;
  icon->opacity_override = opacity;

  return (Clay_ElementDeclaration){
      .custom = {.customData = icon},
      .layout = {.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}}};
}

void HandleIconButtonInteraction(Clay_ElementId elementId,
                                 Clay_PointerData pointerData,
                                 intptr_t userData) {
  void (*onClick)(void) = (void (*)(void))userData;
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME && onClick) {
    onClick();
  }
}

void RenderIconButton(Clay_String id, int icon_idx, uint32_t normal_color,
                      uint32_t hover_color, void (*onClick)(void), float size,
                      float_t opacity) {
  CLAY(IconButtonStyle(Clay_Hovered(), size, Clay_GetElementId(id))) {
    Clay_OnHover(HandleIconButtonInteraction, (intptr_t)onClick);
    svg_element_t *icon = &g_app->gfx.icons[icon_idx];
    CLAY(IconStyle(icon, Clay_Hovered(), normal_color, hover_color, opacity)) {}
  }
}

static void toggleHistoryBar(void) {
  g_app->show_sidebar = !g_app->show_sidebar;
}

static void toggleTheme(void) { g_app->dark_mode = !g_app->dark_mode; }

static void HandleOpenDirectory(Clay_ElementId elementId,
                                Clay_PointerData pointerData,
                                intptr_t userData) {
  if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
    files_open_directory(g_app->documents_path);
  }
}

static void render_sidebar() {
  if (!g_app->show_sidebar)
    return;

  CLAY({.id = CLAY_ID("Sidebar"),
        .backgroundColor = get_sidebar_bg_color(),
        .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .sizing = {CLAY_SIZING_FIXED(SIDEBAR_WIDTH),
                              CLAY_SIZING_GROW(0)}}}) {

    CLAY({.id = CLAY_ID("SidebarHeader"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, 0)},
                     .padding = {12, 16, 10, 16},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 4},
          .clip = {.horizontal = true}}) {

      CLAY({.layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                       .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, 0)},
                       .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                       .childGap = 4}}) {

        Clay_OnHover(HandleOpenDirectory, 0);
        bool header_hover = Clay_Hovered();

        CLAY_TEXT(CLAY_STRING("History"),
                  CLAY_TEXT_CONFIG({.fontId = 0,
                                    .fontSize = 16,
                                    .textColor = get_ui_text_color()}));

        Clay_Color icon_color = get_ui_text_color();
        uint32_t icon_color_hex =
            ((uint8_t)(icon_color.r) << 24) | ((uint8_t)(icon_color.g) << 16) |
            ((uint8_t)(icon_color.b) << 8) | ((uint8_t)(icon_color.a));
        svg_element_t *open_icon = &g_app->gfx.icons[RES_ICON_TURN_UP_RIGHT];
        open_icon->fill_color = icon_color_hex;
        open_icon->stroke_color = icon_color_hex;

        CLAY({.custom = {.customData = open_icon},
              .layout = {
                  .sizing = {CLAY_SIZING_FIXED(12), CLAY_SIZING_FIXED(12)}}}) {}
      }

      Clay_String p = {.chars = g_app->documents_path,
                       .isStaticallyAllocated = true,
                       .length = (int)strlen(g_app->documents_path)};
      CLAY_TEXT(p, CLAY_TEXT_CONFIG(
                       {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                        .fontSize = 10,
                        .textColor = get_secondary_text_color(),
                        .wrapMode = CLAY_TEXT_WRAP_NONE}));
    }

    CLAY({.backgroundColor = get_border_color(),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1)}}}) {}

    CLAY({.id = CLAY_ID("EntriesList"),
          .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          .clip = {.vertical = true}}) {

      for (size_t i = 0; i < g_app->history.count; i++) {
        FileEntry *entry = &g_app->history.entries[i];
        Clay_ElementId row_id = CLAY_IDI("HistoryRow", i);
        bool row_hover = Clay_PointerOver(row_id);

        Clay_Color row_bg =
            entry->is_selected
                ? (Clay_Color){255, 255, 255, 14}
                : (row_hover ? get_hover_color() : (Clay_Color){0, 0, 0, 0});

        CLAY({.id = row_id,
              .backgroundColor = row_bg,
              .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .padding = {8, 16, 8, 16},
                         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, 0)},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .clip = {.horizontal = true}}) {

          CLAY({.layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 4,
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0, 0)}}}) {

            Clay_String pv_s;
            pv_s = entry->preview[0]
                       ? (Clay_String){.chars = entry->preview,
                                       .isStaticallyAllocated = true,
                                       .length = (int)strlen(entry->preview)}
                       : CLAY_STRING("(empty)");
            CLAY_TEXT(pv_s,
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = get_text_color(),
                           .wrapMode = CLAY_TEXT_WRAP_NONE}));

            Clay_String date = {.chars = entry->date,
                                .isStaticallyAllocated = true,
                                .length = (int)strlen(entry->date)};
            CLAY_TEXT(date,
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 15,
                           .textColor = get_secondary_text_color(),
                           .wrapMode = CLAY_TEXT_WRAP_NONE}));
          }

          if (row_hover) {
            Clay_ElementId export_id = CLAY_IDI("Export", i);
            bool hot = Clay_PointerOver(export_id);
            uint32_t base = g_app->dark_mode ? 0xA0A0A0FF : 0x808080FF;
            uint32_t on = g_app->dark_mode ? 0xFFFFFFFF : 0x000000FF;
            uint32_t col = hot ? on : base;
            svg_element_t *ic = &g_app->gfx.icons[RES_ICON_ARROW_DOWN_CIRCLE];
            ic->fill_color = col;
            ic->stroke_color = col;
            CLAY({.id = export_id,
                  .custom = {.customData = ic},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(12),
                                        CLAY_SIZING_FIXED(12)}}}) {
              Clay_OnHover(HandleEntryExport, (intptr_t)i);
            }

            CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(8),
                                        CLAY_SIZING_FIXED(1)}}}) {}

            Clay_ElementId del_id = CLAY_IDI("Delete", i);
            bool hot2 = Clay_PointerOver(del_id);
            uint32_t col2 = hot2 ? 0xFF5A5AFF
                                 : (g_app->dark_mode ? 0xB8B8B8FF : 0x808080FF);
            svg_element_t *ic2 = &g_app->gfx.icons[RES_ICON_TRASH];
            ic2->fill_color = col2;
            ic2->stroke_color = col2;
            CLAY({.id = del_id,
                  .custom = {.customData = ic2},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(12),
                                        CLAY_SIZING_FIXED(12)}}}) {
              Clay_OnHover(HandleEntryDelete, (intptr_t)i);
            }
          }
        }

        if (row_hover) {
          g_app->hovered_entry_index = (int)i;
          Clay_OnHover(HandleEntrySelect, (intptr_t)i);
        } else if (g_app->hovered_entry_index == (int)i) {
          g_app->hovered_entry_index = -1;
        }

        if (i < g_app->history.count - 1) {
          CLAY({.backgroundColor = get_border_color(),
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1)}}}) {}
        }
      }
    }
  }
}

static void render_editor_ui() {
  TextEditor *editor = &g_app->editor;

  CLAY({.id = CLAY_ID("MainContainer"),
        .backgroundColor = get_background_color(),
        .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {

    CLAY({.id = CLAY_ID("ContentArea"),
          .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {

      CLAY(
          {.id = CLAY_ID("EditorContainer"),
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                      .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
           .clip = {.vertical = true, .childOffset = {0, -editor->scroll_y}}}) {
        if (g_app->lol) {
          CLAY({.image = {.imageData = &g_app->gfx.images[RES_IMG_WIN]},
                .layout = {.sizing = {CLAY_SIZING_FIXED(512.0f),
                                      CLAY_SIZING_FIXED(512.0f)}},
                .floating = {
                    .attachTo = CLAY_ATTACH_TO_PARENT,
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                     .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                    .zIndex = 100}}) {}
        }

        CLAY({.id = CLAY_ID("TextEditor"),
              .layout = {
                  .sizing = {CLAY_SIZING_FIXED(650.0f), CLAY_SIZING_FIT(0, 0)},
                  .padding = {40, 40, 40, 40},
                  .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {

          if (editor->render_line_count == 0 ||
              (editor->render_line_count == 1 &&
               strlen(editor->render_lines[0]) == 0)) {
            CLAY_TEXT(*get_welcome_message(),
                      CLAY_TEXT_CONFIG(
                          {.fontId = get_current_font(),
                           .fontSize = font_sizes[g_app->font_size_index],
                           .lineHeight = font_sizes[g_app->font_size_index] - 2,
                           .textColor = get_secondary_text_color(),
                           .textAlignment = CLAY_TEXT_ALIGN_CENTER}));
          } else {
            for (size_t i = 0; i < editor->render_line_count; i++) {
              Clay_ElementId line_id = CLAY_IDI("EditorLine", i);
              void *line_tag =
                  (i == editor->cursor.line) ? &g_app->current_line_tag : NULL;

              CLAY({.id = line_id,
                    .layout = {.sizing = {CLAY_SIZING_FIT(0, 0),
                                          CLAY_SIZING_GROW(0)}}}) {
                if (strlen(editor->render_lines[i]) > 0) {
                  Clay_String text = {.chars = editor->render_lines[i],
                                      .isStaticallyAllocated = true,
                                      .length =
                                          (int)strlen(editor->render_lines[i])};

                  CLAY_TEXT(
                      text,
                      CLAY_TEXT_CONFIG(
                          {.userData = line_tag,
                           .textColor = get_text_color(),
                           .fontId = get_current_font(),
                           .fontSize = font_sizes[g_app->font_size_index],
                           .lineHeight = font_sizes[g_app->font_size_index] - 2,
                           .wrapMode = CLAY_TEXT_WRAP_OVERFLOW_BREAK_WORD,
                           .textAlignment = CLAY_TEXT_ALIGN_LEFT}));
                } else {
                  CLAY_TEXT(
                      EMPTY_STRING,
                      CLAY_TEXT_CONFIG(
                          {.userData = line_tag,
                           .textColor = get_text_color(),
                           .fontId = get_current_font(),
                           .fontSize = font_sizes[g_app->font_size_index],
                           .lineHeight = font_sizes[g_app->font_size_index] - 2,
                           .wrapMode = CLAY_TEXT_WRAP_OVERFLOW_BREAK_WORD,
                           .textAlignment = CLAY_TEXT_ALIGN_LEFT}));
                }
              }
            }
          }
        }
      }

      CLAY(
          {.id = CLAY_ID("BottomNav"),
           .backgroundColor = get_background_color(),
           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(68.0f)},
                      .padding = {16, 16, 16, 16},
                      .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {

        bool is_hovering = Clay_Hovered();
        g_app->bottom_nav_hovering = is_hovering;

        if (is_hovering && g_app->timer_running) {
          g_app->bottom_nav_opacity = 1.0f;
        }

        float opacity = g_app->bottom_nav_opacity;
        Clay_Color dot_color = get_secondary_text_color();
        dot_color.a = (uint8_t)(dot_color.a * opacity);
        CLAY({.id = CLAY_ID("FontControls"),
              .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .padding = {8, 8, 8, 8}}}) {

          CLAY({.id = CLAY_ID("FontSize")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(font_size_strings[g_app->font_size_index],
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFontSizeChange, 0);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("FontLato")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("Lato"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFontSelect, (intptr_t)RES_FONT_LATO_REGULAR);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("FontArial")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("Arial"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFontSelect, (intptr_t)RES_FONT_ARIAL);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("FontSerif")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("Serif"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFontSelect, (intptr_t)RES_FONT_SERIF);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("FontMono")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("Mono"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFontSelect, (intptr_t)RES_FONT_FIRA_MONO);
          }
        }

        CLAY({.layout = {
                  .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(0.0f)}}}) {}

        CLAY({.id = CLAY_ID("UtilityControls"),
              .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .padding = {8, 8, 8, 8}}}) {

          CLAY({.id = CLAY_ID("Timer")}) {
            Clay_String time = {.chars = g_app->timer_string,
                                .isStaticallyAllocated = true,
                                .length = (int)strlen(g_app->timer_string)};
            Clay_Color timer_color =
                g_app->timer_running ? get_text_color() : get_ui_text_color();
            timer_color.a = (uint8_t)(timer_color.a * opacity);
            CLAY_TEXT(time,
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = timer_color}));
            Clay_OnHover(HandleTimerToggle, 0);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("ChatButton")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("Chat"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleLulCow, 0);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("FullscreenButton")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(g_app->window_state == APP_WINDOW_FULLSCREEN
                          ? CLAY_STRING("Minimize")
                          : CLAY_STRING("Fullscreen"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleFullscreenToggle, 0);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          CLAY({.id = CLAY_ID("NewEntryButton")}) {
            Clay_Color text_color = get_ui_text_color();
            text_color.a = (uint8_t)(text_color.a * opacity);
            CLAY_TEXT(CLAY_STRING("New Entry"),
                      CLAY_TEXT_CONFIG(
                          {.fontId = g_app->gfx.fonts[RES_FONT_LATO_REGULAR],
                           .fontSize = 16,
                           .textColor = text_color}));
            Clay_OnHover(HandleNewEntry, 0);
          }

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          uint32_t icon_normal = g_app->dark_mode ? 0x808080FF : 0x808080FF;
          uint32_t icon_hover = g_app->dark_mode ? 0xFFFFFFFF : 0x000000FF;

          uint8_t alpha_normal = (uint8_t)(0x80 * opacity);
          uint8_t alpha_hover = (uint8_t)(0xFF * opacity);
          icon_normal = (icon_normal & 0xFFFFFF00) | alpha_normal;
          icon_hover = (icon_hover & 0xFFFFFF00) | alpha_hover;

          RenderIconButton(CLAY_STRING("ThemeToggle"),
                           g_app->dark_mode ? RES_ICON_SUN : RES_ICON_MOON,
                           icon_normal, icon_hover, toggleTheme, 16.0f,
                           opacity);

          CLAY_TEXT(CLAY_STRING("•"),
                    CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = dot_color}));

          RenderIconButton(CLAY_STRING("HistoryButton"), RES_ICON_HISTORY,
                           icon_normal, icon_hover, toggleHistoryBar, 16.0f,
                           opacity);
        }
      }
    }

    if (g_app->show_sidebar) {
      CLAY(
          {.backgroundColor = get_border_color(),
           .layout = {.sizing = {CLAY_SIZING_FIXED(1), CLAY_SIZING_GROW(0)}}}) {
      }
      render_sidebar();
    }
  }
}

static void render_selection_quads(Clay_RenderCommandArray commands) {
  TextEditor *editor = &g_app->editor;

  if (!editor->has_selection)
    return;

  size_t sel_start = editor->sel_start;
  size_t sel_end = editor->sel_end;
  if (sel_start > sel_end) {
    size_t tmp = sel_start;
    sel_start = sel_end;
    sel_end = tmp;
  }

  SelectionVertex *sel_vertices = g_app->selection.vertices;
  uint16_t *sel_indices = g_app->selection.indices;
  int sel_quad_count = 0;

  float dpi_scale = sapp_dpi_scale();
  float screen_width = sapp_width();
  float screen_height = sapp_height();

  Clay_Color sel_color = get_selection_color();
  float r = sel_color.r / 255.0f;
  float g = sel_color.g / 255.0f;
  float b = sel_color.b / 255.0f;
  float a = sel_color.a / 255.0f;

  size_t byte_pos = 0;

  for (size_t line_idx = 0; line_idx < editor->render_line_count; line_idx++) {
    const char *line_chars = editor->render_lines[line_idx];
    size_t line_len = strlen(line_chars);
    size_t line_start = byte_pos;
    size_t line_end = byte_pos + line_len;

    if (sel_end <= line_start || sel_start > line_end) {
      byte_pos += line_len + 1;
      continue;
    }

    int32_t sel_in_line_start =
        (int32_t)((sel_start > line_start) ? (sel_start - line_start) : 0);
    int32_t sel_in_line_end =
        (int32_t)((sel_end < line_end) ? (sel_end - line_start) : line_len);

    if (line_len == 0) {

      Clay_ElementId line_id = CLAY_IDI("EditorLine", line_idx);
      Clay_ElementData line_data = Clay_GetElementData(line_id);

      if (line_data.found) {
        float x1 = line_data.boundingBox.x;
        float x2 = x1 + 20.0f;
        float y1 = line_data.boundingBox.y;
        float y2 = y1 + line_data.boundingBox.height;

        float ndc_x1 = ((x1 * dpi_scale) / screen_width) * 2.0f - 1.0f;
        float ndc_x2 = ((x2 * dpi_scale) / screen_width) * 2.0f - 1.0f;
        float ndc_y1 = 1.0f - ((y1 * dpi_scale) / screen_height) * 2.0f;
        float ndc_y2 = 1.0f - ((y2 * dpi_scale) / screen_height) * 2.0f;

        int base_idx = sel_quad_count * 4;
        sel_vertices[base_idx + 0] =
            (SelectionVertex){ndc_x1, ndc_y1, r, g, b, a};
        sel_vertices[base_idx + 1] =
            (SelectionVertex){ndc_x2, ndc_y1, r, g, b, a};
        sel_vertices[base_idx + 2] =
            (SelectionVertex){ndc_x2, ndc_y2, r, g, b, a};
        sel_vertices[base_idx + 3] =
            (SelectionVertex){ndc_x1, ndc_y2, r, g, b, a};

        int ibase = sel_quad_count * 6;
        sel_indices[ibase + 0] = base_idx + 0;
        sel_indices[ibase + 1] = base_idx + 1;
        sel_indices[ibase + 2] = base_idx + 2;
        sel_indices[ibase + 3] = base_idx + 0;
        sel_indices[ibase + 4] = base_idx + 2;
        sel_indices[ibase + 5] = base_idx + 3;

        sel_quad_count++;
      }
    } else {

      for (int i = 0; i < commands.length; i++) {
        Clay_RenderCommand *cmd = &commands.internalArray[i];
        if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT)
          continue;

        const Clay_TextRenderData *td = &cmd->renderData.text;
        if (td->stringContents.baseChars != line_chars)
          continue;

        int32_t wl_start = td->wrapLineStartOffset;
        int32_t wl_end = wl_start + td->wrapLineLength;

        int32_t is =
            sel_in_line_start > wl_start ? sel_in_line_start : wl_start;
        int32_t ie = sel_in_line_end < wl_end ? sel_in_line_end : wl_end;
        if (ie <= is)
          continue;

        int start_idx = 0;
        while (start_idx < (int)td->caret.caretCount &&
               td->caret.byteOffsets[start_idx] < is)
          start_idx++;
        if (start_idx >= (int)td->caret.caretCount)
          start_idx = (int)td->caret.caretCount - 1;

        int end_idx = 0;
        while (end_idx < (int)td->caret.caretCount &&
               td->caret.byteOffsets[end_idx] < ie)
          end_idx++;
        if (end_idx >= (int)td->caret.caretCount)
          end_idx = (int)td->caret.caretCount - 1;

        float x_start = cmd->boundingBox.x + td->caret.prefixX[start_idx];
        float x_end = cmd->boundingBox.x + td->caret.prefixX[end_idx];
        float y_top = cmd->boundingBox.y;
        float y_bot = y_top + cmd->boundingBox.height;

        float ndc_x1 = ((x_start * dpi_scale) / screen_width) * 2.0f - 1.0f;
        float ndc_x2 = ((x_end * dpi_scale) / screen_width) * 2.0f - 1.0f;
        float ndc_y1 = 1.0f - ((y_top * dpi_scale) / screen_height) * 2.0f;
        float ndc_y2 = 1.0f - ((y_bot * dpi_scale) / screen_height) * 2.0f;

        int base_idx = sel_quad_count * 4;
        sel_vertices[base_idx + 0] =
            (SelectionVertex){ndc_x1, ndc_y1, r, g, b, a};
        sel_vertices[base_idx + 1] =
            (SelectionVertex){ndc_x2, ndc_y1, r, g, b, a};
        sel_vertices[base_idx + 2] =
            (SelectionVertex){ndc_x2, ndc_y2, r, g, b, a};
        sel_vertices[base_idx + 3] =
            (SelectionVertex){ndc_x1, ndc_y2, r, g, b, a};

        int ibase = sel_quad_count * 6;
        sel_indices[ibase + 0] = base_idx + 0;
        sel_indices[ibase + 1] = base_idx + 1;
        sel_indices[ibase + 2] = base_idx + 2;
        sel_indices[ibase + 3] = base_idx + 0;
        sel_indices[ibase + 4] = base_idx + 2;
        sel_indices[ibase + 5] = base_idx + 3;

        sel_quad_count++;
      }
    }

    byte_pos += line_len + 1;
  }

  if (sel_quad_count > 0) {
    sg_update_buffer(
        g_app->selection.vbuf,
        &(sg_range){.ptr = sel_vertices,
                    .size = sel_quad_count * 4 * sizeof(SelectionVertex)});

    sg_update_buffer(
        g_app->selection.ibuf,
        &(sg_range){.ptr = sel_indices,
                    .size = sel_quad_count * 6 * sizeof(uint16_t)});

    sg_apply_pipeline(g_app->selection.pip);
    sg_apply_bindings(&(sg_bindings){.vertex_buffers[0] = g_app->selection.vbuf,
                                     .index_buffer = g_app->selection.ibuf});
    sg_draw(0, sel_quad_count * 6, 1);
  }
}

static void update_cursor_position(Clay_RenderCommandArray commands) {
  TextEditor *editor = &g_app->editor;
  size_t col = editor->cursor.col;
  bool caret_set = false;

  Clay_RenderCommand *last_cmd = NULL;
  const Clay_TextRenderData *last_td = NULL;

  for (int i = 0; i < commands.length; i++) {
    Clay_RenderCommand *cmd = &commands.internalArray[i];
    if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT)
      continue;
    if (cmd->userData != &g_app->current_line_tag)
      continue;

    const Clay_TextRenderData *td = &cmd->renderData.text;

    if (!last_cmd || td->wrapLineIndex > last_td->wrapLineIndex) {
      last_cmd = cmd;
      last_td = td;
    }

    const int32_t wl_start = td->wrapLineStartOffset;
    const int32_t wl_end = wl_start + td->wrapLineLength;

    if ((int32_t)col < wl_start || (int32_t)col > wl_end)
      continue;

    int idx;
    if ((int32_t)col == wl_end) {
      idx = (int)td->caret.caretCount - 1;
      if (idx < 0)
        idx = 0;
    } else {
      idx = 0;
      while (idx < (int)td->caret.caretCount &&
             td->caret.byteOffsets[idx] < (int32_t)col)
        idx++;
      if (idx >= (int)td->caret.caretCount)
        idx = (int)td->caret.caretCount - 1;
    }

    float x = cmd->boundingBox.x + td->caret.prefixX[idx];
    float y = cmd->boundingBox.y;

    g_app->cursor.cursor_x = x;
    g_app->cursor.cursor_y = y;
    g_app->cursor.cursor_height = cmd->boundingBox.height;

    caret_set = true;
    break;
  }

  if (!caret_set && last_cmd) {
    int last_idx = (int)last_td->caret.caretCount - 1;
    if (last_idx < 0)
      last_idx = 0;

    float x = last_cmd->boundingBox.x + last_td->caret.prefixX[last_idx];
    float y = last_cmd->boundingBox.y;

    g_app->cursor.cursor_x = x;
    g_app->cursor.cursor_y = y;
    g_app->cursor.cursor_height = last_cmd->boundingBox.height;

    caret_set = true;
  }

  if (!caret_set) {
    Clay_ElementId line_id = CLAY_IDI("EditorLine", editor->cursor.line);
    Clay_ElementData line_data = Clay_GetElementData(line_id);
    if (line_data.found) {
      g_app->cursor.cursor_x = line_data.boundingBox.x;
      g_app->cursor.cursor_y = line_data.boundingBox.y;
      g_app->cursor.cursor_height = line_data.boundingBox.height;
    }
  }
}

static void render_cursor() {
  float dpi_scale = sapp_dpi_scale();
  float time = stm_sec(stm_since(g_app->start_time));

  g_app->cursor.uniforms.iResolution.X = (float)sapp_width();
  g_app->cursor.uniforms.iResolution.Y = (float)sapp_height();
  g_app->cursor.uniforms.iTime = time;

  float new_x = g_app->cursor.cursor_x * dpi_scale;
  float new_y = g_app->cursor.uniforms.iResolution.Y -
                (g_app->cursor.cursor_y * dpi_scale) -
                (g_app->cursor.cursor_height * dpi_scale);

  if (fabsf(new_x - g_app->cursor.uniforms.iCurrentCursor.X) > 0.1f ||
      fabsf(new_y - g_app->cursor.uniforms.iCurrentCursor.Y) > 0.1f) {
    g_app->cursor.uniforms.iPreviousCursor =
        g_app->cursor.uniforms.iCurrentCursor;
    g_app->cursor.uniforms.iTimeCursorChange = time;
  }

  g_app->cursor.uniforms.iCurrentCursor.X = new_x;
  g_app->cursor.uniforms.iCurrentCursor.Y = new_y;
  g_app->cursor.uniforms.iCurrentCursor.Z = 2.0f * dpi_scale;
  g_app->cursor.uniforms.iCurrentCursor.W =
      g_app->cursor.cursor_height * dpi_scale;

  if (g_app->cursor.uniforms.iPreviousCursor.Z == 0) {
    g_app->cursor.uniforms.iPreviousCursor =
        g_app->cursor.uniforms.iCurrentCursor;
  }

  sg_apply_pipeline(g_app->cursor.pip);
  sg_apply_bindings(&g_app->cursor.bind);
  sg_apply_uniforms(UB_globals, &SG_RANGE(g_app->cursor.uniforms));
  sg_draw(0, 6, 1);
}

static void HandleClayErrors(Clay_ErrorData errorData) {
  printf("%s", errorData.errorText.chars);
  exit(1);
}

char *app_strdup(const char *s, size_t len) {
  if (s == NULL) {
    return NULL;
  }
  char *dup = malloc(len);
  if (dup == NULL) {
    return NULL;
  }
  memcpy(dup, s, len);
  return dup;
}

static void init(void) {
#if !defined(__EMSCRIPTEN__)
  app_make_compact_window(false);
  app_set_minimum_window_size(APP_MIN_WIDTH, APP_MIN_HEIGHT);
#endif

  stm_setup();
  srand(time(NULL));
  sg_setup(
      &(sg_desc){.environment = sglue_environment(), .logger.func = slog_func});
  sgl_setup(&(sgl_desc_t){.logger.func = slog_func});
  sclay_setup();
  Clay_SetMaxMeasureTextCacheWordCount(65536);

  g_app = calloc(1, sizeof(AppState));

  g_app->dark_mode = false;
  g_app->window_state = APP_WINDOW_NORMAL;
  g_app->show_sidebar = false;
  g_app->bottom_nav_opacity = 1.0f;
  g_app->bottom_nav_hovering = false;
  g_app->bottom_nav_fade_time = 0;
  g_app->hovered_entry_index = -1;

  timer_init(900);
  g_app->last_timer_update = stm_now();

  g_app->start_time = stm_now();
  g_app->last_save_time = stm_now();
  g_app->needs_save = false;

  g_app->history.capacity = 32;
  g_app->history.entries = calloc(g_app->history.capacity, sizeof(FileEntry));
  g_app->history.count = 0;
  g_app->history.selected_index = -1;

  files_get_home_directory(g_app->documents_path,
                           sizeof(g_app->documents_path));
  files_ensure_directory(g_app->documents_path);

  uint64_t totalMemorySize = Clay_MinMemorySize() * 2;
  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(
      clayMemory, (Clay_Dimensions){(float)sapp_width(), (float)sapp_height()},
      (Clay_ErrorHandler){.errorHandlerFunction = HandleClayErrors});

  RES_FOR_EACH_FONT(i) {
    const unsigned char *font_data = RES_GET_FONT_DATA(i);
    size_t font_size = RES_GET_FONT_SIZE(i);
    g_app->gfx.fonts[i] =
        sclay_add_font_mem((unsigned char *)font_data, font_size);
  }
  Clay_SetMeasureTextFunction(sclay_measure_text, g_app->gfx.fonts);

  RES_FOR_EACH_IMAGE(i) {
    const EmbeddedBlob *img_blob = RES_GET_IMAGE(i);
    g_app->gfx.images[i] = make_image_from_embed(img_blob);
  }

  RES_FOR_EACH_ICON(i) {
    const EmbeddedBlob *icon_blob = RES_GET_ICON(i);
    char *svg_copy = app_strdup((const char *)icon_blob->data, icon_blob->size);
    printf("Parsing SVG icon %d: %s\n", i, icon_blob->name);
    NSVGimage *svg = svg_parse(svg_copy, 96.0f);
    free(svg_copy);

    if (!svg) {
      fprintf(stderr, "Failed to parse SVG icon %d: %s\n", i, icon_blob->name);
      g_app->gfx.icons[i] =
          (svg_element_t){.image = NULL,
                          .fill_color = RES_GET_ICON_FILL(i),
                          .stroke_color = RES_GET_ICON_STROKE(i),
                          .opacity_override = 1.0f};
      continue;
    }

    g_app->gfx.icons[i] =
        (svg_element_t){.image = svg,
                        .fill_color = RES_GET_ICON_FILL(i),
                        .stroke_color = RES_GET_ICON_STROKE(i),
                        .opacity_override = 1.0f};
  }

  text_editor_init(&g_app->editor, 4096);
  load_existing_entries();

  sg_shader cursor_shd = sg_make_shader(cursor_shader_desc(sg_query_backend()));

  float cursor_vertices[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,
  };
  sg_buffer cursor_vbuf =
      sg_make_buffer(&(sg_buffer_desc){.data = SG_RANGE(cursor_vertices)});

  uint16_t cursor_indices[] = {0, 1, 2, 0, 2, 3};
  sg_buffer cursor_ibuf = sg_make_buffer(&(sg_buffer_desc){
      .usage = {.index_buffer = true}, .data = SG_RANGE(cursor_indices)});

  g_app->cursor.pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = cursor_shd,
      .index_type = SG_INDEXTYPE_UINT16,
      .layout = {.attrs = {[ATTR_cursor_position] =
                               {.format = SG_VERTEXFORMAT_FLOAT2}}},
      .colors[0].blend = {.enabled = true,
                          .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                          .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                          .src_factor_alpha = SG_BLENDFACTOR_ONE,
                          .dst_factor_alpha =
                              SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}});

  g_app->cursor.bind = (sg_bindings){.vertex_buffers[0] = cursor_vbuf,
                                     .index_buffer = cursor_ibuf};

  g_app->cursor.uniforms =
      (globals_t){.iResolution = {g_app->screen_width, g_app->screen_height, 0},
                  .iCurrentCursorColor = {0.0f, 0.478f, 1.0f, 1.0f},
                  .iCurrentCursor = {100.0f, 100.0f, 1.0f, 20.0f}};

  sg_shader sel_shd = sg_make_shader(selection_shader_desc(sg_query_backend()));

  g_app->selection.capacity_quads = 512;
  g_app->selection.vertices =
      malloc(g_app->selection.capacity_quads * 4 * sizeof(SelectionVertex));
  g_app->selection.indices =
      malloc(g_app->selection.capacity_quads * 6 * sizeof(uint16_t));

  g_app->selection.vbuf = sg_make_buffer(&(sg_buffer_desc){
      .usage = {.stream_update = true, .vertex_buffer = true},
      .size = g_app->selection.capacity_quads * 4 * sizeof(SelectionVertex),
  });

  g_app->selection.ibuf = sg_make_buffer(&(sg_buffer_desc){
      .usage = {.stream_update = true, .index_buffer = true},
      .size = g_app->selection.capacity_quads * 6 * sizeof(uint16_t),
  });

  g_app->selection.pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = sel_shd,
      .index_type = SG_INDEXTYPE_UINT16,
      .layout =
          {.attrs =
               {[ATTR_selection_position] = {.format = SG_VERTEXFORMAT_FLOAT2},
                [ATTR_selection_color0] = {.format = SG_VERTEXFORMAT_FLOAT4}}},
      .colors[0] = {.blend = {.enabled = true,
                              .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                              .dst_factor_rgb =
                                  SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                              .src_factor_alpha = SG_BLENDFACTOR_ONE,
                              .dst_factor_alpha =
                                  SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA}},
      .cull_mode = SG_CULLMODE_NONE,
      .face_winding = SG_FACEWINDING_CCW});

  svg_init(sapp_sample_count());

  g_app->initialized = true;
}

static void event_cb(const sapp_event *ev) {
  if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_F1) {
    Clay_SetDebugModeEnabled(true);
  } else {
    sclay_handle_event(ev);
    handle_mouse_input(ev);
    handle_key_input(ev);
    handle_char_input(ev);
  }
}

static void render_svgs(Clay_RenderCommandArray commands) {
  const float dpi = sapp_dpi_scale();

  for (uint32_t i = 0; i < commands.length; i++) {
    Clay_RenderCommand *cmd = &commands.internalArray[i];
    if (cmd->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM)
      continue;

    svg_element_t *el = (svg_element_t *)cmd->renderData.custom.customData;
    if (!el || !el->image)
      continue;

    const Clay_BoundingBox bb = cmd->boundingBox;
    svg_render_element(el, bb.x, bb.y, bb.width, bb.height, dpi);
  }
}

static void frame(void) {
  if (!g_app->initialized) {
    return;
  }
  g_app->screen_width = sapp_widthf();
  g_app->screen_height = sapp_heightf();

  uint64_t now = timer_update();

  if (stm_sec(stm_diff(now, g_app->last_save_time)) >= 60.0) {
    save_current_entry();
    g_app->last_save_time = now;
  }

  if (g_app->timer_running && !g_app->bottom_nav_hovering) {

    uint64_t fade_elapsed = stm_diff(now, g_app->bottom_nav_fade_time);
    if (stm_sec(fade_elapsed) > 1.0) {
      g_app->bottom_nav_opacity -= 0.02f;
      if (g_app->bottom_nav_opacity < 0.0f) {
        g_app->bottom_nav_opacity = 0.0f;
      }
    }
  } else if (!g_app->timer_running || g_app->bottom_nav_hovering) {

    g_app->bottom_nav_opacity += 0.05f;
    if (g_app->bottom_nav_opacity > 1.0f) {
      g_app->bottom_nav_opacity = 1.0f;
    }
  }

  g_app->editor.scroll_y +=
      (g_app->editor.target_scroll_y - g_app->editor.scroll_y) * 0.2f;

  text_editor_prepare_render_lines(&g_app->editor);

  sclay_new_frame();
  Clay_BeginLayout();

  render_editor_ui();

  Clay_RenderCommandArray commands = Clay_EndLayout();

  if (g_app->editor.mouse.mouse_down) {
    size_t mouse_byte_pos = caret_byte_from_xy(&g_app->editor, commands,
                                               g_app->editor.mouse.mouse_x,
                                               g_app->editor.mouse.mouse_y);

    if (!g_app->editor.mouse.dragging) {
      text_editor_move_to_pos(&g_app->editor, mouse_byte_pos);
      text_editor_clear_selection(&g_app->editor);
      g_app->editor.sel_start = mouse_byte_pos;
      g_app->editor.sel_end = mouse_byte_pos;
      g_app->editor.mouse.dragging = true;
    } else {
      g_app->editor.sel_end = mouse_byte_pos;
      g_app->editor.has_selection =
          (g_app->editor.sel_start != g_app->editor.sel_end);
      text_editor_move_to_pos(&g_app->editor, mouse_byte_pos);
    }
  } else {
    g_app->editor.mouse.dragging = false;
  }

  update_cursor_position(commands);

  sg_begin_pass(&(sg_pass){
      .action = {.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                               .clear_value = {0.95f, 0.95f, 0.95f, 1.0f}}},
      .swapchain = sglue_swapchain()});

  sgl_load_identity();
  sclay_render(commands, g_app->gfx.fonts);
  sgl_draw();

  render_selection_quads(commands);
  render_cursor();

  svg_begin_draw(sapp_width(), sapp_height());
  render_svgs(commands);
  svg_end_draw();

  sg_end_pass();
  sg_commit();
}

static void cleanup(void) {
  if (g_app) {
    save_current_entry();

    if (g_app->history.entries) {
      free(g_app->history.entries);
    }

    if (g_app->selection.vertices) {
      free(g_app->selection.vertices);
    }
    if (g_app->selection.indices) {
      free(g_app->selection.indices);
    }

    text_editor_destroy(&g_app->editor);

    for (int i = 0; i < RES_ICON_COUNT; i++) {
      if (g_app->gfx.fonts[i] != 0) {
        svg_free(g_app->gfx.icons[i].image);
        g_app->gfx.icons[i].image = NULL;
      }
    }

    for (int i = 0; i < RES_IMG_COUNT; i++) {
      sg_destroy_image(g_app->gfx.images[i].image);
    }

    free(g_app);
    g_app = NULL;
  }
  sgl_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = event_cb,
      .sample_count = 4,

      .width = 1280,
      .height = 720,
      .enable_clipboard = true,
      .enable_dragndrop = false,
      .high_dpi = true,
      .window_title = "andex",
      .icon.sokol_default = true,
      .logger = {.func = slog_func},
      .html5_canvas_selector = "#canvas",
      .html5_canvas_resize = false,
      .html5_preserve_drawing_buffer = false,
      .html5_premultiplied_alpha = false,
      .html5_ask_leave_site = false,
  };
}