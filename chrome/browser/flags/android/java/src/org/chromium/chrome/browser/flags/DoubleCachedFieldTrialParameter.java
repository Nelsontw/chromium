// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A double-type {@link CachedFieldTrialParameter}.
 */
public class DoubleCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private double mDefaultValue;

    public DoubleCachedFieldTrialParameter(
            String featureName, String variationName, double defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.DOUBLE, null);
        mDefaultValue = defaultValue;
    }

    public double getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        double value = ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                getFeatureName(), getParameterName(), getDefaultValue());
        SharedPreferencesManager.getInstance().writeDouble(getSharedPreferenceKey(), value);
    }
}
