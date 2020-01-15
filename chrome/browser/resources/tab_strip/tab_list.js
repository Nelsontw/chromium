// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './tab.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';

import {CustomElement} from './custom_element.js';
import {TabElement} from './tab.js';
import {TabGroupElement} from './tab_group.js';
import {TabStripEmbedderProxy} from './tab_strip_embedder_proxy.js';
import {tabStripOptions} from './tab_strip_options.js';
import {TabData, TabsApiProxy} from './tabs_api_proxy.js';

/**
 * The amount of padding to leave between the edge of the screen and the active
 * tab when auto-scrolling. This should leave some room to show the previous or
 * next tab to afford to users that there more tabs if the user scrolls.
 * @const {number}
 */
const SCROLL_PADDING = 32;

/** @type {boolean} */
let scrollAnimationEnabled = true;

/** @param {boolean} enabled */
export function setScrollAnimationEnabledForTesting(enabled) {
  scrollAnimationEnabled = enabled;
}

/**
 * @enum {string}
 */
const LayoutVariable = {
  VIEWPORT_WIDTH: '--tabstrip-viewport-width',
  NEW_TAB_BUTTON_MARGIN: '--tabstrip-new-tab-button-margin',
  NEW_TAB_BUTTON_WIDTH: '--tabstrip-new-tab-button-width',
  TAB_WIDTH: '--tabstrip-tab-thumbnail-width',
};

/**
 * @param {!Element} element
 * @return {boolean}
 */
function isTabElement(element) {
  return element.tagName === 'TABSTRIP-TAB';
}

/**
 * @param {!Element} element
 * @return {boolean}
 */
function isTabGroupElement(element) {
  return element.tagName === 'TABSTRIP-TAB-GROUP';
}

class TabListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /**
     * A chain of promises that the tab list needs to keep track of. The chain
     * is useful in cases when the list needs to wait for all animations to
     * finish in order to get accurate pixels (such as getting the position of a
     * tab) or accurate element counts.
     * @type {!Promise}
     */
    this.animationPromises = Promise.resolve();

    /**
     * The ID of the current animation frame that is in queue to update the
     * scroll position.
     * @private {?number}
     */
    this.currentScrollUpdateFrame_ = null;

    /** @private {!Function} */
    this.documentVisibilityChangeListener_ = () =>
        this.onDocumentVisibilityChange_();

    /**
     * The TabElement that is currently being dragged.
     * @private {!TabElement|undefined}
     */
    this.draggedItem_;

    /** @private @const {!FocusOutlineManager} */
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);

    /**
     * An intersection observer is needed to observe which TabElements are
     * currently in view or close to being in view, which will help determine
     * which thumbnails need to be tracked to stay fresh and which can be
     * untracked until they become visible.
     * @private {!IntersectionObserver}
     */
    this.intersectionObserver_ = new IntersectionObserver(entries => {
      for (const entry of entries) {
        this.tabsApi_.setThumbnailTracked(
            entry.target.tab.id, entry.isIntersecting);
      }
    }, {
      root: this,
      // The horizontal root margin is set to 100% to also track thumbnails that
      // are one standard finger swipe away.
      rootMargin: '0% 100%',
    });

    /** @private {number|undefined} */
    this.activatingTabId_;

    /** @private {number|undefined} Timestamp in ms */
    this.activatingTabIdTimestamp_;

    /** @private {!Element} */
    this.newTabButtonElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#newTabButton'));

    /** @private {!Element} */
    this.pinnedTabsElement_ =
        /** @type {!Element} */ (this.shadowRoot.querySelector('#pinnedTabs'));

    /** @private {!TabStripEmbedderProxy} */
    this.tabStripEmbedderProxy_ = TabStripEmbedderProxy.getInstance();

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxy.getInstance();

    /** @private {!Element} */
    this.unpinnedTabsElement_ =
        /** @type {!Element} */ (
            this.shadowRoot.querySelector('#unpinnedTabs'));

    /** @private {!Function} */
    this.windowBlurListener_ = () => this.onWindowBlur_();

    /** @private {!Function} */
    this.contextMenuListener_ = e => this.onContextMenu_(e);

    addWebUIListener(
        'layout-changed', layout => this.applyCSSDictionary_(layout));
    addWebUIListener('theme-changed', () => this.fetchAndUpdateColors_());
    this.tabStripEmbedderProxy_.observeThemeChanges();

    addWebUIListener(
        'tab-thumbnail-updated', this.tabThumbnailUpdated_.bind(this));

    this.addEventListener(
        'dragstart', (e) => this.onDragStart_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragend', (e) => this.onDragEnd_(/** @type {!DragEvent} */ (e)));
    this.addEventListener(
        'dragover', (e) => this.onDragOver_(/** @type {!DragEvent} */ (e)));

    document.addEventListener('contextmenu', this.contextMenuListener_);
    document.addEventListener(
        'visibilitychange', this.documentVisibilityChangeListener_);
    addWebUIListener(
        'received-keyboard-focus', () => this.onReceivedKeyboardFocus_());
    window.addEventListener('blur', this.windowBlurListener_);

    this.newTabButtonElement_.addEventListener('click', () => {
      this.tabsApi_.createNewTab();
    });

    if (loadTimeData.getBoolean('showDemoOptions')) {
      this.shadowRoot.querySelector('#demoOptions').style.display = 'block';

      const autoCloseCheckbox =
          this.shadowRoot.querySelector('#autoCloseCheckbox');
      autoCloseCheckbox.checked = tabStripOptions.autoCloseEnabled;
      autoCloseCheckbox.addEventListener('change', () => {
        tabStripOptions.autoCloseEnabled = autoCloseCheckbox.checked;
      });
    }
  }

  /**
   * @param {!Promise} promise
   * @private
   */
  addAnimationPromise_(promise) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  /**
   * @param {number} scrollBy
   * @private
   */
  animateScrollPosition_(scrollBy) {
    if (this.currentScrollUpdateFrame_) {
      cancelAnimationFrame(this.currentScrollUpdateFrame_);
      this.currentScrollUpdateFrame_ = null;
    }

    const prevScrollLeft = this.scrollLeft;
    if (!scrollAnimationEnabled || !this.tabStripEmbedderProxy_.isVisible()) {
      // Do not animate if tab strip is not visible.
      this.scrollLeft = prevScrollLeft + scrollBy;
      return;
    }

    const duration = 350;
    let startTime;

    const onAnimationFrame = (currentTime) => {
      const startScroll = this.scrollLeft;
      if (!startTime) {
        startTime = currentTime;
      }

      const elapsedRatio = Math.min(1, (currentTime - startTime) / duration);

      // The elapsed ratio should be decelerated such that the elapsed time
      // of the animation gets less and less further apart as time goes on,
      // giving the effect of an animation that slows down towards the end. When
      // 0ms has passed, the decelerated ratio should be 0. When the full
      // duration has passed, the ratio should be 1.
      const deceleratedRatio =
          1 - (1 - elapsedRatio) / Math.pow(2, 6 * elapsedRatio);

      this.scrollLeft = prevScrollLeft + (scrollBy * deceleratedRatio);

      this.currentScrollUpdateFrame_ =
          deceleratedRatio < 1 ? requestAnimationFrame(onAnimationFrame) : null;
    };
    this.currentScrollUpdateFrame_ = requestAnimationFrame(onAnimationFrame);
  }

  /**
   * @param {!Object<string, string>} dictionary
   * @private
   */
  applyCSSDictionary_(dictionary) {
    for (const [cssVariable, value] of Object.entries(dictionary)) {
      this.style.setProperty(cssVariable, value);
    }
  }

  connectedCallback() {
    this.tabStripEmbedderProxy_.getLayout().then(
        layout => this.applyCSSDictionary_(layout));
    this.fetchAndUpdateColors_();

    const getTabsStartTimestamp = Date.now();
    this.tabsApi_.getTabs().then(tabs => {
      this.tabStripEmbedderProxy_.reportTabDataReceivedDuration(
          tabs.length, Date.now() - getTabsStartTimestamp);

      const createTabsStartTimestamp = Date.now();
      tabs.forEach(tab => this.onTabCreated_(tab));
      this.tabStripEmbedderProxy_.reportTabCreationDuration(
          tabs.length, Date.now() - createTabsStartTimestamp);

      addWebUIListener('tab-created', tab => this.onTabCreated_(tab));
      addWebUIListener(
          'tab-moved', (tabId, newIndex) => this.onTabMoved_(tabId, newIndex));
      addWebUIListener('tab-removed', tabId => this.onTabRemoved_(tabId));
      addWebUIListener(
          'tab-replaced', (oldId, newId) => this.onTabReplaced_(oldId, newId));
      addWebUIListener('tab-updated', tab => this.onTabUpdated_(tab));
      addWebUIListener(
          'tab-active-changed', tabId => this.onTabActivated_(tabId));
      addWebUIListener(
          'tab-group-state-changed',
          (tabId, index, groupId) =>
              this.onTabGroupStateChanged_(tabId, index, groupId));
      addWebUIListener(
          'tab-group-closed', groupId => this.onTabGroupClosed_(groupId));
    });
  }

  disconnectedCallback() {
    document.removeEventListener('contextmenu', this.contextMenuListener_);
    document.removeEventListener(
        'visibilitychange', this.documentVisibilityChangeListener_);
    window.removeEventListener('blur', this.windowBlurListener_);
  }

  /**
   * @param {!TabData} tab
   * @return {!TabElement}
   * @private
   */
  createTabElement_(tab) {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.onTabActivating = (id) => {
      this.onTabActivating_(id);
    };
    return tabElement;
  }

  /**
   * @param {number} tabId
   * @return {?TabElement}
   * @private
   */
  findTabElement_(tabId) {
    return /** @type {?TabElement} */ (
        this.shadowRoot.querySelector(`tabstrip-tab[data-tab-id="${tabId}"]`));
  }

  /**
   * @param {string} groupId
   * @return {?TabGroupElement}
   * @private
   */
  findTabGroupElement_(groupId) {
    return /** @type {?TabGroupElement} */ (this.shadowRoot.querySelector(
        `tabstrip-tab-group[data-group-id="${groupId}"]`));
  }

  /** @private */
  fetchAndUpdateColors_() {
    this.tabStripEmbedderProxy_.getColors().then(
        colors => this.applyCSSDictionary_(colors));
  }

  /**
   * @return {?TabElement}
   * @private
   */
  getActiveTab_() {
    return /** @type {?TabElement} */ (
        this.shadowRoot.querySelector('tabstrip-tab[active]'));
  }

  /**
   * @param {!LayoutVariable} variable
   * @return {number} in pixels
   */
  getLayoutVariable_(variable) {
    return parseInt(this.style.getPropertyValue(variable), 10);
  }

  /**
   * @param {!TabElement} tabElement
   * @param {number} modelIndex
   * @private
   */
  insertTabOrMoveTo_(tabElement, modelIndex) {
    const isInserting = !tabElement.isConnected;

    // Remove the tabElement if it already exists in the DOM
    tabElement.remove();

    if (tabElement.tab.pinned) {
      this.pinnedTabsElement_.insertBefore(
          tabElement, this.pinnedTabsElement_.childNodes[modelIndex]);
    } else {
      let elementToInsert = tabElement;
      let elementAtIndex =
          this.shadowRoot.querySelectorAll('tabstrip-tab').item(modelIndex);
      let parentElement = this.unpinnedTabsElement_;

      if (tabElement.tab.groupId) {
        let tabGroupElement = this.findTabGroupElement_(tabElement.tab.groupId);
        if (tabGroupElement) {
          // If a TabGroupElement already exists, add the TabElement to it.
          parentElement = tabGroupElement;
        } else {
          // If a TabGroupElement does not exist, create one and add the
          // TabGroupElement into the DOM.
          tabGroupElement = document.createElement('tabstrip-tab-group');
          tabGroupElement.setAttribute('data-group-id', tabElement.tab.groupId);
          tabGroupElement.appendChild(tabElement);
          elementToInsert = tabGroupElement;
        }
      } else if (
          elementAtIndex && elementAtIndex.parentElement &&
          isTabGroupElement(elementAtIndex.parentElement) &&
          elementAtIndex.previousElementSibling === null) {
        // If the element at the model index is in a group and is the first
        // element in its group, insert the new element before its
        // TabGroupElement. If a TabElement is being sandwiched between two
        // TabElements in a group, it can be assumed that the tab will
        // eventually be inserted into the group as well.
        elementAtIndex = elementAtIndex.parentElement;
      }

      if (elementAtIndex && elementAtIndex.parentElement === parentElement) {
        parentElement.insertBefore(elementToInsert, elementAtIndex);
      } else {
        parentElement.appendChild(elementToInsert);
      }
    }

    if (isInserting) {
      this.updateThumbnailTrackStatus_(tabElement);
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenu_(event) {
    event.preventDefault();
    this.tabStripEmbedderProxy_.showBackgroundContextMenu(
        event.clientX, event.clientY);
  }

  /** @private */
  onDocumentVisibilityChange_() {
    if (!this.tabStripEmbedderProxy_.isVisible()) {
      this.scrollToActiveTab_();
    }

    this.unpinnedTabsElement_.childNodes.forEach(element => {
      if (isTabGroupElement(/** @type {!Element} */ (element))) {
        element.childNodes.forEach(
            tabElement => this.updateThumbnailTrackStatus_(
                /** @type {!TabElement} */ (tabElement)));
      } else {
        this.updateThumbnailTrackStatus_(
            /** @type {!TabElement} */ (element));
      }
    });
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragEnd_(event) {
    if (!this.draggedItem_) {
      return;
    }

    this.draggedItem_.setDragging(false);
    this.draggedItem_ = undefined;
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragOver_(event) {
    event.preventDefault();
    const dragOverItem = event.path.find((pathItem) => {
      return pathItem !== this.draggedItem_ && isTabElement(pathItem);
    });

    if (!dragOverItem || !this.draggedItem_ ||
        dragOverItem.tab.pinned !== this.draggedItem_.tab.pinned) {
      return;
    }

    event.dataTransfer.dropEffect = 'move';

    const dragOverIndex =
        Array.from(this.shadowRoot.querySelectorAll('tabstrip-tab'))
            .indexOf(dragOverItem);
    this.tabsApi_.moveTab(this.draggedItem_.tab.id, dragOverIndex);
  }

  /**
   * @param {!DragEvent} event
   * @private
   */
  onDragStart_(event) {
    const draggedItem = event.path[0];
    if (!isTabElement(draggedItem)) {
      return;
    }

    this.draggedItem_ = /** @type {!TabElement} */ (draggedItem);
    this.draggedItem_.setDragging(true);
    event.dataTransfer.effectAllowed = 'move';
    const draggedItemRect = this.draggedItem_.getBoundingClientRect();
    event.dataTransfer.setDragImage(
        this.draggedItem_.getDragImage(), event.clientX - draggedItemRect.left,
        event.clientY - draggedItemRect.top);
  }

  /** @private */
  onReceivedKeyboardFocus_() {
    // FocusOutlineManager relies on the most recent event fired on the
    // document. When the tab strip first gains keyboard focus, no such event
    // exists yet, so the outline needs to be explicitly set to visible.
    this.focusOutlineManager_.visible = true;
    this.shadowRoot.querySelector('tabstrip-tab').focus();
  }

  /**
   * @param {number} tabId
   * @private
   */
  onTabActivated_(tabId) {
    if (this.activatingTabId_ === tabId) {
      this.tabStripEmbedderProxy_.reportTabActivationDuration(
          Date.now() - this.activatingTabIdTimestamp_);
    }
    this.activatingTabId_ = undefined;
    this.activatingTabIdTimestamp_ = undefined;

    // There may be more than 1 TabElement marked as active if other events
    // have updated a Tab to have an active state. For example, if a
    // tab is created with an already active state, there may be 2 active
    // TabElements: the newly created tab and the previously active tab.
    this.shadowRoot.querySelectorAll('tabstrip-tab[active]')
        .forEach((previouslyActiveTab) => {
          if (previouslyActiveTab.tab.id !== tabId) {
            previouslyActiveTab.tab = /** @type {!TabData} */ (
                Object.assign({}, previouslyActiveTab.tab, {active: false}));
          }
        });

    const newlyActiveTab = this.findTabElement_(tabId);
    if (newlyActiveTab) {
      newlyActiveTab.tab = /** @type {!TabData} */ (
          Object.assign({}, newlyActiveTab.tab, {active: true}));
      if (!this.tabStripEmbedderProxy_.isVisible()) {
        this.scrollToTab_(newlyActiveTab);
      }
    }
  }

  /**
   * @param {number} id The tab ID
   * @private
   */
  onTabActivating_(id) {
    assert(this.activatingTabId_ === undefined);
    const activeTab = this.getActiveTab_();
    if (activeTab && activeTab.tab.id === id) {
      return;
    }
    this.activatingTabId_ = id;
    this.activatingTabIdTimestamp_ = Date.now();
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabCreated_(tab) {
    const tabElement = this.createTabElement_(tab);
    this.insertTabOrMoveTo_(tabElement, tab.index);
    this.addAnimationPromise_(tabElement.slideIn());
    if (tab.active) {
      this.scrollToTab_(tabElement);
    }
  }

  /**
   * @param {string} groupId
   * @private
   */
  onTabGroupClosed_(groupId) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      return;
    }
    tabGroupElement.remove();
  }

  /**
   * @param {number} tabId
   * @param {number} index
   * @param {string} groupId
   * @private
   */
  onTabGroupStateChanged_(tabId, index, groupId) {
    const tabElement = this.findTabElement_(tabId);
    tabElement.tab = /** @type {!TabData} */ (
        Object.assign({}, tabElement.tab, {groupId: groupId}));
    this.insertTabOrMoveTo_(tabElement, index);
  }

  /**
   * @param {number} tabId
   * @param {number} newIndex
   * @private
   */
  onTabMoved_(tabId, newIndex) {
    const movedTab = this.findTabElement_(tabId);
    if (movedTab) {
      this.insertTabOrMoveTo_(movedTab, newIndex);
      if (movedTab.tab.active) {
        this.scrollToTab_(movedTab);
      }
    }
  }

  /**
   * @param {number} tabId
   * @private
   */
  onTabRemoved_(tabId) {
    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      this.addAnimationPromise_(tabElement.slideOut());
    }
  }

  /**
   * @param {number} oldId
   * @param {number} newId
   * @private
   */
  onTabReplaced_(oldId, newId) {
    const tabElement = this.findTabElement_(oldId);
    if (!tabElement) {
      return;
    }

    tabElement.tab = /** @type {!TabData} */ (
        Object.assign({}, tabElement.tab, {id: newId}));
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabUpdated_(tab) {
    const tabElement = this.findTabElement_(tab.id);
    if (!tabElement) {
      return;
    }

    const previousTab = tabElement.tab;
    tabElement.tab = tab;

    if (previousTab.pinned !== tab.pinned) {
      // If the tab is being pinned or unpinned, we need to move it to its new
      // location
      this.insertTabOrMoveTo_(tabElement, tab.index);
      if (tab.active) {
        this.scrollToTab_(tabElement);
      }
      this.updateThumbnailTrackStatus_(tabElement);
    }
  }

  /** @private */
  onWindowBlur_() {
    if (this.shadowRoot.activeElement) {
      // Blur the currently focused element when the window is blurred. This
      // prevents the screen reader from momentarily reading out the
      // previously focused element when the focus returns to this window.
      this.shadowRoot.activeElement.blur();
    }
  }

  /** @private */
  scrollToActiveTab_() {
    const activeTab = this.getActiveTab_();
    if (!activeTab) {
      return;
    }

    this.scrollToTab_(activeTab);
  }

  /**
   * @param {!TabElement} tabElement
   * @private
   */
  scrollToTab_(tabElement) {
    const tabElementWidth = this.getLayoutVariable_(LayoutVariable.TAB_WIDTH);
    const tabElementRect = tabElement.getBoundingClientRect();
    // In RTL languages, the TabElement's scale animation scales from right to
    // left. Therefore, the value of its getBoundingClientRect().left may not be
    // accurate of its final rendered size because the element may not have
    // fully scaled to the left yet.
    const tabElementLeft =
        isRTL() ? tabElementRect.right - tabElementWidth : tabElementRect.left;

    const newTabButtonSpace =
        this.getLayoutVariable_(LayoutVariable.NEW_TAB_BUTTON_WIDTH) +
        this.getLayoutVariable_(LayoutVariable.NEW_TAB_BUTTON_MARGIN);
    const leftBoundary =
        isRTL() ? SCROLL_PADDING + newTabButtonSpace : SCROLL_PADDING;

    let scrollBy = 0;
    if (tabElementLeft === leftBoundary) {
      // Perfectly aligned to the left.
      return;
    } else if (tabElementLeft < leftBoundary) {
      // If the element's left is to the left of the left boundary, scroll
      // such that the element's left edge is aligned with the left boundary.
      scrollBy = tabElementLeft - leftBoundary;
    } else {
      const tabElementRight = tabElementLeft + tabElementWidth;
      const rightBoundary = isRTL() ?
          this.getLayoutVariable_(LayoutVariable.VIEWPORT_WIDTH) -
              SCROLL_PADDING :
          this.getLayoutVariable_(LayoutVariable.VIEWPORT_WIDTH) -
              SCROLL_PADDING - newTabButtonSpace;

      if (tabElementRight > rightBoundary) {
        scrollBy = (tabElementRight) - rightBoundary;
      } else {
        // Perfectly aligned to the right.
        return;
      }
    }

    this.animateScrollPosition_(scrollBy);
  }

  /**
   * @param {number} tabId
   * @param {string} imgData
   * @private
   */
  tabThumbnailUpdated_(tabId, imgData) {
    const tab = this.findTabElement_(tabId);
    if (tab) {
      tab.updateThumbnail(imgData);
    }
  }

  /**
   * @param {!TabElement} tabElement
   * @private
   */
  updateThumbnailTrackStatus_(tabElement) {
    if (this.tabStripEmbedderProxy_.isVisible() && !tabElement.tab.pinned) {
      // If the tab strip is visible and the tab is not pinned, let the
      // IntersectionObserver start observing the TabElement to automatically
      // determine if the tab's thumbnail should be tracked.
      this.intersectionObserver_.observe(tabElement);
    } else {
      // If the tab strip is not visible or the tab is pinned, the tab does not
      // need to show or update any thumbnails.
      this.intersectionObserver_.unobserve(tabElement);
      this.tabsApi_.setThumbnailTracked(tabElement.tab.id, false);
    }
  }
}

customElements.define('tabstrip-tab-list', TabListElement);
