// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/full_screen_rect.h"

#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/transform.h"

namespace vr {

FullScreenRect::FullScreenRect() = default;
FullScreenRect::~FullScreenRect() = default;

void FullScreenRect::Render(UiElementRenderer* renderer,
                            const gfx::Transform& view_proj_matrix) const {
  gfx::Transform m;
  m.Scale3d(2.0f, 2.0f, 1.0f);
  renderer->DrawGradientQuad(m, edge_color(), center_color(),
                             computed_opacity(), size(), corner_radius());
}

}  // namespace vr
