#include <string>
#include <vector>

#include "check.h"
#include "group_preset.h"

namespace {

void CheckSamePreset(const GroupPreset& actual, const GroupPreset& expected) {
	CHECK(actual.name == expected.name);
	CHECK(actual.sortPattern == expected.sortPattern);
	CHECK(actual.headerPattern == expected.headerPattern);
	CHECK(actual.coverPattern == expected.coverPattern);
	CHECK(actual.showCoverText == expected.showCoverText);
	CHECK(actual.subgroupPatterns == expected.subgroupPatterns);
	CHECK(actual.hideUniformLabels == expected.hideUniformLabels);
}

void APresetListSurvivesSerializeAndParse() {
	const std::vector<GroupPreset> presets = {
		{ "Plain", "", "[%album%]", "", false, {} },
		{ "With text", "%path_sort%", "[%album artist%]", "[%album%|][%date%]", true, { "[Disc %discnumber%]" } },
		{ "Two levels", "%path_sort%", "", "[%album%]", false, { "[%date%]", "[Disc %discnumber%]" } },
	};
	const std::vector<GroupPreset> parsed = ParsePresets(SerializePresets(presets));
	CHECK(parsed.size() == presets.size());
	for (size_t index = 0; index < parsed.size() && index < presets.size(); ++index) {
		CheckSamePreset(parsed[index], presets[index]);
	}
}

void TheDefaultPresetsSurviveARoundTrip() {
	const std::vector<GroupPreset> defaults = DefaultGroupPresets();
	const std::vector<GroupPreset> parsed = ParsePresets(SerializePresets(defaults));
	CHECK(parsed.size() == defaults.size());
	for (size_t index = 0; index < parsed.size() && index < defaults.size(); ++index) {
		CheckSamePreset(parsed[index], defaults[index]);
	}
}

void AMalformedPresetLineIsSkippedAndTheRestIsKept() {
	const std::vector<GroupPreset> parsed = ParsePresets("too\tfew\tfields\n\tsort\theader\tcover\t1\nGood\tsort\theader\tcover\t1\tlabel\n");
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].name == "Good");
	CHECK(parsed[0].showCoverText);
	CHECK(parsed[0].subgroupPatterns == std::vector<std::string>{ "label" });
}

void APresetIsFoundByNameWhateverTheLetterCase() {
	const std::vector<GroupPreset> presets = DefaultGroupPresets();
	const std::optional<size_t> found = FindPresetByName(presets, "album + disc");
	CHECK(found.has_value());
	if (found) CHECK(presets[*found].name == "Album + Disc");
	CHECK(!FindPresetByName(presets, "Album + Dis").has_value());
	CHECK(!FindPresetByName(presets, "No such preset").has_value());
}

// Panels saved before the names were capitalized hold these spellings.
void EveryOldPresetNameStillFindsItsPreset() {
	const std::vector<GroupPreset> presets = DefaultGroupPresets();
	const char* const oldNames[] = { "Artist - album / cover", "Artist / cover + album info", "Artist / cover / album", "album", "album + disc" };
	for (size_t index = 0; index < std::size(oldNames); ++index) {
		const std::optional<size_t> found = FindPresetByName(presets, oldNames[index]);
		CHECK(found.has_value());
		if (found) CHECK(*found == index);
	}
}

std::vector<GroupPreset> Named(const std::vector<std::string>& names) {
	std::vector<GroupPreset> presets;
	for (const std::string& name : names) presets.push_back({ name, "", "", "", false, {} });
	return presets;
}

void DistinctPresetNamesPassTheNameCheck() {
	CHECK(CheckPresetNames(Named({ "Album", "Album + Disc" })).problem == PresetNameProblem::None);
	CHECK(CheckPresetNames(DefaultGroupPresets()).problem == PresetNameProblem::None);
}

void AnEmptyOrBlankPresetNameIsReported() {
	const PresetNameCheck check = CheckPresetNames(Named({ "Album", "  " }));
	CHECK(check.problem == PresetNameProblem::Empty);
	CHECK(check.index == 1);
}

void PresetNamesThatDifferOnlyInLetterCaseAreDuplicates() {
	const PresetNameCheck check = CheckPresetNames(Named({ "Album", "Other", "album" }));
	CHECK(check.problem == PresetNameProblem::Duplicate);
	CHECK(check.index == 2);
}

void TheHideUniformLabelsFlagSurvivesARoundTrip() {
	const std::vector<GroupPreset> presets = {
		{ "Both", "", "[%album%]", "", true, { "[Disc %discnumber%]" }, true },
		{ "Neither", "", "[%album%]", "", false, {}, false },
		{ "Hide only", "", "[%album%]", "", false, { "x" }, true },
	};
	const std::vector<GroupPreset> parsed = ParsePresets(SerializePresets(presets));
	CHECK(parsed.size() == 3);
	if (parsed.size() != 3) return;
	CHECK(parsed[0].showCoverText && parsed[0].hideUniformLabels);
	CHECK(!parsed[1].showCoverText && !parsed[1].hideUniformLabels);
	CHECK(!parsed[2].showCoverText && parsed[2].hideUniformLabels);
}

// Lists saved before the flag existed hold "1" or "0" in that field.
void AnOldPresetLineReadsWithTheFlagOff() {
	const std::vector<GroupPreset> parsed = ParsePresets("Old\t\t[%album%]\tcover\t1\t[Disc %discnumber%]\n");
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].showCoverText);
	CHECK(!parsed[0].hideUniformLabels);
	CHECK(parsed[0].showInMenu);
}

void TheShowInMenuFlagOfAPresetSurvivesARoundTrip() {
	std::vector<GroupPreset> presets = { { "Shown", "", "[%album%]", "", true, {}, true }, { "Hidden", "", "[%album%]", "", false, {}, false } };
	presets[1].showInMenu = false;
	const std::vector<GroupPreset> parsed = ParsePresets(SerializePresets(presets));
	CHECK(parsed.size() == 2);
	if (parsed.size() != 2) return;
	CHECK(parsed[0].showInMenu && parsed[0].showCoverText && parsed[0].hideUniformLabels);
	CHECK(!parsed[1].showInMenu && !parsed[1].showCoverText && !parsed[1].hideUniformLabels);
}

void TheDefaultPresetNameNamesADefaultPreset() {
	CHECK(FindPresetByName(DefaultGroupPresets(), kDefaultGroupPresetName).has_value());
}

} // namespace

void RunGroupPresetTests() {
	APresetListSurvivesSerializeAndParse();
	TheDefaultPresetsSurviveARoundTrip();
	AMalformedPresetLineIsSkippedAndTheRestIsKept();
	APresetIsFoundByNameWhateverTheLetterCase();
	EveryOldPresetNameStillFindsItsPreset();
	DistinctPresetNamesPassTheNameCheck();
	AnEmptyOrBlankPresetNameIsReported();
	PresetNamesThatDifferOnlyInLetterCaseAreDuplicates();
	TheDefaultPresetNameNamesADefaultPreset();
	TheHideUniformLabelsFlagSurvivesARoundTrip();
	AnOldPresetLineReadsWithTheFlagOff();
	TheShowInMenuFlagOfAPresetSurvivesARoundTrip();
}
