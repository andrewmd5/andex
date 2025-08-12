#include "editor.h"
#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void text_editor_init(TextEditor *editor, size_t initial_capacity) {
  char_buffer_init(&editor->chars, initial_capacity);
  line_buffer_init(&editor->lines, 256);

  editor->cursor.byte_pos = 0;
  editor->cursor.line = 0;
  editor->cursor.col = 0;

  editor->has_selection = false;
  editor->sel_start = 0;
  editor->sel_end = 0;

  editor->undo_head = NULL;
  editor->undo_current = NULL;

  editor->render_line_capacity = 256;
  editor->render_lines = malloc(editor->render_line_capacity * sizeof(char *));
  editor->render_line_count = 0;

  editor->render_buffer_capacity = 65536;
  editor->render_line_buffer = malloc(editor->render_buffer_capacity);
  editor->render_buffer_used = 0;

  editor->scroll_y = 0;
  editor->target_scroll_y = 0;

  editor->mouse.dragging = false;
  editor->mouse.mouse_down = false;
  editor->mouse.mouse_x = 0;
  editor->mouse.mouse_y = 0;
  editor->mouse.drag_anchor_byte = 0;
}

void text_editor_destroy(TextEditor *editor) {
  char_buffer_destroy(&editor->chars);
  line_buffer_destroy(&editor->lines);

  free(editor->render_lines);
  free(editor->render_line_buffer);

  UndoAction *action = editor->undo_head;
  while (action) {
    UndoAction *next = action->next;
    free(action->text);
    free(action);
    action = next;
  }
}

void text_editor_rebuild_lines(TextEditor *editor) {

  editor->lines.gap_start = editor->lines.lines;
  editor->lines.gap_end = editor->lines.lines + editor->lines.capacity;

  size_t current_line_len = 0;
  size_t text_len = char_buffer_len(&editor->chars);

  for (size_t i = 0; i < text_len; i++) {
    char c = char_buffer_get_at(&editor->chars, i);
    if (c == '\n') {
      line_buffer_ensure_gap(&editor->lines, 1);
      *editor->lines.gap_start++ = current_line_len;
      current_line_len = 0;
    } else {
      current_line_len++;
    }
  }

  line_buffer_ensure_gap(&editor->lines, 1);
  *editor->lines.gap_start++ = current_line_len;
}

void text_editor_update_cursor_pos(TextEditor *editor) {
  size_t byte_pos = editor->chars.gap_start - editor->chars.buf;
  editor->cursor.byte_pos = byte_pos;

  size_t line = 0;
  size_t col = 0;

  for (size_t i = 0; i < byte_pos; i++) {
    char c = char_buffer_get_at(&editor->chars, i);
    if (c == '\n') {
      line++;
      col = 0;
    } else {
      col++;
    }
  }

  editor->cursor.line = line;
  editor->cursor.col = col;
}

void text_editor_ensure_cursor_visible(TextEditor *editor) {
  float line_height = 20.0f;
  float cursor_y = editor->cursor.line * line_height;
  float viewport_height = 600.0f;

  if (cursor_y > editor->scroll_y + viewport_height - line_height * 2) {
    editor->target_scroll_y = cursor_y - viewport_height + line_height * 4;
  }

  else if (cursor_y < editor->scroll_y) {
    editor->target_scroll_y = cursor_y - line_height;
  }

  if (editor->target_scroll_y < 0) {
    editor->target_scroll_y = 0;
  }
}

void text_editor_prepare_render_lines(TextEditor *editor) {
  size_t line_count = line_buffer_count(&editor->lines);

  if (line_count > editor->render_line_capacity) {
    editor->render_line_capacity = line_count * 2;
    editor->render_lines = realloc(
        editor->render_lines, editor->render_line_capacity * sizeof(char *));
  }

  size_t total_needed = 0;
  for (size_t i = 0; i < line_count; i++) {
    size_t line_len = line_buffer_get(&editor->lines, i);
    total_needed += line_len + 1;
  }

  if (total_needed > editor->render_buffer_capacity) {
    editor->render_buffer_capacity = total_needed * 2;
    editor->render_line_buffer =
        realloc(editor->render_line_buffer, editor->render_buffer_capacity);
  }

  editor->render_line_count = line_count;
  editor->render_buffer_used = 0;

  size_t byte_pos = 0;
  size_t gap_pos = editor->chars.gap_start - editor->chars.buf;
  size_t gap_size = editor->chars.gap_end - editor->chars.gap_start;

  for (size_t i = 0; i < line_count; i++) {
    size_t line_len = line_buffer_get(&editor->lines, i);

    char *line_text = editor->render_line_buffer + editor->render_buffer_used;
    editor->render_lines[i] = line_text;

    if (byte_pos + line_len <= gap_pos) {

      memcpy(line_text, editor->chars.buf + byte_pos, line_len);
    } else if (byte_pos >= gap_pos) {

      memcpy(line_text, editor->chars.buf + byte_pos + gap_size, line_len);
    } else {

      size_t before_gap = gap_pos - byte_pos;
      memcpy(line_text, editor->chars.buf + byte_pos, before_gap);
      memcpy(line_text + before_gap, editor->chars.gap_end,
             line_len - before_gap);
    }

    line_text[line_len] = '\0';

    editor->render_buffer_used += line_len + 1;
    byte_pos += line_len + 1;
  }
}

