// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/testing/SelectionSample.h"

#include "core/dom/ProcessingInstruction.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class SelectionSampleTest : public EditingTestBase {
 protected:
  std::string SetAndGetSelectionText(const std::string& sample_text) {
    return SelectionSample::GetSelectionText(
        *GetDocument().body(),
        SelectionSample::SetSelectionText(GetDocument().body(), sample_text));
  }
};

TEST_F(SelectionSampleTest, SetEmpty1) {
  const SelectionInDOMTree& selection =
      SelectionSample::SetSelectionText(GetDocument().body(), "|");
  EXPECT_EQ("", GetDocument().body()->InnerHTMLAsString());
  EXPECT_EQ(0u, GetDocument().body()->CountChildren());
  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position(GetDocument().body(), 0))
                .Build(),
            selection);
}

TEST_F(SelectionSampleTest, SetEmpty2) {
  const SelectionInDOMTree& selection =
      SelectionSample::SetSelectionText(GetDocument().body(), "^|");
  EXPECT_EQ("", GetDocument().body()->InnerHTMLAsString());
  EXPECT_EQ(0u, GetDocument().body()->CountChildren());
  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position(GetDocument().body(), 0))
                .Build(),
            selection);
}

TEST_F(SelectionSampleTest, SetElement) {
  const SelectionInDOMTree& selection = SelectionSample::SetSelectionText(
      GetDocument().body(), "<p>^<a>0</a>|<b>1</b></p>");
  const Element* const sample = GetDocument().QuerySelector("p");
  EXPECT_EQ(2u, sample->CountChildren())
      << "We should remove Text node for '^' and '|'.";
  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position(sample, 0))
                .Extend(Position(sample, 1))
                .Build(),
            selection);
}

TEST_F(SelectionSampleTest, SetText) {
  {
    const auto& selection =
        SelectionSample::SetSelectionText(GetDocument().body(), "^ab|c");
    EXPECT_EQ("abc", GetDocument().body()->InnerHTMLAsString());
    EXPECT_EQ(SelectionInDOMTree::Builder()
                  .Collapse(Position(GetDocument().body()->firstChild(), 0))
                  .Extend(Position(GetDocument().body()->firstChild(), 2))
                  .Build(),
              selection);
  }
  {
    const auto& selection =
        SelectionSample::SetSelectionText(GetDocument().body(), "a^b|c");
    EXPECT_EQ("abc", GetDocument().body()->InnerHTMLAsString());
    EXPECT_EQ(SelectionInDOMTree::Builder()
                  .Collapse(Position(GetDocument().body()->firstChild(), 1))
                  .Extend(Position(GetDocument().body()->firstChild(), 2))
                  .Build(),
              selection);
  }
  {
    const auto& selection =
        SelectionSample::SetSelectionText(GetDocument().body(), "ab^|c");
    EXPECT_EQ("abc", GetDocument().body()->InnerHTMLAsString());
    EXPECT_EQ(SelectionInDOMTree::Builder()
                  .Collapse(Position(GetDocument().body()->firstChild(), 2))
                  .Build(),
              selection);
  }
  {
    const auto& selection =
        SelectionSample::SetSelectionText(GetDocument().body(), "ab|c^");
    EXPECT_EQ("abc", GetDocument().body()->InnerHTMLAsString());
    EXPECT_EQ(SelectionInDOMTree::Builder()
                  .Collapse(Position(GetDocument().body()->firstChild(), 3))
                  .Extend(Position(GetDocument().body()->firstChild(), 2))
                  .Build(),
              selection);
  }
}

