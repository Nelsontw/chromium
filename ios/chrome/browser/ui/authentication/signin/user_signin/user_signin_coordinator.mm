
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface UserSigninCoordinator () <UnifiedConsentCoordinatorDelegate,
                                     UserSigninViewControllerDelegate,
                                     UserSigninMediatorDelegate>

// Coordinator that handles the user consent before the user signs in.
@property(nonatomic, strong)
    UnifiedConsentCoordinator* unifiedConsentCoordinator;
// Coordinator that handles adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
// View controller that handles the sign-in UI.
@property(nonatomic, strong) UserSigninViewController* viewController;
// Mediator that handles the sign-in authentication state.
@property(nonatomic, strong) UserSigninMediator* mediator;
// Suggested identity shown at sign-in.
@property(nonatomic, strong) ChromeIdentity* defaultIdentity;
// View where the sign-in button was displayed.
@property(nonatomic, assign) AccessPoint accessPoint;
// Promo button used to trigger the sign-in.
@property(nonatomic, assign) PromoAction promoAction;

@end

@implementation UserSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(ChromeIdentity*)identity
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _defaultIdentity = identity;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)start {
  [super start];
  self.viewController = [[UserSigninViewController alloc] init];
  self.viewController.delegate = self;

  self.mediator = [[UserSigninMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::
                                        GetForBrowserState(self.browserState)
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        self.browserState)
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        self.browserState)
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(self.browserState)
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        self.browserState)];
  self.mediator.delegate = self;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  self.unifiedConsentCoordinator.delegate = self;

  // Set UnifiedConsentCoordinator properties.
  self.unifiedConsentCoordinator.selectedIdentity = self.defaultIdentity;
  self.unifiedConsentCoordinator.autoOpenIdentityPicker =
      self.promoAction == PromoAction::PROMO_ACTION_NOT_DEFAULT;

  [self.unifiedConsentCoordinator start];

  // Display UnifiedConsentViewController within the host.
  self.viewController.unifiedConsentViewController =
      self.unifiedConsentCoordinator.viewController;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  if (self.addAccountSigninCoordinator) {
    // |self.addAccountSigninCoordinator| needs to be interupted before
    // interrupting |self.viewController|.
    // The add account view should not be dismissed since the
    // |self.viewController| will take care of that according to |action|.
    __weak UserSigninCoordinator* weakSelf = self;
    [self.addAccountSigninCoordinator
        interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
                 completion:^{
                   // |self.addAccountSigninCoordinator.signinCompletion|
                   // is expected to be called before this block.
                   // Therefore |weakSelf.addAccountSigninCoordinator| is
                   // expected to be nil.
                   DCHECK(!weakSelf.addAccountSigninCoordinator);
                   [weakSelf interruptUserSigninUIWithAction:action
                                                  completion:completion];
                 }];
    return;
  }
  [self interruptUserSigninUIWithAction:action completion:completion];
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  // TODO(crbug.com/971989): Needs implementation.
}

- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self.viewController markUnifiedConsentScreenReachedBottom];
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self userSigninViewControllerDidTapOnAddAccount];
}

- (void)unifiedConsentCoordinatorNeedPrimaryButtonUpdate:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self userSigninMediatorNeedPrimaryButtonUpdate];
}

#pragma mark - UserSigninViewControllerDelegate

- (BOOL)unifiedConsentCoordinatorHasIdentity {
  return self.unifiedConsentCoordinator.selectedIdentity != nil;
}

- (void)userSigninViewControllerDidTapOnAddAccount {
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:self.accessPoint];

  __weak UserSigninCoordinator* weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult, ChromeIdentity* identity) {
        if (signinResult == SigninCoordinatorResultSuccess) {
          weakSelf.unifiedConsentCoordinator.selectedIdentity = identity;
        }
        [weakSelf.addAccountSigninCoordinator stop];
        weakSelf.addAccountSigninCoordinator = nil;
      };
  [self.addAccountSigninCoordinator start];
}

