#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "group_preset.h"
#include "tab_lines.h"

// The lines of album information beside a cover, picked with check boxes (the info popup) instead of typed as a pattern.
// Standard library only, so the unit tests build without the foobar2000 SDK.
//
// Without a choice a preset shows its cover pattern as it is, one line per '|' (showCoverText). Once lines are picked
// for a preset, those lines show instead, and the cover pattern only decides which tracks share a cover.
// A picked line is an entry: "@" and the key of an item of the list below, or a line of the preset's cover pattern
// that is not one of those items, kept as it is written there.

struct InfoItem {
	const char* key;
	// The English name, translated where it is shown.
	const char* name;
	// The value, a title formatting pattern for the first track of the cover's tracks.
	const char* pattern;
	// Whether the name is shown before the value. Names, such as the artist, are their own label.
	bool labelled;
};

inline const std::vector<InfoItem>& InfoItems() {
	static const std::vector<InfoItem> items = {
		{ "artist", "Artist", "[%artist%]", false },
		{ "album artist", "Album artist", "[%album artist%]", false },
		{ "album", "Album", "[%album%]", false },
		{ "date", "Year", "[%date%]", true },
		{ "genre", "Genre", "[%genre%]", true },
		{ "label", "Label", "[%label%]", true },
		{ "codec", "Codec", "[%codec%]", true },
		{ "bitrate", "Bitrate", "[%bitrate% kbps]", true },
		{ "samplerate", "Sample rate", "[%samplerate% Hz]", true },
		{ "bitspersample", "Bit depth", "[%bitspersample% bit]", true },
		{ "channels", "Channels", "[%channels%]", true },
		{ "comment", "Comment", "[%comment%]", false },
	};
	return items;
}

inline std::string InfoItemEntry(const InfoItem& item) {
	return std::string("@") + item.key;
}

inline const InfoItem* InfoItemOfEntry(const std::string& entry) {
	if (entry.empty() || entry[0] != '@') return nullptr;
	for (const InfoItem& item : InfoItems()) {
		if (entry.compare(1, std::string::npos, item.key) == 0) return &item;
	}
	return nullptr;
}

// The cover pattern cut at every '|', as the panel cuts the text it gives. A '|' inside brackets cuts there too,
// so a part can be a piece of a pattern, such as "][%date%".
inline std::vector<std::string> PatternLines(const std::string& pattern) {
	std::vector<std::string> lines(1);
	for (const char c : pattern) {
		if (c == '|') lines.emplace_back();
		else lines.back() += c;
	}
	return lines;
}

// The field names in a line, such as "date" in "Year    [%date%]". %% is a literal percent sign.
inline std::vector<std::string> FieldNames(const std::string& line) {
	std::vector<std::string> names;
	size_t at = 0;
	while ((at = line.find('%', at)) != std::string::npos) {
		const size_t end = line.find('%', at + 1);
		if (end == std::string::npos) break;
		if (end > at + 1) names.push_back(line.substr(at + 1, end - at - 1));
		at = end + 1;
	}
	return names;
}

// A line of a cover pattern as a pattern of its own, so it gives its value whatever the other lines give. A '|' inside
// brackets cuts a bracket in two ("[%album%|][%date%]" gives "[%album%" and "][%date%]"): a closing bracket with no
// opening one is left out and an opening one with no closing one is closed, which keeps what the line shows. Quoted
// text ('...') counts as text. Nothing when the line cannot stand alone (a function cut in two, or an open quote).
inline std::optional<std::string> WholePatternOf(const std::string& line) {
	std::string pattern;
	int brackets = 0;
	int parentheses = 0;
	bool quoted = false;
	for (const char c : line) {
		if (c == '\'') {
			quoted = !quoted;
		} else if (!quoted) {
			if (c == '[') {
				++brackets;
			} else if (c == ']') {
				if (brackets == 0) continue;
				--brackets;
			} else if (c == '(') {
				++parentheses;
			} else if (c == ')') {
				if (--parentheses < 0) return std::nullopt;
			}
		}
		pattern += c;
	}
	if (quoted || parentheses != 0) return std::nullopt;
	pattern.append(static_cast<size_t>(brackets), ']');
	return pattern;
}

// The item a line of a cover pattern shows: a line with one field, the field of an item. Anything else is kept as it is.
inline std::string EntryOfPatternLine(const std::string& line) {
	const std::vector<std::string> names = FieldNames(line);
	if (names.size() == 1) {
		for (const InfoItem& item : InfoItems()) {
			if (FieldNames(item.pattern) == names) return InfoItemEntry(item);
		}
	}
	return line;
}

