// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_preview_element_info_internal.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Tests CWVPreviewElementInfoTest initialization.
TEST(CWVPreviewElementInfoTest, Initialization) {
  NSURL* const linkURL = [NSURL URLWithString:@"https://chromium.test"];
  CWVPreviewElementInfo* element =
      [[CWVPreviewElementInfo alloc] initWithLinkURL:linkURL];
  EXPECT_NSEQ(linkURL, element.linkURL);
}

}  // namespace ios_web_view
