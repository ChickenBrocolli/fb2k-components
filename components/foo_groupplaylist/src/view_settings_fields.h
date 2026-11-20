#pragma once

// Every setting of ViewSettings, listed once. The foobar2000 configuration (view_settings.cpp), the settings file
// (settings_transfer.cpp) and the preferences pages that compare or copy their part all go through this list, so a
// setting added here is saved, exported, imported and compared everywhere, and one left out of here is caught by
// the unit tests (view_settings_fields_tests.cpp).
//
// Each entry keeps the key it has always had in a settings file and the number of its configuration entry
// (SettingGuid), so settings saved by earlier versions, and files they exported, still read.
// Needs <windows.h> (LOGFONTW, COLORREF) but not the foobar2000 SDK, so the unit tests can build it.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>

#include "settings_file.h"
#include "tab_lines.h"
#include "view_settings.h"

// The preferences page a setting is on. Each page compares and saves only its own part.
enum class SettingsPage { Main, Scrolling, Ratings };

namespace view_settings_fields {

struct BoolField {
	const char* key;
	uint8_t store;  // SettingGuid number
	SettingsPage page;
	bool ViewSettings::* member;
};

struct IntField {
	const char* key;
	uint8_t store;
	SettingsPage page;
	int ViewSettings::* member;
	int minimum, maximum;
};

struct ColorField {
	const char* key;
	uint8_t store;
	SettingsPage page;
	COLORREF ViewSettings::* member;
};

struct FontField {
	const char* key;
	uint8_t store;
	SettingsPage page;
	LOGFONTW ViewSettings::* member;
};

// Stored and written as its number; a number outside the list reads as from() decides.
template <typename E>
struct EnumField {
	const char* key;
	uint8_t store;
	SettingsPage page;
	E ViewSettings::* member;
	int count;
	E (*from)(int);
};

// Whether a text colour is the user's own. Saved as -1 until saved once, and a settings file from before 0.9.2
// has the colour but not this; then UsesOwnTextColor decides from the colour, which comes before it in the list.
struct OwnColorField {
	const char* key;
	uint8_t store;
	SettingsPage page;
	bool ViewSettings::* member;
	COLORREF ViewSettings::* color;
	const char* colorKey;
};

template <typename T> struct IsEnumField : std::false_type {};
template <typename E> struct IsEnumField<EnumField<E>> : std::true_type {};

} // namespace view_settings_fields

