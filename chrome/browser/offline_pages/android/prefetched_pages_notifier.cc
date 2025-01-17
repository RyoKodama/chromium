// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetched_pages_notifier.h"

#include "base/android/jni_string.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "jni/PrefetchedPagesNotifier_jni.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;

namespace offline_pages {

void ShowPrefetchedContentNotification(const GURL& page_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PrefetchedPagesNotifier_showDebuggingNotification(
      env, base::android::ConvertUTF8ToJavaString(env, page_url.host()));
}

}  // namespace offline_pages
