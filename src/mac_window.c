#ifdef __APPLE__
#include <Foundation/Foundation.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdbool.h>
#include <stddef.h>

extern const void *sapp_macos_get_window(void);

static IMP original_mouseDown = NULL;
static IMP original_windowWillResize = NULL;

static const CGFloat MIN_WINDOW_WIDTH = 800.0;
static const CGFloat MIN_WINDOW_HEIGHT = 600.0;

static NSSize windowWillResize_toSize(id self, SEL _cmd, id sender,
                                      NSSize frameSize) {

  if (frameSize.width < MIN_WINDOW_WIDTH) {
    frameSize.width = MIN_WINDOW_WIDTH;
  }
  if (frameSize.height < MIN_WINDOW_HEIGHT) {
    frameSize.height = MIN_WINDOW_HEIGHT;
  }

  if (original_windowWillResize) {
    return ((NSSize (*)(id, SEL, id, NSSize))original_windowWillResize)(
        self, _cmd, sender, frameSize);
  }

  return frameSize;
}

static void custom_mouseDown(id self, SEL _cmd, id event) {
  NSPoint locationInWindow = ((NSPoint (*)(id, SEL))objc_msgSend)(
      event, sel_registerName("locationInWindow"));
  NSPoint locationInView = ((NSPoint (*)(id, SEL, NSPoint, id))objc_msgSend)(
      self, sel_registerName("convertPoint:fromView:"), locationInWindow, nil);
  NSRect bounds =
      ((NSRect (*)(id, SEL))objc_msgSend)(self, sel_registerName("bounds"));

  if (locationInView.y >= (bounds.size.height - 28.0)) {
    id window =
        ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("window"));
    SEL performDragSel = sel_registerName("performWindowDragWithEvent:");
    if (((BOOL (*)(id, SEL, SEL))objc_msgSend)(
            window, sel_registerName("respondsToSelector:"), performDragSel)) {
      ((void (*)(id, SEL, id))objc_msgSend)(window, performDragSel, event);
    }
  }

  if (original_mouseDown) {
    ((void (*)(id, SEL, id))original_mouseDown)(self, _cmd, event);
  }
}

void app_set_minimum_window_size(float width, float height) {
  const void *winptr = sapp_macos_get_window();
  if (!winptr)
    return;

  id window = (id)winptr;

  NSSize minSize = {(CGFloat)width, (CGFloat)height};
  ((void (*)(id, SEL, NSSize))objc_msgSend)(
      window, sel_registerName("setMinSize:"), minSize);

  ((void (*)(id, SEL, NSSize))objc_msgSend)(
      window, sel_registerName("setContentMinSize:"), minSize);

  NSRect frame =
      ((NSRect (*)(id, SEL))objc_msgSend)(window, sel_registerName("frame"));

  BOOL needsResize = NO;
  if (frame.size.width < width) {
    frame.size.width = width;
    needsResize = YES;
  }
  if (frame.size.height < height) {
    frame.size.height = height;
    needsResize = YES;
  }

  if (needsResize) {
    ((void (*)(id, SEL, NSRect, BOOL))objc_msgSend)(
        window, sel_registerName("setFrame:display:"), frame, YES);
  }
}

void app_install_resize_handler(void) {
  const void *winptr = sapp_macos_get_window();
  if (!winptr)
    return;

  id window = (id)winptr;
  id delegate =
      ((id (*)(id, SEL))objc_msgSend)(window, sel_registerName("delegate"));

  if (!delegate) {

    Class NSObjectClass = objc_getClass("NSObject");
    Class DelegateClass =
        objc_allocateClassPair(NSObjectClass, "WindowResizeDelegate", 0);

    if (DelegateClass) {
      SEL resizeSel = sel_registerName("windowWillResize:toSize:");
      class_addMethod(DelegateClass, resizeSel, (IMP)windowWillResize_toSize,
                      "d@:@{CGSize=dd}");
      objc_registerClassPair(DelegateClass);

      delegate = ((id (*)(Class, SEL))objc_msgSend)((id)DelegateClass,
                                                    sel_registerName("alloc"));
      delegate =
          ((id (*)(id, SEL))objc_msgSend)(delegate, sel_registerName("init"));

      ((void (*)(id, SEL, id))objc_msgSend)(
          window, sel_registerName("setDelegate:"), delegate);
    }
  } else {

    Class delegateClass = object_getClass(delegate);
    SEL resizeSel = sel_registerName("windowWillResize:toSize:");

    Method originalMethod = class_getInstanceMethod(delegateClass, resizeSel);
    if (originalMethod) {
      original_windowWillResize = method_getImplementation(originalMethod);
      method_setImplementation(originalMethod, (IMP)windowWillResize_toSize);
    } else {
      class_addMethod(delegateClass, resizeSel, (IMP)windowWillResize_toSize,
                      "d@:@{CGSize=dd}");
    }
  }
}

void app_make_compact_window(bool show_controls) {
  const void *winptr = sapp_macos_get_window();
  if (!winptr)
    return;

  id window = (id)winptr;

  NSUInteger style_mask = ((NSUInteger (*)(id, SEL))objc_msgSend)(
      window, sel_registerName("styleMask"));
  style_mask |=
      (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 15) | (1 << 12);
  ((void (*)(id, SEL, NSUInteger))objc_msgSend)(
      window, sel_registerName("setStyleMask:"), style_mask);

  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      window, sel_registerName("setTitlebarAppearsTransparent:"), YES);
  ((void (*)(id, SEL, NSInteger))objc_msgSend)(
      window, sel_registerName("setTitleVisibility:"), 1);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(window,
                                          sel_registerName("setMovable:"), YES);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      window, sel_registerName("setMovableByWindowBackground:"), NO);
  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      window, sel_registerName("setReleasedWhenClosed:"), NO);

  SEL setToolbarStyleSEL = sel_registerName("setToolbarStyle:");
  if (class_respondsToSelector(object_getClass(window), setToolbarStyleSEL)) {
    ((void (*)(id, SEL, NSInteger))objc_msgSend)(window, setToolbarStyleSEL, 4);
  }

  id closeButton = ((id (*)(id, SEL, NSInteger))objc_msgSend)(
      window, sel_registerName("standardWindowButton:"), 0);
  id miniButton = ((id (*)(id, SEL, NSInteger))objc_msgSend)(
      window, sel_registerName("standardWindowButton:"), 1);
  id zoomButton = ((id (*)(id, SEL, NSInteger))objc_msgSend)(
      window, sel_registerName("standardWindowButton:"), 2);

  BOOL hidden = show_controls ? NO : YES;
  if (closeButton) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        closeButton, sel_registerName("setHidden:"), hidden);
  }
  if (miniButton) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        miniButton, sel_registerName("setHidden:"), hidden);
  }
  if (zoomButton) {
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        zoomButton, sel_registerName("setHidden:"), hidden);
  }

  id contentView =
      ((id (*)(id, SEL))objc_msgSend)(window, sel_registerName("contentView"));
  if (!contentView)
    return;

  ((void (*)(id, SEL, BOOL))objc_msgSend)(
      contentView, sel_registerName("setWantsLayer:"), YES);

  Class viewClass = object_getClass(contentView);
  SEL mouseDownSel = sel_registerName("mouseDown:");
  Method originalMethod = class_getInstanceMethod(viewClass, mouseDownSel);
  if (originalMethod && !original_mouseDown) {
    original_mouseDown = method_getImplementation(originalMethod);
    method_setImplementation(originalMethod, (IMP)custom_mouseDown);
  }

  app_set_minimum_window_size(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

  app_install_resize_handler();
}

#endif