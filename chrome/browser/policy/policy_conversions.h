// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
#define CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "ui/base/webui/web_ui_util.h"

namespace policy {

class PolicyConversionsClient;

extern const webui::LocalizedString kPolicySources[POLICY_SOURCE_COUNT];

// A convenience class to retrieve all policies values.
class PolicyConversions {
 public:
  // |client| provides embedder-specific policy information and must not be
  // nullptr.
  explicit PolicyConversions(std::unique_ptr<PolicyConversionsClient> client);
  virtual ~PolicyConversions();

  // Set to get policy types as human friendly string instead of enum integer.
  // Policy types includes policy source, policy scope and policy level.
  // Enabled by default.
  PolicyConversions& EnableConvertTypes(bool enabled);
  // Set to get dictionary policy value as JSON string.
  // Disabled by default.
  PolicyConversions& EnableConvertValues(bool enabled);
  // Set to get device local account policies on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceLocalAccountPolicies(bool enabled);
  // Set to get device basic information on ChromeOS.
  // Disabled by default.
  PolicyConversions& EnableDeviceInfo(bool enabled);
  // Set to enable pretty print for all JSON string.
  // Enabled by default.
  PolicyConversions& EnablePrettyPrint(bool enabled);
  // Set to get all user scope policies.
  // Enabled by default.
  PolicyConversions& EnableUserPolicies(bool enabled);

  // Returns the policy data as a base::Value object.
  virtual base::Value ToValue() = 0;

  // Returns the policy data as a JSON string;
  virtual std::string ToJSON();

 protected:
  PolicyConversionsClient* client() { return client_.get(); }

 private:
  std::unique_ptr<PolicyConversionsClient> client_;

  DISALLOW_COPY_AND_ASSIGN(PolicyConversions);
};

class DictionaryPolicyConversions : public PolicyConversions {
 public:
  explicit DictionaryPolicyConversions(
      std::unique_ptr<PolicyConversionsClient> client);
  ~DictionaryPolicyConversions() override;

  base::Value ToValue() override;

 private:
  base::Value GetExtensionPolicies(PolicyDomain policy_domain);

#if defined(OS_CHROMEOS)
  base::Value GetDeviceLocalAccountPolicies();
#endif

  DISALLOW_COPY_AND_ASSIGN(DictionaryPolicyConversions);
};

class ArrayPolicyConversions : public PolicyConversions {
 public:
  explicit ArrayPolicyConversions(
      std::unique_ptr<PolicyConversionsClient> client);
  ~ArrayPolicyConversions() override;

  base::Value ToValue() override;

 private:
  base::Value GetChromePolicies();

  DISALLOW_COPY_AND_ASSIGN(ArrayPolicyConversions);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_H_
