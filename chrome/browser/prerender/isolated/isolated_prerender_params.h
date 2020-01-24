// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_

#include "base/optional.h"
#include "url/gurl.h"

// TODO(robertogden): Add feature enabled checks here.

// Returns the URL of the proxy server to use in isolated prerenders, if any.
base::Optional<GURL> IsolatedPrerenderProxyServer();

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
