// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_voice_interaction_framework_instance.h"

namespace arc {

FakeVoiceInteractionFrameworkInstance::FakeVoiceInteractionFrameworkInstance() =
    default;

FakeVoiceInteractionFrameworkInstance::
    ~FakeVoiceInteractionFrameworkInstance() = default;

void FakeVoiceInteractionFrameworkInstance::Init(
    mojom::VoiceInteractionFrameworkHostPtr host_ptr) {}

void FakeVoiceInteractionFrameworkInstance::StartVoiceInteractionSession() {
  start_session_count_++;
}

void FakeVoiceInteractionFrameworkInstance::ToggleVoiceInteractionSession() {
  toggle_session_count_++;
}

void FakeVoiceInteractionFrameworkInstance::
    StartVoiceInteractionSessionForRegion(const gfx::Rect& region) {}

void FakeVoiceInteractionFrameworkInstance::SetMetalayerVisibility(
    bool visible) {}

void FakeVoiceInteractionFrameworkInstance::SetVoiceInteractionEnabled(
    bool enable) {}

void FakeVoiceInteractionFrameworkInstance::SetVoiceInteractionContextEnabled(
    bool enable) {}

void FakeVoiceInteractionFrameworkInstance::StartVoiceInteractionSetupWizard() {
  setup_wizard_count_++;
}

void FakeVoiceInteractionFrameworkInstance::ShowVoiceInteractionSettings() {
  show_settings_count_++;
}

void FakeVoiceInteractionFrameworkInstance::GetVoiceInteractionSettings(
    const GetVoiceInteractionSettingsCallback& callback) {}

}  // namespace arc