// Demonstrates attribute handling in HTML parser and serializer.
TEST_F(SelectionSampleTest, SerializeAttribute) {
  EXPECT_EQ("<a x=\"1\" y=\"2\" z=\"3\">b|ar</a>",
            SetAndGetSelectionText("<a z='3' x='1' y='2'>b|ar</a>"))
      << "Attributes are alphabetically ordered.";
  EXPECT_EQ("<a x=\"'\" y=\"&quot;\" z=\"&amp;\">f|o^o</a>",
            SetAndGetSelectionText("<a x=\"'\" y='\"' z=&>f|o^o</a>"))
      << "Attributes with character entity.";
  EXPECT_EQ(
      "<foo:a foo:x=\"1\" xmlns:foo=\"http://foo\">x|y</foo:a>",
      SetAndGetSelectionText("<foo:a foo:x=1 xmlns:foo=http://foo>x|y</foo:a>"))
      << "namespace prefix should be supported";
  EXPECT_EQ(
      "<foo:a foo:x=\"1\" xmlns:foo=\"http://foo\">x|y</foo:a>",
      SetAndGetSelectionText("<foo:a foo:x=1 xmlns:Foo=http://foo>x|y</foo:a>"))
      << "namespace prefix is converted to lowercase by HTML parrser";
  EXPECT_EQ("<foo:a foo:x=\"1\" x=\"2\" xmlns:foo=\"http://foo\">xy|z</foo:a>",
            SetAndGetSelectionText(
                "<Foo:a x=2 Foo:x=1 xmlns:foo='http://foo'>xy|z</a>"))
      << "namespace prefix affects attribute ordering";
}

TEST_F(SelectionSampleTest, SerializeComment) {
  EXPECT_EQ("<!-- f|oo -->", SetAndGetSelectionText("<!-- f|oo -->"));
}

TEST_F(SelectionSampleTest, SerializeElement) {
  EXPECT_EQ("<a>|</a>", SetAndGetSelectionText("<a>|</a>"));
  EXPECT_EQ("<a>^</a>|", SetAndGetSelectionText("<a>^</a>|"));
  EXPECT_EQ("<a>^foo</a><b>bar</b>|",
            SetAndGetSelectionText("<a>^foo</a><b>bar</b>|"));
}

TEST_F(SelectionSampleTest, SerializeEmpty) {
  EXPECT_EQ("|", SetAndGetSelectionText("|"));
  EXPECT_EQ("|", SetAndGetSelectionText("^|"));
  EXPECT_EQ("|", SetAndGetSelectionText("|^"));
}

TEST_F(SelectionSampleTest, SerializeNamespace) {
  SetBodyContent("<div xmlns:foo='http://xyz'><foo:bar></foo:bar>");
  ContainerNode& sample = *ToContainerNode(GetDocument().body()->firstChild());
  EXPECT_EQ("<foo:bar></foo:bar>",
            SelectionSample::GetSelectionText(sample, SelectionInDOMTree()))
      << "GetSelectionText() does not insert namespace declaration.";
}

TEST_F(SelectionSampleTest, SerializeProcessingInstruction) {
  EXPECT_EQ("<!--?foo ba|r ?-->", SetAndGetSelectionText("<?foo ba|r ?>"))
      << "HTML parser turns PI into comment";
}

TEST_F(SelectionSampleTest, SerializeProcessingInstruction2) {
  GetDocument().body()->appendChild(GetDocument().createProcessingInstruction(
      "foo", "bar", ASSERT_NO_EXCEPTION));

  // Note: PI ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'
  EXPECT_EQ("<?foo bar?>", SelectionSample::GetSelectionText(
                               *GetDocument().body(), SelectionInDOMTree()))
      << "No space after 'bar'";
}

// Demonstrate magic TABLE element parsing.
TEST_F(SelectionSampleTest, SerializeTable) {
  EXPECT_EQ("|<table></table>", SetAndGetSelectionText("<table>|</table>"))
      << "Parser moves Text before TABLE.";
  EXPECT_EQ("<table>|</table>",
            SetAndGetSelectionText("<table><!--|--!></table>"))
      << "Parser does not inserts TBODY and comment is removed.";
  EXPECT_EQ(
      "|start^end<table><tbody><tr><td>a</td></tr></tbody></table>",
      SetAndGetSelectionText("<table>|start<tr><td>a</td></tr>^end</table>"))
      << "Parser moves |Text| nodes inside TABLE to before TABLE.";
  EXPECT_EQ(
      "<table>|<tbody><tr><td>a</td></tr></tbody>^</table>",
      SetAndGetSelectionText(
          "<table><!--|--><tbody><tr><td>a</td></tr></tbody><!--^--></table>"))
      << "We can use |Comment| node to put selection marker inside TABLE.";
  EXPECT_EQ("<table>|<tbody><tr><td>a</td></tr>^</tbody></table>",
            SetAndGetSelectionText(
                "<table><!--|--><tr><td>a</td></tr><!--^--></table>"))
      << "Parser inserts TBODY auto magically.";
}

