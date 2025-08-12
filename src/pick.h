// # pick.h — cross-platform, non-blocking native dialog library (file pickers, message boxes)
// 
//
// ## Table of Contents
//
// - [Quick Start](#quick-start)
// - [API Reference](#api-reference)
//   - [File Picker Functions](#file-picker-functions)
//   - [Message Functions](#message-functions)
//   - [Callback Signatures](#callback-signatures)
// - [Data Structures](#data-structures)
//   - [PickFileOptions](#pickfileoptions)
//   - [PickFilter](#pickfilter)
//   - [PickMessageOptions](#pickmessageoptions)
//   - [PickIconType](#pickicontype)
// - [Platform-Specific Details](#platform-specific-details)
//   - [macOS](#macos)
//   - [Windows](#windows)
//   - [Linux](#linux)
//   - [Web/Emscripten](#webemscripten)
// - [Configuration Macros](#configuration-macros)
// - [Examples](#examples)
// - [License](#license)
//
// ---
//
// ## Quick Start
// 
// Define `PICK_IMPLEMENTATION` in **ONE** source file before including:
//
// ```c
// #define PICK_IMPLEMENTATION
// #include "pick.h"
//
// void file_picked(const char* path, void* user) {
//     if (path) printf("Selected: %s\n", path);
//     else      printf("Cancelled\n");
// }
//
// PickFileOptions opts = {0};
// opts.title = "Select a file";
// pick_file(&opts, file_picked, NULL);
// ```
//
// ---
//
// ## API Reference
//
// ### File Picker Functions
//
// | Function | Description | Callback Type |
// |----------|-------------|---------------|
// | `pick_file()` | Select single file | `PickFileCallback` |
// | `pick_files()` | Select multiple files | `PickMultiFileCallback` |
// | `pick_folder()` | Select single folder | `PickFileCallback` |
// | `pick_folders()` | Select multiple folders | `PickMultiFileCallback` |
// | `pick_save()` | Save file dialog | `PickFileCallback` |
//
// ### Message Functions
//
// | Function | Description | Callback Type | Blocking |
// |----------|-------------|---------------|----------|
// | `pick_message()` | Custom message dialog | `PickMessageCallback` | No |
// | `pick_alert()` | Simple alert box | None | No |
// | `pick_confirm()` | OK/Cancel confirmation | `PickMessageCallback` | No |
//
// ### Callback Signatures
//
// | Type | Signature | Notes |
// |------|-----------|-------|
// | `PickFileCallback` | `void (*)(const char* path, void* user)` | `path` is NULL on cancel |
// | `PickMultiFileCallback` | `void (*)(const char** paths, int count, void* user)` | `paths` is NULL on cancel |
// | `PickMessageCallback` | `void (*)(PickButtonResult result, void* user)` | `result` indicates which button |
//
// **Important:** 
// - All APIs are asynchronous (non-blocking) if you provide a parent window handle. Otherwise, they are blocking.
// - Callbacks are invoked on the main thread
// - String pointers are only valid during callback execution
//
// ---
//
// ## Data Structures
//
// ### PickFileOptions
//
// Configuration for file/folder picker dialogs.
//
// | Field | Type | Description | Default |
// |-------|------|-------------|---------|
// | `title` | `const char*` | Dialog title | Platform default |
// | `default_path` | `const char*` | Initial directory | Last used / home |
// | `default_name` | `const char*` | Suggested filename (save only) | Empty |
// | `filters` | `PickFilter*` | File type filters | All files |
// | `filter_count` | `int` | Number of filters | 0 |
// | `can_create_dirs` | `bool` | Allow creating directories (save only) | false |
// | `allow_multiple` | `bool` | Allow multiple selection | false |
// | `parent_handle` | `const void*` | Parent window handle | NULL |
//
// ### PickFilter
//
// File type filter specification.
//
// | Field | Type | Description | Example |
// |-------|------|-------------|---------|
// | `name` | `const char*` | Display name | "Images" |
// | `extensions` | `const char**` | File extensions (no dots) | {"png", "jpg"} |
// | `extension_count` | `int` | Number of extensions | 2 |
//
// ### PickMessageOptions
//
// Configuration for message dialogs.
//
// | Field | Type | Description | Default |
// |-------|------|-------------|---------|
// | `title` | `const char*` | Dialog title | "Message" |
// | `message` | `const char*` | Message text | Required |
// | `detail` | `const char*` | Additional detail text | NULL |
// | `buttons` | `PickButtonType` | Button configuration | `PICK_BUTTON_OK` |
// | `style` | `PickMessageStyle` | Visual style | `PICK_STYLE_INFO` |
// | `icon_type` | `PickIconType` | Icon type | `PICK_ICON_DEFAULT` |
// | `icon_path` | `const char*` | Path to custom icon | NULL |
// | `parent_handle` | `const void*` | Parent window | NULL |
//
// ### PickButtonType
//
// Button configurations for message dialogs.
//
// | Enum Value | Description | Buttons |
// |------------|-------------|---------|
// | `PICK_BUTTON_OK` | Single OK button | OK |
// | `PICK_BUTTON_OK_CANCEL` | OK and Cancel | OK, Cancel |
// | `PICK_BUTTON_YES_NO` | Yes and No | Yes, No |
// | `PICK_BUTTON_YES_NO_CANCEL` | Yes, No, and Cancel | Yes, No, Cancel |
//
// ### PickButtonResult
//
// Result from message dialog interaction.
//
// | Enum Value | Description |
// |------------|-------------|
// | `PICK_RESULT_OK` | OK button pressed |
// | `PICK_RESULT_CANCEL` | Cancel button pressed |
// | `PICK_RESULT_YES` | Yes button pressed |
// | `PICK_RESULT_NO` | No button pressed |
// | `PICK_RESULT_CLOSED` | Dialog closed without button |
//
// ### PickMessageStyle
//
// Visual styles for message dialogs.
//
// | Enum Value | Description | Usage |
// |------------|-------------|-------|
// | `PICK_STYLE_INFO` | Information | General information |
// | `PICK_STYLE_WARNING` | Warning | Warning messages |
// | `PICK_STYLE_ERROR` | Error | Error messages |
// | `PICK_STYLE_QUESTION` | Question | Yes/No questions |
//
// ### PickIconType
//
// Icon types for message dialogs.
//
// | Enum Value | Description | Platform Support |
// |------------|-------------|------------------|
// | `PICK_ICON_DEFAULT` | Platform default | All |
// | `PICK_ICON_CUSTOM` | Custom from file | macOS, Web |
// | `PICK_ICON_APP` | Application icon | macOS |
// | `PICK_ICON_TRASH` | Trash/Recycle bin | macOS |
// | `PICK_ICON_FOLDER` | Folder | macOS, Web |
// | `PICK_ICON_DOCUMENT` | Document(s) | macOS, Web |
// | `PICK_ICON_LOCKED` | Lock (closed) | macOS |
// | `PICK_ICON_UNLOCKED` | Lock (open) | macOS |
// | `PICK_ICON_NETWORK` | Network | macOS |
// | `PICK_ICON_USER` | User account | macOS |
// | `PICK_ICON_CAUTION` | Caution/Warning | macOS |
// | `PICK_ICON_ERROR` | Error/Critical | macOS, Web |
// | `PICK_ICON_STOP` | Stop sign | macOS |
// | `PICK_ICON_INVALID` | Invalid data | macOS |
//
// ---
//
// ## Platform-Specific Details
//
// ### macOS
//
// **Status:** Implemented  
// **Backend:** NSOpenPanel, NSSavePanel, NSAlert
//
// #### Build Requirements
//
// For single-file implementation:
// ```bash
// clang -x objective-c -framework Foundation -framework Cocoa myapp.c -o myapp
// ```
//
// #### CMake Example
//
// ```cmake
// if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
//   target_compile_options(myapp PRIVATE -x objective-c)
//   target_link_libraries(myapp PRIVATE 
//     "-framework Foundation" 
//     "-framework Cocoa"
//     objc)
// endif()
// ```
//
// #### Sheet Attachment
//
// To attach dialog as sheet, pass `NSWindow*` in `options->parent_handle`.
//
// ### Windows
//
// **Status:** Not implemented  
// **Planned Backend:** IFileDialog, TaskDialog
//
// Including the implementation will trigger `#error`.
//
// ### Linux
//
// **Status:** Not implemented  
// **Planned Backend:** xdg-desktop-portal, GTK
//
// Including the implementation will trigger `#error`.
//
// ### Web/Emscripten
//
// **Status:** Implemented  
// **Backend:** File System Access API, HTML5
//
// #### Build Requirements
//
// ```bash
// emcc main.c -DPICK_IMPLEMENTATION \
//   -sEXPORTED_FUNCTIONS='["_pick__deliver_single","_pick__deliver_multi_lines","_pick__deliver_msg","_main"]' \
//   -sEXPORTED_RUNTIME_METHODS='["ccall"]' \
//   -sFORCE_FILESYSTEM=1 \
//   -sALLOW_MEMORY_GROWTH=1 \
//   -o app.html
// ```
//
// #### CMake Example
//
// ```cmake
// if(CMAKE_SYSTEM_NAME STREQUAL Emscripten)
//   target_link_options(myapp PRIVATE
//     -sFORCE_FILESYSTEM=1
//     "-sEXPORTED_RUNTIME_METHODS=['ccall']"
//     "-sEXPORTED_FUNCTIONS=['_pick__deliver_single','_pick__deliver_multi_lines','_pick__deliver_msg','_main']"
//     -sALLOW_MEMORY_GROWTH=1
//   )
// endif()
// ```
//
// #### File System Paths
//
// | Operation | Default Path | Description |
// |-----------|--------------|-------------|
// | File import | `/picked/` | Selected files copied here |
// | Save operations | `/saved/` | Created files go here |
// | Directory import | `/picked/{structure}/` | Preserves folder hierarchy |
//
// #### Extended API (Emscripten Only)
//
// ```c
// typedef void (*PickResultCallback)(bool success, void* user_data);
//
// void pick_export_file(
//     const char* src_path, 
//     const PickFileOptions* options,
//     PickResultCallback callback, 
//     void* user_data
// );
// ```
//
// Exports a file from MEMFS to user's downloads folder using File System Access API when available.
//
// ---
//
// ## Configuration Macros
//
// Define before including the header to customize behavior.
//
// | Macro | Description | Default | Platform |
// |-------|-------------|---------|----------|
// | `PICK_IMPLEMENTATION` | Enable implementation | undefined | All |
// | `PICK_EM_MAX_REQUESTS` | Max concurrent operations | 64 | Emscripten |
// | `PICK_EM_BASE_PICKED` | Import directory | "/picked" | Emscripten |
// | `PICK_EM_BASE_SAVED` | Save directory | "/saved" | Emscripten |
//
// ---
//
// ## Examples
//
// ### Basic File Selection
//
// ```c
// void on_file_selected(const char* path, void* user) {
//     if (path) {
//         printf("Selected: %s\n", path);
//     }
// }
//
// PickFileOptions opts = {0};
// opts.title = "Choose Document";
// pick_file(&opts, on_file_selected, NULL);
// ```
//
// ### File Type Filtering
//
// ```c
// const char* img_ext[] = {"png", "jpg", "jpeg"};
// const char* doc_ext[] = {"pdf", "doc", "docx"};
//
// PickFilter filters[] = {
//     {"Images", img_ext, 3},
//     {"Documents", doc_ext, 3}
// };
//
// PickFileOptions opts = {0};
// opts.title = "Select File";
// opts.filters = filters;
// opts.filter_count = 2;
// pick_file(&opts, callback, NULL);
// ```
//
// ### Custom Message Dialog
//
// ```c
// void on_button(PickButtonResult result, void* user) {
//     switch(result) {
//         case PICK_RESULT_YES:    printf("Save\n"); break;
//         case PICK_RESULT_NO:     printf("Don't Save\n"); break;
//         case PICK_RESULT_CANCEL: printf("Cancel\n"); break;
//         default: break;
//     }
// }
//
// PickMessageOptions opts = {0};
// opts.title = "Unsaved Changes";
// opts.message = "Do you want to save your changes?";
// opts.buttons = PICK_BUTTON_YES_NO_CANCEL;
// opts.style = PICK_STYLE_WARNING;
// pick_message(&opts, on_button, NULL);
// ```
//
// ### Platform-Specific Parent Window
//
// ```c
// #ifdef PICK_PLATFORM_MACOS
// // NSWindow* window = ...;
// opts.parent_handle = window;
// #endif
//
// pick_file(&opts, callback, NULL);
// ```
//
// ---
//
// ## License
//
// MIT

