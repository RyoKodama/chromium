// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WEB_PAGE_NOTIFIER_SOURCE_H_
#define CHROME_BROWSER_NOTIFICATIONS_WEB_PAGE_NOTIFIER_SOURCE_H_

#include "chrome/browser/notifications/notifier_source.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace base {
class CancelableTaskTracker;
}

namespace favicon_base {
struct FaviconImageResult;
}

class WebPageNotifierSource : public NotifierSource {
 public:
  explicit WebPageNotifierSource(Observer* observer);
  ~WebPageNotifierSource() override;

  std::vector<std::unique_ptr<message_center::Notifier>> GetNotifierList(
      Profile* profile) override;

  void SetNotifierEnabled(Profile* profile,
                          const message_center::NotifierId& notifier_id,
                          bool enabled) override;

  void OnNotifierSettingsClosing() override;

  message_center::NotifierId::NotifierType GetNotifierType() override;

 private:
  void OnFaviconLoaded(const GURL& url,
                       const favicon_base::FaviconImageResult& favicon_result);

  std::map<std::string, ContentSettingsPattern> patterns_;

  // The task tracker for loading favicons.
  std::unique_ptr<base::CancelableTaskTracker> favicon_tracker_;

  // Lifetime of parent must be longer than the source.
  Observer* observer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WEB_PAGE_NOTIFIER_SOURCE_H_