TEST_F(SelectionSampleTest, SerializeText) {
  EXPECT_EQ("012^3456|789", SetAndGetSelectionText("012^3456|789"));
  EXPECT_EQ("012|3456^789", SetAndGetSelectionText("012|3456^789"));
}

TEST_F(SelectionSampleTest, SerializeVoidElement) {
  EXPECT_EQ("|<div></div>", SetAndGetSelectionText("|<div></div>"))
      << "DIV requires end tag.";
  EXPECT_EQ("|<br>", SetAndGetSelectionText("|<br>"))
      << "BR doesn't need to have end tag.";
  EXPECT_EQ("|<br>1<br>", SetAndGetSelectionText("|<br>1</br>"))
      << "Parser converts </br> to <br>.";
  EXPECT_EQ("|<img>", SetAndGetSelectionText("|<img>"))
      << "IMG doesn't need to have end tag.";
}

TEST_F(SelectionSampleTest, SerializeVoidElementBR) {
  Element* const br = GetDocument().createElement("br");
  br->appendChild(GetDocument().createTextNode("abc"));
  GetDocument().body()->appendChild(br);
  EXPECT_EQ(
      "<br>abc|</br>",
      SelectionSample::GetSelectionText(
          *GetDocument().body(),
          SelectionInDOMTree::Builder().Collapse(Position(br, 1)).Build()))
      << "When BR has child nodes, it is not void element.";
}

TEST_F(SelectionSampleTest, ConvertTemplatesToShadowRoots) {
  SetBodyContent(
      "<div id=host>"
        "<template data-mode='open'>"
          "<div>shadow_first</div>"
          "<div>shadow_second</div>"
        "</template>"
      "</div>");
  Element* body = GetDocument().body();
  Element* host = body->getElementById("host");
  SelectionSample::ConvertTemplatesToShadowRootsForTesring(
      *(ToHTMLElement(host)));
  ShadowRoot* shadow_root = host->ShadowRootIfV1();
  ASSERT_TRUE(shadow_root->IsShadowRoot());
  EXPECT_EQ("<div>shadow_first</div><div>shadow_second</div>",
            shadow_root->InnerHTMLAsString());
}

TEST_F(SelectionSampleTest, ConvertTemplatesToShadowRootsNoTemplates) {
  SetBodyContent(
      "<div id=host>"
        "<div>first</div>"
        "<div>second</div>"
      "</div>");
  Element* body = GetDocument().body();
  Element* host = body->getElementById("host");
  SelectionSample::ConvertTemplatesToShadowRootsForTesring(
      *(ToHTMLElement(host)));
  EXPECT_FALSE(host->ShadowRootIfV1());
  EXPECT_EQ("<div>first</div><div>second</div>", host->InnerHTMLAsString());
}

TEST_F(SelectionSampleTest, ConvertTemplatesToShadowRootsMultipleTemplates) {
  SetBodyContent(
      "<div id=host1>"
        "<template data-mode='open'>"
          "<div>shadow_first</div>"
          "<div>shadow_second</div>"
        "</template>"
      "</div>"
      "<div id=host2>"
        "<template data-mode='open'>"
          "<div>shadow_third</div>"
          "<div>shadow_forth</div>"
        "</template>"
      "</div>");
  Element* body = GetDocument().body();
  Element* host1 = body->getElementById("host1");
  Element* host2 = body->getElementById("host2");
  SelectionSample::ConvertTemplatesToShadowRootsForTesring(
      *(ToHTMLElement(body)));
  ShadowRoot* shadow_root_1 = host1->ShadowRootIfV1();
  ShadowRoot* shadow_root_2 = host2->ShadowRootIfV1();

  EXPECT_TRUE(shadow_root_1->IsShadowRoot());
  EXPECT_EQ("<div>shadow_first</div><div>shadow_second</div>",
            shadow_root_1->InnerHTMLAsString());
  EXPECT_TRUE(shadow_root_2->IsShadowRoot());
  EXPECT_EQ("<div>shadow_third</div><div>shadow_forth</div>",
            shadow_root_2->InnerHTMLAsString());
}

}  // namespace blink
