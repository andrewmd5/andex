#include "files.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif
#define PICK_IMPLEMENTATION
#include "pick.h"

bool files_ensure_directory(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
#ifdef _WIN32
    return mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
  }
  return S_ISDIR(st.st_mode);
}

bool files_read_file(const char *path, char **out_data, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return false;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0) {
    fclose(file);
    return false;
  }

  char *data = malloc(size + 1);
  if (!data) {
    fclose(file);
    return false;
  }

  size_t read = fread(data, 1, size, file);
  fclose(file);

  if (read != (size_t)size) {
    free(data);
    return false;
  }

  data[size] = '\0';
  *out_data = data;
  if (out_size)
    *out_size = size;
  return true;
}

bool files_read_bytes(const char *path, uint8_t **out_data, size_t *out_size) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return false;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0) {
    fclose(file);
    return false;
  }

  uint8_t *data = malloc(size);
  if (!data) {
    fclose(file);
    return false;
  }

  size_t read = fread(data, 1, size, file);
  fclose(file);

  if (read != (size_t)size) {
    free(data);
    return false;
  }

  *out_data = data;
  if (out_size)
    *out_size = size;
  return true;
}

bool files_write_file(const char *path, const char *data, size_t size) {
  FILE *file = fopen(path, "wb");
  if (!file)
    return false;

  size_t written = fwrite(data, 1, size, file);
  fclose(file);
  return written == size;
}

static void move_file_callback(const char *dest_path, void *user_data) {
  char *src_path = (char *)user_data;

  if (dest_path) {
    if (rename(src_path, dest_path) != 0) {
      fprintf(stderr, "Failed to move file from %s to %s\n", src_path,
              dest_path);
    }
  }
  free(src_path);
}

#ifdef __EMSCRIPTEN__
typedef struct {
  char *src;        // malloc'd copy of the MEMFS source path
  int unlink_after; // delete after successful export
} MoveCtx;

static void export_done(bool ok, void *user_data) {
  MoveCtx *ctx = (MoveCtx *)user_data;
  if (ok && ctx->unlink_after) {
    if (remove(ctx->src) != 0) {
      fprintf(stderr, "Warning: exported but failed to delete source %s\n",
              ctx->src);
    } else {
      printf("Successfully exported and deleted source: %s\n", ctx->src);
    }
  } else if (!ok) {
    fprintf(stderr, "Export canceled/failed for %s\n", ctx->src);
  }
  free(ctx->src);
  free(ctx);
}
#endif

bool files_move_file(const char *src, char *default_name, const void *window) {
  PickFileOptions opts = {0};
  opts.title = "Move File To...";
  opts.default_name = default_name;
  opts.can_create_dirs = true;
  opts.parent_handle = window;

#if defined(__EMSCRIPTEN__)
  // IMPORTANT: src must be a valid file path in MEMFS
  if (access(src, F_OK) != 0) {
    fprintf(stderr, "Export failed: source not found in MEMFS: %s\n", src);
    return false;
  }

  MoveCtx *ctx = malloc(sizeof *ctx);
  if (!ctx)
    return false;
  ctx->src = malloc(strlen(src) + 1);
  if (!ctx->src) {
    free(ctx);
    return false;
  }
  memcpy(ctx->src, src, strlen(src) + 1);
  ctx->unlink_after = 1; // emulate move by deleting source on success

  pick_export_file(ctx->src, &opts, export_done, ctx);
#else
  char *src_copy = malloc(strlen(src) + 1);
  if (!src_copy)
    return false;
  memcpy(src_copy, src, strlen(src) + 1);

  pick_save(&opts, move_file_callback, src_copy);
#endif

  return true; // dialog was shown successfully
}

bool files_delete_file(const char *path) {
  if (!path || strlen(path) == 0) {
    fprintf(stderr, "Invalid path for deletion: %s\n", path);
    return false;
  }
  return remove(path) == 0;
}

static void on_delete_confirm(PickButtonResult result, void *user_data) {
    DeleteFileContext *ctx = (DeleteFileContext *)user_data;
    
    printf("Delete confirmation result: %d for path: %s\n", result, ctx->path);
    
    if (result == PICK_RESULT_YES) {  // Should be checking for value 2
        printf("User confirmed deletion, deleting file...\n");
        bool success = (remove(ctx->path) == 0);
        if (ctx->callback) {
            ctx->callback(success, ctx->user_data);
        }
    } else {
        printf("File deletion canceled.\n");
        if (ctx->callback) {
            ctx->callback(false, ctx->user_data);
        }
    }
    
    free(ctx->path);
    free(ctx);
}

void files_delete_confirm(const char *path, void (*callback)(bool success, void *user_data), const void* parent, void *user_data) {
    PickMessageOptions opts = {0};
    opts.title = "Delete File";
    opts.message = "Are you sure you want to delete this file?";
    opts.buttons = PICK_BUTTON_YES_NO;
    opts.style = PICK_STYLE_WARNING;
    opts.icon_type = PICK_ICON_CAUTION;
    opts.parent_handle = parent;
    
    DeleteFileContext *ctx = malloc(sizeof(DeleteFileContext));
    if (!ctx) {
        if (callback) callback(false, user_data);
        return;
    }
    ctx->path = malloc(strlen(path) + 1);
    if (!ctx->path) {
        free(ctx);
        if (callback) callback(false, user_data);
        return;
    }
    strcpy(ctx->path, path);
    ctx->callback = callback;
    ctx->user_data = user_data;
    pick_message(&opts, on_delete_confirm, ctx);
}


bool files_list_directory(const char *path, FileList *out_list) {
  DIR *dir = opendir(path);
  if (!dir)
    return false;

  out_list->count = 0;
  if (!out_list->items) {
    out_list->capacity = 64;
    out_list->items = malloc(out_list->capacity * sizeof(FileMetadata));
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.')
      continue;

    if (out_list->count >= out_list->capacity) {
      out_list->capacity *= 2;
      out_list->items =
          realloc(out_list->items, out_list->capacity * sizeof(FileMetadata));
    }

    FileMetadata *info = &out_list->items[out_list->count];
    snprintf(info->path, sizeof(info->path), "%s/%s", path, entry->d_name);
    strncpy(info->name, entry->d_name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    struct stat st;
    if (stat(info->path, &st) == 0) {
      info->modified = st.st_mtime;
      info->size = st.st_size;
      info->is_directory = S_ISDIR(st.st_mode);
    } else {
      info->modified = 0;
      info->size = 0;
      info->is_directory = false;
    }

    out_list->count++;
  }

  closedir(dir);
  return true;
}

void files_free_list(FileList *list) {
  if (list->items) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
  }
}

void files_open_directory(const char *path) {
#ifdef __APPLE__
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8 *)path, strlen(path), true);
  if (url) {
    LSOpenCFURLRef(url, NULL);
    CFRelease(url);
  }
#elif defined(_WIN32)
  ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWDEFAULT);
#else

  char command[1024];
  snprintf(command, sizeof(command), "xdg-open \"%s\"", path);
  system(command);
#endif
}

void files_get_home_directory(char *out_path, size_t size) {
#ifdef __EMSCRIPTEN__
  snprintf(out_path, size, "/app");
#elif defined(_WIN32)
  char *home = getenv("USERPROFILE");
  if (home) {
    snprintf(out_path, size, "%s\\Documents\\andex", home);
  } else {
    out_path[0] = '\0';
  }
#else
  char *home = getenv("HOME");
  if (home) {
    snprintf(out_path, size, "%s/Documents/andex", home);
  } else {
    out_path[0] = '\0';
  }
#endif
}