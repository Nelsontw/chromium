// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_privacy_page', function() {

  function registerUMALoggingTests() {
    suite('PrivacyPageUMACheck', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {settings.TestMetricsBrowserProxy} */
      let testMetricsBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          privacySettingsRedesignEnabled: false,
        });
      });

      setup(function() {
        testMetricsBrowserProxy = new TestMetricsBrowserProxy();
        settings.MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        const testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        page.prefs = {
          profile: {password_manager_leak_detection: {value: true}},
          signin: {
            allowed_on_next_startup:
                {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
          },
          safebrowsing:
              {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        };
        document.body.appendChild(page);
        Polymer.dom.flush();
      });

      teardown(function() {
        page.remove();
      });

      test('LogAllPrivacyPageClicks', async function() {
        page.$$('#manageCertificates').click();
        let result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_MANAGE_CERTIFICATES,
            result);

        settings.Router.getInstance().navigateTo(settings.routes.PRIVACY);
        testMetricsBrowserProxy.reset();

        page.$$('#doNotTrack').click();
        result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK, result);

        settings.Router.getInstance().navigateTo(settings.routes.PRIVACY);
        testMetricsBrowserProxy.reset();

        page.$$('#canMakePaymentToggle').click();
        result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_PAYMENT_METHOD, result);

        settings.Router.getInstance().navigateTo(settings.routes.PRIVACY);
        testMetricsBrowserProxy.reset();

        page.$$('#site-settings-subpage-trigger').click();
        result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SITE_SETTINGS, result);

        settings.Router.getInstance().navigateTo(settings.routes.PRIVACY);
        testMetricsBrowserProxy.reset();

        page.$$('#safeBrowsingToggle').click();
        result = await testMetricsBrowserProxy.whenCalled(
            'recordSettingsPageHistogram');
        assertEquals(
            settings.SettingsPageInteractions.PRIVACY_SAFE_BROWSING, result);
      });

      test('LogDoNotTrackClick', function() {
        page.$$('#doNotTrack').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_DO_NOT_TRACK,
                  result);
            });
      });

      test('LogSafeBrowsingReportingToggleClick', function() {
        page.$$('#safeBrowsingReportingToggle').click();
        return testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram')
            .then(result => {
              assertEquals(
                  settings.SettingsPageInteractions.PRIVACY_IMPROVE_SECURITY,
                  result);
            });
      });
    });
  }

  function registerNativeCertificateManagerTests() {
    assert(cr.isMac || cr.isWindows);
    suite('NativeCertificateManager', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          privacySettingsRedesignEnabled: false,
        });
      });

      setup(function() {
        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
      });

      teardown(function() {
        page.remove();
      });

      test('NativeCertificateManager', function() {
        page.$$('#manageCertificates').click();
        return testBrowserProxy.whenCalled('showManageSSLCertificates');
      });
    });
  }

  function registerPrivacyPageTests() {
    suite('PrivacyPage', function() {
      /** @type {SettingsPrivacyPageElement} */
      let page;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          privacySettingsRedesignEnabled: false,
        });
      });

      setup(function() {
        const testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        const testSyncBrowserProxy = new TestSyncBrowserProxy();
        settings.SyncBrowserProxyImpl.instance_ = testSyncBrowserProxy;
        PolymerTest.clearBody();

        page = document.createElement('settings-privacy-page');
        page.prefs = {
          profile: {password_manager_leak_detection: {value: true}},
          signin: {
            allowed_on_next_startup:
                {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
          },
          safebrowsing:
              {enabled: {value: true}, scout_reporting_enabled: {value: true}},
        };
        document.body.appendChild(page);
        return testSyncBrowserProxy.whenCalled('getSyncStatus');
      });

      teardown(function() {
        page.remove();
      });

      test('showClearBrowsingDataDialog', function() {
        assertFalse(!!page.$$('settings-clear-browsing-data-dialog'));
        page.$$('#clearBrowsingData').click();
        Polymer.dom.flush();

        const dialog = page.$$('settings-clear-browsing-data-dialog');
        assertTrue(!!dialog);

        // Ensure that the dialog is fully opened before returning from this
        // test, otherwise asynchronous code run in attached() can cause flaky
        // errors.
        return test_util.whenAttributeIs(
            dialog.$$('#clearBrowsingDataDialog'), 'open', '');
      });

      test('safeBrowsingReportingToggle', function() {
        const safeBrowsingToggle = page.$$('#safeBrowsingToggle');
        const safeBrowsingReportingToggle =
            page.$$('#safeBrowsingReportingToggle');
        assertTrue(safeBrowsingToggle.checked);
        assertFalse(safeBrowsingReportingToggle.disabled);
        assertTrue(safeBrowsingReportingToggle.checked);
        safeBrowsingToggle.click();
        Polymer.dom.flush();

        assertFalse(safeBrowsingToggle.checked);
        assertTrue(safeBrowsingReportingToggle.disabled);
        assertFalse(safeBrowsingReportingToggle.checked);
        assertTrue(page.prefs.safebrowsing.scout_reporting_enabled.value);
        safeBrowsingToggle.click();
        Polymer.dom.flush();

        assertTrue(safeBrowsingToggle.checked);
        assertFalse(safeBrowsingReportingToggle.disabled);
        assertTrue(safeBrowsingReportingToggle.checked);
      });

      test('ElementVisibility', async function() {
        await test_util.flushTasks();
        assertFalse(test_util.isChildVisible(page, '#cookiesLinkRow'));
        assertFalse(test_util.isChildVisible(page, '#securityLinkRow'));
        assertFalse(test_util.isChildVisible(page, '#permissionsLinkRow'));

        assertTrue(test_util.isChildVisible(page, '#clearBrowsingData'));
        assertTrue(
            test_util.isChildVisible(page, '#site-settings-subpage-trigger'));
        assertTrue(test_util.isChildVisible(page, '#moreExpansion'));

        page.$$('#moreExpansion').click();

        assertTrue(test_util.isChildVisible(page, '#safeBrowsingToggle'));
        assertTrue(
            test_util.isChildVisible(page, '#passwordsLeakDetectionToggle'));
        assertTrue(
            test_util.isChildVisible(page, '#safeBrowsingReportingToggle'));
        assertTrue(test_util.isChildVisible(page, '#doNotTrack'));
        assertTrue(test_util.isChildVisible(page, '#canMakePaymentToggle'));
        if (loadTimeData.getBoolean('enableSecurityKeysSubpage')) {
          assertTrue(
              test_util.isChildVisible(page, '#security-keys-subpage-trigger'));
        }
      });
    });
  }

  function registerPrivacyPageRedesignTests() {
    suite('PrivacyPageRedesignEnabled', function() {
      /** @type {SettingsPrivacyPageElement} */
      let page;

      suiteSetup(function() {
        loadTimeData.overrideValues({
          privacySettingsRedesignEnabled: true,
        });
      });

      setup(function() {
        PolymerTest.clearBody();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
        return test_util.flushTasks();
      });

      teardown(function() {
        page.remove();
      });

      test('ElementVisibility', function() {
        assertTrue(test_util.isChildVisible(page, '#clearBrowsingData'));
        assertTrue(test_util.isChildVisible(page, '#cookiesLinkRow'));
        assertTrue(test_util.isChildVisible(page, '#securityLinkRow'));
        assertTrue(test_util.isChildVisible(page, '#permissionsLinkRow'));

        ['#site-settings-subpage-trigger',
         '#moreExpansion',
         '#safeBrowsingToggle',
         '#passwordsLeakDetectionToggle',
         '#safeBrowsingToggle',
         '#safeBrowsingReportingToggle',
         '#doNotTrack',
         '#canMakePaymentToggle',
         '#security-keys-subpage-trigger',
        ].forEach(selector => {
          assertFalse(test_util.isChildVisible(page, selector));
        });
      });
    });
  }

  function registerPrivacyPageSoundTests() {
    suite('PrivacyPageSound', function() {
      /** @type {settings.TestPrivacyPageBrowserProxy} */
      let testBrowserProxy;

      /** @type {SettingsPrivacyPageElement} */
      let page;

      function flushAsync() {
        Polymer.dom.flush();
        return new Promise(resolve => {
          page.async(resolve);
        });
      }

      function getToggleElement() {
        return page.$$('settings-animated-pages')
            .queryEffectiveChildren('settings-subpage')
            .queryEffectiveChildren('#block-autoplay-setting');
      }

      suiteSetup(function() {
        loadTimeData.overrideValues({
          privacySettingsRedesignEnabled: false,
        });
      });

      setup(() => {
        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});

        testBrowserProxy = new TestPrivacyPageBrowserProxy();
        settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
        PolymerTest.clearBody();

        settings.Router.getInstance().navigateTo(
            settings.routes.SITE_SETTINGS_SOUND);
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);
        return flushAsync();
      });

      teardown(() => {
        page.remove();
      });

      test('UpdateStatus', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Toggle the pref off.
          cr.webUIListenerCallback(
              'onBlockAutoplayStatusChanged',
              {pref: {value: false}, enabled: true});

          return flushAsync().then(() => {
            // Check that we are off and enabled.
            assertFalse(getToggleElement().hasAttribute('disabled'));
            assertFalse(getToggleElement().hasAttribute('checked'));

            // Disable the autoplay status toggle.
            cr.webUIListenerCallback(
                'onBlockAutoplayStatusChanged',
                {pref: {value: false}, enabled: false});

            return flushAsync().then(() => {
              // Check that we are off and disabled.
              assertTrue(getToggleElement().hasAttribute('disabled'));
              assertFalse(getToggleElement().hasAttribute('checked'));
            });
          });
        });
      });

      test('Hidden', () => {
        assertTrue(
            loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
        assertFalse(getToggleElement().hidden);

        loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});

        page.remove();
        page = document.createElement('settings-privacy-page');
        document.body.appendChild(page);

        return flushAsync().then(() => {
          assertFalse(
              loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
          assertTrue(getToggleElement().hidden);
        });
      });

      test('Click', () => {
        assertTrue(getToggleElement().hasAttribute('disabled'));
        assertFalse(getToggleElement().hasAttribute('checked'));

        cr.webUIListenerCallback(
            'onBlockAutoplayStatusChanged',
            {pref: {value: true}, enabled: true});

        return flushAsync().then(() => {
          // Check that we are on and enabled.
          assertFalse(getToggleElement().hasAttribute('disabled'));
          assertTrue(getToggleElement().hasAttribute('checked'));

          // Click on the toggle and wait for the proxy to be called.
          getToggleElement().click();
          return testBrowserProxy.whenCalled('setBlockAutoplayEnabled')
              .then((enabled) => {
                assertFalse(enabled);
              });
        });
      });
    });
  }

  return {
    registerNativeCertificateManagerTests,
    registerPrivacyPageTests,
    registerPrivacyPageRedesignTests,
    registerPrivacyPageSoundTests,
    registerUMALoggingTests,
  };
});
