// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/extension_task.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/view_type.h"
#include "ui/base/resource/resource_bundle.h"

namespace task_manager {

namespace {

gfx::ImageSkia* g_default_icon = nullptr;

gfx::ImageSkia* GetDefaultIcon() {
  if (!ResourceBundle::HasSharedInstance())
    return nullptr;

  if (!g_default_icon) {
    g_default_icon = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_EXTENSIONS_FAVICON);
  }

  return g_default_icon;
}

}  // namespace

ExtensionTask::ExtensionTask(content::WebContents* web_contents,
                             const extensions::Extension* extension,
                             extensions::ViewType view_type)
    : RendererTask(GetExtensionTitle(web_contents, extension, view_type),
                   GetDefaultIcon(),
                   web_contents),
      view_type_(view_type) {
  LoadExtensionIcon(extension);
}

ExtensionTask::~ExtensionTask() {
}

void ExtensionTask::UpdateTitle() {
  // The title of the extension should not change as a result of title change
  // in its WebContents, so we ignore this.
}

void ExtensionTask::UpdateFavicon() {
  // We don't care about the favicon of the WebContents but rather of the
  // extension.
}

void ExtensionTask::Activate() {
  // This task represents the extension view of (for example) a background page
  // or browser action button, so there is no top-level window to bring to the
  // front. Instead, when this task is double-clicked, we bring up the
  // chrome://extensions page in a tab, and highlight the details for this
  // extension.
  //
  // TODO(nick): For extensions::VIEW_TYPE_APP_WINDOW, and maybe others, there
  // may actually be a window we could focus. Special case those here as needed.
  const extensions::Extension* extension =
      extensions::ProcessManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionForWebContents(web_contents());

  if (!extension)
    return;

  Browser* browser = chrome::FindTabbedBrowser(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), true);

  // If an existing browser isn't found, don't create a new one.
  if (!browser)
    return;

  chrome::ShowExtensions(browser, extension->id());
}

Task::Type ExtensionTask::GetType() const {
  return Task::EXTENSION;
}

int ExtensionTask::GetKeepaliveCount() const {
  if (view_type_ != extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
    return -1;

  const extensions::Extension* extension =
      extensions::ProcessManager::Get(web_contents()->GetBrowserContext())
          ->GetExtensionForWebContents(web_contents());
  if (!extension)
    return -1;

  return extensions::ProcessManager::Get(web_contents()->GetBrowserContext())
      ->GetLazyKeepaliveCount(extension);
}

void ExtensionTask::OnExtensionIconImageChanged(extensions::IconImage* image) {
  DCHECK_EQ(extension_icon_.get(), image);

  if (!image->image_skia().isNull())
    set_icon(image->image_skia());
}

base::string16 ExtensionTask::GetExtensionTitle(
    content::WebContents* web_contents,
    const extensions::Extension* extension,
    extensions::ViewType view_type) const {
  DCHECK(web_contents);

  base::string16 title = extension ?
      base::UTF8ToUTF16(extension->name()) :
      RendererTask::GetTitleFromWebContents(web_contents);

  bool is_background =
      view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;

  return RendererTask::PrefixRendererTitle(
      title,
      extension && extension->is_app(),
      true,  // is_extension
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      is_background);
}

void ExtensionTask::LoadExtensionIcon(const extensions::Extension* extension) {
  if (!extension)
    return;

  extension_icon_.reset(
      new extensions::IconImage(web_contents()->GetBrowserContext(),
                                extension,
                                extensions::IconsInfo::GetIcons(extension),
                                extension_misc::EXTENSION_ICON_SMALL,
                                icon(),
                                this));

  // Triggers actual image loading with 1x resources.
  extension_icon_->image_skia().GetRepresentation(1.0f);
  set_icon(extension_icon_->image_skia());
}

}  // namespace task_manager
