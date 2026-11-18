#include "check.h"
#include "text_parts.h"

namespace {

void CheckParts(const std::vector<TextPart>& parts, const std::vector<TextPart>& expected) {
	CHECK(parts.size() == expected.size());
	for (size_t index = 0; index < parts.size() && index < expected.size(); ++index) {
		CHECK(parts[index].text == expected[index].text);
		CHECK(parts[index].dim == expected[index].dim);
	}
}

void ABracketedPartIsDim() {
	CheckParts(SplitBracketParts("Song (Remastered 2012)"), { { "Song ", false }, { "(Remastered 2012)", true } });
}

void TextAfterTheBracketIsPlainAgain() {
	CheckParts(SplitBracketParts("A [live] B {x}"), { { "A ", false }, { "[live]", true }, { " B ", false }, { "{x}", true } });
}

void NestedBracketsStayInOneDimPart() {
	CheckParts(SplitBracketParts("T (mix (edit)) end"), { { "T ", false }, { "(mix (edit))", true }, { " end", false } });
}

void AnUnclosedBracketDimsTheRest() {
	CheckParts(SplitBracketParts("T (open"), { { "T ", false }, { "(open", true } });
}

void AStrayClosingBracketIsPlain() {
	CheckParts(SplitBracketParts("a) b"), { { "a) b", false } });
}

void TextWithoutBracketsIsOnePlainPart() {
	CheckParts(SplitBracketParts("Plain"), { { "Plain", false } });
	CHECK(SplitBracketParts("").empty());
	CHECK(!HasBracket("Plain"));
	CHECK(HasBracket("x [y]"));
}

} // namespace

void RunTextPartsTests() {
	ABracketedPartIsDim();
	TextAfterTheBracketIsPlainAgain();
	NestedBracketsStayInOneDimPart();
	AnUnclosedBracketDimsTheRest();
	AStrayClosingBracketIsPlain();
	TextWithoutBracketsIsOnePlainPart();
}