#ifndef PICK_H
#define PICK_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__) && defined(__MACH__)
  #define PICK_PLATFORM_MACOS
#elif defined(_WIN32) || defined(_WIN64)
  #define PICK_PLATFORM_WINDOWS
#elif defined(__linux__)
  #define PICK_PLATFORM_LINUX
#elif defined(__EMSCRIPTEN__)
  #define PICK_PLATFORM_EMSCRIPTEN
#else
  #error "Unsupported platform"
#endif

/// @brief File type filter for file dialogs
typedef struct PickFilter {
  const char *name;        ///< Display name (e.g., "Images")
  const char **extensions; ///< Array of extensions without dots (e.g., ["png", "jpg"])
  int extension_count;     ///< Number of extensions
} PickFilter;

/// @brief Button configuration for message boxes
typedef enum PickButtonType {
  PICK_BUTTON_OK,
  PICK_BUTTON_OK_CANCEL,
  PICK_BUTTON_YES_NO,
  PICK_BUTTON_YES_NO_CANCEL
} PickButtonType;

/// @brief Visual style for message boxes
typedef enum PickMessageStyle {
  PICK_STYLE_INFO,
  PICK_STYLE_WARNING,
  PICK_STYLE_ERROR,
  PICK_STYLE_QUESTION
} PickMessageStyle;

/// @brief Result from message box interaction
typedef enum PickButtonResult {
  PICK_RESULT_OK,
  PICK_RESULT_CANCEL,
  PICK_RESULT_YES,
  PICK_RESULT_NO,
  PICK_RESULT_CLOSED ///< Window closed without button click
} PickButtonResult;

/// @brief Icon types for message boxes
typedef enum PickIconType {
  PICK_ICON_DEFAULT,
  PICK_ICON_CUSTOM,
  PICK_ICON_APP,
  PICK_ICON_TRASH,
  PICK_ICON_FOLDER,
  PICK_ICON_DOCUMENT,
  PICK_ICON_LOCKED,
  PICK_ICON_UNLOCKED,
  PICK_ICON_NETWORK,
  PICK_ICON_USER,
  PICK_ICON_CAUTION,
  PICK_ICON_ERROR,
  PICK_ICON_STOP,
  PICK_ICON_INVALID
} PickIconType;

/// @brief Configuration for file picker dialogs
typedef struct PickFileOptions {
  const char *title;        ///< Dialog title/message
  const char *default_path; ///< Starting directory or default file path
  const char *default_name; ///< Default filename (save dialogs)
  PickFilter *filters;      ///< Array of file filters
  int filter_count;         ///< Number of filters
  bool can_create_dirs;     ///< Allow creating directories (save dialogs)
  bool allow_multiple;      ///< Allow selecting multiple items
  const void *parent_handle;///< Platform-specific parent window handle (optional)
} PickFileOptions;

/// @brief Configuration for message boxes and sheets
typedef struct PickMessageOptions {
  const char *title;       ///< Title bar text (windows) or bold heading (sheets)
  const char *message;     ///< Main message text
  const char *detail;      ///< Additional detail text (optional)
  PickButtonType buttons;  ///< Which buttons to show
  PickMessageStyle style;  ///< Icon/style to use
  PickIconType icon_type;  ///< Icon type to use
  const char *icon_path;   ///< Path to custom icon (if icon_type == PICK_ICON_CUSTOM)
  const void *parent_handle;///< If set, shows as sheet/modal dialog on parent
} PickMessageOptions;

/// @brief Callback for single file selection
/// @param path Selected file path or NULL if cancelled
/// @param user_data User-provided context
typedef void (*PickFileCallback)(const char *path, void *user_data);

/// @brief Callback for multiple file selection
/// @param paths Array of selected file paths or NULL if cancelled
/// @param count Number of selected files
/// @param user_data User-provided context
typedef void (*PickMultiFileCallback)(const char **paths, int count,
                                      void *user_data);

/// @brief Callback for message box response
/// @param result Button that was clicked
/// @param user_data User-provided context
typedef void (*PickMessageCallback)(PickButtonResult result, void *user_data);

/// @brief Shows file open dialog for single file selection 
/// @param options Dialog configuration (can be NULL for defaults)
/// @param callback Function called with selected file
/// @param user_data Context passed to callback
void pick_file(const PickFileOptions *options, PickFileCallback callback,
               void *user_data);

/// @brief Shows file open dialog for multiple file selection 
/// @param options Dialog configuration (can be NULL for defaults)
/// @param callback Function called with selected files
/// @param user_data Context passed to callback
void pick_files(const PickFileOptions *options, PickMultiFileCallback callback,
                void *user_data);

/// @brief Shows folder selection dialog 
/// @param options Dialog configuration (can be NULL for defaults)
/// @param callback Function called with selected folder
/// @param user_data Context passed to callback
void pick_folder(const PickFileOptions *options, PickFileCallback callback,
                 void *user_data);

/// @brief Shows folder selection dialog for multiple folders 
/// @param options Dialog configuration (can be NULL for defaults)
/// @param callback Function called with selected folders
/// @param user_data Context passed to callback
void pick_folders(const PickFileOptions *options,
                  PickMultiFileCallback callback, void *user_data);

/// @brief Shows file save dialog 
/// @param options Dialog configuration (can be NULL for defaults)
/// @param callback Function called with save path
/// @param user_data Context passed to callback
void pick_save(const PickFileOptions *options, PickFileCallback callback,
               void *user_data);

