// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSecurity = kItemTypeEnumZero,
};

}  // namespace

@implementation PageInfoViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_INFORMATION);
}

#pragma mark - PageInfoConsumer

- (void)pageInfoChanged:(PageInfoDescription*)pageInfoDescription {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  // Create the Security item.
  TableViewDetailIconItem* securityItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSecurity];
  securityItem.text = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);
  securityItem.detailText = pageInfoDescription.pageSecurityStatusDescription;
  securityItem.iconImageName =
      pageInfoDescription.pageSecurityStatusIconImageName;
  securityItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  [self.tableViewModel addItem:securityItem
       toSectionWithIdentifier:SectionIdentifierContent];

  [self.tableView reloadData];
}

@end
