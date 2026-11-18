#include "check.h"

#include <set>
#include <string>

#include "column_def.h"
#include "ui_text.h"

namespace {

void EachEnglishTextHasOneKoreanText() {
	std::set<std::string> seen;
	for (const UiText& text : kKoreanTexts) {
		CHECK(seen.insert(text.english).second);
		CHECK(text.korean != nullptr && text.korean[0] != '\0');
	}
}

void EveryBuiltInColumnHasAKoreanTitle() {
	for (const ColumnDef& def : BuiltInColumnDefs()) CHECK(KoreanTextFor(def.name) != nullptr);
}

void AnUnknownTextHasNoKoreanText() {
	CHECK(KoreanTextFor("No such text") == nullptr);
	CHECK(std::string(KoreanTextFor("Title")) == "제목");
}

void TrackCountsReadNaturally() {
	CHECK(TrackCountText(1, false) == "1 track");
	CHECK(TrackCountText(12, false) == "12 tracks");
	CHECK(TrackCountText(12, true) == "12곡");
}

} // namespace

void RunUiTextTests() {
	EachEnglishTextHasOneKoreanText();
	EveryBuiltInColumnHasAKoreanTitle();
	AnUnknownTextHasNoKoreanText();
	TrackCountsReadNaturally();
}
