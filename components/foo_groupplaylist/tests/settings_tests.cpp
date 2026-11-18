#include "check.h"

#include "scroll_presets.h"
#include "settings_file.h"

namespace {

void TheBuiltInScrollPresetsAreInRangeAndDefaultIsTheOldFeel() {
	const std::vector<ScrollPreset> presets = BuiltInScrollPresets();
	CHECK(presets.size() == 3);
	CHECK(presets[0].name == kDefaultScrollPresetName);
	CHECK(presets[0].params == smooth_scroll::InertiaParams{});
	for (const ScrollPreset& preset : presets) CHECK(ClampInertiaParams(preset.params) == preset.params);
	// Light stops sooner than Default and speeds up less; Heavy glides longer, moves less per slow notch and speeds up more.
	CHECK(presets[1].params.tauMs < presets[0].params.tauMs && presets[0].params.tauMs < presets[2].params.tauMs);
	CHECK(presets[1].params.maxAcceleration < presets[0].params.maxAcceleration && presets[0].params.maxAcceleration < presets[2].params.maxAcceleration);
	CHECK(presets[1].params.distanceScale == 1.0 && presets[2].params.distanceScale < 1.0);
	CHECK(IsBuiltInScrollPreset("Heavy") && !IsBuiltInScrollPreset("Mine"));
}

void UserScrollPresetsSurviveTheirTextForm() {
	const std::vector<ScrollPreset> presets = { { "Fast", { 45.0, 1.5, 90.0, 0.25, 2.5 } }, { "Slow glide", { 300.0, 0.75, 200.0, 0.0, 1.0 } } };
	const std::string text = SerializeScrollPresets(presets);
	CHECK(text == "Fast\t45\t1.5\t90\t0.25\t2.5\nSlow glide\t300\t0.75\t200\t0\t1\n");
	CHECK(ParseScrollPresets(text) == presets);
}

void BrokenScrollPresetLinesAreSkippedAndNumbersPulledIn() {
	const std::vector<ScrollPreset> presets = ParseScrollPresets(
		"Good\t90\t1\t120\t0.4\t3\n"
		"\t90\t1\t120\t0.4\t3\n"        // no name
		"Short\t90\t1\n"                // numbers missing
		"Word\t90\tfast\t120\t0.4\t3\n" // not a number
		"Good\t50\t1\t120\t0.4\t3\n"    // the same name again
		"Default\t50\t1\t120\t0.4\t3\n" // a built-in name
		"Heavy\t50\t1\t120\t0.4\t3\n"   // a built-in name
		"Wild\t1\t99\t120\t0.4\t3\n"
		"Fine\t90\t1.234\t120\t0.4\t3\n");
	CHECK(presets.size() == 3);
	CHECK(presets[0].name == "Good" && presets[0].params.tauMs == 90.0);
	CHECK(presets[1].name == "Wild" && presets[1].params.tauMs == 30.0 && presets[1].params.distanceScale == 4.0);
	CHECK(presets[2].name == "Fine" && presets[2].params.distanceScale == 1.23);
}

void ANewScrollPresetNeedsAFreeName() {
	const std::vector<ScrollPreset> user = { { "Mine", {} } };
	const std::vector<std::string> shown = { "기본", "가볍게", "묵직하게" };
	CHECK(CheckNewScrollPresetName("Other", user, shown) == ScrollPresetNameProblem::None);
	CHECK(CheckNewScrollPresetName("", user, shown) == ScrollPresetNameProblem::Empty);
	CHECK(CheckNewScrollPresetName("   ", user, shown) == ScrollPresetNameProblem::Empty);
	CHECK(CheckNewScrollPresetName("Mine", user, shown) == ScrollPresetNameProblem::Taken);
	CHECK(CheckNewScrollPresetName("Default", user, shown) == ScrollPresetNameProblem::Taken);
	CHECK(CheckNewScrollPresetName("기본", user, shown) == ScrollPresetNameProblem::Taken);
}

void NumbersAreWrittenWithoutTrailingZeros() {
	CHECK(FormatSettingNumber(90.0) == "90");
	CHECK(FormatSettingNumber(1.5) == "1.5");
	CHECK(FormatSettingNumber(0.25) == "0.25");
	CHECK(FormatSettingNumber(0.0) == "0");
	CHECK(ParseSettingNumber("0.4") == 0.4);
	CHECK(!ParseSettingNumber("0.4x"));
	CHECK(!ParseSettingNumber(""));
}

void ASettingsFileReadsBackWhatWasWritten() {
	SettingsWriter writer;
	writer.Comment("Look");
	writer.AddBool("smoothScroll", true);
	writer.AddInt("coverSize", 98);
	writer.Add("trackFont", "Segoe UI\t-12\t400\t0");
	writer.Add("broken", "two\nlines");
	writer.AddList("customColumns", "column", { "A\t[%a%]\t0\t100\t1", "B\t[%b%]\t2\t60\t0" });
	writer.AddList("groupPresets", "groupPreset", {});

	const std::optional<SettingsEntries> entries = ParseSettingsText(writer.Text());
	CHECK(entries.has_value());
	if (!entries) return;
	CHECK(SettingValue(*entries, "version") == "1");
	CHECK(SettingValue(*entries, "smoothScroll") == "1");
	CHECK(SettingValue(*entries, "trackFont") == "Segoe UI\t-12\t400\t0");
	CHECK(SettingValue(*entries, "broken") == "two lines");
	CHECK(!SettingValue(*entries, "missing"));
	const std::optional<std::vector<std::string>> columns = SettingList(*entries, "customColumns", "column");
	CHECK(columns && columns->size() == 2 && (*columns)[1] == "B\t[%b%]\t2\t60\t0");
	// An empty list is there, a list the file does not have is not.
	CHECK(SettingList(*entries, "groupPresets", "groupPreset") && SettingList(*entries, "groupPresets", "groupPreset")->empty());
	CHECK(!SettingList(*entries, "userScrollPresets", "userScrollPreset"));
}

void ASettingsFileEditedInNotepadStillReads() {
	const std::optional<SettingsEntries> entries = ParseSettingsText(
		"\xEF\xBB\xBF" "format = GroupPlaylist settings\r\n# a note\r\n\r\n  coverSize=120\r\nno equals sign\r\n");
	CHECK(entries.has_value());
	if (entries) CHECK(SettingValue(*entries, "coverSize") == "120");
	CHECK(!ParseSettingsText("coverSize = 120\n"));  // not a GroupPlaylist settings file
	CHECK(!ParseSettingsText("format = Something else\n"));
}

void SettingValuesAreReadStrictly() {
	CHECK(ParseSettingBool("1") == true && ParseSettingBool("0") == false && !ParseSettingBool("yes"));
	CHECK(ParseSettingInt("-12") == -12 && ParseSettingInt("98") == 98);
	CHECK(!ParseSettingInt("") && !ParseSettingInt("-") && !ParseSettingInt("9a"));
	// #RRGGBB, kept as COLORREF (red in the low byte).
	CHECK(FormatSettingColor(0x00227EE6) == "#E67E22");
	CHECK(ParseSettingColor("#E67E22") == 0x00227EE6ul);
	CHECK(ParseSettingColor("#e67e22") == 0x00227EE6ul);
	CHECK(!ParseSettingColor("E67E22") && !ParseSettingColor("#E67E2G"));
}

} // namespace

void RunSettingsTests() {
	TheBuiltInScrollPresetsAreInRangeAndDefaultIsTheOldFeel();
	UserScrollPresetsSurviveTheirTextForm();
	BrokenScrollPresetLinesAreSkippedAndNumbersPulledIn();
	ANewScrollPresetNeedsAFreeName();
	NumbersAreWrittenWithoutTrailingZeros();
	ASettingsFileReadsBackWhatWasWritten();
	ASettingsFileEditedInNotepadStillReads();
	SettingValuesAreReadStrictly();
}
