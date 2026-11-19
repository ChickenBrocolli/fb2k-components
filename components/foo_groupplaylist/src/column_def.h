#pragma once

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "column_places.h"
#include "tab_lines.h"

// Column definitions and the rules for storing and naming them.
// Standard library only, so the unit tests build without the foobar2000 SDK.

enum class ColumnAlignment { Left = 0, Center = 1, Right = 2 };

// Text shows what the pattern produces. Rating shows the track's rating as stars and changes it on a click
// (rating_cell.h). Its pattern is unused, because a setting decides where the rating is kept (rating_store.h).
// Duration shows the length, and on the playing track the time played or left (see PlaylistView).
enum class ColumnKind { Text, Rating, Duration };

// One track column the user can show from the Columns menu.
struct ColumnDef {
	std::string name;
	std::string pattern;
	ColumnAlignment alignment;
	float width;
	ColumnKind kind = ColumnKind::Text;
	// Custom columns only. A column left out of the Columns menu still shows in panels that have it.
	bool showInMenu = true;
};

// Which columns a panel shows, in display order, with their widths.
// All widths are pixels at 96 DPI, so a saved layout looks the same at another display scale.
struct ColumnLayout {
	struct Column {
		std::string name;
		float width;
	};
	float coverWidth = 106;
	float infoWidth = 130;
	std::vector<Column> columns;
	// Where the cover and info columns sit on screen. They start at the left, as before they could be moved.
	BlockColumnPlaces places;
};

// The built-in columns in menu order, one inner list per menu group:
// playlist, tags, audio, ReplayGain, playback statistics, file.
// Which columns are here comes from a survey of other players, not from the original's list. The patterns are standard title formatting.
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

// The built-in columns, with the named ones left out of the Columns menu.
inline std::vector<ColumnDef> BuiltInColumnDefsForMenu(const std::vector<std::string>& hiddenNames) {
	std::vector<ColumnDef> defs = BuiltInColumnDefs();
	for (ColumnDef& def : defs) {
		def.showInMenu = std::find(hiddenNames.begin(), hiddenNames.end(), def.name) == hiddenNames.end();
	}
	return defs;
}

// A new installation starts with no custom columns. The Columns preferences page adds them.
inline std::vector<ColumnDef> DefaultCustomColumnDefs() {
	return {};
}

// What a new panel shows. Album and artist are left out because the group header and the cover text show them.
inline ColumnLayout DefaultColumnLayout() {
	ColumnLayout layout;
	layout.columns = { { "Playing", 28 }, { "Track", 50 }, { "Title", 300 }, { "Duration", 70 } };
	return layout;
}

// Text form of a column list (tab_lines.h). The fields are name, pattern, alignment number, width,
// and 1 or 0 for whether the column shows in the Columns menu. Older lists have no fifth field.
inline std::string SerializeColumnDefs(const std::vector<ColumnDef>& defs) {
	std::string out;
	for (const ColumnDef& def : defs) {
		out += JoinFieldLine({ def.name, def.pattern, std::to_string(static_cast<int>(def.alignment)), std::to_string(def.width), def.showInMenu ? "1" : "0" });
	}
	return out;
}

// A line with fewer than four fields or an empty name is skipped. An unknown alignment reads as Left.
inline std::vector<ColumnDef> ParseColumnDefs(const std::string& text) {
	std::vector<ColumnDef> defs;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (fields.size() < 4 || fields[0].empty()) continue;

		const int alignment = std::atoi(fields[2].c_str());
		const float width = static_cast<float>(std::atof(fields[3].c_str()));
		ColumnDef def = { fields[0], fields[1],
			alignment == 1 ? ColumnAlignment::Center : alignment == 2 ? ColumnAlignment::Right : ColumnAlignment::Left,
			width > 0 ? width : 100.f };
		def.showInMenu = fields.size() < 5 || fields[4] != "0";
		defs.push_back(def);
	}
	return defs;
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
