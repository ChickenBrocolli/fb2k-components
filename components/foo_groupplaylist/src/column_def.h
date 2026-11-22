#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "column_places.h"
#include "tab_lines.h"

// Standard library only, so the unit tests build without the foobar2000 SDK.

enum class ColumnAlignment { Left = 0, Center = 1, Right = 2 };

// An alignment stored as a number. An unknown number reads as Left.
inline ColumnAlignment ColumnAlignmentFrom(int number) {
	return number == 1 ? ColumnAlignment::Center : number == 2 ? ColumnAlignment::Right : ColumnAlignment::Left;
}

// Text shows what the pattern produces. Rating shows the track's rating as stars and changes it on a click
// (rating_cell.h). Like shows a heart and likes or unlikes the track on a click. Their patterns are unused, because
// a setting decides where ratings and likes are kept (rating_store.h).
// Duration shows the length, and on the playing track the time played or left (see PlaylistView). Its pattern only
// sorts and is shown on the Columns page.
enum class ColumnKind { Text, Rating, Duration, Like };

// The columns whose cells are clicked to change the track: their look and alignment come from code.
inline bool IsClickColumn(ColumnKind kind) {
	return kind == ColumnKind::Rating || kind == ColumnKind::Like;
}

// The columns whose pattern the user can change. The others draw from code.
inline bool TakesPattern(ColumnKind kind) {
	return kind == ColumnKind::Text;
}

// A width in pixels at 96 DPI (100% display scaling), as widths are saved, so a saved layout looks the same at
// another display scale. PlaylistView::ScreenWidth turns it into pixels on screen.
struct WidthAt96Dpi {
	float value = 0;
	bool operator==(const WidthAt96Dpi&) const = default;
};

// One track column the user can show from the Columns menu.
struct ColumnDef {
	std::string name;
	std::string pattern;
	ColumnAlignment alignment;
	WidthAt96Dpi width;  // when it is turned on
	ColumnKind kind = ColumnKind::Text;
	// A column left out of the Columns menu still shows in panels that have it. Saved with a custom column, and as a
	// list of names for the built-in ones (LoadHiddenBuiltInColumns).
	bool showInMenu = true;
	// Built-in columns only: the name the user gave it. Empty shows the built-in name in the current language.
	// The column is still found by name, so panels keep its place and width.
	std::string title;
};

// Which columns a panel shows, in display order, with their widths.
struct ColumnLayout {
	struct Column {
		std::string name;
		WidthAt96Dpi width;
	};
	WidthAt96Dpi coverWidth = { 106 };
	WidthAt96Dpi infoWidth = { 130 };
	std::vector<Column> columns;
	// Where the cover and info columns sit on screen. They start at the left, as before they could be moved.
	BlockColumnPlaces places;
};

// The built-in columns in the order they come in, one inner list per kind (the user can reorder them, column_store.h):
// playlist, tags, audio, ReplayGain, playback statistics, file.
inline std::vector<std::vector<ColumnDef>> BuiltInColumnGroups() {
	return {
		{
			{ "Playing", "$if(%isplaying%,$if(%ispaused%,$char(9208),$char(9654)),[%queue_index%])", ColumnAlignment::Center, 28 },
		},
		{
			{ "Track", "%tracknumber%", ColumnAlignment::Right, 50 },
			{ "Title", "%title%", ColumnAlignment::Left, 300 },
			{ "Title / track artist", "%title%[ - %track artist%]", ColumnAlignment::Left, 300 },
			{ "Artist", "%artist%", ColumnAlignment::Left, 200 },
			{ "Album artist", "[%album artist%]", ColumnAlignment::Left, 200 },
			{ "Album", "%album%", ColumnAlignment::Left, 200 },
			{ "Date", "%date%", ColumnAlignment::Left, 60 },
			{ "Genre", "[%genre%]", ColumnAlignment::Left, 120 },
			{ "Disc", "[%discnumber%]", ColumnAlignment::Right, 40 },
			{ "Composer", "[%composer%]", ColumnAlignment::Left, 160 },
			{ "Comment", "[%comment%]", ColumnAlignment::Left, 200 },
		},
		{
			{ "Duration", "%length%", ColumnAlignment::Right, 80, ColumnKind::Duration },
			{ "Codec", "%codec%", ColumnAlignment::Left, 60 },
			{ "Bitrate", "[%bitrate% kbps]", ColumnAlignment::Right, 80 },
			{ "Sample rate", "[%samplerate% Hz]", ColumnAlignment::Right, 80 },
		},
		{
			{ "Track gain", "[%replaygain_track_gain%]", ColumnAlignment::Right, 80 },
			{ "Album gain", "[%replaygain_album_gain%]", ColumnAlignment::Right, 80 },
		},
		{
			{ "Rating", "", ColumnAlignment::Left, 80, ColumnKind::Rating },
			{ "Like", "", ColumnAlignment::Center, 40, ColumnKind::Like },
			{ "Played", "[%play_count%]", ColumnAlignment::Right, 55 },
		},
		{
			{ "File name", "%filename_ext%", ColumnAlignment::Left, 200 },
			{ "File path", "%path%", ColumnAlignment::Left, 300 },
		},
	};
}

