// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
#define ASH_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ui {
class ComboboxModel;
}

namespace views {
class Combobox;
class Label;
}  // namespace views

namespace ash {

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class ASH_EXPORT NotifierSettingsView
    : public message_center::NotifierSettingsObserver,
      public views::View,
      public views::ButtonListener,
      public views::ComboboxListener {
 public:
  explicit NotifierSettingsView(
      message_center::NotifierSettingsProvider* provider);
  ~NotifierSettingsView() override;

  bool IsScrollable();

  // Overridden from NotifierSettingsDelegate:
  void UpdateIconImage(const message_center::NotifierId& notifier_id,
                       const gfx::Image& icon) override;
  void NotifierGroupChanged() override;
  void NotifierEnabledChanged(const message_center::NotifierId& notifier_id,
                              bool enabled) override;

  void set_provider(message_center::NotifierSettingsProvider* new_provider) {
    provider_ = new_provider;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestLearnMoreButton);

  class ASH_EXPORT NotifierButton : public views::Button,
                                    public views::ButtonListener {
   public:
    NotifierButton(message_center::NotifierSettingsProvider* provider,
                   std::unique_ptr<message_center::Notifier> notifier,
                   views::ButtonListener* listener);
    ~NotifierButton() override;

    void UpdateIconImage(const gfx::Image& icon);
    void SetChecked(bool checked);
    bool checked() const;
    bool has_learn_more() const;
    const message_center::Notifier& notifier() const;

    void SendLearnMorePressedForTest();

   private:
    // Overridden from views::ButtonListener:
    void ButtonPressed(views::Button* button, const ui::Event& event) override;
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    bool ShouldHaveLearnMoreButton() const;
    // Helper function to reset the layout when the view has substantially
    // changed.
    void GridChanged(bool has_learn_more, bool has_icon_view);

    message_center::NotifierSettingsProvider* provider_;  // Weak.
    std::unique_ptr<message_center::Notifier> notifier_;
    // |icon_view_| is owned by us because sometimes we don't leave it
    // in the view hierarchy.
    std::unique_ptr<views::ImageView> icon_view_;
    views::Label* name_view_;
    views::Checkbox* checkbox_;
    views::ImageButton* learn_more_;

    DISALLOW_COPY_AND_ASSIGN(NotifierButton);
  };

  // Given a new list of notifiers, updates the view to reflect it.
  void UpdateContentsView(
      std::vector<std::unique_ptr<message_center::Notifier>> notifiers);

  // Overridden from views::View:
  void Layout() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::ComboboxListener:
  void OnPerformAction(views::Combobox* combobox) override;

  // Callback for views::MenuModelAdapter.
  void OnMenuClosed();

  views::ImageButton* title_arrow_;
  views::Label* title_label_;
  views::Combobox* notifier_group_combobox_;
  views::ScrollView* scroller_;
  message_center::NotifierSettingsProvider* provider_;
  std::set<NotifierButton*> buttons_;
  std::unique_ptr<ui::ComboboxModel> notifier_group_model_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