- (void)userSigninViewControllerDidScrollOnUnifiedConsent {
  [self.unifiedConsentCoordinator scrollToBottom];
}

- (void)userSigninViewControllerDidTapOnSkipSignin {
  [self.mediator cancelSignin];
}

- (void)userSigninViewControllerDidTapOnSignin {
  DCHECK(self.unifiedConsentCoordinator.selectedIdentity);
  // TODO(crbug.com/971989): Pass ShouldClearDataOption from main controller.
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:self.unifiedConsentCoordinator.selectedIdentity
               shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
              postSignInAction:POST_SIGNIN_ACTION_NONE
      presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);

  [self.mediator
      authenticateWithIdentity:self.unifiedConsentCoordinator.selectedIdentity
            authenticationFlow:authenticationFlow];
}

#pragma mark - UserSigninMediatorDelegate

- (void)userSigninMediatorDidTapResetSettingLink {
  [self.unifiedConsentCoordinator resetSettingLinkTapped];
}

- (BOOL)userSigninMediatorGetSettingsLinkWasTapped {
  return self.unifiedConsentCoordinator.settingsLinkWasTapped;
}

- (int)userSigninMediatorGetConsentConfirmationId {
  if (self.userSigninMediatorGetSettingsLinkWasTapped) {
    base::RecordAction(
        base::UserMetricsAction("Signin_Signin_WithAdvancedSyncSettings"));
    return self.unifiedConsentCoordinator.openSettingsStringId;
  } else {
    base::RecordAction(
        base::UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
    return self.viewController.acceptSigninButtonStringId;
  }
}

- (const std::vector<int>&)userSigninMediatorGetConsentStringIds {
  return self.unifiedConsentCoordinator.consentStringIds;
}

- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult {
  [self recordSigninMetricsWithResult:signinResult];

  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock completion = ^void() {
    [weakSelf
        runCompletionCallbackWithSigninResult:signinResult
                                     identity:weakSelf.unifiedConsentCoordinator
                                                  .selectedIdentity];
  };

  [self.viewController dismissViewControllerAnimated:YES completion:completion];

  self.unifiedConsentCoordinator.delegate = nil;
  [self.unifiedConsentCoordinator stop];
  self.unifiedConsentCoordinator = nil;
}

- (void)userSigninMediatorNeedPrimaryButtonUpdate {
  [self.viewController updatePrimaryButtonStyle];
}

#pragma mark - Private

// Records the metrics when the sign-in is finished.
- (void)recordSigninMetricsWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      signin_metrics::LogSigninAccessPointCompleted(self.accessPoint,
                                                    self.promoAction);
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      // TODO(crbug.com/951145): Add metric when the sign-in has been
      // interrupted.
      break;
    }
  }
}

// Interrupts the sign-in when |self.viewController| is presented, by dismissing
// it if needed (according to |action|). Then |completion| is called.
// This method should not be called if |self.addAccountSigninCoordinator| has
// not been stopped before.
- (void)interruptUserSigninUIWithAction:(SigninCoordinatorInterruptAction)action
                             completion:(ProceduralBlock)completion {
  DCHECK(!self.addAccountSigninCoordinator);
  [self.mediator cancelAndDismissAuthenticationFlow];
  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock runCompletionCallback = ^{
    [weakSelf
        runCompletionCallbackWithSigninResult:SigninCoordinatorResultInterrupted
                                     identity:self.unifiedConsentCoordinator
                                                  .selectedIdentity];
    if (completion) {
      completion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss: {
      runCompletionCallback();
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      [self.viewController dismissViewControllerAnimated:YES
                                              completion:runCompletionCallback];
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithoutAnimation: {
      [self.viewController dismissViewControllerAnimated:NO
                                              completion:runCompletionCallback];
      break;
    }
  }
}

@end
