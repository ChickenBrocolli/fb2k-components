#include "stdafx.h"

#include "settings_transfer.h"

#include <fstream>
#include <iterator>
#include <sstream>

#include <commdlg.h>

#include "column_store.h"
#include "playlist_view.h"
#include "preset_store.h"
#include "rating_store.h"
#include "settings_file.h"
#include "ui_language.h"
#include "view_settings.h"

namespace {

// The key of each smooth scrolling number, in the order of kInertiaSettings.
const char* const kInertiaKeys[] = { "glideMs", "notchDistance", "quickTurnMs", "speedUpStep", "maxSpeedUp" };
static_assert(std::size(kInertiaKeys) == std::size(kInertiaSettings));

// The lines of a tab_lines.h text, without their line breaks.
std::vector<std::string> Lines(const std::string& text) {
	std::vector<std::string> lines;
	std::istringstream stream(text);
	for (std::string line; std::getline(stream, line);) {
		if (!line.empty()) lines.push_back(line);
	}
	return lines;
}

std::string JoinLines(const std::vector<std::string>& lines) {
	std::string text;
	for (const std::string& line : lines) text += line + '\n';
	return text;
}

std::string Utf8(const wchar_t* text) {
	return pfc::stringcvt::string_utf8_from_wide(text).get_ptr();
}

// A font as face, height at 96 DPI, weight and italic, separated by tabs.
std::string FontText(const LOGFONTW& font) {
	std::string line = JoinFieldLine({ Utf8(font.lfFaceName), std::to_string(font.lfHeight), std::to_string(font.lfWeight), font.lfItalic ? "1" : "0" });
	line.pop_back();  // the line break
	return line;
}

std::optional<LOGFONTW> ParseFont(const std::string& text, LOGFONTW font) {
	const std::vector<std::vector<std::string>> lines = SplitFieldLines(text);
	if (lines.empty() || lines[0].size() < 4 || lines[0][0].empty()) return std::nullopt;
	const std::vector<std::string>& fields = lines[0];
	const std::optional<long long> height = ParseSettingInt(fields[1]);
	const std::optional<long long> weight = ParseSettingInt(fields[2]);
	if (!height || !weight || *height == 0 || std::abs(*height) > 200 || *weight < 0 || *weight > 1000) return std::nullopt;
	wcsncpy_s(font.lfFaceName, pfc::stringcvt::string_wide_from_utf8(fields[0].c_str()).get_ptr(), _TRUNCATE);
	font.lfHeight = static_cast<LONG>(*height);
	font.lfWeight = static_cast<LONG>(*weight);
	font.lfItalic = fields[3] == "1" ? TRUE : FALSE;
	return font;
}

class Reader {
public:
	explicit Reader(const SettingsEntries& entries) : m_entries(entries) {}

	void Bool(const char* key, bool& field) const {
		if (const auto text = SettingValue(m_entries, key)) {
			if (const auto value = ParseSettingBool(*text)) field = *value;
		}
	}
	void Int(const char* key, int& field, int minimum, int maximum) const {
		if (const auto text = SettingValue(m_entries, key)) {
			if (const auto value = ParseSettingInt(*text)) field = static_cast<int>(std::clamp<long long>(*value, minimum, maximum));
		}
	}
	void Color(const char* key, COLORREF& field) const {
		if (const auto text = SettingValue(m_entries, key)) {
			if (const auto value = ParseSettingColor(*text)) field = static_cast<COLORREF>(*value);
		}
	}
	void Font(const char* key, LOGFONTW& field) const {
		if (const auto text = SettingValue(m_entries, key)) {
			if (const auto value = ParseFont(*text, field)) field = *value;
		}
	}
	void Number(const char* key, double& field) const {
		if (const auto text = SettingValue(m_entries, key)) {
			if (const auto value = ParseSettingNumber(*text)) field = *value;
		}
	}
	std::optional<std::string> Text(const char* key) const { return SettingValue(m_entries, key); }
	std::optional<std::vector<std::string>> List(const char* countKey, const char* itemKey) const { return SettingList(m_entries, countKey, itemKey); }

private:
	const SettingsEntries& m_entries;
};

} // namespace

