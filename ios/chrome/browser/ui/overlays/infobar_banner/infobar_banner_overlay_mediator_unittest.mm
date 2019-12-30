// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"

#import "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_banner_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kBannerAccessibilityLabel = @"a11y label";
NSString* const kButtonText = @"button text";
NSString* const kIconImageName = @"";
const bool kPresentsModal(false);
NSString* const kTitleText = @"title text";
NSString* const kSubtitleText = @"subtitle text";
}

// Test fixture for InfobarBannerOverlayMediator.
class InfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  InfobarBannerOverlayMediatorTest()
      : request_(
            OverlayRequest::CreateWithConfig<InfobarBannerOverlayRequestConfig>(
                kBannerAccessibilityLabel,
                kButtonText,
                kIconImageName,
                kPresentsModal,
                kTitleText,
                kSubtitleText)),
        delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))),
        mediator_([[InfobarBannerOverlayMediator alloc]
            initWithRequest:request_.get()]) {
    mediator_.delegate = delegate_;
  }
  ~InfobarBannerOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

 protected:
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  InfobarBannerOverlayMediator* mediator_ = nil;
};

// Tests that an InfobarBannerOverlayMediator correctly sets up its consumer.
TEST_F(InfobarBannerOverlayMediatorTest, SetUpConsumer) {
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_NSEQ(kBannerAccessibilityLabel, consumer.bannerAccessibilityLabel);
  EXPECT_NSEQ(kButtonText, consumer.buttonText);
  EXPECT_NSEQ(nil, consumer.iconImage);
  EXPECT_EQ(kPresentsModal, consumer.presentsModal);
  EXPECT_NSEQ(kTitleText, consumer.titleText);
  EXPECT_NSEQ(kSubtitleText, consumer.subtitleText);
}

// Tests that an InfobarBannerOverlayMediator correctly dispatches a response
// for confirm button taps before stopping itself.
TEST_F(InfobarBannerOverlayMediatorTest, ConfirmButtonTapped) {
  __block bool confirm_button_tapped = false;
  void (^confirm_button_tapped_callback)(OverlayResponse* response) =
      ^(OverlayResponse* response) {
        confirm_button_tapped = true;
      };
  request_->GetCallbackManager()
      ->AddDispatchCallback<InfobarBannerMainActionResponse>(
          base::BindRepeating(confirm_button_tapped_callback));
  ASSERT_FALSE(confirm_button_tapped);

  // Notify the mediator of the button tap via its InfobarBannerDelegate
  // implementation and verify that the confirm button callback was executed and
  // that the mediator's delegate was instructed to stop.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ bannerInfobarButtonWasPressed:nil];
  EXPECT_TRUE(confirm_button_tapped);
}

// Tests that an InfobarBannerOverlayMediator correctly dispatches a response
// for modal button taps before stopping itself.
TEST_F(InfobarBannerOverlayMediatorTest, ModalButtonTapped) {
  __block bool modal_button_tapped = false;
  void (^modal_button_tapped_callback)(OverlayResponse* response) =
      ^(OverlayResponse* response) {
        modal_button_tapped = true;
      };
  request_->GetCallbackManager()
      ->AddDispatchCallback<InfobarBannerShowModalResponse>(
          base::BindRepeating(modal_button_tapped_callback));
  ASSERT_FALSE(modal_button_tapped);

  // Notify the mediator of the button tap via its InfobarBannerDelegate
  // implementation and verify that the modal button callback was executed and
  // that the mediator's delegate was instructed to stop.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ presentInfobarModalFromBanner];
  EXPECT_TRUE(modal_button_tapped);
}

// Tests that an InfobarBannerOverlayMediator correctly sets the completion
// response for user-initiated dismissals triggered by the banner UI.
TEST_F(InfobarBannerOverlayMediatorTest, UserInitiatedDismissal) {
  __block bool user_initiated = false;
  void (^completion_callback)(OverlayResponse* response) =
      ^(OverlayResponse* response) {
        user_initiated = true;
      };
  request_->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(completion_callback));
  ASSERT_FALSE(user_initiated);

  // Notify the mediator of the dismissal via its InfobarBannerDelegate
  // implementation and verify that the completion callback was executed with
  // the correct info and that the mediator's delegate was instructed to stop.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarBannerForUserInteraction:YES];
  request_ = nullptr;
  EXPECT_TRUE(user_initiated);
}
