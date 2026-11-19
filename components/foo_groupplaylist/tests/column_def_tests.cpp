#include <string>
#include <vector>

#include "check.h"
#include "column_def.h"

namespace {

void AColumnListSurvivesSerializeAndParse() {
	const std::vector<ColumnDef> defs = {
		{ "Gain", "%replaygain_track_gain%", ColumnAlignment::Right, 75 },
		{ "Mood", "$if(%mood%,%mood%,none)", ColumnAlignment::Center, 120.5f },
		{ "Plain", "", ColumnAlignment::Left, 40 },
	};
	const std::vector<ColumnDef> parsed = ParseColumnDefs(SerializeColumnDefs(defs));
	CHECK(parsed.size() == 3);
	if (parsed.size() != 3) return;
	CHECK(parsed[0].name == "Gain");
	CHECK(parsed[0].pattern == "%replaygain_track_gain%");
	CHECK(parsed[0].alignment == ColumnAlignment::Right);
	CHECK(parsed[0].width == 75);
	CHECK(parsed[1].pattern == "$if(%mood%,%mood%,none)");
	CHECK(parsed[1].alignment == ColumnAlignment::Center);
	CHECK(parsed[1].width == 120.5f);
	CHECK(parsed[2].name == "Plain");
	CHECK(parsed[2].pattern.empty());
	CHECK(parsed[2].alignment == ColumnAlignment::Left);
}

void AnEmptyTextParsesToNoColumns() {
	CHECK(ParseColumnDefs("").empty());
}

void ATabOrLineBreakInsideAFieldBecomesASpace() {
	const std::vector<ColumnDef> parsed = ParseColumnDefs(SerializeColumnDefs({ { "Two\twords", "a\nb", ColumnAlignment::Left, 50 } }));
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].name == "Two words");
	CHECK(parsed[0].pattern == "a b");
}

void AMalformedLineIsSkippedAndTheRestIsKept() {
	const std::vector<ColumnDef> parsed = ParseColumnDefs("broken line\n\t%title%\t0\t50\nGood\t%title%\t2\t60\n");
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].name == "Good");
	CHECK(parsed[0].alignment == ColumnAlignment::Right);
	CHECK(parsed[0].width == 60);
}

void ABadWidthOrAlignmentFallsBackToADefault() {
	const std::vector<ColumnDef> parsed = ParseColumnDefs("Odd\t%title%\t9\tzero\n");
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].alignment == ColumnAlignment::Left);
	CHECK(parsed[0].width == 100);
}

void TheShowInMenuFlagSurvivesARoundTrip() {
	std::vector<ColumnDef> defs = { { "Shown", "%title%", ColumnAlignment::Left, 50 }, { "Hidden", "%artist%", ColumnAlignment::Left, 50 } };
	defs[1].showInMenu = false;
	const std::vector<ColumnDef> parsed = ParseColumnDefs(SerializeColumnDefs(defs));
	CHECK(parsed.size() == 2);
	if (parsed.size() != 2) return;
	CHECK(parsed[0].showInMenu);
	CHECK(!parsed[1].showInMenu);
}

// Lists saved before the flag existed have four fields per column.
void AnOldColumnLineIsShownInTheMenu() {
	const std::vector<ColumnDef> parsed = ParseColumnDefs("Gain\t%replaygain_track_gain%\t1\t75\n");
	CHECK(parsed.size() == 1);
	if (parsed.size() != 1) return;
	CHECK(parsed[0].showInMenu);
}

void AListOfColumnNamesSurvivesARoundTrip() {
	const std::vector<std::string> names = { "Track gain", "Album gain", "File path" };
	CHECK(ParseColumnNames(SerializeColumnNames(names)) == names);
	CHECK(ParseColumnNames("").empty());
}

void OnlyTheNamedBuiltInColumnsLeaveTheMenu() {
	const std::vector<ColumnDef> defs = BuiltInColumnDefsForMenu({ "Track gain", "No such column" });
	CHECK(defs.size() == BuiltInColumnDefs().size());
	for (const ColumnDef& def : defs) CHECK(def.showInMenu == (def.name != "Track gain"));
}

void ANewInstallationHasNoCustomColumns() {
	CHECK(DefaultCustomColumnDefs().empty());
	CHECK(ParseColumnDefs(SerializeColumnDefs(DefaultCustomColumnDefs())).empty());
}

void CustomColumnsSurviveARoundTrip() {
	const std::vector<ColumnDef> defaults = {
		{ "Time", "%length%", ColumnAlignment::Center, 70 },
		{ "P", "$if(%isplaying%, $char(9654))", ColumnAlignment::Right, 28 },
	};
	const std::vector<ColumnDef> parsed = ParseColumnDefs(SerializeColumnDefs(defaults));
	CHECK(parsed.size() == defaults.size());
	for (size_t index = 0; index < parsed.size() && index < defaults.size(); ++index) {
		CHECK(parsed[index].name == defaults[index].name);
		CHECK(parsed[index].pattern == defaults[index].pattern);
	}
}