std::string ExportSettingsText() {
	const ViewSettings view = LoadViewSettings();
	SettingsWriter out;
	out.Comment("Look and behaviour");
	out.AddBool("customTrackFont", view.customTrackFont);
	out.Add("trackFont", FontText(view.trackFont));
	out.AddBool("customHeaderFont", view.customHeaderFont);
	out.Add("headerFont", FontText(view.headerFont));
	out.Add("headerColor", FormatSettingColor(view.headerColor));
	out.Add("labelColor", FormatSettingColor(view.labelColor));
	out.AddInt("rowPadding", view.rowPadding);
	out.AddInt("coverSize", view.coverSize);
	out.AddInt("ratingStorage", static_cast<int>(GetRatingStorage()));
	out.AddInt("language", static_cast<int>(GetUiLanguage()));
	out.AddBool("presetPerPlaylist", view.presetPerPlaylist);
	out.AddBool("scrollPerPlaylist", view.scrollPerPlaylist);
	out.AddBool("sortOnHeaderClick", view.sortOnHeaderClick);
	out.AddBool("dragColumns", view.dragColumns);
	out.AddBool("dimBrackets", view.dimBrackets);
	out.AddBool("boldPlaying", view.boldPlaying);
	out.AddBool("durationCountsDown", GetDurationCountsDown());

	out.Comment("Scrolling");
	out.AddBool("pinHeaders", view.pinHeaders);
	out.AddBool("smoothScroll", view.smoothScroll);
	const smooth_scroll::InertiaParams params = LoadInertiaParams();
	for (size_t index = 0; index < std::size(kInertiaSettings); ++index) {
		out.Add(kInertiaKeys[index], FormatSettingNumber(params.*kInertiaSettings[index].field));
	}
	out.Add("scrollPreset", LoadScrollPresetName());
	out.AddList("userScrollPresets", "userScrollPreset", Lines(SerializeScrollPresets(LoadUserScrollPresets())));

	out.Comment("Columns: name, pattern, alignment (0 left, 1 center, 2 right), width, shown in the menu");
	std::string hidden;
	for (const std::string& name : LoadHiddenBuiltInColumns()) hidden += (hidden.empty() ? "" : "\t") + name;
	out.Add("hiddenBuiltInColumns", hidden);
	out.AddList("customColumns", "column", Lines(SerializeColumnDefs(LoadCustomColumnDefs())));

	out.Comment("Group presets: name, sort, header, cover, flags, labels");
	out.AddList("groupPresets", "groupPreset", Lines(SerializePresets(LoadGroupPresets())));
	return out.Text();
}

bool IsSettingsText(const std::string& text) {
	return ParseSettingsText(text).has_value();
}

