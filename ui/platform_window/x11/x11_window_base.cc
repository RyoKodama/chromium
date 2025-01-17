// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_base.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_window_event_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/platform_window/platform_window_delegate.h"

#include "ui/base/x/x11_pointer_grab.h"
#include "ui/events/devices/x11/touch_factory_x11.h"

namespace ui {

namespace {

// These constants are defined in the Extended Window Manager Hints
// standard...and aren't in any header that I can find.
const int k_NET_WM_MOVERESIZE_SIZE_TOPLEFT = 0;
const int k_NET_WM_MOVERESIZE_SIZE_TOP = 1;
const int k_NET_WM_MOVERESIZE_SIZE_TOPRIGHT = 2;
const int k_NET_WM_MOVERESIZE_SIZE_RIGHT = 3;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT = 4;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOM = 5;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT = 6;
const int k_NET_WM_MOVERESIZE_SIZE_LEFT = 7;
const int k_NET_WM_MOVERESIZE_MOVE = 8;

// Identifies the direction of the "hittest" for X11.
bool IdentifyDirection(int hittest, int* direction) {
  DCHECK(direction);
  *direction = -1;
  switch (hittest) {
    case HTBOTTOM:
      *direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
      break;
    case HTBOTTOMRIGHT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
      break;
    case HTCAPTION:
      *direction = k_NET_WM_MOVERESIZE_MOVE;
      break;
    case HTLEFT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_LEFT;
      break;
    case HTRIGHT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_RIGHT;
      break;
    case HTTOP:
      *direction = k_NET_WM_MOVERESIZE_SIZE_TOP;
      break;
    case HTTOPLEFT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_TOPLEFT;
      break;
    case HTTOPRIGHT:
      *direction = k_NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
      break;
    default:
      return false;
  }
  return true;
}

// Constants that are part of EWMH.
const int k_NET_WM_STATE_ADD = 1;
const int k_NET_WM_STATE_REMOVE = 0;

XID FindXEventTarget(const XEvent& xev) {
  XID target = xev.xany.window;
  if (xev.type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev.xcookie.data)->event;
  return target;
}

}  // namespace

X11WindowBase::X11WindowBase(PlatformWindowDelegate* delegate,
                             const gfx::Rect& bounds)
    : delegate_(delegate),
      xdisplay_(gfx::GetXDisplay()),
      xwindow_(None),
      xroot_window_(DefaultRootWindow(xdisplay_)),
      bounds_(bounds) {
  DCHECK(delegate_);
}

X11WindowBase::~X11WindowBase() {
  Destroy();
}

void X11WindowBase::Destroy() {
  if (xwindow_ == None)
    return;

  // Stop processing events.
  XID xwindow = xwindow_;
  XDisplay* xdisplay = xdisplay_;
  xwindow_ = None;
  delegate_->OnClosed();
  // |this| might be deleted because of the above call.

  XDestroyWindow(xdisplay, xwindow);
}

void X11WindowBase::SetPointerGrab() {
  if (has_pointer_grab_)
    return;
  has_pointer_grab_ |= !ui::GrabPointer(xwindow_, true, None);
}

void X11WindowBase::ReleasePointerGrab() {
  ui::UngrabPointer();
  has_pointer_grab_ = false;
}

void X11WindowBase::Create() {
  DCHECK(!bounds_.size().IsEmpty());

  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  swa.bit_gravity = NorthWestGravity;
  swa.override_redirect = UseTestConfigForPlatformWindows();

  ::Atom window_type;
  // There is now default initialization for this type. Initialize it
  // to ::WINDOW here. It will be changed by delelgate if it know the
  // type of the window.
  ui::PlatformWindowType ui_window_type =
      ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_WINDOW;
  delegate_->GetWindowType(&ui_window_type);
  switch (ui_window_type) {
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_MENU:
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_MENU");
      swa.override_redirect = True;
      break;
    case ui::PlatformWindowType::PLATFORM_WINDOW_TYPE_POPUP:
      swa.override_redirect = True;
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
      break;
    default:
      window_type = gfx::GetAtom("_NET_WM_WINDOW_TYPE_NORMAL");
      break;
  }

  xwindow_ =
      XCreateWindow(xdisplay_, xroot_window_, bounds_.x(), bounds_.y(),
                    bounds_.width(), bounds_.height(),
                    0,               // border width
                    CopyFromParent,  // depth
                    InputOutput,
                    CopyFromParent,  // visual
                    CWBackPixmap | CWBitGravity | CWOverrideRedirect, &swa);

  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_WINDOW_TYPE"),
                  XA_ATOM, 32, PropModeReplace,
                  reinterpret_cast<unsigned char*>(&window_type), 1);

