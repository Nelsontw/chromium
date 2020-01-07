// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/action_tracker.h"

#include <tuple>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

bool IsMainFrameNavigationRequest(const WebRequestInfo& request_info) {
  return request_info.is_navigation_request &&
         request_info.type == content::ResourceType::kMainFrame;
}

}  // namespace

ActionTracker::ActionTracker(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_prefs_ = ExtensionPrefs::Get(browser_context_);
}

ActionTracker::~ActionTracker() {
  // Sanity check that only rules corresponding to the unknown tab ID remain.
  DCHECK(std::all_of(
      rules_tracked_.begin(), rules_tracked_.end(),
      [](const std::pair<const ExtensionTabIdKey, TrackedInfo>& key_value) {
        return key_value.first.secondary_id == extension_misc::kUnknownTabId;
      }));

  DCHECK(pending_navigation_actions_.empty());
}

void ActionTracker::OnRuleMatched(const RequestAction& request_action,
                                  const WebRequestInfo& request_info) {
  DispatchOnRuleMatchedDebugIfNeeded(request_action,
                                     CreateRequestDetails(request_info));

  const ExtensionId& extension_id = request_action.extension_id;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  DCHECK(extension);

  const bool has_feedback_permission =
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kDeclarativeNetRequestFeedback);

  auto add_matched_rule_if_needed = [has_feedback_permission](
                                        TrackedInfo* tracked_info,
                                        const RequestAction& request_action) {
    // Only record a matched rule if |extension| has the feedback permission.
    if (!has_feedback_permission)
      return;

    tracked_info->matched_rules.emplace_back(request_action.rule_id,
                                             request_action.source_type);
  };

  const int tab_id = request_info.frame_data.tab_id;

  // Allow rules do not result in any action being taken on the request, and
  // badge text should only be set for valid tab IDs.
  const bool increment_action_count =
      tab_id != extension_misc::kUnknownTabId &&
      request_action.type != RequestAction::Type::ALLOW;

  if (IsMainFrameNavigationRequest(request_info)) {
    DCHECK(request_info.navigation_id);
    TrackedInfo& pending_info = pending_navigation_actions_[{
        extension_id, *request_info.navigation_id}];
    add_matched_rule_if_needed(&pending_info, request_action);

    if (increment_action_count)
      pending_info.action_count++;
    return;
  }

  TrackedInfo& tracked_info = rules_tracked_[{extension_id, tab_id}];
  add_matched_rule_if_needed(&tracked_info, request_action);

  if (!increment_action_count)
    return;

  size_t action_count = ++tracked_info.action_count;
  if (!extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id))
    return;

  DCHECK(ExtensionsAPIClient::Get());
  ExtensionsAPIClient::Get()->UpdateActionCount(browser_context_, extension_id,
                                                tab_id, action_count,
                                                false /* clear_badge_text */);
}

void ActionTracker::OnPreferenceEnabled(const ExtensionId& extension_id) const {
  DCHECK(extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id));

  for (auto it = rules_tracked_.begin(); it != rules_tracked_.end(); ++it) {
    const ExtensionTabIdKey& key = it->first;
    const TrackedInfo& value = it->second;

    if (key.extension_id != extension_id ||
        key.secondary_id == extension_misc::kUnknownTabId) {
      continue;
    }

    ExtensionsAPIClient::Get()->UpdateActionCount(
        browser_context_, extension_id, key.secondary_id /* tab_id */,
        value.action_count, true /* clear_badge_text */);
  }
}

void ActionTracker::ClearExtensionData(const ExtensionId& extension_id) {
  auto compare_by_extension_id = [&extension_id](const auto& it) {
    return it.first.extension_id == extension_id;
  };

  base::EraseIf(rules_tracked_, compare_by_extension_id);
  base::EraseIf(pending_navigation_actions_, compare_by_extension_id);
}

void ActionTracker::ClearTabData(int tab_id) {
  TransferRulesOnTabInvalid(tab_id);

  auto compare_by_tab_id =
      [&tab_id](const std::pair<const ExtensionTabIdKey, TrackedInfo>& it) {
        bool matches_tab_id = it.first.secondary_id == tab_id;
        DCHECK(!matches_tab_id || it.second.matched_rules.empty());

        return matches_tab_id;
      };

  base::EraseIf(rules_tracked_, compare_by_tab_id);
}