// Calls visit with every field, in the order of a settings file. SettingGuid 15 and 17 to 24 hold settings kept
// apart from ViewSettings (view_settings.cpp); 27 was a switch of an unreleased build and stays unused.
template <typename Visit>
void VisitViewSettings(Visit&& visit) {
	using namespace view_settings_fields;
	using Page = SettingsPage;
	visit(BoolField{ "customTrackFont", 2, Page::Main, &ViewSettings::customTrackFont });
	visit(FontField{ "trackFont", 3, Page::Main, &ViewSettings::trackFont });
	visit(BoolField{ "customHeaderFont", 4, Page::Main, &ViewSettings::customHeaderFont });
	visit(FontField{ "headerFont", 5, Page::Main, &ViewSettings::headerFont });
	visit(ColorField{ "headerColor", 6, Page::Main, &ViewSettings::headerColor });
	visit(ColorField{ "labelColor", 7, Page::Main, &ViewSettings::labelColor });
	visit(OwnColorField{ "customHeaderColor", 29, Page::Main, &ViewSettings::customHeaderColor, &ViewSettings::headerColor, "headerColor" });
	visit(OwnColorField{ "customLabelColor", 30, Page::Main, &ViewSettings::customLabelColor, &ViewSettings::labelColor, "labelColor" });
	visit(IntField{ "rowPadding", 8, Page::Main, &ViewSettings::rowPadding, kMinRowPadding, kMaxRowPadding });
	visit(IntField{ "coverSize", 9, Page::Main, &ViewSettings::coverSize, kMinCoverSize, kMaxCoverSize });
	visit(BoolField{ "presetPerPlaylist", 10, Page::Main, &ViewSettings::presetPerPlaylist });
	visit(BoolField{ "scrollPerPlaylist", 13, Page::Main, &ViewSettings::scrollPerPlaylist });
	visit(BoolField{ "sortOnHeaderClick", 12, Page::Main, &ViewSettings::sortOnHeaderClick });
	visit(BoolField{ "dragColumns", 25, Page::Main, &ViewSettings::dragColumns });
	visit(BoolField{ "showColumnHeaders", 36, Page::Main, &ViewSettings::showColumnHeaders });
	visit(BoolField{ "dimBrackets", 11, Page::Main, &ViewSettings::dimBrackets });
	visit(BoolField{ "showTrackSummary", 26, Page::Main, &ViewSettings::showTrackSummary });
	visit(EnumField<TrackSummaryPlace>{ "trackSummaryPlace", 28, Page::Main, &ViewSettings::trackSummaryPlace, kTrackSummaryPlaceCount, TrackSummaryPlaceFrom });
	visit(BoolField{ "boldPlaying", 14, Page::Main, &ViewSettings::boldPlaying });
	visit(BoolField{ "customStarColor", 31, Page::Ratings, &ViewSettings::customStarColor });
	visit(ColorField{ "starColor", 32, Page::Ratings, &ViewSettings::starColor });
	visit(EnumField<LikeStyle>{ "likeStyle", 33, Page::Ratings, &ViewSettings::likeStyle, kLikeStyleCount, LikeStyleFrom });
	visit(BoolField{ "customLikeColor", 34, Page::Ratings, &ViewSettings::customLikeColor });
	visit(ColorField{ "likeColor", 35, Page::Ratings, &ViewSettings::likeColor });
	visit(BoolField{ "pinHeaders", 16, Page::Scrolling, &ViewSettings::pinHeaders });
	visit(BoolField{ "smoothScroll", 1, Page::Scrolling, &ViewSettings::smoothScroll });
}

namespace view_settings_fields {

// Whether one setting is the same in both.
template <typename F>
bool SameField(const ViewSettings& a, const ViewSettings& b, const F& field) {
	if constexpr (std::is_same_v<F, FontField>) return std::memcmp(&(a.*field.member), &(b.*field.member), sizeof(LOGFONTW)) == 0;
	else return a.*field.member == b.*field.member;
}

} // namespace view_settings_fields

// Whether the settings of one page are the same in both.
inline bool SameSettings(const ViewSettings& a, const ViewSettings& b, SettingsPage page) {
	bool same = true;
	VisitViewSettings([&](const auto& field) {
		if (field.page == page) same = same && view_settings_fields::SameField(a, b, field);
	});
	return same;
}

// Copies the settings of one page.
inline void CopySettings(const ViewSettings& from, ViewSettings& to, SettingsPage page) {
	VisitViewSettings([&](const auto& field) {
		if (field.page == page) to.*field.member = from.*field.member;
	});
}

// A page holding unapplied edits takes, of its own settings, only those saved elsewhere since it last looked (the
// panel header menu, Ctrl+wheel on the covers), so its Apply does not put the old value back and its other edits
// stay. lastSaved is what was saved when it last looked. Returns whether it took any.
inline bool TakeChangedSettings(const ViewSettings& saved, const ViewSettings& lastSaved, ViewSettings& edited, SettingsPage page) {
	bool taken = false;
	VisitViewSettings([&](const auto& field) {
		if (field.page != page || view_settings_fields::SameField(saved, lastSaved, field)) return;
		edited.*field.member = saved.*field.member;
		taken = true;
	});
	return taken;
}

namespace view_settings_fields {

inline std::string Utf8(const wchar_t* text) {
	const int size = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 1) return {};
	std::string out(static_cast<size_t>(size), '\0');
	::WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
	out.pop_back();  // the terminating zero
	return out;
}

