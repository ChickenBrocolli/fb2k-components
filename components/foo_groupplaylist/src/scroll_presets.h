#pragma once

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include "smooth_scroll_math.h"
#include "tab_lines.h"
#include "view_settings_rules.h"

// Named sets of the five numbers of the Inertia + acceleration scroll style, picked on the Scrolling page.
// Three are built in and cannot be changed; the user saves more. The page always scrolls with its own numbers,
// and a preset only fills them in.
// Standard library only, so the unit tests build without the foobar2000 SDK.

struct ScrollPreset {
	std::string name;
	smooth_scroll::InertiaParams params;

	bool operator==(const ScrollPreset&) const = default;
};

// The English names are the keys (ui_text.h has the Korean ones). The first is the default.
// The glide time acts as the weight of a sledge that a notch pushes: a light one jumps to speed and stops at once,
// a heavy one gets going slowly and keeps rolling.
//   Default  the feel the user picked from the test styles
//   Light    half the glide, hardly any speed-up: easy to place exactly
//   Heavy    nearly three times the glide, a slow notch moves only three quarters, quick notches pile up to five times
inline std::vector<ScrollPreset> BuiltInScrollPresets() {
	return {
		{ "Default", { 90.0, 1.0, 120.0, 0.4, 3.0 } }, // ui-text
		{ "Light", { 45.0, 1.0, 80.0, 0.15, 1.5 } }, // ui-text
		{ "Heavy", { 250.0, 0.75, 180.0, 0.6, 5.0 } }, // ui-text
	};
}

inline constexpr char kDefaultScrollPresetName[] = "Default";

// A number without trailing zeros: 90, 1.5, 0.25. Two decimals at most, as the page shows them.
inline std::string FormatSettingNumber(double value) {
	char text[32];
	std::snprintf(text, sizeof(text), "%.2f", value);
	std::string out = text;
	while (!out.empty() && out.back() == '0') out.pop_back();
	if (!out.empty() && out.back() == '.') out.pop_back();
	return out == "-0" ? "0" : out;
}

// The number in text, or nothing when the whole text is not a number.
inline std::optional<double> ParseSettingNumber(const std::string& text) {
	if (text.empty()) return std::nullopt;
	char* end = nullptr;
	const double value = std::strtod(text.c_str(), &end);
	if (end == text.c_str() || *end != 0) return std::nullopt;
	return value;
}

// The fields of one preset: name, then the five numbers in the order of kInertiaSettings.
inline std::vector<std::string> ScrollPresetFields(const ScrollPreset& preset) {
	std::vector<std::string> fields = { preset.name };
	for (const InertiaSetting& setting : kInertiaSettings) fields.push_back(FormatSettingNumber(preset.params.*setting.field));
	return fields;
}

// A preset from its fields, or nothing when the name is empty or a number is missing. Numbers are pulled into range.
inline std::optional<ScrollPreset> ScrollPresetFromFields(const std::vector<std::string>& fields) {
	if (fields.size() < 1 + std::size(kInertiaSettings) || fields[0].empty()) return std::nullopt;
	ScrollPreset preset{ fields[0], {} };
	for (size_t index = 0; index < std::size(kInertiaSettings); ++index) {
		const std::optional<double> value = ParseSettingNumber(fields[index + 1]);
		if (!value) return std::nullopt;
		preset.params.*kInertiaSettings[index].field = *value;
	}
	preset.params = ClampInertiaParams(preset.params);
	return preset;
}

// Text form of the user's presets (tab_lines.h), one per line.
inline std::string SerializeScrollPresets(const std::vector<ScrollPreset>& presets) {
	std::string out;
	for (const ScrollPreset& preset : presets) out += JoinFieldLine(ScrollPresetFields(preset));
	return out;
}

inline std::optional<size_t> FindScrollPreset(const std::vector<ScrollPreset>& presets, const std::string& name) {
	for (size_t index = 0; index < presets.size(); ++index) {
		if (presets[index].name == name) return index;
	}
	return std::nullopt;
}

inline bool IsBuiltInScrollPreset(const std::string& name) {
	return FindScrollPreset(BuiltInScrollPresets(), name).has_value();
}

// Broken lines and a second preset of the same name are skipped. So is a preset with a built-in name, which a
// settings file edited by hand can hold: the page would list the name twice and could not remove the second one.
inline std::vector<ScrollPreset> ParseScrollPresets(const std::string& text) {
	std::vector<ScrollPreset> presets;
	for (const std::vector<std::string>& fields : SplitFieldLines(text)) {
		const std::optional<ScrollPreset> preset = ScrollPresetFromFields(fields);
		if (!preset || IsBuiltInScrollPreset(preset->name)) continue;
		bool taken = false;
		for (const ScrollPreset& other : presets) taken = taken || other.name == preset->name;
		if (!taken) presets.push_back(*preset);
	}
	return presets;
}

// The built-in presets first, then the user's.
inline std::vector<ScrollPreset> AllScrollPresets(const std::vector<ScrollPreset>& userPresets) {
	std::vector<ScrollPreset> all = BuiltInScrollPresets();
	all.insert(all.end(), userPresets.begin(), userPresets.end());
	return all;
}

// Whether the numbers in use belong to no preset: they differ from those of the preset last picked,
// or that preset is gone. The page and the header menu then show them as "Custom (unsaved)".
// The preset last picked stays remembered, so Overwrite can still save the numbers into it.
inline bool IsCustomScroll(const std::vector<ScrollPreset>& presets, const std::string& presetName, const smooth_scroll::InertiaParams& params) {
	const std::optional<size_t> index = FindScrollPreset(presets, presetName);
	return !index || !(presets[*index].params == params);
}

// A new preset needs a name that no preset has yet. shownBuiltInNames are the built-in names in the current
// language, which are taken too.
enum class ScrollPresetNameProblem { None, Empty, Taken };

inline ScrollPresetNameProblem CheckNewScrollPresetName(const std::string& name, const std::vector<ScrollPreset>& userPresets,
	const std::vector<std::string>& shownBuiltInNames) {
	if (name.find_first_not_of(' ') == std::string::npos) return ScrollPresetNameProblem::Empty;
	if (FindScrollPreset(AllScrollPresets(userPresets), name)) return ScrollPresetNameProblem::Taken;
	for (const std::string& shown : shownBuiltInNames) {
		if (shown == name) return ScrollPresetNameProblem::Taken;
	}
	return ScrollPresetNameProblem::None;
}

// The first of "base 1", "base 2", ... that a new preset can take, offered as its name.
inline std::string FreeScrollPresetName(const std::string& base, const std::vector<ScrollPreset>& userPresets,
	const std::vector<std::string>& shownBuiltInNames) {
	for (size_t number = 1;; ++number) {
		const std::string name = base + " " + std::to_string(number);
		if (CheckNewScrollPresetName(name, userPresets, shownBuiltInNames) == ScrollPresetNameProblem::None) return name;
	}
}
