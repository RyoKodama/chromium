// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/builtin_provider.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace {

const char kEmbedderAboutScheme[] = "chrome";
const char kDefaultURL1[] = "chrome://default1/";
const char kDefaultURL2[] = "chrome://default2/";
const char kDefaultURL3[] = "chrome://foo/";
const char kSubpageURL[] = "chrome://subpage/";

// Arbitrary host constants, chosen to start with the letters "b" and "me".
const char kHostBar[] = "bar";
const char kHostMedia[] = "media";
const char kHostMemory[] = "memory";
const char kHostMemoryInternals[] = "memory-internals";
const char kHostSubpage[] = "subpage";

const char kSubpageOne[] = "one";
const char kSubpageTwo[] = "two";
const char kSubpageThree[] = "three";

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient() {}

  std::string GetEmbedderRepresentationOfAboutScheme() override {
    return kEmbedderAboutScheme;
  }

  std::vector<base::string16> GetBuiltinURLs() override {
    std::vector<base::string16> urls;
    urls.push_back(ASCIIToUTF16(kHostBar));
    urls.push_back(ASCIIToUTF16(kHostMedia));
    urls.push_back(ASCIIToUTF16(kHostMemory));
    urls.push_back(ASCIIToUTF16(kHostMemoryInternals));
    urls.push_back(ASCIIToUTF16(kHostSubpage));

    base::string16 prefix = ASCIIToUTF16(kHostSubpage) + ASCIIToUTF16("/");
    urls.push_back(prefix + ASCIIToUTF16(kSubpageOne));
    urls.push_back(prefix + ASCIIToUTF16(kSubpageTwo));
    urls.push_back(prefix + ASCIIToUTF16(kSubpageThree));
    return urls;
  }

  std::vector<base::string16> GetBuiltinsToProvideAsUserTypes() override {
    std::vector<base::string16> urls;
    urls.push_back(ASCIIToUTF16(kDefaultURL1));
    urls.push_back(ASCIIToUTF16(kDefaultURL2));
    urls.push_back(ASCIIToUTF16(kDefaultURL3));
    return urls;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

}  // namespace

class BuiltinProviderTest : public testing::Test {
 protected:
  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const GURL output[3];
  };

  BuiltinProviderTest() : provider_(NULL) {}
  ~BuiltinProviderTest() override {}

  void SetUp() override {
    client_.reset(new FakeAutocompleteProviderClient());
    provider_ = new BuiltinProvider(client_.get());
  }
  void TearDown() override { provider_ = NULL; }

  void RunTest(const TestData cases[], size_t num_cases) {
    ACMatches matches;
    for (size_t i = 0; i < num_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      const AutocompleteInput input(
          cases[i].input, base::string16::npos, std::string(), GURL(),
          base::string16(), metrics::OmniboxEventProto::INVALID_SPEC, true,
          false, true, true, false, TestSchemeClassifier());
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());
      matches = provider_->matches();
      EXPECT_EQ(cases[i].num_results, matches.size());
      if (matches.size() == cases[i].num_results) {
        size_t j = 0;
        for (const auto& match : matches) {
          EXPECT_EQ(cases[i].output[j], match.destination_url);
          EXPECT_FALSE(match.allowed_to_be_default_match);
          ++j;
          if (j == cases[i].num_results)
            break;
        }
      }
    }
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<BuiltinProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BuiltinProviderTest);
};

