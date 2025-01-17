// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppapi_preferences_builder.h"

#include "content/public/common/web_preferences.h"
#include "ppapi/shared_impl/ppapi_preferences.h"

namespace content {

ppapi::Preferences PpapiPreferencesBuilder::Build(
    const WebPreferences& prefs) {
  ppapi::Preferences ppapi_prefs;
  ppapi_prefs.standard_font_family_map = prefs.standard_font_family_map;
  ppapi_prefs.fixed_font_family_map = prefs.fixed_font_family_map;
  ppapi_prefs.serif_font_family_map = prefs.serif_font_family_map;
  ppapi_prefs.sans_serif_font_family_map = prefs.sans_serif_font_family_map;
  ppapi_prefs.default_font_size = prefs.default_font_size;
  ppapi_prefs.default_fixed_font_size = prefs.default_fixed_font_size;
  ppapi_prefs.number_of_cpu_cores = prefs.number_of_cpu_cores;
  ppapi_prefs.is_3d_supported = prefs.flash_3d_enabled;
  ppapi_prefs.is_stage3d_supported = prefs.flash_stage3d_enabled;
  ppapi_prefs.is_stage3d_baseline_supported =
      prefs.flash_stage3d_baseline_enabled;
  ppapi_prefs.is_accelerated_video_decode_enabled =
      prefs.pepper_accelerated_video_decode_enabled;
  return ppapi_prefs;
}

}  // namespace content
