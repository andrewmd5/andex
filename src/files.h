#ifndef FILES_H
#define FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>


typedef struct {
    char *path;
    void (*callback)(bool success, void *user_data);
    void *user_data;
} DeleteFileContext;

typedef struct {
  char path[512];
  char name[256];
  time_t modified;
  size_t size;
  bool is_directory;
} FileMetadata;

typedef struct {
  FileMetadata *items;
  size_t count;
  size_t capacity;
} FileList;

bool files_ensure_directory(const char *path);
bool files_read_file(const char *path, char **out_data, size_t *out_size);
bool files_read_bytes(const char *path, uint8_t **out_data, size_t *out_size);
bool files_write_file(const char *path, const char *data, size_t size);
bool files_move_file(const char *src, char *default_name, const void *window);
bool files_delete_file(const char *path);
bool files_list_directory(const char *path, FileList *out_list);
void files_free_list(FileList *list);
void files_delete_confirm(const char *path, void (*callback)(bool success, void *user_data), const void* parent, void *user_data);

void files_open_directory(const char *path);
void files_get_home_directory(char *out_path, size_t size);

#endif