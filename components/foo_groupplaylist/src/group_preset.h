#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "tab_lines.h"

// Group presets and the rules for storing and naming them.
// Standard library only, so the unit tests build without the foobar2000 SDK.

// The preset the Library > GroupPlaylist autoplaylists sorted by play count or date are shown with.
constexpr char kArtistCoverPresetName[] = "Artist / Cover";

// One way to group a playlist, picked from the Groups menu.
//
// headerPattern  Tracks with the same value share an album header line.
// coverPattern   Inside a header, tracks with the same value share one cover image.
// showCoverText  Shows the cover pattern value next to the cover, one line per '|'.
//                The original calls this display type "Front/Text", and "Front" without it.
// sortPattern    The order of "Sort playlist by this preset" and of a click on the cover column header.
// subgroupPatterns  Label rows inside a cover block, such as "Disc 2", outermost level first.
struct GroupPreset {
	std::string name;
	std::string sortPattern;
	std::string headerPattern;
	std::string coverPattern;
	bool showCoverText;
	std::vector<std::string> subgroupPatterns;
	// Hides a label level whose text is the same for the whole group, such as Disc 1 on a one-disc album.
	bool hideUniformLabels = false;
	// A preset left out of the Groups menu can still be the active one.
	bool showInMenu = true;
};

// What a new installation starts with. The Groups preferences page edits the list.
// The presets headed by the artist come first, then those headed by the album; each part runs from the most
// detailed to the plainest. In the names "/" is one level down, "-" shares a line and "+" sits beside.
// Artist - Album / Cover, Artist / Cover + Album Info, Artist / Cover / Album and Album are the original's.
inline std::vector<GroupPreset> DefaultGroupPresets() {
	return {
		{ "Artist / Cover / Album", "%path_sort%",
		  "[%album artist%]", "[%album%]", false, { "['['%date%']' ][%album%]", "[Disc %discnumber%]" } },
		{ "Artist / Cover + Album Info", "%path_sort%",
		  "[%album artist%]", "[%album%|][%date%|][%genre%]", true, { "[Disc %discnumber%]" } },
		{ "Artist - Album / Cover", "%path_sort%",
		  "[%album artist% - ]['['%date%']' ][%album%]", "[%album%]", false, { "[Disc %discnumber%]" }, true },
		// Not in the original. The artist, then a cover per album with its tracks and nothing else: it suits lists
		// sorted by play count or date, where nearly every track starts a group, like a viewing history.
		{ kArtistCoverPresetName, "%path_sort%",
		  "[%album artist%]", "[%album%]", false, {} },
		// Not in the original. The "Album" preset with a label row where the disc changes.
		{ "Album / Disc", "%path_sort%",
		  "[%album%]",
		  "[%artist%]|Year    [%date%]|Codec  [%codec%]|Bitrate  [%bitrate%]kbps|Sample [%samplerate%]Hz|[%comment%]",
		  true, { "[Disc %discnumber%]" }, true },
		{ "Album", "%path_sort%",
		  "[%album%]",
		  "[%artist%]|Year    [%date%]|Codec  [%codec%]|Bitrate  [%bitrate%]kbps|Sample [%samplerate%]Hz|[%comment%]", true, {} },
	};
}

constexpr char kDefaultGroupPresetName[] = "Album";

// Text form of a preset list (tab_lines.h). The fields are name, sort pattern, header pattern,
// cover pattern, "1" or "0" for the cover text, then one field per subgroup pattern.
inline std::string SerializePresets(const std::vector<GroupPreset>& presets) {
	std::string out;
	for (const GroupPreset& preset : presets) {
		// The fifth field holds flags: 1 shows the cover text, 2 hides uniform labels. Older lists hold only 0 or 1.
		const int flags = (preset.showCoverText ? 1 : 0) | (preset.hideUniformLabels ? 2 : 0) | (preset.showInMenu ? 0 : 4);
		std::vector<std::string> fields = { preset.name, preset.sortPattern, preset.headerPattern, preset.coverPattern, std::to_string(flags) };
		fields.insert(fields.end(), preset.subgroupPatterns.begin(), preset.subgroupPatterns.end());
		out += JoinFieldLine(fields);
	}
	return out;
}

// A line with fewer than five fields or an empty name is skipped.
inline std::vector<GroupPreset> ParsePresets(const std::string& text) {
	std::vector<GroupPreset> presets;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (fields.size() < 5 || fields[0].empty()) continue;
		const int flags = std::atoi(fields[4].c_str());
		presets.push_back({ fields[0], fields[1], fields[2], fields[3], (flags & 1) != 0, { fields.begin() + 5, fields.end() }, (flags & 2) != 0, (flags & 4) == 0 });
	}
	return presets;
}

// Preset names are compared without regard to the case of ASCII letters. A panel saves the name of
// its preset, and the shipped names changed case once ("album" became "Album"), so a panel saved
// with the old spelling still finds its preset.
inline bool SamePresetName(const std::string& a, const std::string& b) {
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	if (a.size() != b.size()) return false;
	for (size_t index = 0; index < a.size(); ++index) {
		if (lower(a[index]) != lower(b[index])) return false;
	}
	return true;
}

// A shipped preset that was renamed is still found by its old name ("Album + Disc" became "Album / Disc").
inline std::optional<size_t> FindPresetByName(const std::vector<GroupPreset>& presets, const std::string& name) {
	for (size_t index = 0; index < presets.size(); ++index) {
		if (SamePresetName(presets[index].name, name)) return index;
	}
	if (SamePresetName(name, "Album + Disc")) return FindPresetByName(presets, "Album / Disc");
	return std::nullopt;
}

enum class PresetNameProblem { None, Empty, Duplicate };

struct PresetNameCheck {
	PresetNameProblem problem = PresetNameProblem::None;
	size_t index = 0;
};

// Every preset needs a name that no other preset has. Reports the first problem, with the index of the preset that has it.
inline PresetNameCheck CheckPresetNames(const std::vector<GroupPreset>& presets) {
	for (size_t index = 0; index < presets.size(); ++index) {
		const std::string& name = presets[index].name;
		if (name.find_first_not_of(' ') == std::string::npos) return { PresetNameProblem::Empty, index };
		for (size_t earlier = 0; earlier < index; ++earlier) {
			if (SamePresetName(presets[earlier].name, name)) return { PresetNameProblem::Duplicate, index };
		}
	}
	return {};
}
