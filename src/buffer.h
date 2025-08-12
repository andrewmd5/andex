#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *buf;
  char *gap_start;
  char *gap_end;
  size_t capacity;
} CharBuffer;

typedef struct {
  size_t *lines;
  size_t *gap_start;
  size_t *gap_end;
  size_t capacity;
} LineBuffer;

void char_buffer_init(CharBuffer *cb, size_t capacity);
void char_buffer_destroy(CharBuffer *cb);
size_t char_buffer_gap_size(CharBuffer *cb);
size_t char_buffer_len(CharBuffer *cb);
void char_buffer_ensure_gap(CharBuffer *cb, size_t needed);
void char_buffer_move_gap(CharBuffer *cb, size_t target_pos);
void char_buffer_insert(CharBuffer *cb, const char *text, size_t len);
void char_buffer_delete_forward(CharBuffer *cb, size_t len);
void char_buffer_delete_backward(CharBuffer *cb, size_t len);
char char_buffer_get_at(CharBuffer *cb, size_t pos);
size_t char_buffer_to_buffer(CharBuffer *cb, char *dest, size_t dest_size);

void line_buffer_init(LineBuffer *lb, size_t capacity);
void line_buffer_destroy(LineBuffer *lb);
size_t line_buffer_count(LineBuffer *lb);
void line_buffer_ensure_gap(LineBuffer *lb, size_t needed);
size_t line_buffer_get(LineBuffer *lb, size_t line);

int utf8_char_len(unsigned char c);
bool is_word_boundary(char c);

#endif