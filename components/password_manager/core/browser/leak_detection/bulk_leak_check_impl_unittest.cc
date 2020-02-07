// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_impl.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class BulkLeakCheckTest : public testing::Test {
 public:
  BulkLeakCheckTest()
      : bulk_check_(
            &delegate_,
            identity_test_env_.identity_manager(),
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}

  MockBulkLeakCheckDelegateInterface& delegate() { return delegate_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  ::testing::StrictMock<MockBulkLeakCheckDelegateInterface> delegate_;
  BulkLeakCheckImpl bulk_check_;
};

TEST_F(BulkLeakCheckTest, Create) {
  EXPECT_CALL(delegate(), OnFinishedCredential).Times(0);
  EXPECT_CALL(delegate(), OnError).Times(0);
  // Destroying |leak_check_| doesn't trigger anything.
}

}  // namespace
}  // namespace password_manager