std::vector<ColumnDef> Named(const std::vector<std::string>& names) {
	std::vector<ColumnDef> defs;
	for (const std::string& name : names) defs.push_back({ name, "", ColumnAlignment::Left, 50 });
	return defs;
}

void DistinctNamesPassTheNameCheck() {
	CHECK(CheckColumnNames(Named({ "Gain", "Mood" }), { "Title", "Artist" }).problem == ColumnNameProblem::None);
	CHECK(CheckColumnNames({}, { "Title" }).problem == ColumnNameProblem::None);
}

void AnEmptyOrBlankNameIsReported() {
	const ColumnNameCheck empty = CheckColumnNames(Named({ "Gain", "" }), {});
	CHECK(empty.problem == ColumnNameProblem::Empty);
	CHECK(empty.index == 1);
	CHECK(CheckColumnNames(Named({ "   " }), {}).problem == ColumnNameProblem::Empty);
}

void ARepeatedNameIsReportedAtItsSecondUse() {
	const ColumnNameCheck check = CheckColumnNames(Named({ "Gain", "Mood", "Gain" }), {});
	CHECK(check.problem == ColumnNameProblem::Duplicate);
	CHECK(check.index == 2);
}

void ANameOfABuiltInColumnIsReported() {
	const ColumnNameCheck check = CheckColumnNames(Named({ "Mood", "Title" }), { "Title", "Artist" });
	CHECK(check.problem == ColumnNameProblem::BuiltIn);
	CHECK(check.index == 1);
}

void TheShippedColumnNamesDoNotCollide() {
	std::vector<std::string> builtInNames;
	for (const ColumnDef& def : BuiltInColumnDefs()) builtInNames.push_back(def.name);
	CHECK(CheckColumnNames(DefaultCustomColumnDefs(), builtInNames).problem == ColumnNameProblem::None);
	CHECK(CheckColumnNames(BuiltInColumnDefs(), {}).problem == ColumnNameProblem::None);
}

// The user can delete any custom column, so a new panel must not depend on one.
void ANewPanelShowsOnlyBuiltInColumns() {
	CHECK(!DefaultColumnLayout().columns.empty());
	for (const ColumnLayout::Column& column : DefaultColumnLayout().columns) {
		bool found = false;
		for (const ColumnDef& def : BuiltInColumnDefs()) found = found || def.name == column.name;
		CHECK(found);
	}
}

void ACustomColumnNamedLikeABuiltInIsRenamedAndKept() {
	const std::vector<ColumnDef> renamed = RenameClashingColumns(
		{ { "Mood", "%mood%", ColumnAlignment::Left, 50 }, { "Bitrate", "%bitrate%k", ColumnAlignment::Center, 60 } },
		{ "Title", "Bitrate" });
	CHECK(renamed.size() == 2);
	if (renamed.size() != 2) return;
	CHECK(renamed[0].name == "Mood");
	CHECK(renamed[1].name == "Bitrate (custom)");
	CHECK(renamed[1].pattern == "%bitrate%k");
	CHECK(renamed[1].alignment == ColumnAlignment::Center);
	CHECK(renamed[1].width == 60);
}

void ARenameNeverLandsOnATakenName() {
	const std::vector<std::string> reserved = { "Bitrate" };
	const std::vector<ColumnDef> renamed = RenameClashingColumns(Named({ "Bitrate", "Bitrate (custom)" }), reserved);
	CHECK(renamed.size() == 2);
	CHECK(CheckColumnNames(renamed, reserved).problem == ColumnNameProblem::None);
	if (renamed.size() != 2) return;
	CHECK(renamed[1].name == "Bitrate (custom)");
}

void ColumnsWithoutAClashStayAsTheyAre() {
	const std::vector<ColumnDef> renamed = RenameClashingColumns(Named({ "Gain", "Gain_A" }), { "Track gain", "Album gain" });
	CHECK(renamed.size() == 2);
	if (renamed.size() != 2) return;
	CHECK(renamed[0].name == "Gain");
	CHECK(renamed[1].name == "Gain_A");
}

void RenamingClashesASecondTimeChangesNothing() {
	const std::vector<std::string> reserved = { "Bitrate", "Title" };
	const std::vector<ColumnDef> once = RenameClashingColumns(Named({ "Bitrate", "Mood", "Title" }), reserved);
	const std::vector<ColumnDef> twice = RenameClashingColumns(once, reserved);
	CHECK(once.size() == twice.size());
	for (size_t index = 0; index < once.size() && index < twice.size(); ++index) {
		CHECK(once[index].name == twice[index].name);
	}
}

