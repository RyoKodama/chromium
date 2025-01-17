// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_

#include <stdint.h>
#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/interfaces/user_info.mojom.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class LoginPasswordView;
class LoginPinView;

// Wraps a UserView which also has authentication available. Adds additional
// views below the UserView instance which show authentication UIs.
class ASH_EXPORT LoginAuthUserView : public NonAccessibleView {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginAuthUserView* view);
    ~TestApi();

    LoginUserView* user_view() const;
    LoginPasswordView* password_view() const;

   private:
    LoginAuthUserView* const view_;
  };

  // Flags which describe the set of currently visible auth methods.
  enum AuthMethods {
    AUTH_NONE = 0,              // No extra auth methods.
    AUTH_PASSWORD = 1 << 0,     // Display password.
    AUTH_PIN = 1 << 1,          // Display PIN keyboard.
    AUTH_EASY_UNLOCK = 1 << 2,  // Display easy unlock icon.
    AUTH_TAP = 1 << 3,          // Tap to unlock.
  };

  using OnAuthCallback = base::Callback<void(bool auth_success)>;

  // |on_auth| is executed whenever an authentication result is available;
  // cannot be null.
  LoginAuthUserView(const mojom::UserInfoPtr& user,
                    const OnAuthCallback& on_auth,
                    const LoginUserView::OnTap& on_tap);
  ~LoginAuthUserView() override;

  // Set the displayed set of auth methods. |auth_methods| contains or-ed
  // together AuthMethod values.
  void SetAuthMethods(uint32_t auth_methods);
  AuthMethods auth_methods() const { return auth_methods_; }

  // Captures any metadata about the current view state that will be used for
  // animation.
  void CaptureStateForAnimationPreLayout();
  // Applies animation based on current layout state compared to the most
  // recently captured state.
  void ApplyAnimationPostLayout();

  // Update the displayed name, icon, etc to that of |user|.
  void UpdateForUser(const mojom::UserInfoPtr& user);

  const mojom::UserInfoPtr& current_user() const;

  LoginPasswordView* password_view() { return password_view_; }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;

 private:
  struct AnimationState;

  // Called when the user submits an auth method. Runs mojo call.
  void OnAuthSubmit(const base::string16& password);

  AuthMethods auth_methods_ = AUTH_NONE;
  views::View* non_pin_root_ = nullptr;
  LoginUserView* user_view_ = nullptr;
  LoginPasswordView* password_view_ = nullptr;
  LoginPinView* pin_view_ = nullptr;
  const OnAuthCallback on_auth_;

  // Animation state that was cached from before a layout. Generated by
  // |CaptureStateForAnimationPreLayout| and consumed by
  // |ApplyAnimationPostLayout|.
  std::unique_ptr<AnimationState> cached_animation_state_;

  DISALLOW_COPY_AND_ASSIGN(LoginAuthUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_USER_VIEW_H_
