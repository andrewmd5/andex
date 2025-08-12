#include "buffer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int utf8_char_len(unsigned char c) {
  if ((c & 0x80) == 0)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

bool is_word_boundary(char c) { return !isalnum(c) && c != '_'; }

void char_buffer_init(CharBuffer *cb, size_t capacity) {
  cb->buf = malloc(capacity);
  cb->gap_start = cb->buf;
  cb->gap_end = cb->buf + capacity;
  cb->capacity = capacity;
}

void char_buffer_destroy(CharBuffer *cb) {
  free(cb->buf);
  cb->buf = NULL;
}

size_t char_buffer_gap_size(CharBuffer *cb) {
  return cb->gap_end - cb->gap_start;
}

size_t char_buffer_len(CharBuffer *cb) {
  return cb->capacity - char_buffer_gap_size(cb);
}

void char_buffer_ensure_gap(CharBuffer *cb, size_t needed) {
  size_t gap_size = char_buffer_gap_size(cb);
  if (gap_size >= needed)
    return;

  size_t old_capacity = cb->capacity;
  size_t new_capacity = old_capacity * 2 + needed;
  char *new_buf = malloc(new_capacity);

  size_t before_len = cb->gap_start - cb->buf;
  memcpy(new_buf, cb->buf, before_len);

  size_t after_len = (cb->buf + old_capacity) - cb->gap_end;
  memcpy(new_buf + new_capacity - after_len, cb->gap_end, after_len);

  cb->gap_start = new_buf + before_len;
  cb->gap_end = new_buf + new_capacity - after_len;
  cb->capacity = new_capacity;

  free(cb->buf);
  cb->buf = new_buf;
}

void char_buffer_move_gap(CharBuffer *cb, size_t target_pos) {
  size_t current_pos = cb->gap_start - cb->buf;

  if (target_pos < current_pos) {

    size_t move_len = current_pos - target_pos;
    cb->gap_end -= move_len;
    memmove(cb->gap_end, cb->gap_start - move_len, move_len);
    cb->gap_start -= move_len;
  } else if (target_pos > current_pos) {

    size_t move_len = target_pos - current_pos;
    memmove(cb->gap_start, cb->gap_end, move_len);
    cb->gap_start += move_len;
    cb->gap_end += move_len;
  }
}

void char_buffer_insert(CharBuffer *cb, const char *text, size_t len) {
  char_buffer_ensure_gap(cb, len);
  memcpy(cb->gap_start, text, len);
  cb->gap_start += len;
}

void char_buffer_delete_forward(CharBuffer *cb, size_t len) {
  size_t available = (cb->buf + cb->capacity) - cb->gap_end;
  if (len > available)
    len = available;
  cb->gap_end += len;
}

void char_buffer_delete_backward(CharBuffer *cb, size_t len) {
  size_t available = cb->gap_start - cb->buf;
  if (len > available)
    len = available;
  cb->gap_start -= len;
}

char char_buffer_get_at(CharBuffer *cb, size_t pos) {
  if (pos >= char_buffer_len(cb))
    return '\0';

  size_t gap_pos = cb->gap_start - cb->buf;
  if (pos < gap_pos) {
    return cb->buf[pos];
  } else {
    size_t gap_size = char_buffer_gap_size(cb);
    return cb->buf[pos + gap_size];
  }
}

size_t char_buffer_to_buffer(CharBuffer *cb, char *dest, size_t dest_size) {
  size_t len = char_buffer_len(cb);
  size_t to_copy = (len < dest_size - 1) ? len : dest_size - 1;

  size_t gap_pos = cb->gap_start - cb->buf;
  size_t before_len = (gap_pos < to_copy) ? gap_pos : to_copy;
  size_t after_len = (to_copy > before_len) ? to_copy - before_len : 0;

  if (before_len > 0) {
    memcpy(dest, cb->buf, before_len);
  }

  if (after_len > 0) {
    memcpy(dest + before_len, cb->gap_end, after_len);
  }

  dest[to_copy] = '\0';

  return to_copy;
}

void line_buffer_init(LineBuffer *lb, size_t capacity) {
  lb->lines = malloc(capacity * sizeof(size_t));
  lb->gap_start = lb->lines;
  lb->gap_end = lb->lines + capacity;
  lb->capacity = capacity;

  *lb->gap_start = 0;
  lb->gap_start++;
}

void line_buffer_destroy(LineBuffer *lb) {
  free(lb->lines);
  lb->lines = NULL;
}

size_t line_buffer_count(LineBuffer *lb) {
  return lb->capacity - (lb->gap_end - lb->gap_start);
}

void line_buffer_ensure_gap(LineBuffer *lb, size_t needed) {
  size_t gap_size = lb->gap_end - lb->gap_start;
  if (gap_size >= needed)
    return;

  size_t old_capacity = lb->capacity;
  size_t new_capacity = old_capacity * 2 + needed;
  size_t *new_lines = malloc(new_capacity * sizeof(size_t));

  size_t before_count = lb->gap_start - lb->lines;
  memcpy(new_lines, lb->lines, before_count * sizeof(size_t));

  size_t after_count = (lb->lines + old_capacity) - lb->gap_end;
  memcpy(new_lines + new_capacity - after_count, lb->gap_end,
         after_count * sizeof(size_t));

  lb->gap_start = new_lines + before_count;
  lb->gap_end = new_lines + new_capacity - after_count;
  lb->capacity = new_capacity;

  free(lb->lines);
  lb->lines = new_lines;
}

size_t line_buffer_get(LineBuffer *lb, size_t line) {
  if (line >= line_buffer_count(lb))
    return 0;

  size_t gap_line = lb->gap_start - lb->lines;
  if (line < gap_line) {
    return lb->lines[line];
  } else {
    size_t gap_size = lb->gap_end - lb->gap_start;
    return lb->lines[line + gap_size];
  }
}