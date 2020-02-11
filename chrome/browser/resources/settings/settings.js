// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './controls/controlled_button.m.js';
import './controls/controlled_radio_button.m.js';
import './controls/extension_controlled_indicator.m.js';
import './controls/settings_checkbox.m.js';
import './controls/settings_dropdown_menu.m.js';
import './controls/settings_slider.m.js';
import './controls/settings_textarea.m.js';
import './controls/settings_toggle_button.m.js';
import './on_startup_page/on_startup_page.m.js';
import './on_startup_page/startup_urls_page.m.js';
import './site_favicon.m.js';
import './search_engines_page/omnibox_extension_entry.m.js';
import './search_engines_page/search_engine_dialog.m.js';
import './search_engines_page/search_engine_entry.m.js';
import './search_engines_page/search_engines_page.m.js';
import './search_page/search_page.m.js';
import './settings_menu/settings_menu.m.js';
import './settings_page/settings_subpage.m.js';
import './settings_page/settings_animated_pages.m.js';

// <if expr="not chromeos">
import './default_browser_page/default_browser_page.m.js';
// </if>

// <if expr="not chromeos">
export {DefaultBrowserBrowserProxyImpl} from './default_browser_page/default_browser_browser_proxy.m.js';
// </if>

export {ExtensionControlBrowserProxyImpl} from './extension_control_browser_proxy.m.js';
export {OnStartupBrowserProxy, OnStartupBrowserProxyImpl} from './on_startup_page/on_startup_browser_proxy.m.js';
export {EDIT_STARTUP_URL_EVENT} from './on_startup_page/startup_url_entry.m.js';
export {StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './on_startup_page/startup_urls_page_browser_proxy.m.js';
export {pageVisibility} from './page_visibility.m.js';
export {prefToString, stringToPrefValue} from './prefs/pref_util.m.js';
export {routes} from './route.m.js';
export {Route, Router} from './router.m.js';
export {SearchEnginesBrowserProxyImpl} from './search_engines_page/search_engines_browser_proxy.m.js';
