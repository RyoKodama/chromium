// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/VideoFrameResourceProvider.h"

#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace blink {

VideoFrameResourceProvider::VideoFrameResourceProvider() = default;

void VideoFrameResourceProvider::AppendQuads(viz::RenderPass& render_pass) {
  gfx::Rect rect(0, 0, 10000, 10000);
  gfx::Rect visible_rect(0, 0, 10000, 10000);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  viz::SharedQuadState* shared_state =
      render_pass.CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), rect, rect, rect, is_clipped,
                       are_contents_opaque, 1, SkBlendMode::kSrcOver, 0);
  viz::SolidColorDrawQuad* solid_color_quad =
      render_pass.CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  // Fluxuate colors for placeholder testing.
  static int r = 0;
  static int g = 0;
  static int b = 0;
  r++;
  g += 2;
  b += 3;
  solid_color_quad->SetNew(shared_state, rect, visible_rect,
                           SkColorSetRGB(r % 255, g % 255, b % 255), false);
}

}  // namespace blink
