// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORD_REUSE_WARNING_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORD_REUSE_WARNING_DIALOG_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

@class PasswordReuseWarningViewController;

// A constrained dialog that warns users about a password reuse.
class PasswordReuseWarningDialogCocoa
    : public ConstrainedWindowMacDelegate,
      public safe_browsing::ChromePasswordProtectionService::Observer {
 public:
  PasswordReuseWarningDialogCocoa(
      content::WebContents* web_contents,
      safe_browsing::ChromePasswordProtectionService* service,
      safe_browsing::OnWarningDone callback);

  ~PasswordReuseWarningDialogCocoa() override;

  // ChromePasswordProtectionService::Observer:
  void OnStartingGaiaPasswordChange() override;
  void OnGaiaPasswordChanged() override;
  void OnMarkingSiteAsLegitimate(const GURL& url) override;
  void InvokeActionForTesting(
      safe_browsing::ChromePasswordProtectionService::WarningAction action)
      override;
  safe_browsing::ChromePasswordProtectionService::WarningUIType
  GetObserverType() override;

  // Called by |controller_| when a dialog button is selected.
  void OnChangePassword();
  void OnIgnore();

 private:
  // ConstrainedWindowMacDelegate:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

  // This class observes the |service_| to check if the password reuse
  // status has changed.
  safe_browsing::ChromePasswordProtectionService* service_;  // weak.

  // The url of the site that triggered this dialog.
  const GURL url_;

  // Dialog button callback.
  safe_browsing::OnWarningDone callback_;

  // Controller for the dialog view.
  base::scoped_nsobject<PasswordReuseWarningViewController> controller_;

  // The constrained window that contains the dialog view.
  std::unique_ptr<ConstrainedWindowMac> window_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseWarningDialogCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORD_REUSE_WARNING_DIALOG_COCOA_H_
