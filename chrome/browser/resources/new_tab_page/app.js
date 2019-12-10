// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './most_visited.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

class NewTabPageApp extends PolymerElement {
  static get is() {
    return 'new-tab-page-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  get pageHandler_() {
    return BrowserProxy.getInstance().handler;
  }

  /** @private */
  onRestoreDefaultsClick_() {
    getToastManager().hide();
    this.pageHandler_.restoreMostVisitedDefaults();
  }

  /** @private */
  onUndoClick_() {
    getToastManager().hide();
    this.pageHandler_.undoMostVisitedTileAction();
  }
}

customElements.define(NewTabPageApp.is, NewTabPageApp);
