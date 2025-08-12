#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  size_t byte_pos;
  size_t line;
  size_t col;
} CursorPos;

typedef enum { ACTION_INSERT, ACTION_DELETE } ActionType;

typedef struct UndoAction {
  ActionType type;
  size_t pos;
  char *text;
  size_t len;
  struct UndoAction *next;
  struct UndoAction *prev;
} UndoAction;

typedef struct {
  bool dragging;
  bool mouse_down;
  float mouse_x, mouse_y;
  size_t drag_anchor_byte;
} MouseState;

typedef struct {
  CharBuffer chars;
  LineBuffer lines;
  CursorPos cursor;

  bool has_selection;
  size_t sel_start;
  size_t sel_end;

  UndoAction *undo_head;
  UndoAction *undo_current;

  char **render_lines;
  size_t render_line_count;
  size_t render_line_capacity;

  char *render_line_buffer;
  size_t render_buffer_capacity;
  size_t render_buffer_used;

  float scroll_y;
  float target_scroll_y;

  MouseState mouse;
} TextEditor;

void text_editor_init(TextEditor *editor, size_t initial_capacity);
void text_editor_destroy(TextEditor *editor);
void text_editor_rebuild_lines(TextEditor *editor);
void text_editor_update_cursor_pos(TextEditor *editor);
void text_editor_ensure_cursor_visible(TextEditor *editor);
void text_editor_prepare_render_lines(TextEditor *editor);
void text_editor_clear(TextEditor *editor);

void text_editor_move_to_pos(TextEditor *editor, size_t byte_pos);
void text_editor_move_to_line_col(TextEditor *editor, size_t line, size_t col);
void text_editor_move_left(TextEditor *editor);
void text_editor_move_right(TextEditor *editor);
void text_editor_move_up(TextEditor *editor);
void text_editor_move_down(TextEditor *editor);
void text_editor_move_home(TextEditor *editor);
void text_editor_move_end(TextEditor *editor);
void text_editor_move_word_left(TextEditor *editor);
void text_editor_move_word_right(TextEditor *editor);

void text_editor_insert(TextEditor *editor, const char *text, size_t len);
void text_editor_delete_backward(TextEditor *editor);
void text_editor_delete_forward(TextEditor *editor);
void text_editor_delete_word_backward(TextEditor *editor);
void text_editor_delete_word_forward(TextEditor *editor);

void text_editor_clear_selection(TextEditor *editor);
void text_editor_set_selection(TextEditor *editor, size_t start, size_t end);
void text_editor_delete_selection(TextEditor *editor);
char *text_editor_get_selection(TextEditor *editor);

void text_editor_add_undo(TextEditor *editor, ActionType type, size_t pos,
                          const char *text, size_t len);
void text_editor_undo(TextEditor *editor);
void text_editor_redo(TextEditor *editor);

#endif