/// @brief Shows message box or sheet 
/// @param options Message box configuration
/// @param callback Function called with user response (can be NULL)
/// @param user_data Context passed to callback
void pick_message(const PickMessageOptions *options,
                  PickMessageCallback callback, void *user_data);

/// @brief Shows simple alert dialog with OK button
/// @param title Alert title
/// @param message Alert message
/// @param parent_handle Parent window (NULL for standalone)
void pick_alert(const char *title, const char *message, void *parent_handle);

/// @brief Shows confirmation dialog with OK/Cancel buttons
/// @param title Dialog title
/// @param message Dialog message
/// @param parent_handle Parent window (NULL for standalone)
/// @param callback Function called with user response
/// @param user_data Context passed to callback
void pick_confirm(const char *title, const char *message, void *parent_handle,
                  PickMessageCallback callback, void *user_data);

/// @brief Frees memory for a single path returned by the library
/// @param path Path to free
void pick_free(char *path);

/// @brief Frees memory for multiple paths returned by the library
/// @param paths Array of paths to free
/// @param count Number of paths
void pick_free_multiple(char **paths, int count);

#ifdef __cplusplus
}
#endif

#ifdef PICK_IMPLEMENTATION

void pick__file_impl(const PickFileOptions *options, PickFileCallback callback, void *user_data);
void pick__files_impl(const PickFileOptions *options, PickMultiFileCallback callback, void *user_data);
void pick__folder_impl(const PickFileOptions *options, PickFileCallback callback, void *user_data);
void pick__folders_impl(const PickFileOptions *options, PickMultiFileCallback callback, void *user_data);
void pick__save_impl(const PickFileOptions *options, PickFileCallback callback, void *user_data);
void pick__message_impl(const PickMessageOptions *options, PickMessageCallback callback, void *user_data);

void pick_file(const PickFileOptions *options, PickFileCallback callback, void *user_data) {
  pick__file_impl(options, callback, user_data);
}

void pick_files(const PickFileOptions *options, PickMultiFileCallback callback, void *user_data) {
  pick__files_impl(options, callback, user_data);
}

void pick_folder(const PickFileOptions *options, PickFileCallback callback, void *user_data) {
  pick__folder_impl(options, callback, user_data);
}

void pick_folders(const PickFileOptions *options, PickMultiFileCallback callback, void *user_data) {
  pick__folders_impl(options, callback, user_data);
}

void pick_save(const PickFileOptions *options, PickFileCallback callback, void *user_data) {
  pick__save_impl(options, callback, user_data);
}

void pick_message(const PickMessageOptions *options, PickMessageCallback callback, void *user_data) {
  pick__message_impl(options, callback, user_data);
}

void pick_alert(const char *title, const char *message, void *parent_handle) {
  PickMessageOptions opts = {
    .title = title,
    .message = message,
    .detail = NULL,
    .buttons = PICK_BUTTON_OK,
    .style = PICK_STYLE_INFO,
    .icon_type = PICK_ICON_DEFAULT,
    .icon_path = NULL,
    .parent_handle = parent_handle
  };
  pick_message(&opts, NULL, NULL);
}

void pick_confirm(const char *title, const char *message, void *parent_handle,
                  PickMessageCallback callback, void *user_data) {
  PickMessageOptions opts = {
    .title = title,
    .message = message,
    .detail = NULL,
    .buttons = PICK_BUTTON_OK_CANCEL,
    .style = PICK_STYLE_QUESTION,
    .icon_type = PICK_ICON_DEFAULT,
    .icon_path = NULL,
    .parent_handle = parent_handle
  };
  pick_message(&opts, callback, user_data);
}

void pick_free(char *path) { 
  free(path); 
}

void pick_free_multiple(char **paths, int count) {
  if (paths) {
    for (int i = 0; i < count; i++) {
      free(paths[i]);
    }
    free(paths);
  }
}

#ifdef PICK_PLATFORM_MACOS

#include <objc/message.h>

#ifndef nil
#define nil ((id)0)
#endif
#ifndef YES
#define YES ((BOOL)1)
#endif
#ifndef NO
#define NO ((BOOL)0)
#endif

typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef double CGFloat;

enum {
  NSAlertStyleWarning = 0,
  NSAlertStyleInformational = 1,
  NSAlertStyleCritical = 2
};

enum {
  NSAlertFirstButtonReturn = 1000,
  NSAlertSecondButtonReturn = 1001,
  NSAlertThirdButtonReturn = 1002
};

enum { NSModalResponseCancel = 0, NSModalResponseOK = 1 };

enum { NSApplicationActivationPolicyRegular = 0 };

static id pick__objc_string(const char *str) {
  if (!str)
    return nil;
  return ((id (*)(id, SEL, const char *))objc_msgSend)(
      (id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"),
      str);
}

static id pick__objc_url_from_path(const char *path, bool is_directory) {
  if (!path)
    return nil;
  id str = pick__objc_string(path);
  if (!str)
    return nil;
  return ((id (*)(id, SEL, id, BOOL))objc_msgSend)(
      (id)objc_getClass("NSURL"),
      sel_registerName("fileURLWithPath:isDirectory:"), str,
      is_directory ? YES : NO);
}

static char *pick__objc_path_from_url(id url) {
  if (!url)
    return NULL;
  id path = ((id (*)(id, SEL))objc_msgSend)(url, sel_registerName("path"));
  if (!path)
    return NULL;
  const char *utf8 = ((const char *(*)(id, SEL))objc_msgSend)(
      path, sel_registerName("UTF8String"));
  if (!utf8)
    return NULL;

  size_t len = strlen(utf8);
  char *result = (char *)malloc(len + 1);
  if (result) {
    memcpy(result, utf8, len + 1);
  }
  return result;
}

static id pick__objc_app_instance(void) {
  return ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSApplication"),
                                         sel_registerName("sharedApplication"));
}

static void pick__objc_ensure_app_initialized(void) {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;

  id app = pick__objc_app_instance();

  ((void (*)(id, SEL, NSInteger))objc_msgSend)(
      app, sel_registerName("setActivationPolicy:"),
      NSApplicationActivationPolicyRegular);

  ((void (*)(id, SEL))objc_msgSend)(app, sel_registerName("finishLaunching"));

  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      app, sel_registerName("activateIgnoringOtherApps:"), YES);
}

static void pick__objc_run_on_main(void (^block)(void)) {
  BOOL is_main = ((BOOL (*)(id, SEL))objc_msgSend)(
      (id)objc_getClass("NSThread"), 
      sel_registerName("isMainThread"));
  
  if (is_main) {
    block();
  } else {
    dispatch_async(dispatch_get_main_queue(), block);
  }
}

static id pick__objc_window_from_handle(const void *handle) {
  if (!handle)
    return nil;

  if ([(id)handle isKindOfClass:[objc_getClass("NSWindow") class]]) {
    return (id)handle;
  }

  if ([(id)handle respondsToSelector:sel_registerName("window")]) {
    return ((id (*)(id, SEL))objc_msgSend)((id)handle,
                                           sel_registerName("window"));
  }

  return nil;
}

static id pick__objc_create_file_extensions_array(const PickFilter *filters,
                                                 int filter_count) {
  if (!filters || filter_count <= 0)
    return nil;

  id array = ((id (*)(id, SEL))objc_msgSend)(
      (id)objc_getClass("NSMutableArray"), sel_registerName("array"));

  for (int i = 0; i < filter_count; i++) {
    for (int j = 0; j < filters[i].extension_count; j++) {
      id ext = pick__objc_string(filters[i].extensions[j]);
      if (ext) {
        ((void (*)(id, SEL, id))objc_msgSend)(
            array, sel_registerName("addObject:"), ext);
      }
    }
  }

  return array;
}

static id pick__objc_create_open_panel(const PickFileOptions *options,
                                      bool allow_dirs, bool allow_files) {
  id panel = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSOpenPanel"),
                                             sel_registerName("openPanel"));

  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      panel, sel_registerName("setCanChooseDirectories:"),
      allow_dirs ? YES : NO);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      panel, sel_registerName("setCanChooseFiles:"), allow_files ? YES : NO);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      panel, sel_registerName("setAllowsMultipleSelection:"),
      (options && options->allow_multiple) ? YES : NO);

  if (options) {
    if (options->title) {
      ((void (*)(id, SEL, id))objc_msgSend)(panel,
                                            sel_registerName("setMessage:"),
                                            pick__objc_string(options->title));
    }

    if (options->default_path) {
      id url = pick__objc_url_from_path(options->default_path, YES);
      if (url) {
        ((void (*)(id, SEL, id))objc_msgSend)(
            panel, sel_registerName("setDirectoryURL:"), url);
      }
    }

    if (allow_files && options->filters && options->filter_count > 0) {
      id extensions = pick__objc_create_file_extensions_array(
          options->filters, options->filter_count);
      if (extensions) {
        ((void (*)(id, SEL, id))objc_msgSend)(
            panel, sel_registerName("setAllowedFileTypes:"), extensions);
      }
    }
  }

  return panel;
}