TEST_F(BuiltinProviderTest, TypingScheme) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSeparator1 = ASCIIToUTF16(":");
  const base::string16 kSeparator2 = ASCIIToUTF16(":/");
  const base::string16 kSeparator3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1(kDefaultURL1);
  const GURL kURL2(kDefaultURL2);
  const GURL kURL3(kDefaultURL3);

  TestData typing_scheme_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("h"),        0, {}},
    {ASCIIToUTF16("http"),     0, {}},
    {ASCIIToUTF16("file"),     0, {}},
    {ASCIIToUTF16("abouz"),    0, {}},
    {ASCIIToUTF16("aboutt"),   0, {}},
    {ASCIIToUTF16("aboutt:"),  0, {}},
    {ASCIIToUTF16("chroma"),   0, {}},
    {ASCIIToUTF16("chromee"),  0, {}},
    {ASCIIToUTF16("chromee:"), 0, {}},

    // Typing a portion of about:// should give the default urls.
    {kAbout.substr(0, 1),      3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("A"),        3, {kURL1, kURL2, kURL3}},
    {kAbout,                   3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator1,     3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator2,     3, {kURL1, kURL2, kURL3}},
    {kAbout + kSeparator3,     3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("aBoUT://"), 3, {kURL1, kURL2, kURL3}},

    // Typing a portion of the embedder scheme should give the default urls.
    {kEmbedder.substr(0, 1),    3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("C"),         3, {kURL1, kURL2, kURL3}},
    {kEmbedder,                 3, {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator1,   3, {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator2,   3, {kURL1, kURL2, kURL3}},
    {kEmbedder + kSeparator3,   3, {kURL1, kURL2, kURL3}},
    {ASCIIToUTF16("ChRoMe://"), 3, {kURL1, kURL2, kURL3}},
  };

  RunTest(typing_scheme_cases, arraysize(typing_scheme_cases));
}

TEST_F(BuiltinProviderTest, NonEmbedderURLs) {
  TestData test_cases[] = {
    // Typing an unrelated scheme should give nothing.
    {ASCIIToUTF16("g@rb@g3"),                      0, {}},
    {ASCIIToUTF16("www.google.com"),               0, {}},
    {ASCIIToUTF16("http:www.google.com"),          0, {}},
    {ASCIIToUTF16("http://www.google.com"),        0, {}},
    {ASCIIToUTF16("file:filename"),                0, {}},
    {ASCIIToUTF16("scheme:"),                      0, {}},
    {ASCIIToUTF16("scheme://"),                    0, {}},
    {ASCIIToUTF16("scheme://host"),                0, {}},
    {ASCIIToUTF16("scheme:host/path?query#ref"),   0, {}},
    {ASCIIToUTF16("scheme://host/path?query#ref"), 0, {}},
  };

  RunTest(test_cases, arraysize(test_cases));
}

TEST_F(BuiltinProviderTest, EmbedderProvidedURLs) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSep1 = ASCIIToUTF16(":");
  const base::string16 kSep2 = ASCIIToUTF16(":/");
  const base::string16 kSep3 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);

  // The following hosts are arbitrary, chosen so that they all start with the
  // letters "me".
  const base::string16 kHostM1 = ASCIIToUTF16(kHostMedia);
  const base::string16 kHostM2 = ASCIIToUTF16(kHostMemory);
  const base::string16 kHostM3 = ASCIIToUTF16(kHostMemoryInternals);
  const GURL kURLM1(kEmbedder + kSep3 + kHostM1);
  const GURL kURLM2(kEmbedder + kSep3 + kHostM2);
  const GURL kURLM3(kEmbedder + kSep3 + kHostM3);

  TestData test_cases[] = {
    // Typing an about URL with an unknown host should give nothing.
    {kAbout + kSep1 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSep2 + ASCIIToUTF16("host"), 0, {}},
    {kAbout + kSep3 + ASCIIToUTF16("host"), 0, {}},

    // Typing an embedder URL with an unknown host should give nothing.
    {kEmbedder + kSep1 + ASCIIToUTF16("host"), 0, {}},
    {kEmbedder + kSep2 + ASCIIToUTF16("host"), 0, {}},
    {kEmbedder + kSep3 + ASCIIToUTF16("host"), 0, {}},

    // Typing an about URL should provide matching URLs.
    {kAbout + kSep1 + kHostM1.substr(0, 1),    3, {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSep2 + kHostM1.substr(0, 2),    3, {kURLM1, kURLM2, kURLM3}},
    {kAbout + kSep3 + kHostM1.substr(0, 3),    1, {kURLM1}},
    {kAbout + kSep3 + kHostM2.substr(0, 3),    2, {kURLM2, kURLM3}},
    {kAbout + kSep3 + kHostM1,                 1, {kURLM1}},
    {kAbout + kSep2 + kHostM2,                 2, {kURLM2, kURLM3}},
    {kAbout + kSep2 + kHostM3,                 1, {kURLM3}},

    // Typing an embedder URL should provide matching URLs.
    {kEmbedder + kSep1 + kHostM1.substr(0, 1), 3, {kURLM1, kURLM2, kURLM3}},
    {kEmbedder + kSep2 + kHostM1.substr(0, 2), 3, {kURLM1, kURLM2, kURLM3}},
    {kEmbedder + kSep3 + kHostM1.substr(0, 3), 1, {kURLM1}},
    {kEmbedder + kSep3 + kHostM2.substr(0, 3), 2, {kURLM2, kURLM3}},
    {kEmbedder + kSep3 + kHostM1,              1, {kURLM1}},
    {kEmbedder + kSep2 + kHostM2,              2, {kURLM2, kURLM3}},
    {kEmbedder + kSep2 + kHostM3,              1, {kURLM3}},
  };

  RunTest(test_cases, arraysize(test_cases));
}

