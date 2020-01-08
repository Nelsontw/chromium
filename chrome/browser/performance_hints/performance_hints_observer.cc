// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/performance_hints_metadata.pb.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::URLPatternWithWildcards;
using optimization_guide::proto::PerformanceHint;

const base::Feature kPerformanceHintsObserver{
    "PerformanceHintsObserver", base::FEATURE_DISABLED_BY_DEFAULT};

PerformanceHintsObserver::PerformanceHintsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypesAndTargets(
        {optimization_guide::proto::PERFORMANCE_HINTS}, {});
  }
}

PerformanceHintsObserver::~PerformanceHintsObserver() = default;

base::Optional<PerformanceHint> PerformanceHintsObserver::HintForURL(
    const GURL& url) const {
  if (!url.is_valid()) {
    return base::nullopt;
  }

  for (const auto& pattern_hint : hints_) {
    if (pattern_hint.first.Matches(url.spec())) {
      return pattern_hint.second;
    }
  }
  return base::nullopt;
}

void PerformanceHintsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    // Use the same hints if the main frame hasn't changed.
    return;
  }

  // We've navigated to a new page, so clear out any hints from the previous
  // page.
  hints_.clear();

  if (!optimization_guide_decider_) {
    return;
  }
  if (navigation_handle->IsErrorPage()) {
    // Don't provide hints on Chrome error pages.
    return;
  }

  optimization_guide::OptimizationMetadata optimization_metadata;
  OptimizationGuideDecision decision =
      optimization_guide_decider_->CanApplyOptimization(
          navigation_handle, optimization_guide::proto::PERFORMANCE_HINTS,
          &optimization_metadata);

  if (decision != OptimizationGuideDecision::kTrue) {
    // Apply results are counted under
    // OptimizationGuide.ApplyDecision.PerformanceHints.
    return;
  }

  if (optimization_metadata.performance_hints_metadata.performance_hints()
          .size() <= 0) {
    return;
  }

  for (PerformanceHint& hint : *optimization_metadata.performance_hints_metadata
                                    .mutable_performance_hints()) {
    hints_.emplace_back(URLPatternWithWildcards(hint.wildcard_pattern()), hint);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceHintsObserver)
