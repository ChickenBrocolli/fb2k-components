#include "check.h"

#include <string>
#include <vector>

#include "info_lines.h"

namespace {

using Entries = std::vector<std::string>;

const std::string kAlbumDiscCover = "[%artist%]|Year    [%date%]|Codec  [%codec%]|Bitrate  [%bitrate%]kbps|Sample [%samplerate%]Hz|[%comment%]";

// The lines of a shipped preset are recognised as items, whatever label text they carry.
void PatternLinesBecomeItems() {
	CHECK(PatternEntries(kAlbumDiscCover, true) == Entries({ "@artist", "@date", "@codec", "@bitrate", "@samplerate", "@comment" }));
	// A '|' inside brackets cuts the pattern too; each piece still names one field.
	CHECK(PatternEntries("[%album%|][%date%|][%genre%]", true) == Entries({ "@album", "@date", "@genre" }));
}

void APresetWithoutCoverTextShowsNoLines() {
	CHECK(PatternEntries("[%album%]", false).empty());
}

// A line with two fields, or a field not in the list, is kept as it is written.
void OtherLinesAreKeptAsTheyAre() {
	CHECK(EntryOfPatternLine("%tracknumber%/%totaltracks%") == "%tracknumber%/%totaltracks%");
	CHECK(EntryOfPatternLine("Mood [%mood%]") == "Mood [%mood%]");
	CHECK(EntryOfPatternLine("100%% sure") == "100%% sure");
}

// The popup lists the lines shown first, in their order, then the other items, then unshown lines of the pattern.
void ThePopupListsShownLinesFirst() {
	const std::vector<InfoChoice> choices = InfoChoices("[%date%]|Mood [%mood%]", true, Entries({ "@codec", "@date" }));
	CHECK(choices.size() == InfoItems().size() + 1);
	CHECK(choices[0].entry == "@codec" && choices[0].shown);
	CHECK(choices[1].entry == "@date" && choices[1].shown);
	CHECK(!choices[2].shown);
	CHECK(choices.back().entry == "Mood [%mood%]" && !choices.back().shown);
	CHECK(ShownEntries(choices) == Entries({ "@codec", "@date" }));
}

void WithoutAChoiceThePopupShowsThePattern() {
	const std::vector<InfoChoice> choices = InfoChoices(kAlbumDiscCover, true, std::nullopt);
	CHECK(ShownEntries(choices) == PatternEntries(kAlbumDiscCover, true));
	CHECK(choices.size() == InfoItems().size());
}

void EveryItemIsFound() {
	for (const InfoItem& item : InfoItems()) CHECK(InfoItemOfEntry(InfoItemEntry(item)) == &item);
	CHECK(InfoItemOfEntry("@nothing") == nullptr);
	CHECK(InfoItemOfEntry("[%date%]") == nullptr);
}

void ChoicesAreKeptByPresetName() {
	InfoLineChoices choices;
	SetPickedInfoLines(choices, "Album / Disc", Entries({ "@date" }));
	CHECK(PickedInfoLines(choices, "album / disc") == Entries({ "@date" }));
	SetPickedInfoLines(choices, "Album / Disc", Entries());
	CHECK(PickedInfoLines(choices, "Album / Disc") == Entries());
	SetPickedInfoLines(choices, "Album / Disc", std::nullopt);
	CHECK(!PickedInfoLines(choices, "Album / Disc"));
}

void ChoicesFollowRenamesAndForgetDeletedPresets() {
	const InfoLineChoices choices = { { "Old", { "@date" } }, { "Gone", { "@codec" } } };
	const std::vector<GroupPreset> presets = { { "New", "", "", "", false, {} } };
	const InfoLineChoices after = InfoLinesAfterPresetChange(choices, presets, { { "Old", "New" } });
	CHECK(after.size() == 1);
	CHECK(after[0].first == "New" && after[0].second == Entries({ "@date" }));
}

void ChoicesSurviveSaving() {
	const InfoLineChoices choices = { { "Album / Disc", { "@date", "Mood [%mood%]" } }, { "Artist / Cover", {} } };
	CHECK(ParseInfoLines(SerializeInfoLines(choices)) == choices);
}

// A line cut out of a pattern stands alone, so it shows its own value when an earlier line gives nothing.
void APatternLineStandsAlone() {
	CHECK(WholePatternOf("Mood [%mood%]") == std::optional<std::string>("Mood [%mood%]"));
	CHECK(WholePatternOf("][%mood%]") == std::optional<std::string>("[%mood%]"));
	CHECK(WholePatternOf("[%album%") == std::optional<std::string>("[%album%]"));
	CHECK(WholePatternOf("'['[%mood%]']'") == std::optional<std::string>("'['[%mood%]']'"));
	CHECK(WholePatternOf("$if(%a%,x") == std::nullopt);
	CHECK(WholePatternOf("y)") == std::nullopt);
	CHECK(WholePatternOf("'open") == std::nullopt);
}

// Hiding every line means nothing: the info column is hidden for that. So the last ticked row stays.
void TheLastShownLineStays() {
	const std::vector<InfoChoice> two = { { "@date", true }, { "@codec", true }, { "@genre", false } };
	CHECK(MayUntick(two, 0));
	CHECK(MayUntick(two, 2));
	const std::vector<InfoChoice> one = { { "@date", true }, { "@codec", false } };
	CHECK(!MayUntick(one, 0));
	CHECK(MayUntick(one, 1));
}

// A dragged row goes to its new place and the rows between close up; the lines shown follow the new order.
void ADraggedRowChangesTheOrder() {
	std::vector<InfoChoice> rows = { { "@artist", true }, { "@date", true }, { "@codec", true }, { "@genre", false } };
	MoveRow(rows, 2, 0);
	CHECK(ShownEntries(rows) == Entries({ "@codec", "@artist", "@date" }));
	MoveRow(rows, 0, 3);
	CHECK(rows[3].entry == "@codec");
	CHECK(ShownEntries(rows) == Entries({ "@artist", "@date", "@codec" }));
	MoveRow(rows, 1, 9);
	CHECK(ShownEntries(rows) == Entries({ "@artist", "@date", "@codec" }));
}

} // namespace

void RunInfoLinesTests() {
	APatternLineStandsAlone();
	TheLastShownLineStays();
	ADraggedRowChangesTheOrder();
	PatternLinesBecomeItems();
	APresetWithoutCoverTextShowsNoLines();
	OtherLinesAreKeptAsTheyAre();
	ThePopupListsShownLinesFirst();
	WithoutAChoiceThePopupShowsThePattern();
	EveryItemIsFound();
	ChoicesAreKeptByPresetName();
	ChoicesFollowRenamesAndForgetDeletedPresets();
	ChoicesSurviveSaving();
}