  // Setup XInput event mask.
  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask | ExposureMask |
                    VisibilityChangeMask | StructureNotifyMask |
                    PropertyChangeMask | PointerMotionMask;

  xwindow_events_.reset(new ui::XScopedEventSelector(xwindow_, event_mask));

  // Setup XInput2 event mask.
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);
  XISetMask(mask, XI_KeyPress);
  XISetMask(mask, XI_KeyRelease);
  XISetMask(mask, XI_HierarchyChanged);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(xdisplay_, xwindow_, &evmask, 1);
  XFlush(xdisplay_);

  ::Atom protocols[2];
  protocols[0] = gfx::GetAtom("WM_DELETE_WINDOW");
  protocols[1] = gfx::GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_PID"), XA_CARDINAL,
                  32, PropModeReplace, reinterpret_cast<unsigned char*>(&pid),
                  1);
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition | PWinGravity;
  size_hints.x = bounds_.x();
  size_hints.y = bounds_.y();
  // Set StaticGravity so that the window position is not affected by the
  // frame width when running with window manager.
  size_hints.win_gravity = StaticGravity;
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

// Disable native frame by default in non-ChromeOS builds for now.
// TODO(msisov, tonikitoo): check if native frame should be used by checking
// Widget::InitParams::remove_standard_frame.
#if !defined(OS_CHROMEOS)
  ui::SetUseOSWindowFrame(xwindow_, false);
#endif

  // TODO(sky): provide real scale factor.
  delegate_->OnAcceleratedWidgetAvailable(xwindow_, 1.f);
}

void X11WindowBase::Show() {
  if (window_mapped_)
    return;
  if (xwindow_ == None)
    Create();

  XMapWindow(xdisplay_, xwindow_);

  // We now block until our window is mapped. Some X11 APIs will crash and
  // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
  // asynchronous.
  if (X11EventSource::GetInstance())
    X11EventSource::GetInstance()->BlockUntilWindowMapped(xwindow_);
  window_mapped_ = true;
}

void X11WindowBase::Hide() {
  if (!window_mapped_ || IsMinimized())
    return;
  XWithdrawWindow(xdisplay_, xwindow_, 0);
  window_mapped_ = false;
}

void X11WindowBase::Close() {
  Destroy();
}

void X11WindowBase::SetBounds(const gfx::Rect& bounds) {
  if (window_mapped_) {
    XWindowChanges changes = {0};
    unsigned value_mask = 0;

    if (bounds_.size() != bounds.size()) {
      changes.width = bounds.width();
      changes.height = bounds.height();
      value_mask |= CWHeight | CWWidth;
    }

    if (bounds_.origin() != bounds.origin()) {
      changes.x = bounds.x();
      changes.y = bounds.y();
      value_mask |= CWX | CWY;
    }

    if (value_mask)
      XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
  }

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  bounds_ = bounds;

  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  delegate_->OnBoundsChanged(bounds_);
}

gfx::Rect X11WindowBase::GetBounds() {
  return bounds_;
}

void X11WindowBase::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;
  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_, xwindow_, gfx::GetAtom("_NET_WM_NAME"),
                  gfx::GetAtom("UTF8_STRING"), 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char* c_utf8_str = const_cast<char*>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1, XUTF8StringStyle,
                                  &xtp) == Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
}

void X11WindowBase::SetCapture() {
  has_pointer_grab_ |= !ui::GrabPointer(xwindow_, true, None);
}

void X11WindowBase::ReleaseCapture() {
  ui::UngrabPointer();
  has_pointer_grab_ = false;
}

void X11WindowBase::ToggleFullscreen() {
  SetWMSpecState(!is_fullscreen_, gfx::GetAtom("_NET_WM_STATE_FULLSCREEN"),
                 None);
  is_fullscreen_ = !is_fullscreen_;
}