inline std::vector<ColumnDef> BuiltInColumnDefs() {
	std::vector<ColumnDef> defs;
	for (const std::vector<ColumnDef>& group : BuiltInColumnGroups()) {
		defs.insert(defs.end(), group.begin(), group.end());
	}
	return defs;
}

// Names of the built-in columns left out of the Columns menu, one per line (tab_lines.h).
inline std::string SerializeColumnNames(const std::vector<std::string>& names) {
	std::string out;
	for (const std::string& name : names) out += JoinFieldLine({ name });
	return out;
}

inline std::vector<std::string> ParseColumnNames(const std::string& text) {
	std::vector<std::string> names;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (!fields.empty() && !fields[0].empty()) names.push_back(fields[0]);
	}
	return names;
}

// The columns in the order the user gave them on the Columns page, which is the order of the Columns menu.
// A column the order does not name (a built-in column of a later version, a column from a settings file) comes after
// the named ones, in the place it had. A name no column has is skipped.
inline std::vector<ColumnDef> OrderColumnDefs(const std::vector<ColumnDef>& defs, const std::vector<std::string>& order) {
	std::vector<ColumnDef> ordered;
	std::vector<bool> placed(defs.size(), false);
	for (const std::string& name : order) {
		for (size_t index = 0; index < defs.size(); ++index) {
			if (placed[index] || defs[index].name != name) continue;
			ordered.push_back(defs[index]);
			placed[index] = true;
			break;
		}
	}
	for (size_t index = 0; index < defs.size(); ++index) {
		if (!placed[index]) ordered.push_back(defs[index]);
	}
	return ordered;
}

// The built-in columns, with the named ones left out of the Columns menu.
inline std::vector<ColumnDef> BuiltInColumnDefsForMenu(const std::vector<std::string>& hiddenNames) {
	std::vector<ColumnDef> defs = BuiltInColumnDefs();
	for (ColumnDef& def : defs) {
		def.showInMenu = std::find(hiddenNames.begin(), hiddenNames.end(), def.name) == hiddenNames.end();
	}
	return defs;
}

// What the user changed in one built-in column, found by its name: the shown name (empty keeps the built-in one),
// the pattern and the alignment.
struct BuiltInColumnEdit {
	std::string name;
	std::string title;
	std::string pattern;
	ColumnAlignment alignment;
};

// Only columns that differ from their built-in definition are kept, one per line (tab_lines.h):
// name, title, pattern, alignment number.
inline std::string SerializeBuiltInEdits(const std::vector<BuiltInColumnEdit>& edits) {
	std::string out;
	for (const BuiltInColumnEdit& edit : edits) {
		out += JoinFieldLine({ edit.name, edit.title, edit.pattern, std::to_string(static_cast<int>(edit.alignment)) });
	}
	return out;
}

// A line with fewer than four fields or an empty name is skipped.
inline std::vector<BuiltInColumnEdit> ParseBuiltInEdits(const std::string& text) {
	std::vector<BuiltInColumnEdit> edits;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (fields.size() < 4 || fields[0].empty()) continue;
		edits.push_back({ fields[0], fields[1], fields[2], ColumnAlignmentFrom(std::atoi(fields[3].c_str())) });
	}
	return edits;
}

