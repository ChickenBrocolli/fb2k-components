#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "check.h"
#include "view_settings_fields.h"

namespace {

// The number of members of an aggregate: the most initializers it takes. Each one converts to any member type.
struct AnyMember {
	template <typename T> operator T() const;
};

template <typename T, typename... Members>
constexpr size_t CountMembers() {
	if constexpr (requires { T{ Members{}..., AnyMember{} }; }) return CountMembers<T, Members..., AnyMember>();
	else return sizeof...(Members);
}

size_t CountFields() {
	size_t count = 0;
	VisitViewSettings([&](const auto&) { ++count; });
	return count;
}

// A member added to ViewSettings without a line in the list would not be saved, exported or compared.
void EverySettingIsInTheList() {
	CHECK(CountFields() == CountMembers<ViewSettings>());
}

// Keys and configuration numbers are what earlier versions saved; two settings sharing one would overwrite each other.
void KeysAndStoreNumbersAreUniqueAndAvoidTheRetiredOnes() {
	std::set<std::string> keys;
	std::set<int> stores;
	const std::set<int> keptApart = { 15, 17, 18, 19, 20, 21, 22, 23, 24, 27 };
	VisitViewSettings([&](const auto& field) {
		CHECK(keys.insert(field.key).second);
		CHECK(stores.insert(field.guidIndex).second);
		CHECK(keptApart.count(field.guidIndex) == 0);
	});
}

// Every setting set to something other than its default.
ViewSettings EverySettingChanged() {
	using namespace view_settings_fields;
	ViewSettings view;
	VisitViewSettings([&](const auto& field) {
		using F = std::decay_t<decltype(field)>;
		auto& value = view.*field.member;
		if constexpr (std::is_same_v<F, BoolField> || std::is_same_v<F, OwnColorField>) value = !value;
		else if constexpr (std::is_same_v<F, IntField>) value = value == field.minimum ? field.maximum : field.minimum;
		else if constexpr (std::is_same_v<F, ColorField>) value ^= 0x00123456;
		else if constexpr (std::is_same_v<F, FontField>) {
			wcscpy_s(value.lfFaceName, L"Consolas \x00E9");
			value.lfHeight = -15;
			value.lfWeight = 700;
			value.lfItalic = TRUE;
		} else value = field.from((static_cast<int>(value) + 1) % field.count);
	});
	return view;
}

bool SameEverywhere(const ViewSettings& a, const ViewSettings& b) {
	return SameSettings(a, b, SettingsPage::Main) && SameSettings(a, b, SettingsPage::Scrolling) && SameSettings(a, b, SettingsPage::Ratings)
		&& SameSettings(a, b, SettingsPage::PanelMenu);
}

void EverySettingSurvivesASettingsFile() {
	const ViewSettings changed = EverySettingChanged();
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Main));
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Scrolling));
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Ratings));
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::PanelMenu));

	SettingsWriter out;
	WriteViewSettings(out, changed, SettingsPage::Main);
	WriteViewSettings(out, changed, SettingsPage::PanelMenu);
	WriteViewSettings(out, changed, SettingsPage::Ratings);
	WriteViewSettings(out, changed, SettingsPage::Scrolling);
	const std::optional<SettingsEntries> entries = ParseSettingsText(out.Text());
	CHECK(entries.has_value());
	if (!entries) return;
	ViewSettings read;
	ReadViewSettings(*entries, read);
	CHECK(SameEverywhere(read, changed));
}

void ASettingTheFileDoesNotHaveStaysAsItWas() {
	const ViewSettings changed = EverySettingChanged();
	ViewSettings read = changed;
	ReadViewSettings(*ParseSettingsText("format = GroupPlaylist settings\nrowPadding = 999\nlikeStyle = 7\n"), read);
	CHECK(read.rowPadding == kMaxRowPadding);
	CHECK(read.likeStyle == LikeStyle::Emoji);
	read.rowPadding = changed.rowPadding;
	read.likeStyle = changed.likeStyle;
	CHECK(SameEverywhere(read, changed));
}

// A file from before 0.9.2 has the colour but not whether it is the user's own.
void AnOldFileDecidesTheOwnColourFromTheColour() {
	ViewSettings read;
	ReadViewSettings(*ParseSettingsText("format = GroupPlaylist settings\nheaderColor = #112233\nlabelColor = #E67E22\n"), read);
	CHECK(read.customHeaderColor);
	CHECK(!read.customLabelColor);
}

void CopyingAPageCopiesOnlyThatPage() {
	const ViewSettings changed = EverySettingChanged();
	ViewSettings to;
	CopySettings(changed, to, SettingsPage::Ratings);
	CHECK(SameSettings(to, changed, SettingsPage::Ratings));
	CHECK(SameSettings(to, ViewSettings{}, SettingsPage::Main));
	CHECK(SameSettings(to, ViewSettings{}, SettingsPage::Scrolling));
}

// The header menu turned pinning on while the page held an unapplied smooth scrolling edit: the page takes the
// pinning and keeps its edit.
void APageTakesOnlyWhatWasSavedElsewhere() {
	const ViewSettings lastSaved;
	ViewSettings saved = lastSaved;
	saved.pinHeaders = !lastSaved.pinHeaders;
	saved.coverSize = kMaxCoverSize;  // another page's setting
	ViewSettings edited = lastSaved;
	edited.smoothScroll = !lastSaved.smoothScroll;
	CHECK(TakeChangedSettings(saved, lastSaved, edited, SettingsPage::Scrolling));
	CHECK(edited.pinHeaders == saved.pinHeaders);
	CHECK(edited.smoothScroll == !lastSaved.smoothScroll);
	CHECK(edited.coverSize == lastSaved.coverSize);
	CHECK(!TakeChangedSettings(lastSaved, lastSaved, edited, SettingsPage::Scrolling));
}