void ActionTracker::ClearPendingNavigation(int64_t navigation_id) {
  auto compare_by_navigation_id =
      [navigation_id](
          const std::pair<const ExtensionNavigationIdKey, TrackedInfo>& it) {
        return it.first.secondary_id == navigation_id;
      };

  base::EraseIf(pending_navigation_actions_, compare_by_navigation_id);
}

void ActionTracker::ResetTrackedInfoForTab(int tab_id, int64_t navigation_id) {
  DCHECK_NE(tab_id, extension_misc::kUnknownTabId);

  // Since the tab ID for a tracked rule corresponds to the current active
  // document, existing rules for this |tab_id| would point to an inactive
  // document. Therefore the tab IDs for these tracked rules should be set to
  // the unknown tab ID.
  TransferRulesOnTabInvalid(tab_id);

  RulesMonitorService* rules_monitor_service =
      RulesMonitorService::Get(browser_context_);

  DCHECK(rules_monitor_service);

  // Use |extensions_with_rulesets| because there may not be an entry for some
  // extensions in |rules_tracked_|. However, the action count should still be
  // surfaced for those extensions if the preference is enabled.
  for (const auto& extension_id :
       rules_monitor_service->extensions_with_rulesets()) {
    ExtensionNavigationIdKey navigation_key(extension_id, navigation_id);

    TrackedInfo& tab_info = rules_tracked_[{extension_id, tab_id}];
    DCHECK(tab_info.matched_rules.empty());

    auto iter = pending_navigation_actions_.find({extension_id, navigation_id});
    if (iter != pending_navigation_actions_.end()) {
      tab_info = std::move(iter->second);
    } else {
      // Reset the count and matched rules for the new document.
      tab_info = TrackedInfo();
    }

    if (extension_prefs_->GetDNRUseActionCountAsBadgeText(extension_id)) {
      DCHECK(ExtensionsAPIClient::Get());
      ExtensionsAPIClient::Get()->UpdateActionCount(
          browser_context_, extension_id, tab_id, tab_info.action_count,
          false /* clear_badge_text */);
    }
  }

  // Double check to make sure the pending counts for |navigation_id| are really
  // cleared from |pending_navigation_actions_|.
  ClearPendingNavigation(navigation_id);
}

std::vector<dnr_api::MatchedRuleInfo> ActionTracker::GetMatchedRules(
    const ExtensionId& extension_id,
    base::Optional<int> tab_id) const {
  std::vector<dnr_api::MatchedRuleInfo> matched_rules;

  auto add_to_matched_rules = [this, &matched_rules](
                                  const std::list<TrackedRule>& tracked_rules,
                                  int tab_id) {
    for (const TrackedRule& tracked_rule : tracked_rules)
      matched_rules.push_back(CreateMatchedRuleInfo(tracked_rule, tab_id));
  };

  if (tab_id.has_value()) {
    ExtensionTabIdKey key(extension_id, *tab_id);

    auto tracked_info = rules_tracked_.find(key);
    if (tracked_info == rules_tracked_.end())
      return matched_rules;

    add_to_matched_rules(tracked_info->second.matched_rules, *tab_id);
    return matched_rules;
  }

  // Iterate over all tabs if |tab_id| is not specified.
  for (auto it = rules_tracked_.begin(); it != rules_tracked_.end(); ++it) {
    if (it->first.extension_id != extension_id)
      continue;

    add_to_matched_rules(it->second.matched_rules, it->first.secondary_id);
  }

  return matched_rules;
}

int ActionTracker::GetMatchedRuleCountForTest(const ExtensionId& extension_id,
                                              int tab_id) {
  ExtensionTabIdKey key(extension_id, tab_id);
  auto tracked_info = rules_tracked_.find(key);

  return tracked_info == rules_tracked_.end()
             ? 0
             : tracked_info->second.matched_rules.size();
}

int ActionTracker::GetPendingRuleCountForTest(const ExtensionId& extension_id,
                                              int64_t navigation_id) {
  ExtensionNavigationIdKey key(extension_id, navigation_id);
  auto tracked_info = pending_navigation_actions_.find(key);

  return tracked_info == pending_navigation_actions_.end()
             ? 0
             : tracked_info->second.matched_rules.size();
}

