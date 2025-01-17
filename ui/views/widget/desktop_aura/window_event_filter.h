// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace views {
class DesktopWindowTreeHost;

// An EventFilter that sets properties on native windows.
// The downstream effort to add wayland and x11 support with ozone
// are using this class (to be upstreamed later).
class VIEWS_EXPORT WindowEventFilter : public ui::EventHandler {
 public:
  explicit WindowEventFilter(DesktopWindowTreeHost* window_tree_host);
  ~WindowEventFilter() override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  // Handles touch and mouse events.
  void HandleEventInternal(ui::Event* event);

  // Called when the user clicked the caption area.
  void OnClickedCaption(ui::Event* event, int previous_click_component);

  // Called when the user clicked the maximize button.
  void OnClickedMaximizeButton(ui::MouseEvent* event);

  void ToggleMaximizedState();

  // Dispatches a message to the window manager to tell it to act as if a border
  // or titlebar drag occurred with left mouse click. In case of X11, a
  // _NET_WM_MOVERESIZE message is sent.
  virtual void MaybeDispatchHostWindowDragMovement(int hittest,
                                                   ui::Event* event);

  // A signal to lower an attached to this filter window to the bottom of the
  // stack.
  virtual void LowerWindow();

  DesktopWindowTreeHost* window_tree_host_;

  // The non-client component for the target of a MouseEvent. Mouse events can
  // be destructive to the window tree, which can cause the component of a
  // ui::EF_IS_DOUBLE_CLICK event to no longer be the same as that of the
  // initial click. Acting on a double click should only occur for matching
  // components.
  int click_component_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventFilter);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_EVENT_FILTER_H_