static id pick__objc_create_save_panel(const PickFileOptions *options) {
  id panel = ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSSavePanel"),
                                             sel_registerName("savePanel"));

  if (options) {
    if (options->title) {
      ((void (*)(id, SEL, id))objc_msgSend)(panel,
                                            sel_registerName("setMessage:"),
                                            pick__objc_string(options->title));
    }

    if (options->default_path) {
      id url = pick__objc_url_from_path(options->default_path, YES);
      if (url) {
        ((void (*)(id, SEL, id))objc_msgSend)(
            panel, sel_registerName("setDirectoryURL:"), url);
      }
    }

    if (options->default_name) {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("setNameFieldStringValue:"),
          pick__objc_string(options->default_name));
    }

    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        panel, sel_registerName("setCanCreateDirectories:"),
        options->can_create_dirs ? YES : NO);

    if (options->filters && options->filter_count > 0) {
      id extensions = pick__objc_create_file_extensions_array(
          options->filters, options->filter_count);
      if (extensions) {
        ((void (*)(id, SEL, id))objc_msgSend)(
            panel, sel_registerName("setAllowedFileTypes:"), extensions);
      }
    }
  }

  return panel;
}

typedef struct {
  PickMessageCallback callback;
  void *user_data;
  PickMessageOptions options;
} pick__message_context;

static NSInteger pick__objc_alert_style(PickMessageStyle style) {
  switch (style) {
  case PICK_STYLE_ERROR:
    return NSAlertStyleCritical;
  case PICK_STYLE_WARNING:
    return NSAlertStyleWarning;
  default:
    return NSAlertStyleInformational;
  }
}

static PickButtonResult pick__objc_button_result(NSInteger response,
                                                PickButtonType buttons) {

  if (response == NSAlertFirstButtonReturn) {
    switch (buttons) {
    case PICK_BUTTON_OK:
      return PICK_RESULT_OK;
    case PICK_BUTTON_OK_CANCEL:
      return PICK_RESULT_OK;
    case PICK_BUTTON_YES_NO:
      return PICK_RESULT_YES;
    case PICK_BUTTON_YES_NO_CANCEL:
      return PICK_RESULT_YES;
    }
  } else if (response == NSAlertSecondButtonReturn) {
    switch (buttons) {
    case PICK_BUTTON_OK:
      return PICK_RESULT_CLOSED;
    case PICK_BUTTON_OK_CANCEL:
      return PICK_RESULT_CANCEL;
    case PICK_BUTTON_YES_NO:
      return PICK_RESULT_NO;
    case PICK_BUTTON_YES_NO_CANCEL:
      return PICK_RESULT_NO;
    }
  } else if (response == NSAlertThirdButtonReturn) {
    if (buttons == PICK_BUTTON_YES_NO_CANCEL) {
      return PICK_RESULT_CANCEL;
    }
    return PICK_RESULT_CLOSED;
  }
  return PICK_RESULT_CLOSED;
}

static void pick__objc_set_alert_icon(id alert, PickIconType icon_type,
                                     const char *icon_path) {
  id icon = nil;

  switch (icon_type) {
  case PICK_ICON_DEFAULT:
    return;

  case PICK_ICON_CUSTOM:
    if (icon_path) {
      id path = pick__objc_string(icon_path);
      icon = ((id (*)(id, SEL, id))objc_msgSend)(
          ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSImage"),
                                          sel_registerName("alloc")),
          sel_registerName("initWithContentsOfFile:"), path);
    }
    break;

  case PICK_ICON_APP:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSApplicationIcon"));
    break;

  case PICK_ICON_TRASH:
    {
      id workspace =
          ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSWorkspace"),
                                          sel_registerName("sharedWorkspace"));
      icon = ((id (*)(id, SEL, id))objc_msgSend)(
          workspace, sel_registerName("iconForFile:"),
          pick__objc_string("~/.Trash"));
    }
    break;

  case PICK_ICON_FOLDER:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSFolder"));
    break;

  case PICK_ICON_DOCUMENT:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSMultipleDocuments"));
    break;

  case PICK_ICON_LOCKED:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSLockLockedTemplate"));
    break;

  case PICK_ICON_UNLOCKED:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSLockUnlockedTemplate"));
    break;

  case PICK_ICON_NETWORK:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSNetwork"));
    break;

  case PICK_ICON_USER:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSUser"));
    break;

  case PICK_ICON_CAUTION:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSCaution"));
    break;
    
  case PICK_ICON_ERROR:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSCriticalAlertIcon"));
    break;

  case PICK_ICON_STOP:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSStopProgressTemplate"));
    break;

  case PICK_ICON_INVALID:
    icon = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)objc_getClass("NSImage"), sel_registerName("imageNamed:"),
        pick__objc_string("NSInvalidDataFreestandingTemplate"));
    break;
  }

  if (icon) {
    ((void (*)(id, SEL, id))objc_msgSend)(alert, sel_registerName("setIcon:"),
                                          icon);
  }
}

static id pick__objc_create_alert(const PickMessageOptions *options) {
  id alert = ((id (*)(id, SEL))objc_msgSend)(
      ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSAlert"),
                                      sel_registerName("alloc")),
      sel_registerName("init"));

  if (options->title) {
    ((void (*)(id, SEL, id))objc_msgSend)(alert,
                                          sel_registerName("setMessageText:"),
                                          pick__objc_string(options->title));
  }

  if (options->message) {
    ((void (*)(id, SEL, id))objc_msgSend)(
        alert, sel_registerName("setInformativeText:"),
        pick__objc_string(options->message));
  }

  ((void (*)(id, SEL, NSInteger))objc_msgSend)(
      alert, sel_registerName("setAlertStyle:"),
      pick__objc_alert_style(options->style));

  if (options->icon_type != PICK_ICON_DEFAULT) {
    pick__objc_set_alert_icon(alert, options->icon_type, options->icon_path);
  }

  switch (options->buttons) {
  case PICK_BUTTON_OK:
    ((id (*)(id, SEL, id))objc_msgSend)(
        alert, sel_registerName("addButtonWithTitle:"), pick__objc_string("OK"));
    break;
  case PICK_BUTTON_OK_CANCEL:
    ((id (*)(id, SEL, id))objc_msgSend)(
        alert, sel_registerName("addButtonWithTitle:"), pick__objc_string("OK"));
    ((id (*)(id, SEL, id))objc_msgSend)(alert,
                                        sel_registerName("addButtonWithTitle:"),
                                        pick__objc_string("Cancel"));
    break;
  case PICK_BUTTON_YES_NO:
    ((id (*)(id, SEL, id))objc_msgSend)(alert,
                                        sel_registerName("addButtonWithTitle:"),
                                        pick__objc_string("Yes"));
    ((id (*)(id, SEL, id))objc_msgSend)(
        alert, sel_registerName("addButtonWithTitle:"), pick__objc_string("No"));
    break;
  case PICK_BUTTON_YES_NO_CANCEL:
    ((id (*)(id, SEL, id))objc_msgSend)(alert,
                                        sel_registerName("addButtonWithTitle:"),
                                        pick__objc_string("Yes"));
    ((id (*)(id, SEL, id))objc_msgSend)(
        alert, sel_registerName("addButtonWithTitle:"), pick__objc_string("No"));
    ((id (*)(id, SEL, id))objc_msgSend)(alert,
                                        sel_registerName("addButtonWithTitle:"),
                                        pick__objc_string("Cancel"));
    break;
  }

  return alert;
}

typedef struct {
  PickFileCallback single_callback;
  PickMultiFileCallback multi_callback;
  void *user_data;
} pick__file_context;

void pick__file_impl(const PickFileOptions *options, PickFileCallback callback,
               void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();

    id panel = pick__objc_create_open_panel(options, false, true);
    id parent_window =
        pick__objc_window_from_handle(options ? options->parent_handle : NULL);

    pick__file_context *ctx =
        (pick__file_context *)malloc(sizeof(pick__file_context));
    ctx->single_callback = callback;
    ctx->multi_callback = NULL;
    ctx->user_data = user_data;

    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      char *path = NULL;
      if (response == NSModalResponseOK) {
        id url =
            ((id (*)(id, SEL))objc_msgSend)(panel, sel_registerName("URL"));
        path = pick__objc_path_from_url(url);
      }
      if (ctx->single_callback) {
        ctx->single_callback(path, ctx->user_data);
      }
      free(path);
      free(ctx);
    };

    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          panel,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("beginWithCompletionHandler:"),
          (id)completion_handler);
    }
  });
}

void pick__files_impl(const PickFileOptions *options, PickMultiFileCallback callback,
                void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();

    PickFileOptions opts = options ? *options : (PickFileOptions){0};
    opts.allow_multiple = true;

    id panel = pick__objc_create_open_panel(&opts, false, true);
    id parent_window =
        pick__objc_window_from_handle(options ? options->parent_handle : NULL);

    pick__file_context *ctx =
        (pick__file_context *)malloc(sizeof(pick__file_context));
    ctx->single_callback = NULL;
    ctx->multi_callback = callback;
    ctx->user_data = user_data;

    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      char **paths = NULL;
      int count = 0;

      if (response == NSModalResponseOK) {
        id urls =
            ((id (*)(id, SEL))objc_msgSend)(panel, sel_registerName("URLs"));
        NSUInteger url_count = ((NSUInteger (*)(id, SEL))objc_msgSend)(
            urls, sel_registerName("count"));

        if (url_count > 0) {
          paths = (char **)calloc(url_count, sizeof(char *));
          if (paths) {
            for (NSUInteger i = 0; i < url_count; i++) {
              id url = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
                  urls, sel_registerName("objectAtIndex:"), i);
              char *path = pick__objc_path_from_url(url);
              if (path) {
                paths[count++] = path;
              }
            }
          }
        }
      }

      if (ctx->multi_callback) {
        ctx->multi_callback((const char **)paths, count, ctx->user_data);
      }

      pick_free_multiple(paths, count);
      free(ctx);
    };

    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          panel,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("beginWithCompletionHandler:"),
          (id)completion_handler);
    }
  });
}

