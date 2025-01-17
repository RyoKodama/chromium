// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_database_host_impl.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/database/vfs_backend.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/quota/quota_status_code.h"
#include "third_party/sqlite/sqlite3.h"

using storage::DatabaseUtil;
using storage::VfsBackend;
using storage::QuotaManager;
using storage::QuotaStatusCode;

namespace content {
namespace {
// The number of times to attempt to delete the SQLite database, if there is
// an error.
const int kNumDeleteRetries = 2;
// The delay between each retry to delete the SQLite database.
const int kDelayDeleteRetryMs = 100;

bool IsOriginValid(const url::Origin& origin) {
  return !origin.unique();
}

}  // namespace

WebDatabaseHostImpl::WebDatabaseHostImpl(
    scoped_refptr<storage::DatabaseTracker> db_tracker)
    : db_tracker_(std::move(db_tracker)) {
  DCHECK(db_tracker_);
}

WebDatabaseHostImpl::~WebDatabaseHostImpl() = default;

void WebDatabaseHostImpl::Create(
    scoped_refptr<storage::DatabaseTracker> db_tracker,
    content::mojom::WebDatabaseHostRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<WebDatabaseHostImpl>(std::move(db_tracker)),
      std::move(request));
}

void WebDatabaseHostImpl::OpenFile(const base::string16& vfs_file_name,
                                   int32_t desired_flags,
                                   OpenFileCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  base::File file;
  const base::File* tracked_file = nullptr;
  std::string origin_identifier;
  base::string16 database_name;

  // When in incognito mode, we want to make sure that all DB files are
  // removed when the incognito browser context goes away, so we add the
  // SQLITE_OPEN_DELETEONCLOSE flag when opening all files, and keep
  // open handles to them in the database tracker to make sure they're
  // around for as long as needed.
  if (vfs_file_name.empty()) {
    file = VfsBackend::OpenTempFileInDirectory(db_tracker_->DatabaseDirectory(),
                                               desired_flags);
  } else if (DatabaseUtil::CrackVfsFileName(vfs_file_name, &origin_identifier,
                                            &database_name, nullptr) &&
             !db_tracker_->IsDatabaseScheduledForDeletion(origin_identifier,
                                                          database_name)) {
    base::FilePath db_file = DatabaseUtil::GetFullFilePathForVfsFile(
        db_tracker_.get(), vfs_file_name);
    if (!db_file.empty()) {
      if (db_tracker_->IsIncognitoProfile()) {
        tracked_file = db_tracker_->GetIncognitoFile(vfs_file_name);
        if (!tracked_file) {
          file = VfsBackend::OpenFile(
              db_file, desired_flags | SQLITE_OPEN_DELETEONCLOSE);
          if (!(desired_flags & SQLITE_OPEN_DELETEONCLOSE)) {
            tracked_file =
                db_tracker_->SaveIncognitoFile(vfs_file_name, std::move(file));
          }
        }
      } else {
        file = VfsBackend::OpenFile(db_file, desired_flags);
      }
    }
  }

  base::File result;
  if (file.IsValid()) {
    result = std::move(file);
  } else if (tracked_file) {
    DCHECK(tracked_file->IsValid());
    result = tracked_file->Duplicate();
  }
  std::move(callback).Run(std::move(result));
}

void WebDatabaseHostImpl::DeleteFile(const base::string16& vfs_file_name,
                                     bool sync_dir,
                                     DeleteFileCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  DatabaseDeleteFile(vfs_file_name, sync_dir, std::move(callback),
                     kNumDeleteRetries);
}

void WebDatabaseHostImpl::GetFileAttributes(
    const base::string16& vfs_file_name,
    GetFileAttributesCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  int32_t attributes = -1;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    attributes = VfsBackend::GetFileAttributes(db_file);
  }
  std::move(callback).Run(attributes);
}

void WebDatabaseHostImpl::GetFileSize(const base::string16& vfs_file_name,
                                      GetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  int64_t size = 0LL;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    size = VfsBackend::GetFileSize(db_file);
  }
  std::move(callback).Run(size);
}

void WebDatabaseHostImpl::SetFileSize(const base::string16& vfs_file_name,
                                      int64_t expected_size,
                                      SetFileSizeCallback callback) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  bool success = false;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    success = VfsBackend::SetFileSize(db_file, expected_size);
  }
  std::move(callback).Run(success);
}

void WebDatabaseHostImpl::GetSpaceAvailable(
    const url::Origin& origin,
    GetSpaceAvailableCallback callback) {
  // QuotaManager is only available on the IO thread.
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());
  DCHECK(db_tracker_->quota_manager_proxy());

  if (!IsOriginValid(origin)) {
    mojo::ReportBadMessage("Invalid Origin.");
    std::move(callback).Run(0);
    return;
  }

  db_tracker_->quota_manager_proxy()->GetUsageAndQuota(
      db_tracker_->task_runner(), origin.GetURL(),
      storage::kStorageTypeTemporary,
      base::Bind(
          [](GetSpaceAvailableCallback callback,
             storage::QuotaStatusCode status, int64_t usage, int64_t quota) {
            int64_t available = 0;
            if ((status == storage::kQuotaStatusOk) && (usage < quota)) {
              available = quota - usage;
            }
            std::move(callback).Run(available);
          },
          base::Passed(std::move(callback))));
}

void WebDatabaseHostImpl::DatabaseDeleteFile(
    const base::string16& vfs_file_name,
    bool sync_dir,
    DeleteFileCallback callback,
    int reschedule_count) {
  DCHECK(db_tracker_->task_runner()->RunsTasksInCurrentSequence());

  // Return an error if the file name is invalid or if the file could not
  // be deleted after kNumDeleteRetries attempts.
  int error_code = SQLITE_IOERR_DELETE;
  base::FilePath db_file =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_.get(), vfs_file_name);
  if (!db_file.empty()) {
    // In order to delete a journal file in incognito mode, we only need to
    // close the open handle to it that's stored in the database tracker.
    if (db_tracker_->IsIncognitoProfile()) {
      const base::string16 wal_suffix(base::ASCIIToUTF16("-wal"));
      base::string16 sqlite_suffix;

      // WAL files can be deleted without having previously been opened.
      if (!db_tracker_->HasSavedIncognitoFileHandle(vfs_file_name) &&
          DatabaseUtil::CrackVfsFileName(vfs_file_name, nullptr, nullptr,
                                         &sqlite_suffix) &&
          sqlite_suffix == wal_suffix) {
        error_code = SQLITE_OK;
      } else {
        db_tracker_->CloseIncognitoFileHandle(vfs_file_name);
        error_code = SQLITE_OK;
      }
    } else {
      error_code = VfsBackend::DeleteFile(db_file, sync_dir);
    }

    if ((error_code == SQLITE_IOERR_DELETE) && reschedule_count) {
      // If the file could not be deleted, try again.
      db_tracker_->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&WebDatabaseHostImpl::DatabaseDeleteFile,
                         base::Unretained(this), vfs_file_name, sync_dir,
                         std::move(callback), reschedule_count - 1),
          base::TimeDelta::FromMilliseconds(kDelayDeleteRetryMs));
      return;
    }
  }

  std::move(callback).Run(error_code);
}

}  // namespace content
