// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"

namespace content {

class CrossOriginOpenerPolicyBrowserTest : public ContentBrowserTest {
 public:
  CrossOriginOpenerPolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::Feature> features;
    feature_list_.InitWithFeatures({network::features::kCrossOriginIsolation},
                                   {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    SetupCrossSiteRedirector(https_server());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_server()->Start());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOrigin) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_InheritsSameOriginAllowPopups) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(
      network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOriginAllowPopups);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NewPopupCOOP_CrossOriginDoesNotInherit) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  EXPECT_EQ(main_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kSameOrigin);
  EXPECT_EQ(popup_frame->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kUnsafeNone);
}

IN_PROC_BROWSER_TEST_F(
    CrossOriginOpenerPolicyBrowserTest,
    NewPopupCOOP_SameOriginPolicyAndCrossOriginIframeSetsNoopener) {
  GURL starting_page(
      https_server()->GetURL("a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  RenderFrameHostImpl* main_frame = current_frame_host();
  main_frame->set_cross_origin_opener_policy(
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  ShellAddedObserver new_shell_observer;
  RenderFrameHostImpl* iframe = main_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(iframe, "window.open('about:blank')"));

  Shell* new_shell = new_shell_observer.GetShell();
  RenderFrameHostImpl* popup_frame =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  scoped_refptr<SiteInstance> main_frame_site_instance(
      main_frame->GetSiteInstance());
  scoped_refptr<SiteInstance> iframe_site_instance(iframe->GetSiteInstance());
  scoped_refptr<SiteInstance> popup_site_instance(
      popup_frame->GetSiteInstance());

  ASSERT_TRUE(main_frame_site_instance);
  ASSERT_TRUE(iframe_site_instance);
  ASSERT_TRUE(popup_site_instance);
  EXPECT_FALSE(main_frame_site_instance->IsRelatedSiteInstance(
      popup_site_instance.get()));
  EXPECT_FALSE(
      iframe_site_instance->IsRelatedSiteInstance(popup_site_instance.get()));

  // Check that `window.opener` is not set.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success) << "window.opener is set";
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NetworkErrorOnSandboxedPopups) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_sandbox_popup.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));

  ShellAddedObserver shell_observer;
  RenderFrameHostImpl* iframe =
      current_frame_host()->child_at(0)->current_frame_host();

  EXPECT_TRUE(ExecJs(
      iframe, "window.open('/cross-origin-opener-policy_same-origin.html')"));

  auto* popup_webcontents =
      static_cast<WebContentsImpl*>(shell_observer.GetShell()->web_contents());
  WaitForLoadStop(popup_webcontents);

  EXPECT_EQ(
      popup_webcontents->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_ERROR);
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       NoNetworkErrorOnSandboxedDocuments) {
  GURL starting_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_csp_sandboxed.html"));
  EXPECT_TRUE(NavigateToURL(shell(), starting_page));
  EXPECT_NE(current_frame_host()->active_sandbox_flags(),
            blink::mojom::WebSandboxFlags::kNone)
      << "Document should be sandboxed.";

  GURL next_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  EXPECT_TRUE(NavigateToURL(shell(), next_page));
  EXPECT_EQ(
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType(),
      PAGE_TYPE_NORMAL);
}

class CrossOriginPolicyHeadersObserver : public WebContentsObserver {
 public:
  explicit CrossOriginPolicyHeadersObserver(
      WebContents* web_contents,
      network::mojom::CrossOriginEmbedderPolicyValue expected_coep,
      network::mojom::CrossOriginOpenerPolicy expected_coop)
      : WebContentsObserver(web_contents),
        expected_coep_(expected_coep),
        expected_coop_(expected_coop) {}

  ~CrossOriginPolicyHeadersObserver() override = default;

  void DidRedirectNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()->cross_origin_opener_policy ==
          expected_coop_);
  }

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Verify that the COOP/COEP headers were parsed.
    NavigationRequest* navigation_request =
        static_cast<NavigationRequest*>(navigation_handle);
    CHECK(navigation_request->response()->cross_origin_embedder_policy.value ==
          expected_coep_);
    CHECK(navigation_request->response()->cross_origin_opener_policy ==
          expected_coop_);
  }

 private:
  network::mojom::CrossOriginEmbedderPolicyValue expected_coep_;
  network::mojom::CrossOriginOpenerPolicy expected_coop_;
};

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       RedirectsParseCoopAndCoepHeaders) {
  GURL redirect_initial_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_initial.html"));
  GURL redirect_final_page(https_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_redirect_final.html"));

  CrossOriginPolicyHeadersObserver obs(
      web_contents(),
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp,
      network::mojom::CrossOriginOpenerPolicy::kSameOrigin);

  EXPECT_TRUE(
      NavigateToURL(shell(), redirect_initial_page, redirect_final_page));
}

IN_PROC_BROWSER_TEST_F(CrossOriginOpenerPolicyBrowserTest,
                       CoopIsIgnoredOverHttp) {
  GURL non_coop_page(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL coop_page(embedded_test_server()->GetURL(
      "a.com", "/cross-origin-opener-policy_same-origin.html"));

  scoped_refptr<SiteInstance> initial_site_instance(
      current_frame_host()->GetSiteInstance());

  EXPECT_TRUE(NavigateToURL(shell(), coop_page));
  EXPECT_EQ(current_frame_host()->GetSiteInstance(), initial_site_instance);
  EXPECT_EQ(current_frame_host()->cross_origin_opener_policy(),
            network::mojom::CrossOriginOpenerPolicy::kUnsafeNone);
}

}  // namespace content