inline std::wstring Wide(const std::string& text) {
	const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
	if (size <= 1) return {};
	std::wstring out(static_cast<size_t>(size), L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
	out.pop_back();
	return out;
}

// A font as face, height at 96 DPI, weight and italic, separated by tabs.
inline std::string FontText(const LOGFONTW& font) {
	std::string line = JoinFieldLine({ Utf8(font.lfFaceName), std::to_string(font.lfHeight), std::to_string(font.lfWeight), font.lfItalic ? "1" : "0" });
	line.pop_back();  // the line break
	return line;
}

// The font of the text over base, or nothing when the text is not a font.
inline std::optional<LOGFONTW> ParseFont(const std::string& text, LOGFONTW font) {
	const std::vector<std::vector<std::string>> lines = SplitFieldLines(text);
	if (lines.empty() || lines[0].size() < 4 || lines[0][0].empty()) return std::nullopt;
	const std::vector<std::string>& fields = lines[0];
	const std::optional<long long> height = ParseSettingInt(fields[1]);
	const std::optional<long long> weight = ParseSettingInt(fields[2]);
	if (!height || !weight || *height == 0 || std::llabs(*height) > 200 || *weight < 0 || *weight > 1000) return std::nullopt;
	wcsncpy_s(font.lfFaceName, Wide(fields[0]).c_str(), _TRUNCATE);
	font.lfHeight = static_cast<LONG>(*height);
	font.lfWeight = static_cast<LONG>(*weight);
	font.lfItalic = fields[3] == "1" ? TRUE : FALSE;
	return font;
}

} // namespace view_settings_fields

// Writes the settings of one page to a settings file.
inline void WriteViewSettings(SettingsWriter& out, const ViewSettings& view, SettingsPage page) {
	using namespace view_settings_fields;
	VisitViewSettings([&](const auto& field) {
		using F = std::decay_t<decltype(field)>;
		if (field.page != page) return;
		const auto& value = view.*field.member;
		if constexpr (std::is_same_v<F, BoolField> || std::is_same_v<F, OwnColorField>) out.AddBool(field.key, value);
		else if constexpr (std::is_same_v<F, IntField>) out.AddInt(field.key, value);
		else if constexpr (std::is_same_v<F, ColorField>) out.Add(field.key, FormatSettingColor(value));
		else if constexpr (std::is_same_v<F, FontField>) out.Add(field.key, FontText(value));
		else out.AddInt(field.key, static_cast<int>(value));
	});
}

// Reads every setting the file has over view. A setting the file does not have, or has in a form that cannot be
// read, stays as it is. A number outside its range is pulled into it.
inline void ReadViewSettings(const SettingsEntries& entries, ViewSettings& view) {
	using namespace view_settings_fields;
	VisitViewSettings([&](const auto& field) {
		using F = std::decay_t<decltype(field)>;
		const std::optional<std::string> text = SettingValue(entries, field.key);
		if constexpr (std::is_same_v<F, OwnColorField>) {
			if (!text) {
				if (SettingValue(entries, field.colorKey)) view.*field.member = UsesOwnTextColor(-1, view.*field.color);
				return;
			}
		}
		if (!text) return;
		if constexpr (std::is_same_v<F, BoolField> || std::is_same_v<F, OwnColorField>) {
			if (const auto value = ParseSettingBool(*text)) view.*field.member = *value;
		} else if constexpr (std::is_same_v<F, IntField>) {
			if (const auto value = ParseSettingInt(*text)) {
				view.*field.member = static_cast<int>(*value < field.minimum ? field.minimum : *value > field.maximum ? field.maximum : *value);
			}
		} else if constexpr (std::is_same_v<F, ColorField>) {
			if (const auto value = ParseSettingColor(*text)) view.*field.member = static_cast<COLORREF>(*value);
		} else if constexpr (std::is_same_v<F, FontField>) {
			if (const auto value = ParseFont(*text, view.*field.member)) view.*field.member = *value;
		} else {
			static_assert(IsEnumField<F>::value);
			if (const auto value = ParseSettingInt(*text)) {
				const long long number = *value < 0 ? 0 : *value >= field.count ? field.count - 1 : *value;
				view.*field.member = field.from(static_cast<int>(number));
			}
		}
	});
}