void pick__folder_impl(const PickFileOptions *options, PickFileCallback callback,
                 void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();

    id panel = pick__objc_create_open_panel(options, true, false);
    id parent_window =
        pick__objc_window_from_handle(options ? options->parent_handle : NULL);

    pick__file_context *ctx =
        (pick__file_context *)malloc(sizeof(pick__file_context));
    ctx->single_callback = callback;
    ctx->multi_callback = NULL;
    ctx->user_data = user_data;

    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      char *path = NULL;
      if (response == NSModalResponseOK) {
        id url =
            ((id (*)(id, SEL))objc_msgSend)(panel, sel_registerName("URL"));
        path = pick__objc_path_from_url(url);
      }
      if (ctx->single_callback) {
        ctx->single_callback(path, ctx->user_data);
      }
      free(path);
      free(ctx);
    };

    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          panel,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("beginWithCompletionHandler:"),
          (id)completion_handler);
    }
  });
}

void pick__folders_impl(const PickFileOptions *options,
                  PickMultiFileCallback callback, void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();

    PickFileOptions opts = options ? *options : (PickFileOptions){0};
    opts.allow_multiple = true;

    id panel = pick__objc_create_open_panel(&opts, true, false);
    id parent_window =
        pick__objc_window_from_handle(options ? options->parent_handle : NULL);

    pick__file_context *ctx =
        (pick__file_context *)malloc(sizeof(pick__file_context));
    ctx->single_callback = NULL;
    ctx->multi_callback = callback;
    ctx->user_data = user_data;

    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      char **paths = NULL;
      int count = 0;

      if (response == NSModalResponseOK) {
        id urls =
            ((id (*)(id, SEL))objc_msgSend)(panel, sel_registerName("URLs"));
        NSUInteger url_count = ((NSUInteger (*)(id, SEL))objc_msgSend)(
            urls, sel_registerName("count"));

        if (url_count > 0) {
          paths = (char **)calloc(url_count, sizeof(char *));
          if (paths) {
            for (NSUInteger i = 0; i < url_count; i++) {
              id url = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
                  urls, sel_registerName("objectAtIndex:"), i);
              char *path = pick__objc_path_from_url(url);
              if (path) {
                paths[count++] = path;
              }
            }
          }
        }
      }

      if (ctx->multi_callback) {
        ctx->multi_callback((const char **)paths, count, ctx->user_data);
      }

      pick_free_multiple(paths, count);
      free(ctx);
    };

    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          panel,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("beginWithCompletionHandler:"),
          (id)completion_handler);
    }
  });
}

void pick__save_impl(const PickFileOptions *options, PickFileCallback callback,
               void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();

    id panel = pick__objc_create_save_panel(options);
    id parent_window =
        pick__objc_window_from_handle(options ? options->parent_handle : NULL);

    pick__file_context *ctx =
        (pick__file_context *)malloc(sizeof(pick__file_context));
    ctx->single_callback = callback;
    ctx->multi_callback = NULL;
    ctx->user_data = user_data;

    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      char *path = NULL;
      if (response == NSModalResponseOK) {
        id url =
            ((id (*)(id, SEL))objc_msgSend)(panel, sel_registerName("URL"));
        path = pick__objc_path_from_url(url);
      }
      if (ctx->single_callback) {
        ctx->single_callback(path, ctx->user_data);
      }
      free(path);
      free(ctx);
    };

    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          panel,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      ((void (*)(id, SEL, id))objc_msgSend)(
          panel, sel_registerName("beginWithCompletionHandler:"),
          (id)completion_handler);
    }
  });
}

void pick__message_impl(const PickMessageOptions *options,
                  PickMessageCallback callback, void *user_data) {
  pick__objc_run_on_main(^{
    pick__objc_ensure_app_initialized();
    pick__message_context *ctx =
        (pick__message_context *)malloc(sizeof(pick__message_context));
    ctx->callback = callback;
    ctx->user_data = user_data;
    if (options) {
      ctx->options = *options;
    } else {
      memset(&ctx->options, 0, sizeof(PickMessageOptions));
      ctx->options.buttons = PICK_BUTTON_OK;
      ctx->options.style = PICK_STYLE_INFO;
    }
    id alert = pick__objc_create_alert(&ctx->options);
    id parent_window = pick__objc_window_from_handle(ctx->options.parent_handle);
    void (^completion_handler)(NSInteger) = ^(NSInteger response) {
      PickButtonResult result =
          pick__objc_button_result(response, ctx->options.buttons);
      if (ctx->callback) {
        ctx->callback(result, ctx->user_data);
      }
      free(ctx);
    };
    if (parent_window) {
      ((void (*)(id, SEL, id, id))objc_msgSend)(
          alert,
          sel_registerName("beginSheetModalForWindow:completionHandler:"),
          parent_window, (id)completion_handler);
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        NSInteger response = ((NSInteger (*)(id, SEL))objc_msgSend)(
            alert, sel_registerName("runModal"));
        completion_handler(response);
      });
    }
  });
}

#endif

#ifdef PICK_PLATFORM_WINDOWS
#error "Windows implementation not yet available"
#endif

#ifdef PICK_PLATFORM_LINUX
#error "Linux implementation not yet available"
#endif

#ifdef PICK_PLATFORM_EMSCRIPTEN

#include <emscripten/emscripten.h>

#ifndef PICK_EM_MAX_REQUESTS
#define PICK_EM_MAX_REQUESTS 64
#endif

#ifndef PICK_EM_BASE_PICKED
#define PICK_EM_BASE_PICKED "/picked"
#endif

#ifndef PICK_EM_BASE_SAVED
#define PICK_EM_BASE_SAVED "/saved"
#endif

typedef enum {
  PICK_REQ_NONE = 0,
  PICK_REQ_OPEN_SINGLE,
  PICK_REQ_OPEN_MULTI,
  PICK_REQ_OPEN_DIR_SINGLE,
  PICK_REQ_OPEN_DIR_MULTI,
  PICK_REQ_SAVE,
  PICK_REQ_MESSAGE,
  PICK_REQ_EXPORT
} pick__req_kind_t;

typedef void (*PickResultCallback)(bool ok, void* user_data);

typedef struct {
  pick__req_kind_t       kind;
  PickFileCallback      single_cb;
  PickMultiFileCallback multi_cb;
  PickMessageCallback   msg_cb;
  PickResultCallback    result_cb;
  void*                 user;
  PickButtonType        button_type;
} pick__em_req_t;

static pick__em_req_t pick__g_reqs[PICK_EM_MAX_REQUESTS];
static int pick__g_next_req_id = 1;

static int pick__alloc_req(void) {
  for (int tries = 0; tries < PICK_EM_MAX_REQUESTS; tries++) {
    int id = pick__g_next_req_id++;
    if (pick__g_next_req_id <= 0) pick__g_next_req_id = 1;
    if (id <= 0 || id >= PICK_EM_MAX_REQUESTS) { id = 1; pick__g_next_req_id = 2; }
    if (pick__g_reqs[id].kind == PICK_REQ_NONE) return id;
  }
  return 0;
}
static void pick__clear_req(int id) { if (id > 0 && id < PICK_EM_MAX_REQUESTS) pick__g_reqs[id] = (pick__em_req_t){0}; }

static void pick__build_accept_string(const PickFileOptions* opts, char* out, size_t cap) {
  if (!out || cap == 0) return;
  out[0] = 0;
  if (!opts || !opts->filters || opts->filter_count <= 0) return;

  size_t used = 0;
  for (int i = 0; i < opts->filter_count; i++) {
    const PickFilter* f = &opts->filters[i];
    for (int j = 0; j < f->extension_count; j++) {
      const char* ext = f->extensions[j];
      if (!ext || !*ext) continue;
      char piece[128];
      int n = snprintf(piece, sizeof(piece), ".%s", ext);
      if (n <= 0) continue;
      if (used > 0 && used + 1 < cap) { out[used++] = ','; out[used] = 0; }
      if (used + (size_t)n + 1 >= cap) return;
      memcpy(out + used, piece, (size_t)n);
      used += (size_t)n;
      out[used] = 0;
    }
  }
}

