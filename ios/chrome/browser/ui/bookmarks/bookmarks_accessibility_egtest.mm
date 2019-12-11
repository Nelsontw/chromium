// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_utils.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;

// Bookmark accessibility tests for Chrome.
@interface BookmarksAccessibilityTestCase : ChromeTestCase
@end

@implementation BookmarksAccessibilityTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];

  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  [BookmarkPathCache
      clearBookmarkTopMostRowCacheWithPrefService:browser_state->GetPrefs()];
}

#pragma mark - BookmarksAccessibilityTestCase Tests

// Tests that all elements on the bookmarks landing page are accessible.
- (void)testAccessibilityOnBookmarksLandingPage {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on mobile bookmarks are accessible.
- (void)testAccessibilityOnMobileBookmarks {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks folder Edit page are accessible.
- (void)testAccessibilityOnBookmarksFolderEditPage {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Edit page are accessible.
- (void)testAccessibilityOnBookmarksEditPage {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];
  // TODO(crbug.com/976930): to support EG2 need to use [ChromeEarlGrey
  // verifyAccessibilityForCurrentScreen]
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move page are accessible.
- (void)testAccessibilityOnBookmarksMovePage {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // TODO(crbug.com/976930): to support EG2 need to use [ChromeEarlGrey
  // verifyAccessibilityForCurrentScreen]
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move to New Folder page are
// accessible.
- (void)testAccessibilityOnBookmarksMoveToNewFolderPage {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on bookmarks Delete and Undo are accessible.
- (void)testAccessibilityOnBookmarksDeleteUndo {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUtils
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUtils waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that all elements on the bookmarks Select page are accessible.
- (void)testAccessibilityOnBookmarksSelect {
  [BookmarkEarlGreyUtils setupStandardBookmarks];
  [BookmarkEarlGreyUtils openBookmarks];
  [BookmarkEarlGreyUtils openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

@end