// The built-in columns with the user's changes. A change for a name no built-in column has is left out.
// Only text columns take a pattern: the Duration column draws the time played or left, and the Rating and Like
// columns' stars and heart come from code. Those two also keep their alignment, which the drawing sets.
inline std::vector<ColumnDef> ApplyBuiltInEdits(std::vector<ColumnDef> defs, const std::vector<BuiltInColumnEdit>& edits) {
	for (ColumnDef& def : defs) {
		for (const BuiltInColumnEdit& edit : edits) {
			if (edit.name != def.name) continue;
			def.title = edit.title;
			if (TakesPattern(def.kind)) def.pattern = edit.pattern;
			if (!IsClickColumn(def.kind)) def.alignment = edit.alignment;
		}
	}
	return defs;
}

inline bool IsEditedBuiltIn(const ColumnDef& def, const ColumnDef& original) {
	return !def.title.empty() || def.pattern != original.pattern || def.alignment != original.alignment;
}

// The changes to keep: the built-in columns in defs that differ from the original of the same name.
inline std::vector<BuiltInColumnEdit> BuiltInEditsFrom(const std::vector<ColumnDef>& originals, const std::vector<ColumnDef>& defs) {
	std::vector<BuiltInColumnEdit> edits;
	for (const ColumnDef& def : defs) {
		for (const ColumnDef& original : originals) {
			if (original.name != def.name || !IsEditedBuiltIn(def, original)) continue;
			edits.push_back({ def.name, def.title, def.pattern, def.alignment });
		}
	}
	return edits;
}

// A new installation starts with no custom columns. The Columns preferences page adds them.
inline std::vector<ColumnDef> DefaultCustomColumnDefs() {
	return {};
}

// What a new panel shows. Album and artist are left out because the group header and the cover text show them.
// The heart sits just before the length, as in most players. A panel set up before keeps its own columns.
inline ColumnLayout DefaultColumnLayout() {
	ColumnLayout layout;
	layout.columns = { { "Playing", 28 }, { "Track", 50 }, { "Title", 300 }, { "Like", 40 }, { "Duration", 70 } };
	return layout;
}

// Text form of a column list (tab_lines.h). The fields are name, pattern, alignment number, width,
// and 1 or 0 for whether the column shows in the Columns menu. Older lists have no fifth field.
inline std::string SerializeColumnDefs(const std::vector<ColumnDef>& defs) {
	std::string out;
	for (const ColumnDef& def : defs) {
		out += JoinFieldLine({ def.name, def.pattern, std::to_string(static_cast<int>(def.alignment)), std::to_string(def.width.value), def.showInMenu ? "1" : "0" });
	}
	return out;
}

// A line with fewer than four fields or an empty name is skipped. An unknown alignment reads as Left.
inline std::vector<ColumnDef> ParseColumnDefs(const std::string& text) {
	std::vector<ColumnDef> defs;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (fields.size() < 4 || fields[0].empty()) continue;

		const float width = static_cast<float>(std::atof(fields[3].c_str()));
		ColumnDef def = { fields[0], fields[1], ColumnAlignmentFrom(std::atoi(fields[2].c_str())), width > 0 ? width : 100.f };
		def.showInMenu = fields.size() < 5 || fields[4] != "0";
		defs.push_back(def);
	}
	return defs;
}

// Text form of a column layout (tab_lines.h), kept with a playlist that remembers its own columns. The first line is
// "layout", the cover and info widths and their places; each further line a track column's name and width.
inline std::string SerializeColumnLayout(const ColumnLayout& layout) {
	std::string out = JoinFieldLine({ "layout", std::to_string(layout.coverWidth.value), std::to_string(layout.infoWidth.value),
		std::to_string(layout.places.cover), std::to_string(layout.places.info) });
	for (const ColumnLayout::Column& column : layout.columns) out += JoinFieldLine({ column.name, std::to_string(column.width.value) });
	return out;
}

