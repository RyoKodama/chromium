// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextDecorationOffsetBase_h
#define TextDecorationOffsetBase_h

#include "core/CoreExport.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/FontBaseline.h"

namespace blink {

class ComputedStyle;
enum class LineVerticalPositionType;
enum class ResolvedUnderlinePosition;
class FontMetrics;

class CORE_EXPORT TextDecorationOffsetBase {
  STACK_ALLOCATED();

 public:
  TextDecorationOffsetBase(const ComputedStyle& style) : style_(style) {}
  ~TextDecorationOffsetBase() {}

  virtual int ComputeUnderlineOffsetForUnder(float text_decoration_thickness,
                                             LineVerticalPositionType) = 0;

  int ComputeUnderlineOffsetForRoman(const FontMetrics&,
                                     float text_decoration_thickness);

  int ComputeUnderlineOffset(ResolvedUnderlinePosition,
                             const FontMetrics&,
                             float text_decoration_thickness);

 protected:
  const ComputedStyle& style_;
};

}  // namespace blink

#endif  // TextDecorationOffsetBase_h