template <typename T>
ActionTracker::TrackedInfoContextKey<T>::TrackedInfoContextKey(
    ExtensionId extension_id,
    T secondary_id)
    : extension_id(std::move(extension_id)), secondary_id(secondary_id) {}

template <typename T>
ActionTracker::TrackedInfoContextKey<T>::TrackedInfoContextKey(
    ActionTracker::TrackedInfoContextKey<T>&&) = default;

template <typename T>
ActionTracker::TrackedInfoContextKey<T>&
ActionTracker::TrackedInfoContextKey<T>::operator=(
    ActionTracker::TrackedInfoContextKey<T>&&) = default;

template <typename T>
bool ActionTracker::TrackedInfoContextKey<T>::operator<(
    const TrackedInfoContextKey<T>& other) const {
  return std::tie(secondary_id, extension_id) <
         std::tie(other.secondary_id, other.extension_id);
}

ActionTracker::TrackedRule::TrackedRule(
    int rule_id,
    api::declarative_net_request::SourceType source_type)
    : rule_id(rule_id), source_type(source_type) {}

ActionTracker::TrackedInfo::TrackedInfo() = default;
ActionTracker::TrackedInfo::~TrackedInfo() = default;
ActionTracker::TrackedInfo::TrackedInfo(ActionTracker::TrackedInfo&&) = default;
ActionTracker::TrackedInfo& ActionTracker::TrackedInfo::operator=(
    ActionTracker::TrackedInfo&&) = default;

void ActionTracker::DispatchOnRuleMatchedDebugIfNeeded(
    const RequestAction& request_action,
    dnr_api::RequestDetails request_details) {
  const ExtensionId& extension_id = request_action.extension_id;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  DCHECK(extension);

  // Do not dispatch an event if the extension has not registered a listener.
  // |event_router| can be null for some unit tests.
  const EventRouter* event_router = EventRouter::Get(browser_context_);
  const bool has_extension_registered_for_event =
      event_router &&
      event_router->ExtensionHasEventListener(
          extension_id, dnr_api::OnRuleMatchedDebug::kEventName);
  if (!has_extension_registered_for_event)
    return;

  DCHECK(Manifest::IsUnpackedLocation(extension->location()));

  // Create and dispatch the OnRuleMatchedDebug event.
  dnr_api::MatchedRule matched_rule;
  matched_rule.rule_id = request_action.rule_id;
  matched_rule.source_type = request_action.source_type;

  dnr_api::MatchedRuleInfoDebug matched_rule_info_debug;
  matched_rule_info_debug.rule = std::move(matched_rule);
  matched_rule_info_debug.request = std::move(request_details);

  auto args = std::make_unique<base::ListValue>();
  args->Append(matched_rule_info_debug.ToValue());

  auto event = std::make_unique<Event>(
      events::DECLARATIVE_NET_REQUEST_ON_RULE_MATCHED_DEBUG,
      dnr_api::OnRuleMatchedDebug::kEventName, std::move(args));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ActionTracker::TransferRulesOnTabInvalid(int tab_id) {
  DCHECK_NE(tab_id, extension_misc::kUnknownTabId);

  for (auto it = rules_tracked_.begin(); it != rules_tracked_.end(); ++it) {
    const ExtensionTabIdKey& key = it->first;
    if (key.secondary_id != tab_id)
      continue;

    TrackedInfo& unknown_tab_info =
        rules_tracked_[{key.extension_id, extension_misc::kUnknownTabId}];

    // Transfer matched rules for this extension and |tab_id| into the matched
    // rule list for this extension and the unknown tab ID.
    TrackedInfo& value = it->second;
    unknown_tab_info.matched_rules.splice(unknown_tab_info.matched_rules.end(),
                                          value.matched_rules);
  }
}

dnr_api::MatchedRuleInfo ActionTracker::CreateMatchedRuleInfo(
    const ActionTracker::TrackedRule& tracked_rule,
    int tab_id) const {
  dnr_api::MatchedRule matched_rule;
  matched_rule.rule_id = tracked_rule.rule_id;
  matched_rule.source_type = tracked_rule.source_type;

  dnr_api::MatchedRuleInfo matched_rule_info;
  matched_rule_info.rule = std::move(matched_rule);
  matched_rule_info.tab_id = tab_id;

  // TODO(crbug.com/983761): Populate timestamp for |matched_rule_info|.
  return matched_rule_info;
}

}  // namespace declarative_net_request
}  // namespace extensions