// Nothing when the text is not a layout. A column line without a name or a width is skipped.
inline std::optional<ColumnLayout> ParseColumnLayout(const std::string& text) {
	const std::vector<std::vector<std::string>> lines = SplitFieldLines(text);
	if (lines.empty() || lines[0].size() < 5 || lines[0][0] != "layout") return std::nullopt;
	ColumnLayout layout;
	const float cover = static_cast<float>(std::atof(lines[0][1].c_str()));
	const float info = static_cast<float>(std::atof(lines[0][2].c_str()));
	if (cover > 0) layout.coverWidth = { cover };
	if (info > 0) layout.infoWidth = { info };
	layout.places = { static_cast<size_t>(std::strtoul(lines[0][3].c_str(), nullptr, 10)), static_cast<size_t>(std::strtoul(lines[0][4].c_str(), nullptr, 10)) };
	for (size_t index = 1; index < lines.size(); ++index) {
		const std::vector<std::string>& fields = lines[index];
		if (fields.size() < 2 || fields[0].empty()) continue;
		const float width = static_cast<float>(std::atof(fields[1].c_str()));
		layout.columns.push_back({ fields[0], width > 0 ? width : 100.f });
	}
	layout.places = NormalizePlaces(layout.columns.size(), layout.places);
	return layout;
}

// The same columns in the same order and places, with widths within half a pixel: the header reads widths back
// as fractions of a pixel, so a layout that was not touched compares equal to itself.
inline bool SameColumnLayout(const ColumnLayout& a, const ColumnLayout& b) {
	const auto within = [](WidthAt96Dpi x, WidthAt96Dpi y) { return std::fabs(x.value - y.value) < 0.5f; };
	if (a.columns.size() != b.columns.size() || !(a.places == b.places) || !within(a.coverWidth, b.coverWidth) || !within(a.infoWidth, b.infoWidth)) return false;
	for (size_t index = 0; index < a.columns.size(); ++index) {
		if (a.columns[index].name != b.columns[index].name || !within(a.columns[index].width, b.columns[index].width)) return false;
	}
	return true;
}

// A panel layout names its columns, so every column name must be non-empty and unique.
enum class ColumnNameProblem { None, Empty, Duplicate, BuiltIn };

struct ColumnNameCheck {
	ColumnNameProblem problem = ColumnNameProblem::None;
	size_t index = 0;
};

// Checks custom column names against each other and against the reserved names of the built-in columns.
// Reports the first problem, with the index of the custom column that has it.
inline ColumnNameCheck CheckColumnNames(const std::vector<ColumnDef>& customDefs, const std::vector<std::string>& reservedNames) {
	for (size_t index = 0; index < customDefs.size(); ++index) {
		const std::string& name = customDefs[index].name;
		if (name.find_first_not_of(' ') == std::string::npos) return { ColumnNameProblem::Empty, index };
		for (const std::string& reserved : reservedNames) {
			if (name == reserved) return { ColumnNameProblem::BuiltIn, index };
		}
		for (size_t earlier = 0; earlier < index; ++earlier) {
			if (customDefs[earlier].name == name) return { ColumnNameProblem::Duplicate, index };
		}
	}
	return {};
}

// A new version can add a built-in column whose name a custom column already has.
// The custom column gets " (custom)" after its name, repeated until no other column has the result.
// Running it again on its own result changes nothing, so it can run on every load.
inline std::vector<ColumnDef> RenameClashingColumns(std::vector<ColumnDef> customDefs, const std::vector<std::string>& reservedNames) {
	const auto isReserved = [&](const std::string& name) {
		return std::find(reservedNames.begin(), reservedNames.end(), name) != reservedNames.end();
	};
	for (size_t index = 0; index < customDefs.size(); ++index) {
		if (!isReserved(customDefs[index].name)) continue;
		const auto isTaken = [&](const std::string& name) {
			if (isReserved(name)) return true;
			for (size_t other = 0; other < customDefs.size(); ++other) {
				if (other != index && customDefs[other].name == name) return true;
			}
			return false;
		};
		std::string name = customDefs[index].name;
		while (isTaken(name)) name += " (custom)";
		customDefs[index].name = name;
	}
	return customDefs;
}
