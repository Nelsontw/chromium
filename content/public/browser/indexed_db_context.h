// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INDEXED_DB_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_INDEXED_DB_CONTEXT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

// Represents the per-BrowserContext IndexedDB data.
// Call these methods only via the exposed IDBTaskRunner.
// Refcounted because this class is used throughout the codebase on different
// threads.
// This class is in the process of being removed in lieu of the
// IndexedDBControl mojo interface.
class IndexedDBContext
    : public base::RefCountedDeleteOnSequence<IndexedDBContext> {
 public:
  // Only call the below methods by posting to this IDBTaskRunner.
  virtual base::SequencedTaskRunner* IDBTaskRunner() = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<IndexedDBContext>;
  friend class base::DeleteHelper<IndexedDBContext>;

  IndexedDBContext(scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
      : base::RefCountedDeleteOnSequence<IndexedDBContext>(
            std::move(owning_task_runner)) {}

  virtual ~IndexedDBContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INDEXED_DB_CONTEXT_H_
