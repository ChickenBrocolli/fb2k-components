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
#include "view_settings_fields.h"

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
	WriteViewSettings(out, view, SettingsPage::Main);
	WriteViewSettings(out, view, SettingsPage::Ratings);
	out.AddInt("ratingStorage", static_cast<int>(GetRatingStorage()));
	out.AddInt("language", static_cast<int>(GetUiLanguage()));
	out.AddBool("durationCountsDown", GetDurationCountsDown());

	out.Comment("Scrolling");
	WriteViewSettings(out, view, SettingsPage::Scrolling);
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
	out.Comment("Changed built-in columns: name, shown name (empty keeps the built-in one), pattern, alignment");
	out.AddList("builtInColumnEdits", "builtInColumnEdit", Lines(SerializeBuiltInEdits(LoadBuiltInColumnEdits())));

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
	ReadViewSettings(*entries, view);

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
	const auto builtInEdits = in.List("builtInColumnEdits", "builtInColumnEdit");
	// Kept as the Columns page would save them: lines for unknown columns, repeats and unchanged columns drop out.
	if (builtInEdits) {
		const std::vector<ColumnDef> edited = ApplyBuiltInEdits(BuiltInColumnDefs(), ParseBuiltInEdits(JoinLines(*builtInEdits)));
		SaveBuiltInColumnEdits(BuiltInEditsFrom(BuiltInColumnDefs(), edited));
	}
	if (hidden) {
		std::vector<std::string> names;
		for (const std::vector<std::string>& fields : SplitFieldLines(*hidden)) {
			for (const std::string& name : fields) {
				if (!name.empty()) names.push_back(name);
			}
		}
		SaveHiddenBuiltInColumns(names);
	}
	if (hidden || columns || builtInEdits) {
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