static const char* pick__icon_token(PickIconType t) {
  switch (t) {
    case PICK_ICON_DEFAULT:   return "default";
    case PICK_ICON_CUSTOM:    return "custom";
    case PICK_ICON_APP:       return "app";
    case PICK_ICON_TRASH:     return "trash";
    case PICK_ICON_FOLDER:    return "folder";
    case PICK_ICON_DOCUMENT:  return "document";
    case PICK_ICON_LOCKED:    return "locked";
    case PICK_ICON_UNLOCKED:  return "unlocked";
    case PICK_ICON_NETWORK:   return "network";
    case PICK_ICON_USER:      return "user";
    case PICK_ICON_CAUTION:   return "caution";
    case PICK_ICON_ERROR:     return "error";
    case PICK_ICON_STOP:      return "stop";
    case PICK_ICON_INVALID:   return "invalid";
    default:                  return "default";
  }
}

EM_JS(void, pick__js_init_buckets, (), {
  if (typeof FS === "undefined") return;
  try { if (!FS.analyzePath("/picked").exists) FS.mkdir("/picked"); } catch (e) { console.error("pick: /picked mkdir", e); }
  try { if (!FS.analyzePath("/saved").exists)  FS.mkdir("/saved");  } catch (e) { console.error("pick: /saved mkdir", e); }
});

EM_JS(void, pick__call_deliver_single, (int id, const char* c_path), {
  var c = (Module && Module.ccall) ? Module.ccall : (typeof ccall !== "undefined" ? ccall : null);
  if (!c) { console.error("pick: ccall missing"); return; }
  function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : null) : (x || null); }
  c("pick__deliver_single","void",["number","string"],[id, S(c_path)]);
});
EM_JS(void, pick__call_deliver_multi_lines, (int id, const char* c_joined), {
  var c = (Module && Module.ccall) ? Module.ccall : (typeof ccall !== "undefined" ? ccall : null);
  if (!c) { console.error("pick: ccall missing"); return; }
  function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
  c("pick__deliver_multi_lines","void",["number","string"],[id, S(c_joined)]);
});
EM_JS(void, pick__call_deliver_msg, (int id, int button_idx), {
  var c = (Module && Module.ccall) ? Module.ccall : (typeof ccall !== "undefined" ? ccall : null);
  if (!c) { console.error("pick: ccall missing"); return; }
  c("pick__deliver_msg","void",["number","number"],[id, button_idx]);
});

EM_JS(void, pick__js_create_dialog, (int req_id, const char* role_label_c, const char* title_c,
                                    const char* message_c, const char* kind_c,
                                    int with_icon, const char* icon_token_c, const char* custom_url_c),
{
  try {
    function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
    var roleLabel = S(role_label_c);
    var title = S(title_c);
    var msg   = S(message_c);
    var kind  = S(kind_c);
    var iconToken = S(icon_token_c) || "default";
    var customURL = S(custom_url_c);

    var overlay = document.createElement("div");
    overlay.setAttribute("data-pick", "overlay");
    overlay.setAttribute("data-pick-kind", kind);
    overlay.setAttribute("data-req-id", String(req_id));
    overlay.tabIndex = -1;

    var dialog = document.createElement("div");
    dialog.setAttribute("data-pick", "dialog");
    dialog.setAttribute("role", "dialog");
    dialog.setAttribute("aria-modal", "true");
    var titleId = "pick-title-" + Date.now() + "-" + Math.random().toString().slice(2);
    var msgId   = "pick-msg-"   + Date.now() + "-" + Math.random().toString().slice(2);
    dialog.setAttribute("aria-labelledby", titleId);
    dialog.setAttribute("aria-describedby", msgId);
    if (roleLabel) dialog.setAttribute("aria-label", roleLabel);

    var head = document.createElement("div");
    head.setAttribute("data-pick", "header");

    if (with_icon) {
      var icoWrap = document.createElement("div");
      icoWrap.setAttribute("data-pick", "icon");
      icoWrap.setAttribute("data-icon", iconToken);
      if (customURL) {
        var img = document.createElement("img");
        img.setAttribute("alt", "");
        img.setAttribute("data-pick", "icon-image");
        img.src = customURL;
        icoWrap.appendChild(img);
      }
      head.appendChild(icoWrap);
    }

    var h = document.createElement("div");
    h.id = titleId;
    h.setAttribute("data-pick", "title");
    h.textContent = title || "";
    head.appendChild(h);

    var body = document.createElement("div");
    body.id = msgId;
    body.setAttribute("data-pick", "message");
    body.textContent = msg || "";

    var actions = document.createElement("div");
    actions.setAttribute("data-pick", "actions");

    dialog.appendChild(head);
    dialog.appendChild(body);
    dialog.appendChild(actions);
    overlay.appendChild(dialog);
    document.body.appendChild(overlay);
  } catch (e) { console.error("pick__js_create_dialog failed", e); }
});

EM_JS(void, pick__js_append_action, (const char* label_c, const char* action_c), {
  try {
    function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
    var label  = S(label_c);
    var action = S(action_c);
    var overlay = document.querySelector('[data-pick="overlay"]:last-of-type');
    if (!overlay) throw new Error("no overlay");
    var actions = overlay.querySelector('[data-pick="actions"]');
    var btn = document.createElement("button");
    btn.setAttribute("type", "button");
    btn.setAttribute("data-pick", "button");
    btn.setAttribute("data-action", action);
    btn.textContent = label;
    actions.appendChild(btn);
  } catch (e) { console.error("pick__js_append_action failed", e); }
});

EM_JS(void, pick__js_bind_message_handlers, (int req_id, int button_count), {
  try {
    var overlay = document.querySelector('[data-pick="overlay"]:last-of-type');
    if (!overlay) throw new Error("no overlay");
    var actions = overlay.querySelector('[data-pick="actions"]');
    var buttons = actions.querySelectorAll('[data-pick="button"]');
    
    for (var i = 0; i < buttons.length; i++) {
      (function(idx) {
        buttons[idx].addEventListener("click", function() {
          overlay.remove();
          pick__call_deliver_msg(req_id, idx);
        }, { once: true });
      })(i);
    }
    
    if (buttons.length > 0) {
      buttons[buttons.length - 1].focus();
    }
  } catch (e) { console.error("pick__js_bind_message_handlers failed", e); }
});

EM_JS(void, pick__js_import_files_to_memfs, (const char* base_c, int req_id, int is_multi), {
  (async function(){
    try {
      function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
      if (typeof FS === "undefined") { pick__call_deliver_single(req_id, 0); return; }
      var base = S(base_c) || "/picked";
      try { if (!FS.analyzePath(base).exists) FS.mkdir(base); } catch (e) {}

      var chosen = (Module.__pickChosen || []);
      if (!chosen.length) { pick__call_deliver_single(req_id, 0); return; }

      var out = [];
      for (var j = 0; j < chosen.length; j++) {
        var f = chosen[j].file;
        var rel = chosen[j].rel;
        var parts = rel.split("/").filter(Boolean);
        var dir = base;
        for (var k = 0; k < Math.max(0, parts.length - 1); k++) {
          dir = dir + "/" + parts[k];
          try { if (!FS.analyzePath(dir).exists) FS.mkdir(dir); } catch (e) {}
        }
        var full = base + "/" + rel;
        var ab = await f.arrayBuffer();
        FS.writeFile(full, new Uint8Array(ab));
        out.push(full);
      }

      if (is_multi) {
        pick__call_deliver_multi_lines(req_id, out.join("\n"));
      } else {
        pick__call_deliver_single(req_id, out.length ? out[0] : 0);
      }
    } catch (e) {
      console.error("pick__js_import_files_to_memfs failed", e);
      pick__call_deliver_single(req_id, 0);
    } finally {
      Module.__pickChosen = [];
    }
  })();
});