void text_editor_move_to_pos(TextEditor *editor, size_t byte_pos) {
  if (byte_pos > char_buffer_len(&editor->chars)) {
    byte_pos = char_buffer_len(&editor->chars);
  }
  char_buffer_move_gap(&editor->chars, byte_pos);
  text_editor_update_cursor_pos(editor);
  text_editor_ensure_cursor_visible(editor);
}

void text_editor_move_to_line_col(TextEditor *editor, size_t line, size_t col) {
  size_t line_count = line_buffer_count(&editor->lines);
  if (line >= line_count)
    line = line_count - 1;

  size_t byte_pos = 0;
  for (size_t i = 0; i < line; i++) {
    byte_pos += line_buffer_get(&editor->lines, i) + 1;
  }

  size_t line_len = line_buffer_get(&editor->lines, line);
  if (col > line_len)
    col = line_len;

  byte_pos += col;
  text_editor_move_to_pos(editor, byte_pos);
}

void text_editor_move_left(TextEditor *editor) {
  if (editor->cursor.byte_pos == 0)
    return;

  size_t move_by = 1;

  while (editor->cursor.byte_pos > move_by &&
         (editor->chars.buf[editor->cursor.byte_pos - move_by] & 0xC0) ==
             0x80) {
    move_by++;
  }

  text_editor_move_to_pos(editor, editor->cursor.byte_pos - move_by);
}

void text_editor_move_right(TextEditor *editor) {
  size_t text_len = char_buffer_len(&editor->chars);
  if (editor->cursor.byte_pos >= text_len)
    return;

  char c = char_buffer_get_at(&editor->chars, editor->cursor.byte_pos);
  size_t move_by = utf8_char_len(c);

  text_editor_move_to_pos(editor, editor->cursor.byte_pos + move_by);
}

void text_editor_move_up(TextEditor *editor) {
  if (editor->cursor.line == 0)
    return;
  text_editor_move_to_line_col(editor, editor->cursor.line - 1,
                               editor->cursor.col);
}

void text_editor_move_down(TextEditor *editor) {
  size_t line_count = line_buffer_count(&editor->lines);
  if (editor->cursor.line >= line_count - 1)
    return;
  text_editor_move_to_line_col(editor, editor->cursor.line + 1,
                               editor->cursor.col);
}

void text_editor_move_home(TextEditor *editor) {
  text_editor_move_to_line_col(editor, editor->cursor.line, 0);
}

void text_editor_move_end(TextEditor *editor) {
  size_t line_len = line_buffer_get(&editor->lines, editor->cursor.line);
  text_editor_move_to_line_col(editor, editor->cursor.line, line_len);
}

void text_editor_move_word_left(TextEditor *editor) {
  if (editor->cursor.byte_pos == 0)
    return;

  size_t pos = editor->cursor.byte_pos;

  while (pos > 0 &&
         !is_word_boundary(char_buffer_get_at(&editor->chars, pos - 1))) {
    pos--;
  }

  while (pos > 0 && isspace(char_buffer_get_at(&editor->chars, pos - 1))) {
    pos--;
  }

  while (pos > 0 &&
         !is_word_boundary(char_buffer_get_at(&editor->chars, pos - 1))) {
    pos--;
  }

  text_editor_move_to_pos(editor, pos);
}