TEST_F(BuiltinProviderTest, AboutBlank) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kAboutBlank = ASCIIToUTF16(url::kAboutBlankURL);
  const base::string16 kBlank = ASCIIToUTF16("blank");
  const base::string16 kSeparator1 =
      ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kSeparator2 = ASCIIToUTF16(":///");
  const base::string16 kSeparator3 = ASCIIToUTF16(";///");

  const GURL kURLBar =
      GURL(kEmbedder + kSeparator1 + ASCIIToUTF16(kHostBar));
  const GURL kURLBlank(kAboutBlank);

  TestData about_blank_cases[] = {
    // Typing an about:blank prefix should yield about:blank, among other URLs.
    {kAboutBlank.substr(0, 7), 2, {kURLBlank, kURLBar}},
    {kAboutBlank.substr(0, 8), 1, {kURLBlank}},

    // Using any separator that is supported by fixup should yield about:blank.
    // For now, BuiltinProvider does not suggest url-what-you-typed matches for
    // for about:blank; check "about:blan" and "about;blan" substrings instead.
    {kAbout + kSeparator2.substr(0, 1) + kBlank.substr(0, 4), 1, {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 2) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator2.substr(0, 3) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator2 + kBlank,                           1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 1) + kBlank.substr(0, 4), 1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 2) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator3.substr(0, 3) + kBlank,              1, {kURLBlank}},
    {kAbout + kSeparator3 + kBlank,                           1, {kURLBlank}},

    // Using the embedder scheme should not yield about:blank.
    {kEmbedder + kSeparator1.substr(0, 1) + kBlank, 0, {}},
    {kEmbedder + kSeparator1.substr(0, 2) + kBlank, 0, {}},
    {kEmbedder + kSeparator1.substr(0, 3) + kBlank, 0, {}},
    {kEmbedder + kSeparator1 + kBlank,              0, {}},

    // Adding trailing text should not yield about:blank.
    {kAboutBlank + ASCIIToUTF16("/"),  0, {}},
    {kAboutBlank + ASCIIToUTF16("/p"), 0, {}},
    {kAboutBlank + ASCIIToUTF16("x"),  0, {}},
    {kAboutBlank + ASCIIToUTF16("?q"), 0, {}},
    {kAboutBlank + ASCIIToUTF16("#r"), 0, {}},

    // Interrupting "blank" with conflicting text should not yield about:blank.
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/"),  0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("/p"), 0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("x"),  0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("?q"), 0, {}},
    {kAboutBlank.substr(0, 9) + ASCIIToUTF16("#r"), 0, {}},
  };

  RunTest(about_blank_cases, arraysize(about_blank_cases));
}

