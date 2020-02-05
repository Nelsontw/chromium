// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/x11_event_translation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/x/x11.h"

namespace ui {

// Ensure DomKey extraction happens lazily in Ozone X11, while in non-Ozone
// path it is set right away in XEvent => ui::Event translation. This prevents
// regressions such as crbug.com/1007389.
TEST(XEventTranslationTest, KeyEventDomKeyExtraction) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event xev;
  xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_RETURN, EF_NONE);

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  KeyEventTestApi test(keyev.get());
#if defined(USE_OZONE)
  EXPECT_EQ(ui::DomKey::NONE, test.dom_key());
#else
  EXPECT_EQ(ui::DomKey::ENTER, test.dom_key());
#endif

  EXPECT_EQ(13, keyev->GetCharacter());
  EXPECT_EQ("Enter", keyev->GetCodeString());
}

// Ensure KeyEvent::Properties is properly set regardless X11 build config is
// in place. This prevents regressions such as crbug.com/1047999.
TEST(XEventTranslationTest, KeyEventXEventPropertiesSet) {
  ui::ScopedKeyboardLayout keyboard_layout(ui::KEYBOARD_LAYOUT_ENGLISH_US);
  ScopedXI2Event scoped_xev;
  scoped_xev.InitKeyEvent(ET_KEY_PRESSED, VKEY_A, EF_NONE);

  XEvent* xev = scoped_xev;
  XDisplay* xdisplay = xev->xkey.display;
  // Set keyboard group in XKeyEvent
  xev->xkey.state = XkbBuildCoreState(xev->xkey.state, 2u);
  // Set IBus-specific flags
  xev->xkey.state |= 0x3 << ui::kPropertyKeyboardIBusFlagOffset;

  auto keyev = ui::BuildKeyEventFromXEvent(*xev);
  EXPECT_TRUE(keyev);

  auto* properties = keyev->properties();
  EXPECT_TRUE(properties);
  EXPECT_EQ(3u, properties->size());

  // Ensure hardware keycode, keyboard group and ibus flag properties are
  // properly set.
  auto hw_keycode_it = properties->find(ui::kPropertyKeyboardHwKeyCode);
  EXPECT_NE(hw_keycode_it, properties->end());
  EXPECT_EQ(1u, hw_keycode_it->second.size());
  EXPECT_EQ(XKeysymToKeycode(xdisplay, XK_a), hw_keycode_it->second[0]);

  auto kbd_group_it = properties->find(ui::kPropertyKeyboardGroup);
  EXPECT_NE(kbd_group_it, properties->end());
  EXPECT_EQ(1u, kbd_group_it->second.size());
  EXPECT_EQ(2u, kbd_group_it->second[0]);

  auto ibus_flag_it = properties->find(ui::kPropertyKeyboardIBusFlag);
  EXPECT_NE(ibus_flag_it, properties->end());
  EXPECT_EQ(1u, ibus_flag_it->second.size());
  EXPECT_EQ(0x3, ibus_flag_it->second[0]);
}

}  // namespace ui
