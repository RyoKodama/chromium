// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_WEB_DATABASE_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_WEB_DATABASE_HOST_IMPL_H_

#include "content/common/web_database.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/common/quota/quota_types.h"

namespace content {

class WebDatabaseHostImpl : public content::mojom::WebDatabaseHost {
 public:
  explicit WebDatabaseHostImpl(
      scoped_refptr<storage::DatabaseTracker> db_tracker);
  ~WebDatabaseHostImpl() override;

  static void Create(scoped_refptr<storage::DatabaseTracker> db_tracker,
                     content::mojom::WebDatabaseHostRequest request);

 private:
  // WebDatabaseHost methods.
  void OpenFile(const base::string16& vfs_file_name,
                int32_t desired_flags,
                OpenFileCallback callback) override;

  void DeleteFile(const base::string16& vfs_file_name,
                  bool sync_dir,
                  DeleteFileCallback callback) override;

  void GetFileAttributes(const base::string16& vfs_file_name,
                         GetFileAttributesCallback callback) override;

  void GetFileSize(const base::string16& vfs_file_name,
                   GetFileSizeCallback callback) override;

  void SetFileSize(const base::string16& vfs_file_name,
                   int64_t expected_size,
                   SetFileSizeCallback callback) override;

  void GetSpaceAvailable(const url::Origin& origin,
                         GetSpaceAvailableCallback callback) override;

  void DatabaseDeleteFile(const base::string16& vfs_file_name,
                          bool sync_dir,
                          DeleteFileCallback callback,
                          int reschedule_count);

  // The database tracker for the current browser context.
  const scoped_refptr<storage::DatabaseTracker> db_tracker_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_WEB_DATABASE_HOST_IMPL_H_
