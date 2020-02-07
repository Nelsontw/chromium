// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/test/mock_platform_notification_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

MockPlatformNotificationService* GetMockPlatformNotificationService() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = client->GetPlatformNotificationService(context);
  return static_cast<MockPlatformNotificationService*>(service);
}

WebTestContentIndexProvider* GetWebTestContentIndexProvider() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  return static_cast<WebTestContentIndexProvider*>(
      context->GetContentIndexProvider());
}

ContentIndexContext* GetContentIndexContext(const url::Origin& origin) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      context, origin.GetURL(), /* can_create= */ false);
  return storage_partition->GetContentIndexContext();
}

}  // namespace

// static
void WebTestClientImpl::Create(
    mojo::PendingReceiver<mojom::WebTestClient> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebTestClientImpl>(),
                              std::move(receiver));
}

void WebTestClientImpl::InspectSecondaryWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnInspectSecondaryWindow();
}

void WebTestClientImpl::TestFinishedInSecondaryRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

void WebTestClientImpl::SimulateWebNotificationClose(const std::string& title,
                                                     bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClose(title, by_user);
}

void WebTestClientImpl::SimulateWebContentIndexDelete(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* provider = GetWebTestContentIndexProvider();

  std::pair<int64_t, url::Origin> registration_data =
      provider->GetRegistrationDataFromId(id);

  auto* context = GetContentIndexContext(registration_data.second);
  context->OnUserDeletedItem(registration_data.first, registration_data.second,
                             id);
}

void WebTestClientImpl::BlockThirdPartyCookies(bool block) {
  BlinkTestController::Get()->OnBlockThirdPartyCookies(block);
}

void WebTestClientImpl::ResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->ResetPermissions();
}

void WebTestClientImpl::SetPermission(const std::string& name,
                                      blink::mojom::PermissionStatus status,
                                      const GURL& origin,
                                      const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::PermissionType type;
  if (name == "midi") {
    type = PermissionType::MIDI;
  } else if (name == "midi-sysex") {
    type = PermissionType::MIDI_SYSEX;
  } else if (name == "push-messaging" || name == "notifications") {
    type = PermissionType::NOTIFICATIONS;
  } else if (name == "geolocation") {
    type = PermissionType::GEOLOCATION;
  } else if (name == "protected-media-identifier") {
    type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (name == "background-sync") {
    type = PermissionType::BACKGROUND_SYNC;
  } else if (name == "accessibility-events") {
    type = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (name == "clipboard-read-write") {
    type = PermissionType::CLIPBOARD_READ_WRITE;
  } else if (name == "clipboard-sanitized-write") {
    type = PermissionType::CLIPBOARD_SANITIZED_WRITE;
  } else if (name == "payment-handler") {
    type = PermissionType::PAYMENT_HANDLER;
  } else if (name == "accelerometer" || name == "gyroscope" ||
             name == "magnetometer" || name == "ambient-light-sensor") {
    type = PermissionType::SENSORS;
  } else if (name == "background-fetch") {
    type = PermissionType::BACKGROUND_FETCH;
  } else if (name == "periodic-background-sync") {
    type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "wake-lock-screen") {
    type = PermissionType::WAKE_LOCK_SCREEN;
  } else if (name == "wake-lock-system") {
    type = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (name == "nfc") {
    type = PermissionType::NFC;
  } else {
    NOTREACHED();
    type = PermissionType::NOTIFICATIONS;
  }

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin);
}

}  // namespace content