const ColumnDef* Named(const std::vector<ColumnDef>& defs, const std::string& name) {
	for (const ColumnDef& def : defs) {
		if (def.name == name) return &def;
	}
	return nullptr;
}

void AnEditedBuiltInColumnSurvivesSavingAndLoading() {
	ColumnDef edit = { "Title", "%title% [%subtitle%]", ColumnAlignment::Center, 0 };
	edit.title = "Song";
	const std::vector<ColumnDef> loaded = ParseBuiltInEdits(SerializeBuiltInEdits({ edit }));
	CHECK(loaded.size() == 1);
	CHECK(!loaded.empty() && loaded[0].name == "Title" && loaded[0].title == "Song");
	CHECK(!loaded.empty() && loaded[0].pattern == "%title% [%subtitle%]" && loaded[0].alignment == ColumnAlignment::Center);
	CHECK(ParseBuiltInEdits("Title\tSong\n\t\tx\t0\n").empty());
}

void OnlyChangedBuiltInColumnsAreKept() {
	const std::vector<ColumnDef> originals = BuiltInColumnDefs();
	std::vector<ColumnDef> defs = originals;
	CHECK(BuiltInEditsFrom(originals, defs).empty());
	for (ColumnDef& def : defs) {
		if (def.name == "Artist") def.alignment = ColumnAlignment::Right;
	}
	const std::vector<ColumnDef> edits = BuiltInEditsFrom(originals, defs);
	CHECK(edits.size() == 1 && edits[0].name == "Artist");
}

void EditsReachTheirColumnOnly() {
	ColumnDef title = { "Title", "%title% [%subtitle%]", ColumnAlignment::Left, 0 };
	title.title = "Song";
	ColumnDef rating = { "Rating", "%rating%", ColumnAlignment::Right, 0 };
	ColumnDef duration = { "Duration", "%length_seconds%", ColumnAlignment::Left, 0 };
	ColumnDef unknown = { "No such column", "%x%", ColumnAlignment::Left, 0 };
	const std::vector<ColumnDef> defs = ApplyBuiltInEdits(BuiltInColumnDefs(), { title, rating, duration, unknown });
	CHECK(defs.size() == BuiltInColumnDefs().size());
	const ColumnDef* shown = Named(defs, "Title");
	CHECK(shown && shown->title == "Song" && shown->pattern == "%title% [%subtitle%]");
	const ColumnDef* stars = Named(defs, "Rating");
	CHECK(stars && stars->pattern.empty() && stars->alignment == ColumnAlignment::Left);
	const ColumnDef* length = Named(defs, "Duration");
	CHECK(length && length->pattern == "%length%" && length->alignment == ColumnAlignment::Left);
	const ColumnDef* artist = Named(defs, "Artist");
	CHECK(artist && artist->title.empty() && artist->pattern == "%artist%");
	CHECK(!Named(defs, "No such column"));
}

void AGivenNameAloneCountsAsAnEdit() {
	const ColumnDef original = *Named(BuiltInColumnDefs(), "Album");
	ColumnDef renamed = original;
	CHECK(!IsEditedBuiltIn(renamed, original));
	renamed.title = "Record";
	CHECK(IsEditedBuiltIn(renamed, original));
}

} // namespace

void RunColumnDefTests() {
	AColumnListSurvivesSerializeAndParse();
	AnEmptyTextParsesToNoColumns();
	ATabOrLineBreakInsideAFieldBecomesASpace();
	AMalformedLineIsSkippedAndTheRestIsKept();
	ABadWidthOrAlignmentFallsBackToADefault();
	ANewInstallationHasNoCustomColumns();
	CustomColumnsSurviveARoundTrip();
	TheShowInMenuFlagSurvivesARoundTrip();
	AnOldColumnLineIsShownInTheMenu();
	AListOfColumnNamesSurvivesARoundTrip();
	OnlyTheNamedBuiltInColumnsLeaveTheMenu();
	DistinctNamesPassTheNameCheck();
	AnEmptyOrBlankNameIsReported();
	ARepeatedNameIsReportedAtItsSecondUse();
	ANameOfABuiltInColumnIsReported();
	TheShippedColumnNamesDoNotCollide();
	ANewPanelShowsOnlyBuiltInColumns();
	ACustomColumnNamedLikeABuiltInIsRenamedAndKept();
	ARenameNeverLandsOnATakenName();
	ColumnsWithoutAClashStayAsTheyAre();
	RenamingClashesASecondTimeChangesNothing();
	AnEditedBuiltInColumnSurvivesSavingAndLoading();
	OnlyChangedBuiltInColumnsAreKept();
	EditsReachTheirColumnOnly();
	AGivenNameAloneCountsAsAnEdit();
}