bool ImportSettingsText(const std::string& text) {
	const std::optional<SettingsEntries> entries = ParseSettingsText(text);
	if (!entries) return false;
	const Reader in(*entries);

	ViewSettings view = LoadViewSettings();
	in.Bool("customTrackFont", view.customTrackFont);
	in.Font("trackFont", view.trackFont);
	in.Bool("customHeaderFont", view.customHeaderFont);
	in.Font("headerFont", view.headerFont);
	in.Color("headerColor", view.headerColor);
	in.Color("labelColor", view.labelColor);
	in.Int("rowPadding", view.rowPadding, kMinRowPadding, kMaxRowPadding);
	in.Int("coverSize", view.coverSize, kMinCoverSize, kMaxCoverSize);
	in.Bool("presetPerPlaylist", view.presetPerPlaylist);
	in.Bool("scrollPerPlaylist", view.scrollPerPlaylist);
	in.Bool("sortOnHeaderClick", view.sortOnHeaderClick);
	in.Bool("dragColumns", view.dragColumns);
	in.Bool("dimBrackets", view.dimBrackets);
	in.Bool("boldPlaying", view.boldPlaying);
	in.Bool("pinHeaders", view.pinHeaders);
	in.Bool("smoothScroll", view.smoothScroll);

	bool countsDown = GetDurationCountsDown();
	in.Bool("durationCountsDown", countsDown);
	SetDurationCountsDown(countsDown);

	int language = static_cast<int>(GetUiLanguage());
	in.Int("language", language, 0, 2);
	SetUiLanguage(static_cast<UiLanguage>(language));

	smooth_scroll::InertiaParams params = LoadInertiaParams();
	for (size_t index = 0; index < std::size(kInertiaSettings); ++index) in.Number(kInertiaKeys[index], params.*kInertiaSettings[index].field);
	SaveInertiaParams(params);
	if (const auto users = in.List("userScrollPresets", "userScrollPreset")) SaveUserScrollPresets(ParseScrollPresets(JoinLines(*users)));
	if (const auto name = in.Text("scrollPreset")) SaveScrollPresetName(*name);

	// Custom columns: a name that a built-in column or an earlier custom column has is left out.
	const auto hidden = in.Text("hiddenBuiltInColumns");
	const auto columns = in.List("customColumns", "column");
	if (hidden) {
		std::vector<std::string> names;
		for (const std::vector<std::string>& fields : SplitFieldLines(*hidden)) {
			for (const std::string& name : fields) {
				if (!name.empty()) names.push_back(name);
			}
		}
		SaveHiddenBuiltInColumns(names);
	}
	if (hidden || columns) {
		std::vector<ColumnDef> custom = columns ? std::vector<ColumnDef>() : LoadCustomColumnDefs();
		if (columns) {
			std::vector<std::string> taken;
			for (const ColumnDef& def : BuiltInColumnDefs()) taken.push_back(def.name);
			for (const ColumnDef& def : ParseColumnDefs(JoinLines(*columns))) {
				if (std::find(taken.begin(), taken.end(), def.name) != taken.end()) continue;
				taken.push_back(def.name);
				custom.push_back(def);
			}
		}
		PlaylistView::ApplyCustomColumns(custom, {});
	}

	// Group presets: a list needs at least one preset, and a name that is there already is left out.
	if (const auto lines = in.List("groupPresets", "groupPreset")) {
		std::vector<GroupPreset> presets;
		for (const GroupPreset& preset : ParsePresets(JoinLines(*lines))) {
			if (!FindPresetByName(presets, preset.name)) presets.push_back(preset);
		}
		if (!presets.empty()) PlaylistView::ApplyGroupPresets(presets, {});
	}

	int rating = static_cast<int>(GetRatingStorage());
	in.Int("ratingStorage", rating, 0, 2);
	if (static_cast<RatingStorage>(rating) != GetRatingStorage()) PlaylistView::ApplyRatingStorage(static_cast<RatingStorage>(rating));

	// Last, so the panels redraw with everything above, in the new language.
	PlaylistView::ApplyViewSettings(view);
	return true;
}

std::optional<std::wstring> AskSettingsFile(HWND parent, bool save) {
	wchar_t path[MAX_PATH] = {};
	if (save) wcscpy_s(path, L"GroupPlaylist settings.txt");
	std::wstring filter = TrW("Text files");
	filter += L" (*.txt)";
	filter += L'\0';
	filter += L"*.txt";
	filter += L'\0';
	filter += TrW("All files");
	filter += L" (*.*)";
	filter += L'\0';
	filter += L"*.*";
	filter += L'\0';
	OPENFILENAMEW dialog = { sizeof(dialog) };
	dialog.hwndOwner = parent;
	dialog.lpstrFilter = filter.c_str();
	dialog.lpstrFile = path;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrDefExt = L"txt";
	dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
	const BOOL chosen = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
	if (!chosen) return std::nullopt;
	return std::wstring(path);
}

bool WriteSettingsFile(const std::wstring& path, const std::string& text) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	file.close();
	return !file.fail();
}

// A settings file is small; anything over a megabyte is not one.
std::optional<std::string> ReadSettingsFile(const std::wstring& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) return std::nullopt;
	std::string text;
	char buffer[4096];
	while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
		text.append(buffer, static_cast<size_t>(file.gcount()));
		if (text.size() > 1024 * 1024) return std::nullopt;
	}
	return text;
}
