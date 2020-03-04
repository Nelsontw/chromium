// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/server/mac/server.h"

#import <Foundation/Foundation.h>
#include <xpc/xpc.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/app/app.h"
#import "chrome/updater/configurator.h"
#include "chrome/updater/server/mac/service_delegate.h"
#import "chrome/updater/update_service.h"
#include "chrome/updater/update_service_in_process.h"
#include "chrome/updater/updater_version.h"

namespace updater {

class AppServer : public App {
 public:
  AppServer();

 private:
  ~AppServer() override;
  void Initialize() override;
  void FirstTaskRun() override;

  scoped_refptr<Configurator> config_;
  base::scoped_nsobject<CRUUpdateCheckXPCServiceDelegate> delegate_;
  base::scoped_nsobject<NSXPCListener> listener_;
};

AppServer::AppServer() = default;
AppServer::~AppServer() = default;

void AppServer::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

void AppServer::FirstTaskRun() {
  @autoreleasepool {
    std::string service_name = MAC_BUNDLE_IDENTIFIER_STRING;
    service_name.append(".UpdaterXPCService");
    delegate_.reset([[CRUUpdateCheckXPCServiceDelegate alloc]
        initWithUpdateService:std::make_unique<UpdateServiceInProcess>(
                                  config_)]);

    listener_.reset([[NSXPCListener alloc]
        initWithMachServiceName:base::SysUTF8ToNSString(service_name)]);
    listener_.get().delegate = delegate_.get();

    [listener_ resume];
  }
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServer>();
}

}  // namespace updater