TEST_F(BuiltinProviderTest, DoesNotSupportMatchesOnFocus) {
  const AutocompleteInput input(
      ASCIIToUTF16("chrome://m"), base::string16::npos, std::string(), GURL(),
      base::string16(), metrics::OmniboxEventProto::INVALID_SPEC, true, false,
      true, true, true, TestSchemeClassifier());
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(BuiltinProviderTest, Subpages) {
  const base::string16 kSubpage = ASCIIToUTF16(kSubpageURL);
  const base::string16 kPageOne = ASCIIToUTF16(kSubpageOne);
  const base::string16 kPageTwo = ASCIIToUTF16(kSubpageTwo);
  const base::string16 kPageThree = ASCIIToUTF16(kSubpageThree);
  const GURL kURLOne(kSubpage + kPageOne);
  const GURL kURLTwo(kSubpage + kPageTwo);
  const GURL kURLThree(kSubpage + kPageThree);

  TestData settings_subpage_cases[] = {
    // Typing the settings path should show settings and the first two subpages.
    {kSubpage, 3, {GURL(kSubpage), kURLOne, kURLTwo}},

    // Typing a subpage path should return the appropriate results.
    {kSubpage + kPageTwo.substr(0, 1),                 2, {kURLTwo, kURLThree}},
    {kSubpage + kPageTwo.substr(0, 2),                 1, {kURLTwo}},
    {kSubpage + kPageThree.substr(0, kPageThree.length() - 1),
                                                       1, {kURLThree}},
    {kSubpage + kPageOne,                              1, {kURLOne}},
    {kSubpage + kPageTwo,                              1, {kURLTwo}},
  };

  RunTest(settings_subpage_cases, arraysize(settings_subpage_cases));
}

TEST_F(BuiltinProviderTest, Inlining) {
  const base::string16 kAbout = ASCIIToUTF16(url::kAboutScheme);
  const base::string16 kEmbedder = ASCIIToUTF16(kEmbedderAboutScheme);
  const base::string16 kSep = ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kHostM = ASCIIToUTF16(kHostMedia);
  const base::string16 kHostB = ASCIIToUTF16(kHostBar);

  struct InliningTestData {
    const base::string16 input;
    const base::string16 expected_inline_autocompletion;
  } cases[] = {
    // Typing along "about://media" should not yield an inline autocompletion
    // until the completion is unique.  We don't bother checking every single
    // character before the first "m" is typed.
    {kAbout.substr(0,2),                  base::string16()},
    {kAbout,                              base::string16()},
    {kAbout + kSep,                       base::string16()},
    {kAbout + kSep + kHostM.substr(0, 1), base::string16()},
    {kAbout + kSep + kHostM.substr(0, 2), base::string16()},
    {kAbout + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
    {kAbout + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

    // Ditto with "chrome://media".
    {kEmbedder.substr(0,2),                  base::string16()},
    {kEmbedder,                              base::string16()},
    {kEmbedder + kSep,                       base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 1), base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 2), base::string16()},
    {kEmbedder + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
    {kEmbedder + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

    // The same rules should apply to "about://bar" and "chrome://bar".
    // At the "a" from "bar" in "about://bar", Chrome should be willing to
    // start inlining.  (Before that it conflicts with about:blank.)  At
    // the "b" from "bar" in "chrome://bar", Chrome should be willing to
    // start inlining.  (There is no chrome://blank page.)
    {kAbout + kSep + kHostB.substr(0, 1),    base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2),    kHostB.substr(2)},
    {kAbout + kSep + kHostB.substr(0, 3),    kHostB.substr(3)},
    {kEmbedder + kSep + kHostB.substr(0, 1), kHostB.substr(1)},
    {kEmbedder + kSep + kHostB.substr(0, 2), kHostB.substr(2)},
    {kEmbedder + kSep + kHostB.substr(0, 3), kHostB.substr(3)},

    // Typing something non-match after an inline autocompletion should stop
    // the inline autocompletion from appearing.
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("/"), base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("a"), base::string16()},
    {kAbout + kSep + kHostB.substr(0, 2) + ASCIIToUTF16("+"), base::string16()},
  };

  ACMatches matches;
  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
        "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
    const AutocompleteInput input(
        cases[i].input, base::string16::npos, std::string(), GURL(),
        base::string16(), metrics::OmniboxEventProto::INVALID_SPEC, false,
        false, true, true, false, TestSchemeClassifier());
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    matches = provider_->matches();
    if (cases[i].expected_inline_autocompletion.empty()) {
      // If we're not expecting an inline autocompletion, make sure that no
      // matches are allowed_to_be_default.
      for (const auto& match : matches)
        EXPECT_FALSE(match.allowed_to_be_default_match);
    } else {
      ASSERT_FALSE(matches.empty());
      EXPECT_TRUE(matches.front().allowed_to_be_default_match);
      EXPECT_EQ(cases[i].expected_inline_autocompletion,
                matches.front().inline_autocompletion);
    }
  }
}