// The lines a preset shows before any are picked: its cover pattern's lines when it shows the cover text, else none.
inline std::vector<std::string> PatternEntries(const std::string& coverPattern, bool showCoverText) {
	std::vector<std::string> entries;
	if (!showCoverText) return entries;
	for (const std::string& line : PatternLines(coverPattern)) entries.push_back(EntryOfPatternLine(line));
	return entries;
}

// One row of the info popup.
struct InfoChoice {
	std::string entry;
	bool shown;
};

// The rows of the info popup: the lines shown, in their order, then every other item, then the lines of the cover
// pattern that are not items and not shown. picked is the choice saved for the preset, if any.
inline std::vector<InfoChoice> InfoChoices(const std::string& coverPattern, bool showCoverText, const std::optional<std::vector<std::string>>& picked) {
	const std::vector<std::string> shown = picked ? *picked : PatternEntries(coverPattern, showCoverText);
	std::vector<InfoChoice> choices;
	const auto listed = [&](const std::string& entry) {
		for (const InfoChoice& choice : choices) {
			if (choice.entry == entry) return true;
		}
		return false;
	};
	for (const std::string& entry : shown) {
		if (!listed(entry)) choices.push_back({ entry, true });
	}
	for (const InfoItem& item : InfoItems()) {
		if (!listed(InfoItemEntry(item))) choices.push_back({ InfoItemEntry(item), false });
	}
	for (const std::string& line : PatternLines(coverPattern)) {
		const std::string entry = EntryOfPatternLine(line);
		if (!line.empty() && !listed(entry)) choices.push_back({ entry, false });
	}
	return choices;
}

// The entries of the rows ticked, in their order.
inline std::vector<std::string> ShownEntries(const std::vector<InfoChoice>& choices) {
	std::vector<std::string> entries;
	for (const InfoChoice& choice : choices) {
		if (choice.shown) entries.push_back(choice.entry);
	}
	return entries;
}

// Whether the row at index may be unticked: one line always stays. To show no info, the info column is hidden instead.
inline bool MayUntick(const std::vector<InfoChoice>& choices, size_t index) {
	if (index >= choices.size() || !choices[index].shown) return true;
	size_t shown = 0;
	for (const InfoChoice& choice : choices) shown += choice.shown ? 1 : 0;
	return shown > 1;
}

// Moves the row at from to the place to, the rows between closing up, as a row dragged in a list.
template <typename T>
void MoveRow(std::vector<T>& rows, size_t from, size_t to) {
	if (from >= rows.size() || to >= rows.size() || from == to) return;
	T moved = std::move(rows[from]);
	rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(from));
	rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
}

// The lines picked for each preset, by preset name. Kept apart from the presets, so a preset list saved by an older
// version reads the same.
using InfoLineChoices = std::vector<std::pair<std::string, std::vector<std::string>>>;

inline std::optional<std::vector<std::string>> PickedInfoLines(const InfoLineChoices& choices, const std::string& presetName) {
	for (const auto& choice : choices) {
		if (SamePresetName(choice.first, presetName)) return choice.second;
	}
	return std::nullopt;
}

// Sets the lines of a preset, or with no lines given drops its choice, so it shows its cover pattern again.
inline void SetPickedInfoLines(InfoLineChoices& choices, const std::string& presetName, const std::optional<std::vector<std::string>>& lines) {
	for (auto choice = choices.begin(); choice != choices.end(); ++choice) {
		if (!SamePresetName(choice->first, presetName)) continue;
		if (lines) choice->second = *lines;
		else choices.erase(choice);
		return;
	}
	if (lines) choices.push_back({ presetName, *lines });
}

// Follows renamed presets (old name, new name) and forgets presets that are gone.
inline InfoLineChoices InfoLinesAfterPresetChange(const InfoLineChoices& choices, const std::vector<GroupPreset>& presets,
	const std::vector<std::pair<std::string, std::string>>& renames) {
	InfoLineChoices kept;
	for (const auto& choice : choices) {
		std::string name = choice.first;
		for (const auto& rename : renames) {
			if (SamePresetName(name, rename.first)) {
				name = rename.second;
				break;
			}
		}
		for (const GroupPreset& preset : presets) {
			if (SamePresetName(preset.name, name)) {
				kept.push_back({ preset.name, choice.second });
				break;
			}
		}
	}
	return kept;
}

// One record per preset: its name, then its lines.
inline std::string SerializeInfoLines(const InfoLineChoices& choices) {
	std::string out;
	for (const auto& choice : choices) {
		std::vector<std::string> fields = { choice.first };
		fields.insert(fields.end(), choice.second.begin(), choice.second.end());
		out += JoinFieldLine(fields);
	}
	return out;
}

inline InfoLineChoices ParseInfoLines(const std::string& text) {
	InfoLineChoices choices;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		if (fields.empty() || fields[0].empty()) continue;
		choices.push_back({ fields[0], { fields.begin() + 1, fields.end() } });
	}
	return choices;
}