// The kind of a field, which decides the type of its configuration entry and its form in a settings file.
template <typename F>
const char* KindOf(const F&) {
	using namespace view_settings_fields;
	if constexpr (std::is_same_v<F, BoolField>) return "bool";
	else if constexpr (std::is_same_v<F, IntField>) return "int";
	else if constexpr (std::is_same_v<F, ColorField>) return "color";
	else if constexpr (std::is_same_v<F, FontField>) return "font";
	else if constexpr (std::is_same_v<F, OwnColorField>) return "own color";
	else return "enum";
}

// Earlier versions saved these keys, numbers and kinds, and files they exported hold them: none may ever change.
// A new setting adds a line at its place in the list.
void TheKeysAndStoreNumbersNeverChange() {
	struct Pinned { const char* key; int guidIndex; const char* kind; };
	const Pinned pinned[] = {
		{ "customTrackFont", 2, "bool" }, { "trackFont", 3, "font" }, { "customHeaderFont", 4, "bool" },
		{ "headerFont", 5, "font" }, { "headerColor", 6, "color" }, { "labelColor", 7, "color" },
		{ "customHeaderColor", 29, "own color" }, { "customLabelColor", 30, "own color" }, { "rowPadding", 8, "int" },
		{ "coverSize", 9, "int" }, { "stubForMissingArt", 39, "bool" }, { "presetPerPlaylist", 10, "bool" }, { "scrollPerPlaylist", 13, "bool" },
		{ "columnsPerPlaylist", 37, "bool" }, { "sortOnHeaderClick", 12, "bool" }, { "dragColumns", 25, "bool" },
		{ "animateColumnMoves", 38, "bool" },
		{ "showColumnHeaders", 36, "bool" }, { "dimBrackets", 11, "bool" }, { "showTrackSummary", 26, "bool" },
		{ "trackSummaryPlace", 28, "enum" }, { "boldPlaying", 14, "bool" }, { "customStarColor", 31, "bool" },
		{ "starColor", 32, "color" }, { "likeStyle", 33, "enum" }, { "customLikeColor", 34, "bool" },
		{ "likeColor", 35, "color" }, { "pinHeaders", 16, "bool" }, { "smoothScroll", 1, "bool" },
	};
	size_t index = 0;
	VisitViewSettings([&](const auto& field) {
		if (index < std::size(pinned)) {
			CHECK(std::string(field.key) == pinned[index].key);
			CHECK(field.guidIndex == pinned[index].guidIndex);
			CHECK(std::string(KindOf(field)) == pinned[index].kind);
		}
		++index;
	});
	CHECK(index == std::size(pinned));
}

// An own-colour flag that was never saved is decided from its colour, so loading and reading a file need the colour
// first: every own-colour field comes after the colour field it looks at.
void EveryOwnColourComesAfterItsColour() {
	using namespace view_settings_fields;
	std::vector<COLORREF ViewSettings::*> colorsSoFar;
	VisitViewSettings([&](const auto& field) {
		using F = std::decay_t<decltype(field)>;
		if constexpr (std::is_same_v<F, ColorField>) colorsSoFar.push_back(field.member);
		else if constexpr (std::is_same_v<F, OwnColorField>) {
			CHECK(std::find(colorsSoFar.begin(), colorsSoFar.end(), field.color) != colorsSoFar.end());
		}
	});
}

// A panel shows a new cover size without laying itself out again only when nothing else differs, on any page.
void SameViewSettingsSeesASettingOfEveryPage() {
	const ViewSettings base;
	ViewSettings other = base;
	CHECK(SameViewSettings(base, other));
	other.coverSize = base.coverSize + 1;
	CHECK(!SameViewSettings(base, other));
	other = base;
	other.trackFont.lfHeight = base.trackFont.lfHeight + 5;
	CHECK(!SameViewSettings(base, other));
	other = base;
	other.starColor = base.starColor + 1;
	CHECK(!SameViewSettings(base, other));
	other = base;
	other.smoothScroll = !base.smoothScroll;
	CHECK(!SameViewSettings(base, other));
}

} // namespace

// A setting made in a panel's menu only (the stub image of the cover menu) is on no preferences page, so Reset page
// and Apply of the main page, which copy that page's settings over the saved ones, leave it as it was.
void APageLeavesAPanelMenuSettingAlone() {
	ViewSettings saved;
	saved.stubForMissingArt = true;
	CopySettings(ViewSettings{}, saved, SettingsPage::Main);
	CHECK(saved.stubForMissingArt);
	CHECK(SameSettings(saved, ViewSettings{}, SettingsPage::Main));
}

void RunViewSettingsFieldsTests() {
	APageLeavesAPanelMenuSettingAlone();
	EverySettingIsInTheList();
	KeysAndStoreNumbersAreUniqueAndAvoidTheRetiredOnes();
	EverySettingSurvivesASettingsFile();
	ASettingTheFileDoesNotHaveStaysAsItWas();
	AnOldFileDecidesTheOwnColourFromTheColour();
	CopyingAPageCopiesOnlyThatPage();
	APageTakesOnlyWhatWasSavedElsewhere();
	TheKeysAndStoreNumbersNeverChange();
	EveryOwnColourComesAfterItsColour();
	SameViewSettingsSeesASettingOfEveryPage();
}
