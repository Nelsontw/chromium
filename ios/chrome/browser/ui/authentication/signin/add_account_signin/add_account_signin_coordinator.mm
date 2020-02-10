// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"

#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AddAccountSigninCoordinator () <
    AddAccountSigninMediatorDelegate,
    ChromeIdentityInteractionManagerDelegate>

// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Coordinator that handles the sign-in UI flow.
@property(nonatomic, strong) SigninCoordinator* userSigninCoordinator;

// Mediator that handles sign-in state.
@property(nonatomic, strong) AddAccountSigninMediator* mediator;

// Manager that handles interactions to add identities.
@property(nonatomic, strong)
    ChromeIdentityInteractionManager* identityInteractionManager;

// View where the sign-in button was displayed.
@property(nonatomic, assign) AccessPoint accessPoint;

// Promo button used to trigger the sign-in.
@property(nonatomic, assign) PromoAction promoAction;

// Intent when the user begins a sign-in flow.
@property(nonatomic, assign) SigninIntent signinIntent;

@end

@implementation AddAccountSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction
                              signinIntent:(SigninIntent)signinIntent {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _signinIntent = signinIntent;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.identityInteractionManager =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateChromeIdentityInteractionManager(
              self.browser->GetBrowserState(), self);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);
  self.mediator = [[AddAccountSigninMediator alloc]
      initWithIdentityInteractionManager:self.identityInteractionManager
                             prefService:self.browserState->GetPrefs()
                         identityManager:identityManager];
  self.mediator.delegate = self;
  [self.mediator handleSigninIntent:self.signinIntent
                        accessPoint:self.accessPoint
                        promoAction:self.promoAction];
}

- (void)stop {
  [self.identityInteractionManager cancelAndDismissAnimated:NO];

  [self.alertCoordinator executeCancelHandler];
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;

  [self.userSigninCoordinator stop];
  self.userSigninCoordinator = nil;
}

#pragma mark - ChromeIdentityInteractionManagerDelegate

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  [self.baseViewController presentViewController:viewController
                                        animated:animated
                                      completion:completion];
}

#pragma mark - AddAccountSigninMediatorDelegate

- (void)handleUserConsentForIdentity:(ChromeIdentity*)identity {
  // The UserSigninViewController is presented on top of the currently displayed
  // view controller.
  self.userSigninCoordinator = [SigninCoordinator
      userSigninCoordinatorWithBaseViewController:self.baseViewController
                                                      .presentedViewController
                                          browser:self.browser
                                         identity:identity
                                      accessPoint:self.accessPoint
                                      promoAction:self.promoAction];
  [self.userSigninCoordinator start];
}

- (void)showAlertWithError:(NSError*)error identity:(ChromeIdentity*)identity {
  DCHECK(error);
  __weak AddAccountSigninCoordinator* weakSelf = self;
  ProceduralBlock dismissAction = ^{
    [weakSelf runCompletionCallbackWithSigninResult:
                  SigninCoordinatorResultCanceledByUser
                                           identity:identity];
  };

  self.alertCoordinator =
      ErrorCoordinator(error, dismissAction, self.baseViewController);
  [self.alertCoordinator start];
}

- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                     identity:(ChromeIdentity*)identity {
  // Cleaning up and calling the |signinCompletion| should be done last.
  self.identityInteractionManager = nil;
  self.alertCoordinator = nil;

  if (self.signinCompletion) {
    self.signinCompletion(signinResult, identity);
    self.signinCompletion = nil;
  }
}

@end
