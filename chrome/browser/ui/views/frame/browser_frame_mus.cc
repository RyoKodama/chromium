// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mus.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/window_manager_frame_values.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/window_style.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#endif

#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#endif

BrowserFrameMus::BrowserFrameMus(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_frame),
      browser_frame_(browser_frame),
      browser_view_(browser_view) {}

BrowserFrameMus::~BrowserFrameMus() {}

views::Widget::InitParams BrowserFrameMus::GetWidgetParams() {
  views::Widget::InitParams params;
  params.name = "BrowserFrame";
  params.native_widget = this;
#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  chrome::GetSavedWindowBoundsAndShowState(browser_view_->browser(),
                                           &params.bounds, &params.show_state);
#else
  params.bounds = gfx::Rect(10, 10, 640, 480);
#endif
  params.delegate = browser_view_;
  std::map<std::string, std::vector<uint8_t>> properties =
      views::MusClient::ConfigurePropertiesFromParams(params);
  const std::string chrome_app_id(extension_misc::kChromeAppId);
  // Indicates mash shouldn't handle immersive, rather we will.
  properties[ui::mojom::WindowManager::kDisableImmersive_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(true);
#if defined(OS_CHROMEOS)
  properties[ash::mojom::kAshWindowStyle_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ash::mojom::WindowStyle::BROWSER));
  // ChromeLauncherController manages the browser shortcut shelf item; set the
  // window's shelf item type property to be ignored by ash::ShelfWindowWatcher.
  properties[ui::mojom::WindowManager::kShelfItemType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int64_t>(ash::TYPE_BROWSER_SHORTCUT));
#endif
  aura::WindowTreeHostMusInitParams window_tree_host_init_params =
      aura::CreateInitParamsForTopLevel(
          views::MusClient::Get()->window_tree_client(), std::move(properties));
#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  window_tree_host_init_params.use_classic_ime =
      !base::CommandLine::ForCurrentProcess()->HasSwitch("use-ime-service");
#endif
  std::unique_ptr<views::DesktopWindowTreeHostMus> desktop_window_tree_host =
      base::MakeUnique<views::DesktopWindowTreeHostMus>(
          std::move(window_tree_host_init_params), browser_frame_, this);
  // BrowserNonClientFrameViewMus::OnBoundsChanged() takes care of updating
  // the insets.
  desktop_window_tree_host->set_auto_update_client_area(false);
  SetDesktopWindowTreeHost(std::move(desktop_window_tree_host));
  return params;
}

bool BrowserFrameMus::UseCustomFrame() const {
  return true;
}

bool BrowserFrameMus::UsesNativeSystemMenu() const {
  return false;
}

bool BrowserFrameMus::ShouldSaveWindowPlacement() const {
#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return nullptr == GetWidget()->GetNativeWindow()->GetProperty(
                        aura::client::kRestoreBoundsKey);
#else
  return false;
#endif
}

void BrowserFrameMus::GetWindowPlacement(
    gfx::Rect* bounds, ui::WindowShowState* show_state) const {
#if defined(USE_OZONE) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  DesktopNativeWidgetAura::GetWindowPlacement(bounds, show_state);

  gfx::Rect* override_bounds = GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kRestoreBoundsKey);
  if (override_bounds) {
    *bounds = *override_bounds;
    *show_state = GetWidget()->GetNativeWindow()->GetProperty(
        aura::client::kShowStateKey);
  }

  if (*show_state != ui::SHOW_STATE_MAXIMIZED &&
      *show_state != ui::SHOW_STATE_MINIMIZED) {
    *show_state = ui::SHOW_STATE_NORMAL;
  }
#else
  *bounds = gfx::Rect(10, 10, 800, 600);
  *show_state = ui::SHOW_STATE_NORMAL;
#endif
}

bool BrowserFrameMus::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

bool BrowserFrameMus::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

int BrowserFrameMus::GetMinimizeButtonOffset() const {
  return 0;
}
