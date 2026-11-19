#define NOMINMAX
#include <windows.h>

#include <set>
#include <string>

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
		CHECK(stores.insert(field.store).second);
		CHECK(keptApart.count(field.store) == 0);
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
	return SameSettings(a, b, SettingsPage::Main) && SameSettings(a, b, SettingsPage::Scrolling) && SameSettings(a, b, SettingsPage::Ratings);
}

void EverySettingSurvivesASettingsFile() {
	const ViewSettings changed = EverySettingChanged();
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Main));
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Scrolling));
	CHECK(!SameSettings(changed, ViewSettings{}, SettingsPage::Ratings));

	SettingsWriter out;
	WriteViewSettings(out, changed, SettingsPage::Main);
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

} // namespace

void RunViewSettingsFieldsTests() {
	EverySettingIsInTheList();
	KeysAndStoreNumbersAreUniqueAndAvoidTheRetiredOnes();
	EverySettingSurvivesASettingsFile();
	ASettingTheFileDoesNotHaveStaysAsItWas();
	AnOldFileDecidesTheOwnColourFromTheColour();
	CopyingAPageCopiesOnlyThatPage();
	APageTakesOnlyWhatWasSavedElsewhere();
}