EM_JS(void, pick__js_open, (int req_id, const char* title_c,
                           int allow_dirs, int allow_files, int allow_multiple,
                           const char* accept_c,
                           int with_icon, const char* icon_token_c, const char* custom_url_c),
{
  (async function() {
    try {
      function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
      var title  = S(title_c);
      var accept = S(accept_c);
      var icon   = S(icon_token_c) || (allow_dirs ? "folder" : "document");
      var custom = S(custom_url_c);

      var role = allow_dirs ? "Choose Folder" : (allow_multiple ? "Choose Files" : "Choose File");
      var msg  = allow_dirs ? "Select a folder." : (allow_multiple ? "Select one or more files." : "Select a file.");
      pick__js_create_dialog(req_id, role, title, msg, "open", with_icon, icon, custom);

      var overlay = document.querySelector('[data-pick="overlay"]:last-of-type');
      var dialog  = overlay.querySelector('[data-pick="dialog"]');

      var row  = document.createElement("div");
      row.setAttribute("data-pick", "row");

      var browse = document.createElement("button");
      browse.setAttribute("type", "button");
      browse.setAttribute("data-pick", "file-button");
      browse.textContent = allow_dirs ? "Browse Folders…" : "Browse…";

      var summary = document.createElement("div");
      summary.setAttribute("data-pick", "summary");
      summary.textContent = "No selection";

      row.appendChild(browse);
      row.appendChild(summary);

      var list = document.createElement("div");
      list.setAttribute("data-pick", "file-list");

      dialog.insertBefore(row, dialog.querySelector('[data-pick="actions"]'));
      dialog.insertBefore(list, dialog.querySelector('[data-pick="actions"]'));

      pick__js_append_action("Cancel", "cancel");
      pick__js_append_action("Import", "ok");

      var actions = dialog.querySelector('[data-pick="actions"]');
      var ok     = actions.querySelector('[data-action="ok"]');
      var cancel = actions.querySelector('[data-action="cancel"]');

      Module.__pickChosen = [];

      function renderList() {
        list.replaceChildren();
        var chosen = Module.__pickChosen || [];
        if (!chosen.length) { summary.textContent = "No selection"; return; }
        summary.textContent = String(chosen.length) + " selected";
        var ul = document.createElement("ul");
        ul.setAttribute("data-pick", "file-items");
        for (var i = 0; i < chosen.length; i++) {
          var li = document.createElement("li");
          li.setAttribute("data-pick", "file-item");
          li.textContent = chosen[i].rel;
          ul.appendChild(li);
        }
        list.appendChild(ul);
      }

      function extTypesFromAccept(str) {
        if (!str) return undefined;
        var exts = str.split(",").map(function(s){return s.trim();}).filter(Boolean);
        if (!exts.length) return undefined;
        return [{ description: "Allowed", accept: { "*/*": exts } }];
      }

      async function browseFSA() {
        try {
          if (allow_dirs) {
            const dir = await window.showDirectoryPicker({ mode: "read" });
            async function* walk(rootHandle, prefix) {
              for await (const [name, handle] of rootHandle.entries()) {
                const rel = prefix ? (prefix + "/" + name) : name;
                if (handle.kind === "file") {
                  const file = await handle.getFile();
                  file._rel = rel;
                  yield file;
                } else if (handle.kind === "directory") {
                  yield* walk(handle, rel);
                }
              }
            }
            for await (const f of walk(dir, "")) {
              Module.__pickChosen.push({ file: f, rel: f._rel || f.name });
            }
          } else {
            const picked = await window.showOpenFilePicker({
              multiple: !!allow_multiple,
              excludeAcceptAllOption: false,
              types: extTypesFromAccept(accept)
            });
            for (const h of picked) {
              const f = await h.getFile();
              Module.__pickChosen.push({ file: f, rel: f.name });
            }
          }
          renderList();
        } catch (err) {
          if (err && err.name === "AbortError") {
          } else {
            console.error("pick: FSA browse failed", err);
          }
        }
      }

      function browseInput() {
        var input = document.createElement("input");
        input.type = "file";
        input.hidden = true;
        if (allow_dirs && !allow_files) { input.setAttribute("webkitdirectory", ""); input.setAttribute("directory", ""); }
        if (!allow_dirs && allow_files) {
          if (allow_multiple) input.multiple = true;
          if (accept && accept.length) input.accept = accept;
        }
        document.body.appendChild(input);
        input.addEventListener("change", function(){
          var files = Array.from(input.files || []);
          for (var i = 0; i < files.length; i++) {
            var f = files[i];
            var rel = (f.webkitRelativePath && f.webkitRelativePath.length) ? f.webkitRelativePath : f.name;
            Module.__pickChosen.push({ file: f, rel: rel });
          }
          renderList();
          setTimeout(function(){ try{ input.remove(); }catch(_){} }, 0);
        }, { once: true });
        input.click();
      }

      var canFSA = (typeof window !== "undefined") &&
                   ((allow_dirs && !!window.showDirectoryPicker) ||
                    (!allow_dirs && !!window.showOpenFilePicker));

      browse.addEventListener("click", function(){
        if (canFSA) browseFSA(); else browseInput();
      });

      cancel.addEventListener("click", function(){
        Module.__pickChosen = [];
        overlay.remove();
        pick__call_deliver_single(req_id, 0);
      }, { once: true });

      ok.addEventListener("click", function(){
        overlay.remove();
        var is_multi = !!allow_multiple;
        pick__js_import_files_to_memfs("/picked", req_id, is_multi ? 1 : 0);
      }, { once: true });

      browse.focus();
    } catch (e) { console.error("pick__js_open failed", e); pick__call_deliver_single(req_id, 0); }
  })();
});

EM_JS(void, pick__js_save, (int req_id, const char* title_c, const char* suggested_c,
                           int with_icon, const char* icon_token_c, const char* custom_url_c),
{
  try {
    function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
    var title     = S(title_c);
    var suggested = S(suggested_c) || "untitled";
    var iconTok   = S(icon_token_c) || "document";
    var custom    = S(custom_url_c);

    pick__js_create_dialog(req_id, "Save As", title, "Choose a file name.", "save",
                          with_icon, iconTok, custom);

    var overlay = document.querySelector('[data-pick="overlay"]:last-of-type');
    var dialog  = overlay.querySelector('[data-pick="dialog"]');

    var row = document.createElement("div");
    row.setAttribute("data-pick", "row");

    var label = document.createElement("label");
    label.setAttribute("data-pick", "label");
    label.textContent = "File name";

    var input = document.createElement("input");
    input.setAttribute("data-pick", "input");
    input.setAttribute("type", "text");
    input.setAttribute("autocomplete", "off");
    input.value = suggested;

    label.appendChild(input);
    row.appendChild(label);
    dialog.insertBefore(row, dialog.querySelector('[data-pick="actions"]'));

    pick__js_append_action("Cancel", "cancel");
    pick__js_append_action("Save", "ok");

    var actions = dialog.querySelector('[data-pick="actions"]');
    var ok = actions.querySelector('[data-action="ok"]');
    var cancel = actions.querySelector('[data-action="cancel"]');

    function finalize(pathOrNull) {
      overlay.remove();
      pick__call_deliver_single(req_id, pathOrNull ? pathOrNull : 0);
    }

    ok.addEventListener("click", function () {
      var name = (input.value || "").trim();
      if (!name) name = "untitled";
      var base = "/saved";
      var full = base + "/" + name;

      if (typeof FS !== "undefined") {
        try { if (!FS.analyzePath(base).exists) FS.mkdir(base); } catch (e) {}
        try { if (!FS.analyzePath(full).exists) FS.writeFile(full, new Uint8Array()); } catch (e) {}
      }
      finalize(full);
    }, { once: true });

    cancel.addEventListener("click", function () { finalize(0); }, { once: true });

    input.focus(); input.select();
  } catch (e) { console.error("pick__js_save failed", e); pick__call_deliver_single(req_id, 0); }
});

EM_JS(void, pick__js_export, (int req_id, const char* src_c, const char* suggested_c), {
  (async function(){
    try {
      function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
      if (typeof FS === "undefined") { pick__call_deliver_msg(req_id, 1); return; }

      var src = S(src_c);
      var suggested = S(suggested_c);
      if (!suggested) {
        var slash = src.lastIndexOf("/");
        suggested = (slash >= 0) ? src.slice(slash+1) : "download.bin";
      }
      var data = FS.readFile(src, { encoding: "binary" });

      if (typeof window !== "undefined" && typeof window.showSaveFilePicker === "function") {
        try {
          var handle = await window.showSaveFilePicker({ suggestedName: suggested });
          var writable = await handle.createWritable();
          await writable.write(new Blob([data], { type: "application/octet-stream" }));
          await writable.close();
          pick__call_deliver_msg(req_id, 0);
        } catch (err) {
          if (err && err.name === "AbortError") {
            pick__call_deliver_msg(req_id, 1);
          } else {
            console.error("pick: export failed", err);
            pick__call_deliver_msg(req_id, 1);
          }
        }
      } else {
        var blob = new Blob([data], { type: "application/octet-stream" });
        var url  = URL.createObjectURL(blob);
        var a = document.createElement("a");
        a.href = url; a.download = suggested;
        document.body.appendChild(a); a.click();
        setTimeout(function(){ URL.revokeObjectURL(url); a.remove(); }, 0);
        pick__call_deliver_msg(req_id, 0);
      }
    } catch (e) {
      console.error("pick__js_export failed", e);
      pick__call_deliver_msg(req_id, 1);
    }
  })();
});

EM_JS(char*, pick__js_custom_icon_url, (const char* path_c), {
  try {
    function S(x){ return (typeof x === "number") ? (x ? UTF8ToString(x) : "") : (x || ""); }
    if (typeof FS === "undefined") return 0;
    var p = S(path_c);
    if (!p) return 0;
    if (!FS.analyzePath(p).exists) return 0;
    var data = FS.readFile(p, { encoding: "binary" });
    var blob = new Blob([data]);
    var url  = URL.createObjectURL(blob);
    var len  = lengthBytesUTF8(url) + 1;
    var mem  = _malloc(len);
    stringToUTF8(url, mem, len);
    return mem;
  } catch (e) { console.error("pick__js_custom_icon_url failed", e); return 0; }
});

