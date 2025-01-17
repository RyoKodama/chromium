// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "url/gurl.h"

namespace proto = url_pattern_index::proto;

using subresource_filter::testing::ScopedSubresourceFilterConfigurator;
using subresource_filter::testing::TestRulesetPublisher;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

class SubresourceFilterContentSettingsManager;
class TestSafeBrowsingDatabaseHelper;

namespace subresource_filter {

class SubresourceFilterBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceFilterBrowserTest();
  ~SubresourceFilterBrowserTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;

  virtual std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase();

  std::vector<base::StringPiece> RequiredFeatures() const;

  GURL GetTestUrl(const std::string& relative_url) const;

  void ConfigureAsPhishingURL(const GURL& url);

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url);

  content::WebContents* web_contents() const;

  SubresourceFilterContentSettingsManager* settings_manager() const {
    return settings_manager_;
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) const;

  bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh);

  void ExpectParsedScriptElementLoadedStatusInFrames(
      const std::vector<const char*>& frame_names,
      const std::vector<bool>& expect_loaded);

  void ExpectFramesIncludedInLayout(const std::vector<const char*>& frame_names,
                                    const std::vector<bool>& expect_displayed);

  bool IsDynamicScriptElementLoaded(content::RenderFrameHost* rfh);

  void InsertDynamicFrameWithScript();

  void NavigateFromRendererSide(const GURL& url);

  void NavigateFrame(const char* frame_name, const GURL& url);

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  void ResetConfiguration(Configuration config);

  void ResetConfigurationToEnableOnPhishingSites(
      bool measure_performance = false,
      bool whitelist_site_on_reload = false);

  TestSafeBrowsingDatabaseHelper* database_helper() {
    return database_helper_.get();
  }

 private:
  TestRulesetCreator ruleset_creator_;
  ScopedSubresourceFilterConfigurator scoped_configuration_;
  TestRulesetPublisher test_ruleset_publisher_;

  std::unique_ptr<TestSafeBrowsingDatabaseHelper> database_helper_;

  // Owned by the profile.
  SubresourceFilterContentSettingsManager* settings_manager_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTest);
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