void text_editor_move_word_right(TextEditor *editor) {
  size_t text_len = char_buffer_len(&editor->chars);
  if (editor->cursor.byte_pos >= text_len)
    return;

  size_t pos = editor->cursor.byte_pos;

  while (pos < text_len &&
         !is_word_boundary(char_buffer_get_at(&editor->chars, pos))) {
    pos++;
  }

  while (pos < text_len && isspace(char_buffer_get_at(&editor->chars, pos))) {
    pos++;
  }

  text_editor_move_to_pos(editor, pos);
}

void text_editor_insert(TextEditor *editor, const char *text, size_t len) {

  if (editor->has_selection) {
    text_editor_delete_selection(editor);
  }

  size_t pos = editor->chars.gap_start - editor->chars.buf;
  text_editor_add_undo(editor, ACTION_INSERT, pos, text, len);

  char_buffer_insert(&editor->chars, text, len);
  text_editor_rebuild_lines(editor);
  text_editor_update_cursor_pos(editor);
  text_editor_ensure_cursor_visible(editor);
}

void text_editor_delete_backward(TextEditor *editor) {
  if (editor->has_selection) {
    text_editor_delete_selection(editor);
    return;
  }

  if (editor->cursor.byte_pos == 0)
    return;

  size_t del_len = 1;

  while (editor->cursor.byte_pos > del_len &&
         (editor->chars.buf[editor->cursor.byte_pos - del_len] & 0xC0) ==
             0x80) {
    del_len++;
  }

  char *deleted = malloc(del_len);
  for (size_t i = 0; i < del_len; i++) {
    deleted[i] = char_buffer_get_at(&editor->chars,
                                    editor->cursor.byte_pos - del_len + i);
  }
  text_editor_add_undo(editor, ACTION_DELETE, editor->cursor.byte_pos - del_len,
                       deleted, del_len);
  free(deleted);

  char_buffer_delete_backward(&editor->chars, del_len);
  text_editor_rebuild_lines(editor);
  text_editor_update_cursor_pos(editor);
}

void text_editor_delete_forward(TextEditor *editor) {
  if (editor->has_selection) {
    text_editor_delete_selection(editor);
    return;
  }

  size_t text_len = char_buffer_len(&editor->chars);
  if (editor->cursor.byte_pos >= text_len)
    return;

  char c = char_buffer_get_at(&editor->chars, editor->cursor.byte_pos);
  size_t del_len = utf8_char_len(c);

  char *deleted = malloc(del_len);
  for (size_t i = 0; i < del_len; i++) {
    deleted[i] =
        char_buffer_get_at(&editor->chars, editor->cursor.byte_pos + i);
  }
  text_editor_add_undo(editor, ACTION_DELETE, editor->cursor.byte_pos, deleted,
                       del_len);
  free(deleted);

  char_buffer_delete_forward(&editor->chars, del_len);
  text_editor_rebuild_lines(editor);
  text_editor_update_cursor_pos(editor);
}

void text_editor_delete_word_backward(TextEditor *editor) {
  if (editor->cursor.byte_pos == 0)
    return;

  size_t start_pos = editor->cursor.byte_pos;
  text_editor_move_word_left(editor);
  size_t end_pos = editor->cursor.byte_pos;

  size_t del_len = start_pos - end_pos;

  char *deleted = malloc(del_len);
  for (size_t i = 0; i < del_len; i++) {
    deleted[i] = char_buffer_get_at(&editor->chars, end_pos + i);
  }
  text_editor_add_undo(editor, ACTION_DELETE, end_pos, deleted, del_len);
  free(deleted);

  char_buffer_delete_forward(&editor->chars, del_len);
  text_editor_rebuild_lines(editor);
}

void text_editor_delete_word_forward(TextEditor *editor) {
  size_t text_len = char_buffer_len(&editor->chars);
  if (editor->cursor.byte_pos >= text_len)
    return;

  size_t start_pos = editor->cursor.byte_pos;

  size_t end_pos = start_pos;
  while (end_pos < text_len &&
         !is_word_boundary(char_buffer_get_at(&editor->chars, end_pos))) {
    end_pos++;
  }
  while (end_pos < text_len &&
         isspace(char_buffer_get_at(&editor->chars, end_pos))) {
    end_pos++;
  }

  size_t del_len = end_pos - start_pos;

  char *deleted = malloc(del_len);
  for (size_t i = 0; i < del_len; i++) {
    deleted[i] = char_buffer_get_at(&editor->chars, start_pos + i);
  }
  text_editor_add_undo(editor, ACTION_DELETE, start_pos, deleted, del_len);
  free(deleted);

  char_buffer_delete_forward(&editor->chars, del_len);
  text_editor_rebuild_lines(editor);
}