#ifdef __cplusplus
extern "C" {
#endif

EMSCRIPTEN_KEEPALIVE
void pick__deliver_single(int id, const char* path) {
  if (id <= 0 || id >= PICK_EM_MAX_REQUESTS) return;
  pick__em_req_t req = pick__g_reqs[id];
  pick__clear_req(id);

  switch (req.kind) {
    case PICK_REQ_OPEN_SINGLE:
    case PICK_REQ_OPEN_DIR_SINGLE:
    case PICK_REQ_SAVE:
      if (req.single_cb) req.single_cb(path, req.user);
      break;
    case PICK_REQ_OPEN_MULTI:
    case PICK_REQ_OPEN_DIR_MULTI:
      if (req.multi_cb) req.multi_cb(NULL, 0, req.user);
      break;
    case PICK_REQ_MESSAGE:
      if (req.msg_cb) req.msg_cb(PICK_RESULT_OK, req.user);
      break;
    default: break;
  }
}

EMSCRIPTEN_KEEPALIVE
void pick__deliver_multi_lines(int id, const char* lines) {
  if (id <= 0 || id >= PICK_EM_MAX_REQUESTS) return;
  pick__em_req_t req = pick__g_reqs[id];
  pick__clear_req(id);

  if (!lines || !*lines) {
    if (req.multi_cb) req.multi_cb(NULL, 0, req.user);
    else if (req.single_cb) req.single_cb(NULL, req.user);
    return;
  }

  if ((req.kind == PICK_REQ_OPEN_DIR_SINGLE || req.kind == PICK_REQ_OPEN_SINGLE) && req.single_cb) {
    const char* nl = strchr(lines, '\n');
    size_t len = nl ? (size_t)(nl - lines) : strlen(lines);
    char* first = (char*)malloc(len + 1);
    if (first) { memcpy(first, lines, len); first[len] = 0; }
    req.single_cb(first, req.user);
    if (first) free(first);
    return;
  }

  if (!req.multi_cb) { if (req.single_cb) req.single_cb(NULL, req.user); return; }

  int count = 1; for (const char* p = lines; *p; p++) if (*p == '\n') count++;
  char** arr = (char**)calloc((size_t)count, sizeof(char*));
  if (!arr) { req.multi_cb(NULL, 0, req.user); return; }

  int idx = 0; const char* start = lines;
  for (const char* p = lines;; p++) {
    if (*p == '\n' || *p == '\0') {
      size_t L = (size_t)(p - start);
      char* s = (char*)malloc(L + 1);
      if (s) { memcpy(s, start, L); s[L] = 0; arr[idx++] = s; }
      if (*p == '\0') break; start = p + 1;
    }
  }
  req.multi_cb((const char**)arr, idx, req.user);
  for (int i = 0; i < idx; i++) free(arr[i]);
  free(arr);
}

EMSCRIPTEN_KEEPALIVE
void pick__deliver_msg(int id, int button_idx) {
  if (id <= 0 || id >= PICK_EM_MAX_REQUESTS) return;
  pick__em_req_t req = pick__g_reqs[id];
  pick__clear_req(id);

  if (req.kind == PICK_REQ_MESSAGE) {
    if (req.msg_cb) {
      PickButtonResult result = PICK_RESULT_CLOSED;
      
      switch (req.button_type) {
        case PICK_BUTTON_OK:
          result = (button_idx == 0) ? PICK_RESULT_OK : PICK_RESULT_CLOSED;
          break;
          
        case PICK_BUTTON_OK_CANCEL:
          if (button_idx == 0) result = PICK_RESULT_CANCEL;
          else if (button_idx == 1) result = PICK_RESULT_OK;
          break;
          
        case PICK_BUTTON_YES_NO:
          if (button_idx == 0) result = PICK_RESULT_NO;
          else if (button_idx == 1) result = PICK_RESULT_YES;
          break;
          
        case PICK_BUTTON_YES_NO_CANCEL:
          if (button_idx == 0) result = PICK_RESULT_CANCEL;
          else if (button_idx == 1) result = PICK_RESULT_NO;
          else if (button_idx == 2) result = PICK_RESULT_YES;
          break;
          
        default:
          result = PICK_RESULT_CLOSED;
          break;
      }
      
      req.msg_cb(result, req.user);
    }
    return;
  }
  if (req.kind == PICK_REQ_EXPORT) {
    if (req.result_cb) req.result_cb(button_idx == 0, req.user);
    return;
  }
}

#ifdef __cplusplus
}
#endif

static const char* pick__message_style_token(PickMessageStyle s) {
  switch (s) {
    case PICK_STYLE_WARNING: return "warning";
    case PICK_STYLE_ERROR:   return "error";
    case PICK_STYLE_QUESTION:return "question";
    case PICK_STYLE_INFO:
    default:                 return "info";
  }
}

void pick__file_impl(const PickFileOptions *options, PickFileCallback cb, void *ud) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (cb) cb(NULL, ud); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_OPEN_SINGLE, .single_cb = cb, .user = ud };

  char accept[512]; pick__build_accept_string(options, accept, sizeof(accept));
  const char* title = (options && options->title) ? options->title : "";

  pick__js_open(id, title, 0, 1, (options && options->allow_multiple) ? 1 : 0,
               accept, 1, "document", "");
}

void pick__files_impl(const PickFileOptions *options, PickMultiFileCallback cb, void *ud) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (cb) cb(NULL, 0, ud); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_OPEN_MULTI, .multi_cb = cb, .user = ud };

  char accept[512]; pick__build_accept_string(options, accept, sizeof(accept));
  const char* title = (options && options->title) ? options->title : "";

  pick__js_open(id, title, 0, 1, 1, accept, 1, "document", "");
}

void pick__folder_impl(const PickFileOptions *options, PickFileCallback cb, void *ud) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (cb) cb(NULL, ud); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_OPEN_DIR_SINGLE, .single_cb = cb, .user = ud };

  const char* title = (options && options->title) ? options->title : "";

  pick__js_open(id, title, 1, 0, 0, "", 1, "folder", "");
}

void pick__folders_impl(const PickFileOptions *options, PickMultiFileCallback cb, void *ud) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (cb) cb(NULL, 0, ud); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_OPEN_DIR_MULTI, .multi_cb = cb, .user = ud };

  const char* title = (options && options->title) ? options->title : "";

  pick__js_open(id, title, 1, 0, 1, "", 1, "folder", "");
}

void pick__save_impl(const PickFileOptions *options, PickFileCallback cb, void *ud) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (cb) cb(NULL, ud); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_SAVE, .single_cb = cb, .user = ud };

  const char* title     = (options && options->title)        ? options->title        : "";
  const char* suggested = (options && options->default_name) ? options->default_name : "untitled";

  pick__js_save(id, title, suggested, 1, "document", "");
}

void pick_export_file(const char* src_path, const PickFileOptions* options,
                      PickResultCallback done, void* user) {
  pick__js_init_buckets();
  int id = pick__alloc_req(); if (!id) { if (done) done(false, user); return; }
  pick__g_reqs[id] = (pick__em_req_t){ .kind = PICK_REQ_EXPORT, .result_cb = done, .user = user };

  const char* suggested = (options && options->default_name) ? options->default_name : "";
  pick__js_export(id, src_path ? src_path : "", suggested);
}

void pick__message_impl(const PickMessageOptions *opts, PickMessageCallback cb, void *ud) {
  int id = pick__alloc_req(); if (!id) { if (cb) cb(PICK_RESULT_CLOSED, ud); return; }
  
  PickButtonType btns = opts ? opts->buttons : PICK_BUTTON_OK;
  pick__g_reqs[id] = (pick__em_req_t){ 
    .kind = PICK_REQ_MESSAGE, 
    .msg_cb = cb, 
    .user = ud,
    .button_type = btns
  };

  const char* title   = (opts && opts->title)   ? opts->title   : "";
  const char* message = (opts && opts->message) ? opts->message : "";
  const char* iconTok = pick__icon_token(opts ? opts->icon_type : PICK_ICON_DEFAULT);

  char* custom_url = NULL;
  if (opts && opts->icon_type == PICK_ICON_CUSTOM && opts->icon_path && *opts->icon_path) {
    custom_url = pick__js_custom_icon_url(opts->icon_path);
  }

  pick__js_create_dialog(id, "Dialog", title, message, pick__message_style_token(opts ? opts->style : PICK_STYLE_INFO), 
                        1, iconTok, custom_url ? custom_url : "");

  switch (btns) {
    case PICK_BUTTON_OK:
      pick__js_append_action("OK", "ok");
      break;
      
    case PICK_BUTTON_OK_CANCEL:
      pick__js_append_action("Cancel", "cancel");
      pick__js_append_action("OK", "ok");
      break;
      
    case PICK_BUTTON_YES_NO:
      pick__js_append_action("No", "no");
      pick__js_append_action("Yes", "yes");
      break;
      
    case PICK_BUTTON_YES_NO_CANCEL:
      pick__js_append_action("Cancel", "cancel");
      pick__js_append_action("No", "no");
      pick__js_append_action("Yes", "yes");
      break;
  }

  int button_count = (btns == PICK_BUTTON_OK) ? 1 : 
                     (btns == PICK_BUTTON_OK_CANCEL || btns == PICK_BUTTON_YES_NO) ? 2 : 3;
  pick__js_bind_message_handlers(id, button_count);

  if (custom_url) { free(custom_url); }
}

#endif 

#endif

#endif // PICK_H