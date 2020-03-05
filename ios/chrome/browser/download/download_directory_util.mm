// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/download_directory_util.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Synchronously deletes downloads directory.
void DeleteDownloadsDirectorySync() {
  base::FilePath downloads_directory;
  if (GetDownloadsDirectory(&downloads_directory)) {
    DeleteFile(downloads_directory, /*recursive=*/true);
  }
}
}  // namespace

bool GetDownloadsDirectory(base::FilePath* directory_path) {
  // If downloads manager's flag is enabled, moves the downloads folder to
  // user's Documents.
  if (base::FeatureList::IsEnabled(web::features::kEnablePersistentDownloads)) {
    *directory_path =
        base::mac::NSStringToFilePath([NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSAllDomainsMask, YES) objectAtIndex:0]);
    return true;
  }
  if (!GetTempDir(directory_path)) {
    return false;
  }
  *directory_path = directory_path->Append("downloads");
  return true;
}

void DeleteDownloadsDirectory() {
  // If downloads manager's flag is enabled, keeps downloads folder.
  if (!base::FeatureList::IsEnabled(web::features::kEnablePersistentDownloads))
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&DeleteDownloadsDirectorySync));
}

NSURL* GetFilesAppDownloadsDirectoryUrl() {
  base::FilePath download_dir;
  if (!GetDownloadsDirectory(&download_dir)) {
    return nil;
  }

  return [NSURL
      URLWithString:[NSString stringWithFormat:@"shareddocuments://%s",
                                               download_dir.value().c_str()]];
}