void X11WindowBase::Maximize() {
  // Unfullscreen the window if it is fullscreen.
  if (IsFullScreen())
    ToggleFullscreen();

  // When we are in the process of requesting to maximize a window, we can
  // accurately keep track of our restored bounds instead of relying on the
  // heuristics that are in the PropertyNotify and ConfigureNotify handlers.
  restored_bounds_in_pixels_ = bounds_;

  SetWMSpecState(true, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                 gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

void X11WindowBase::Minimize() {
  if (IsMinimized())
    return;
  XIconifyWindow(xdisplay_, xwindow_, 0);
}

void X11WindowBase::Restore() {
  if (IsFullScreen())
    ToggleFullscreen();

  if (IsMaximized()) {
    SetWMSpecState(false, gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"),
                   gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ"));
  }
}

void X11WindowBase::MoveCursorTo(const gfx::Point& location) {
  XWarpPointer(xdisplay_, None, xroot_window_, 0, 0, 0, 0,
               bounds_.x() + location.x(), bounds_.y() + location.y());
}

void X11WindowBase::ConfineCursorToBounds(const gfx::Rect& bounds) {}

PlatformImeController* X11WindowBase::GetPlatformImeController() {
  return nullptr;
}

void X11WindowBase::PerformNativeWindowDragOrResize(uint32_t hittest) {
  int direction;
  if (!IdentifyDirection(hittest, &direction))
    return;

  // We most likely have an implicit grab right here. We need to dump it
  // because what we're about to do is tell the window manager
  // that it's now responsible for moving the window around; it immediately
  // grabs when it receives the event below.
  XUngrabPointer(xdisplay_, CurrentTime);

  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.display = xdisplay_;
  event.xclient.window = xwindow_;
  event.xclient.message_type = gfx::GetAtom("_NET_WM_MOVERESIZE");
  event.xclient.format = 32;
  event.xclient.data.l[0] = xroot_window_event_location_.x();
  event.xclient.data.l[1] = xroot_window_event_location_.y();
  event.xclient.data.l[2] = direction;
  event.xclient.data.l[3] = 0;
  event.xclient.data.l[4] = 0;

  XSendEvent(xdisplay_, xroot_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

bool X11WindowBase::IsEventForXWindow(const XEvent& xev) const {
  return xwindow_ != None && FindXEventTarget(xev) == xwindow_;
}

void X11WindowBase::ProcessXWindowEvent(XEvent* xev) {
  switch (xev->type) {
    case EnterNotify:
    case LeaveNotify: {
      OnCrossingEvent(xev->type == EnterNotify, xev->xcrossing.focus,
                      xev->xcrossing.mode, xev->xcrossing.detail);
      break;
    }

    case Expose: {
      gfx::Rect damage_rect(xev->xexpose.x, xev->xexpose.y, xev->xexpose.width,
                            xev->xexpose.height);
      delegate_->OnDamageRect(damage_rect);
      break;
    }

    case FocusIn:
    case FocusOut: {
      OnFocusEvent(xev->type == FocusIn, xev->xfocus.mode, xev->xfocus.detail);
      break;
    }

    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      int translated_x_in_pixels = xev->xconfigure.x;
      int translated_y_in_pixels = xev->xconfigure.y;
      if (!xev->xconfigure.send_event && !xev->xconfigure.override_redirect) {
        Window unused;
        XTranslateCoordinates(xdisplay_, xwindow_, xroot_window_, 0, 0,
                              &translated_x_in_pixels, &translated_y_in_pixels,
                              &unused);
      }
      gfx::Rect bounds(translated_x_in_pixels, translated_y_in_pixels,
                       xev->xconfigure.width, xev->xconfigure.height);
      if (bounds_ != bounds) {
        bounds_ = bounds;
        delegate_->OnBoundsChanged(bounds_);
      }
      break;
    }

    case MapNotify: {
      window_mapped_in_server_ = true;
      break;
    }

    case UnmapNotify: {
      window_mapped_in_server_ = false;
      has_pointer_ = false;
      has_pointer_grab_ = false;
      has_pointer_focus_ = false;
      has_window_focus_ = false;
      break;
    }

    case ClientMessage: {
      Atom message = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message == gfx::GetAtom("WM_DELETE_WINDOW")) {
        delegate_->OnCloseRequest();
      } else if (message == gfx::GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = xroot_window_;

        XSendEvent(xdisplay_, reply_event.xclient.window, False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
        XFlush(xdisplay_);
      }
      break;
    }

    case PropertyNotify: {
      ::Atom changed_atom = xev->xproperty.atom;
      if (changed_atom == gfx::GetAtom("_NET_WM_STATE"))
        OnWMStateUpdated();
      break;
    }
  }
}

void X11WindowBase::SetWMSpecState(bool enabled, ::Atom state1, ::Atom state2) {
  XEvent xclient;
  memset(&xclient, 0, sizeof(xclient));
  xclient.type = ClientMessage;
  xclient.xclient.window = xwindow_;
  xclient.xclient.message_type = gfx::GetAtom("_NET_WM_STATE");
  xclient.xclient.format = 32;
  xclient.xclient.data.l[0] =
      enabled ? k_NET_WM_STATE_ADD : k_NET_WM_STATE_REMOVE;
  xclient.xclient.data.l[1] = state1;
  xclient.xclient.data.l[2] = state2;
  xclient.xclient.data.l[3] = 1;
  xclient.xclient.data.l[4] = 0;

  XSendEvent(xdisplay_, xroot_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask, &xclient);
}

void X11WindowBase::OnWMStateUpdated() {
  std::vector<::Atom> atom_list;
  // Ignore the return value of ui::GetAtomArrayProperty(). Fluxbox removes the
  // _NET_WM_STATE property when no _NET_WM_STATE atoms are set.
  ui::GetAtomArrayProperty(xwindow_, "_NET_WM_STATE", &atom_list);

  bool was_minimized = IsMinimized();

  window_properties_.clear();
  std::copy(atom_list.begin(), atom_list.end(),
            inserter(window_properties_, window_properties_.begin()));

  // Propagate the window minimization information to the client.
  ui::PlatformWindowState state =
      ui::PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;
  if (IsMinimized() != was_minimized) {
    if (IsMinimized()) {
      state = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
    } else if (IsMaximized()) {
      // When the window is recovered from minimized state, set state to the
      // previous maximized state if it was like that. Otherwise, NORMAL state
      // will be set.
      state = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
    }
    delegate_->OnWindowStateChanged(state);
  }
}

void X11WindowBase::BeforeActivationStateChanged() {
  was_active_ = IsActive();
  had_pointer_ = has_pointer_;
  had_pointer_grab_ = has_pointer_grab_;
  had_window_focus_ = has_window_focus_;
}

void X11WindowBase::AfterActivationStateChanged() {
  if (had_pointer_grab_ && !has_pointer_grab_) {
    // TODO(msisov, tonikitoo): think how to make a call to
    // dispatcher()->OnHostLostMouseGrab(). That's done in
    // DesktopWindowTreeHostX11::AfterActivationStateChanged also.
  }

  bool had_pointer_capture = had_pointer_ || had_pointer_grab_;
  bool has_pointer_capture = has_pointer_ || has_pointer_grab_;
  if (had_pointer_capture && !has_pointer_capture)
    delegate_->OnLostCapture();

  if (was_active_ != IsActive())
    delegate_->OnActivationChanged(IsActive());
}

bool X11WindowBase::IsActive() const {
  // Focus and stacking order are independent in X11.  Since we cannot guarantee
  // a window is topmost if it has focus, just use the focus state to determine
  // if a window is active.
  bool is_active = has_window_focus_ || has_pointer_focus_;

  // is_active => window_mapped_in_server_
  // !window_mapped_in_server_ => !is_active
  DCHECK(!is_active || window_mapped_in_server_);

  // |has_window_focus_| and |has_pointer_focus_| are mutually exclusive.
  DCHECK(!has_window_focus_ || !has_pointer_focus_);

  return is_active;
}

void X11WindowBase::OnCrossingEvent(bool enter,
                                    bool focus_in_window_or_ancestor,
                                    int mode,
                                    int detail) {
  // NotifyInferior on a crossing event means the pointer moved into or out of a
  // child window, but the pointer is still within |xwindow_|.
  if (detail == NotifyInferior)
    return;

  BeforeActivationStateChanged();

  if (mode == NotifyGrab)
    has_pointer_grab_ = enter;
  else if (mode == NotifyUngrab)
    has_pointer_grab_ = false;

  has_pointer_ = enter;
  if (focus_in_window_or_ancestor && !has_window_focus_) {
    // If we reach this point, we know the focus is in an ancestor or the
    // pointer root.  The definition of |has_pointer_focus_| is (An ancestor
    // window or the PointerRoot is focused) && |has_pointer_|.  Therefore, we
    // can just use |has_pointer_| in the assignment.  The transitions for when
    // the focus changes are handled in OnFocusEvent().
    has_pointer_focus_ = has_pointer_;
  }

  AfterActivationStateChanged();
}

void X11WindowBase::OnFocusEvent(bool focus_in, int mode, int detail) {
  // NotifyInferior on a focus event means the focus moved into or out of a
  // child window, but the focus is still within |xwindow_|.
  if (detail == NotifyInferior)
    return;

  bool notify_grab = mode == NotifyGrab || mode == NotifyUngrab;

  BeforeActivationStateChanged();

  // For every focus change, the X server sends normal focus events which are
  // useful for tracking |has_window_focus_|, but supplements these events with
  // NotifyPointer events which are only useful for tracking pointer focus.

  // For |has_pointer_focus_| and |has_window_focus_|, we continue tracking
  // state during a grab, but ignore grab/ungrab events themselves.
  if (!notify_grab && detail != NotifyPointer)
    has_window_focus_ = focus_in;

  if (!notify_grab && has_pointer_) {
    switch (detail) {
      case NotifyAncestor:
      case NotifyVirtual:
        // If we reach this point, we know |has_pointer_| was true before and
        // after this event.  Since the definition of |has_pointer_focus_| is
        // (An ancestor window or the PointerRoot is focused) && |has_pointer_|,
        // we only need to worry about transitions on the first conjunct.
        // Therefore, |has_pointer_focus_| will become true when:
        // 1. Focus moves from |xwindow_| to an ancestor
        //    (FocusOut with NotifyAncestor)
        // 2. Focus moves from a decendant of |xwindow_| to an ancestor
        //    (FocusOut with NotifyVirtual)
        // |has_pointer_focus_| will become false when:
        // 1. Focus moves from an ancestor to |xwindow_|
        //    (FocusIn with NotifyAncestor)
        // 2. Focus moves from an ancestor to a child of |xwindow_|
        //    (FocusIn with NotifyVirtual)
        has_pointer_focus_ = !focus_in;
        break;
      case NotifyPointer:
        // The remaining cases for |has_pointer_focus_| becoming true are:
        // 3. Focus moves from |xwindow_| to the PointerRoot
        // 4. Focus moves from a decendant of |xwindow_| to the PointerRoot
        // 5. Focus moves from None to the PointerRoot
        // 6. Focus moves from Other to the PointerRoot
        // 7. Focus moves from None to an ancestor of |xwindow_|
        // 8. Focus moves from Other to an ancestor fo |xwindow_|
        // In each case, we will get a FocusIn with a detail of NotifyPointer.
        // The remaining cases for |has_pointer_focus_| becoming false are:
        // 3. Focus moves from the PointerRoot to |xwindow_|
        // 4. Focus moves from the PointerRoot to a decendant of |xwindow|
        // 5. Focus moves from the PointerRoot to None
        // 6. Focus moves from an ancestor of |xwindow_| to None
        // 7. Focus moves from the PointerRoot to Other
        // 8. Focus moves from an ancestor of |xwindow_| to Other
        // In each case, we will get a FocusOut with a detail of NotifyPointer.
        has_pointer_focus_ = focus_in;
        break;
      case NotifyNonlinear:
      case NotifyNonlinearVirtual:
        // We get Nonlinear(Virtual) events when
        // 1. Focus moves from Other to |xwindow_|
        //    (FocusIn with NotifyNonlinear)
        // 2. Focus moves from Other to a decendant of |xwindow_|
        //    (FocusIn with NotifyNonlinearVirtual)
        // 3. Focus moves from |xwindow_| to Other
        //    (FocusOut with NotifyNonlinear)
        // 4. Focus moves from a decendant of |xwindow_| to Other
        //    (FocusOut with NotifyNonlinearVirtual)
        // |has_pointer_focus_| should be false before and after this event.
        has_pointer_focus_ = false;
        break;
      default:
        break;
    }
  }

  AfterActivationStateChanged();
}

bool X11WindowBase::HasWMSpecProperty(const char* property) const {
  return window_properties_.find(gfx::GetAtom(property)) !=
         window_properties_.end();
}

bool X11WindowBase::IsMinimized() const {
  return HasWMSpecProperty("_NET_WM_STATE_HIDDEN");
}

bool X11WindowBase::IsMaximized() const {
  return (HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_VERT") &&
          HasWMSpecProperty("_NET_WM_STATE_MAXIMIZED_HORZ"));
}

bool X11WindowBase::IsFullScreen() const {
  return is_fullscreen_;
}

}  // namespace ui