void text_editor_clear_selection(TextEditor *editor) {
  editor->has_selection = false;
  editor->sel_start = 0;
  editor->sel_end = 0;
}

void text_editor_set_selection(TextEditor *editor, size_t start, size_t end) {
  editor->has_selection = true;
  editor->sel_start = start;
  editor->sel_end = end;
}

void text_editor_delete_selection(TextEditor *editor) {
  if (!editor->has_selection)
    return;

  size_t start = editor->sel_start;
  size_t end = editor->sel_end;
  if (start > end) {
    size_t tmp = start;
    start = end;
    end = tmp;
  }

  size_t len = end - start;
  char *deleted = malloc(len);
  for (size_t i = 0; i < len; i++) {
    deleted[i] = char_buffer_get_at(&editor->chars, start + i);
  }
  text_editor_add_undo(editor, ACTION_DELETE, start, deleted, len);
  free(deleted);

  text_editor_move_to_pos(editor, start);
  char_buffer_delete_forward(&editor->chars, len);
  text_editor_rebuild_lines(editor);
  text_editor_clear_selection(editor);
}

char *text_editor_get_selection(TextEditor *editor) {
  if (!editor->has_selection)
    return NULL;

  size_t start = editor->sel_start;
  size_t end = editor->sel_end;
  if (start > end) {
    size_t tmp = start;
    start = end;
    end = tmp;
  }

  size_t len = end - start;
  char *text = malloc(len + 1);
  for (size_t i = 0; i < len; i++) {
    text[i] = char_buffer_get_at(&editor->chars, start + i);
  }
  text[len] = '\0';
  return text;
}

void text_editor_add_undo(TextEditor *editor, ActionType type, size_t pos,
                          const char *text, size_t len) {
  UndoAction *action = malloc(sizeof(UndoAction));
  action->type = type;
  action->pos = pos;
  action->text = malloc(len);
  memcpy(action->text, text, len);
  action->len = len;

  if (editor->undo_current) {
    UndoAction *next = editor->undo_current->next;
    while (next) {
      UndoAction *to_free = next;
      next = next->next;
      free(to_free->text);
      free(to_free);
    }
    editor->undo_current->next = NULL;
  }

  action->prev = editor->undo_current;
  action->next = NULL;

  if (editor->undo_current) {
    editor->undo_current->next = action;
  } else {
    editor->undo_head = action;
  }

  editor->undo_current = action;
}

void text_editor_undo(TextEditor *editor) {
  if (!editor->undo_current)
    return;

  UndoAction *action = editor->undo_current;

  if (action->type == ACTION_INSERT) {

    text_editor_move_to_pos(editor, action->pos);
    char_buffer_delete_forward(&editor->chars, action->len);
  } else {

    text_editor_move_to_pos(editor, action->pos);
    char_buffer_insert(&editor->chars, action->text, action->len);
  }

  text_editor_rebuild_lines(editor);
  editor->undo_current = action->prev;
}

void text_editor_redo(TextEditor *editor) {
  UndoAction *next =
      editor->undo_current ? editor->undo_current->next : editor->undo_head;
  if (!next)
    return;

  if (next->type == ACTION_INSERT) {

    text_editor_move_to_pos(editor, next->pos);
    char_buffer_insert(&editor->chars, next->text, next->len);
  } else {

    text_editor_move_to_pos(editor, next->pos);
    char_buffer_delete_forward(&editor->chars, next->len);
  }

  text_editor_rebuild_lines(editor);
  editor->undo_current = next;
}

void text_editor_clear(TextEditor *editor) {

  editor->chars.gap_start = editor->chars.buf;
  editor->chars.gap_end = editor->chars.buf + editor->chars.capacity;

  text_editor_clear_selection(editor);

  editor->cursor.byte_pos = 0;
  editor->cursor.line = 0;
  editor->cursor.col = 0;

  text_editor_rebuild_lines(editor);

  editor->scroll_y = 0;
  editor->target_scroll_y = 0;

  editor->render_buffer_used = 0;
  editor->render_line_count = 0;

  text_editor_prepare_render_lines(editor);

  UndoAction *action = editor->undo_head;
  while (action) {
    UndoAction *next = action->next;
    free(action->text);
    free(action);
    action = next;
  }
  editor->undo_head = NULL;
  editor->undo_current = NULL;
}