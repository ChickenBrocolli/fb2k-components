#include <string>
#include <vector>

#include "check.h"
#include "group_model.h"

namespace {

typedef std::vector<std::string> Keys;

GroupModel Build(const Keys& albumKeys, const Keys& blockKeys) {
	GroupModel model;
	model.Rebuild(albumKeys, blockKeys);
	return model;
}

GroupModel BuildAlbumsOnly(const Keys& albumKeys) {
	return Build(albumKeys, Keys(albumKeys.size(), "same"));
}

void EmptyListHasNoAlbumsOrBlocks() {
	const GroupModel model = Build({}, {});
	CHECK(model.ItemCount() == 0);
	CHECK(model.AlbumCount() == 0);
	CHECK(model.BlockCount() == 0);
}

void SingleItemIsOneAlbumWithOneBlock() {
	const GroupModel model = Build({ "Harbor Lights" }, { "North Pier" });
	CHECK(model.AlbumCount() == 1);
	CHECK(model.AlbumAt(0).firstItem == 0);
	CHECK(model.AlbumAt(0).key == "Harbor Lights");
	CHECK(model.BlockCount() == 1);
	CHECK(model.BlockSize(0) == 1);
	CHECK(model.AlbumIndexOf(0) == 0);
	CHECK(model.BlockIndexOf(0) == 0);
}

void AdjacentEqualAlbumKeysShareAnAlbum() {
	const GroupModel model = BuildAlbumsOnly({ "A", "A", "A", "B", "B" });
	CHECK(model.AlbumCount() == 2);
	CHECK(model.AlbumAt(0).firstItem == 0);
	CHECK(model.AlbumAt(1).firstItem == 3);
	CHECK(model.AlbumAt(1).key == "B");
}

void AnAlbumKeyThatComesBackStartsANewAlbum() {
	const GroupModel model = BuildAlbumsOnly({ "A", "B", "A" });
	CHECK(model.AlbumCount() == 3);
	CHECK(model.AlbumAt(2).firstItem == 2);
	CHECK(model.AlbumAt(2).key == "A");
}

void EveryItemMapsToItsAlbum() {
	const GroupModel model = BuildAlbumsOnly({ "A", "A", "B", "C", "C", "C" });
	const size_t expected[] = { 0, 0, 1, 2, 2, 2 };
	for (size_t item = 0; item < 6; ++item) {
		CHECK(model.AlbumIndexOf(item) == expected[item]);
	}
}

void EmptyAlbumKeysGroupLikeAnyOtherKey() {
	const GroupModel model = BuildAlbumsOnly({ "", "", "A", "" });
	CHECK(model.AlbumCount() == 3);
	CHECK(model.AlbumAt(0).key.empty());
	CHECK(model.AlbumAt(1).firstItem == 2);
	CHECK(model.AlbumAt(2).firstItem == 3);
}

void ABlockKeyChangeSplitsTheAlbumIntoBlocks() {
	const GroupModel model = Build({ "A", "A", "A", "A" }, { "x", "x", "y", "x" });
	CHECK(model.AlbumCount() == 1);
	CHECK(model.BlockCount() == 3);
	CHECK(model.BlockAt(0).firstItem == 0);
	CHECK(model.BlockSize(0) == 2);
	CHECK(model.BlockAt(1).firstItem == 2);
	CHECK(model.BlockSize(1) == 1);
	CHECK(model.BlockAt(2).firstItem == 3);
	CHECK(model.BlockSize(2) == 1);
}

void AnAlbumChangeStartsABlockEvenWhenTheBlockKeyStays() {
	const GroupModel model = Build({ "A", "A", "B", "B" }, { "x", "x", "x", "x" });
	CHECK(model.AlbumCount() == 2);
	CHECK(model.BlockCount() == 2);
	CHECK(model.BlockAt(1).firstItem == 2);
}

void EveryItemMapsToItsBlock() {
	const GroupModel model = Build({ "A", "A", "A", "B", "B" }, { "x", "y", "y", "y", "z" });
	const size_t expected[] = { 0, 1, 1, 2, 3 };
	for (size_t item = 0; item < 5; ++item) {
		CHECK(model.BlockIndexOf(item) == expected[item]);
	}
}

void BlocksKnowTheirAlbumAndAlbumsKnowTheirBlocks() {
	const GroupModel model = Build({ "A", "A", "A", "B", "B" }, { "x", "y", "y", "y", "z" });
	CHECK(model.BlockAt(0).album == 0);
	CHECK(model.BlockAt(1).album == 0);
	CHECK(model.BlockAt(2).album == 1);
	CHECK(model.BlockAt(3).album == 1);
	CHECK(model.AlbumAt(0).firstBlock == 0);
	CHECK(model.AlbumBlockEnd(0) == 2);
	CHECK(model.AlbumAt(1).firstBlock == 2);
	CHECK(model.AlbumBlockEnd(1) == 4);
}

void OnlyTheLastBlockOfAnAlbumIsMarkedLast() {
	const GroupModel model = Build({ "A", "A", "A", "B" }, { "x", "y", "z", "z" });
	CHECK(!model.IsLastBlockOfAlbum(0));
	CHECK(!model.IsLastBlockOfAlbum(1));
	CHECK(model.IsLastBlockOfAlbum(2));
	CHECK(model.IsLastBlockOfAlbum(3));
}

void TheLastBlockReachesTheEndOfTheList() {
	const GroupModel model = Build({ "A", "A", "A", "A" }, { "x", "y", "y", "y" });
	CHECK(model.BlockSize(1) == 3);
	CHECK(model.BlockIndexOf(3) == 1);
}

void RebuildReplacesTheOldContent() {
	GroupModel model = Build({ "A", "B", "C" }, { "x", "y", "z" });
	model.Rebuild({ "X", "X" }, { "p", "p" });
	CHECK(model.ItemCount() == 2);
	CHECK(model.AlbumCount() == 1);
	CHECK(model.AlbumAt(0).key == "X");
	CHECK(model.BlockCount() == 1);
	CHECK(model.BlockSize(0) == 2);
}

GroupModel BuildWithSubgroups(const Keys& albumKeys, const Keys& blockKeys, const std::vector<Keys>& subgroupTexts) {
	GroupModel model;
	model.Rebuild(albumKeys, blockKeys, subgroupTexts);
	return model;
}

// Zero when the item has no label, so a missing label fails a check without a null dereference.
size_t LabelLines(const GroupModel& model, size_t item) {
	const GroupModel::Label* label = model.LabelAt(item);
	return label ? label->lineCount : 0;
}

constexpr size_t kNoLabel = static_cast<size_t>(-1);

size_t LabelFirstLevel(const GroupModel& model, size_t item) {
	const GroupModel::Label* label = model.LabelAt(item);
	return label ? label->firstLevel : kNoLabel;
}

void WithoutSubgroupsNoItemHasALabel() {
	const GroupModel model = Build({ "A", "A" }, { "x", "y" });
	CHECK(LabelLines(model, 0) == 0);
	CHECK(LabelLines(model, 1) == 0);
	CHECK(model.LabelLinesInBlock(0) == 0);
}

void ABlockStartShowsItsSubgroupText() {
	const GroupModel model = BuildWithSubgroups({ "A", "A" }, { "x", "x" }, { { "Disc 1", "Disc 1" } });
	CHECK(LabelLines(model, 0) > 0);
	CHECK(LabelFirstLevel(model, 0) == 0);
	CHECK(LabelLines(model, 0) == 1);
	CHECK(LabelLines(model, 1) == 0);
}

void ASubgroupChangeInsideABlockAddsALabel() {
	const GroupModel model = BuildWithSubgroups({ "A", "A", "A" }, { "x", "x", "x" }, { { "Disc 1", "Disc 1", "Disc 2" } });
	CHECK(model.BlockCount() == 1);
	CHECK(LabelLines(model, 0) > 0);
	CHECK(LabelLines(model, 1) == 0);
	CHECK(LabelLines(model, 2) > 0);
	CHECK(LabelLines(model, 2) == 1);
	CHECK(model.LabelLinesInBlock(0) == 2);
}

void AnEmptySubgroupTextShowsNoLabel() {
	const GroupModel untagged = BuildWithSubgroups({ "A", "A" }, { "x", "x" }, { { "", "" } });
	CHECK(LabelLines(untagged, 0) == 0);
	CHECK(LabelLines(untagged, 1) == 0);

	const GroupModel becomesEmpty = BuildWithSubgroups({ "A", "A" }, { "x", "x" }, { { "Disc 1", "" } });
	CHECK(LabelLines(becomesEmpty, 0) > 0);
	CHECK(LabelLines(becomesEmpty, 1) == 0);

	const GroupModel becomesTagged = BuildWithSubgroups({ "A", "A" }, { "x", "x" }, { { "", "Disc 2" } });
	CHECK(LabelLines(becomesTagged, 0) == 0);
	CHECK(LabelLines(becomesTagged, 1) > 0);
}

void AnOuterSubgroupChangeShowsTheInnerLevelsToo() {
	const GroupModel model = BuildWithSubgroups(
		{ "A", "A", "A" }, { "x", "x", "x" },
		{ { "[2011] One", "[2011] One", "[2012] Two" }, { "Disc 1", "Disc 2", "Disc 2" } });
	CHECK(LabelFirstLevel(model, 0) == 0);
	CHECK(LabelLines(model, 0) == 2);
	CHECK(LabelFirstLevel(model, 1) == 1);
	CHECK(LabelLines(model, 1) == 1);
	CHECK(LabelFirstLevel(model, 2) == 0);
	CHECK(LabelLines(model, 2) == 2);
}

void ANewBlockShowsItsSubgroupTextEvenWhenUnchanged() {
	const GroupModel model = BuildWithSubgroups({ "A", "A" }, { "x", "y" }, { { "Disc 1", "Disc 1" } });
	CHECK(model.BlockCount() == 2);
	CHECK(LabelLines(model, 1) > 0);
	CHECK(LabelFirstLevel(model, 1) == 0);
}

void LabelLinesAreCountedPerBlock() {
	const GroupModel model = BuildWithSubgroups(
		{ "A", "A", "A", "A" }, { "x", "x", "y", "y" },
		{ { "One", "One", "Two", "Two" }, { "Disc 1", "Disc 2", "", "Disc 9" } });
	CHECK(model.LabelLinesInBlock(0) == 3);
	CHECK(model.LabelLinesInBlock(1) == 2);
}

GroupModel BuildHidingUniform(const Keys& albumKeys, const Keys& blockKeys, const std::vector<Keys>& subgroupTexts) {
	GroupModel model;
	model.Rebuild(albumKeys, blockKeys, subgroupTexts, true);
	return model;
}

// One disc in the whole album, split into blocks by artist: "Disc 1" would repeat on every block.
void ALabelThatIsTheSameForTheWholeAlbumIsHidden() {
	const GroupModel model = BuildHidingUniform({ "A", "A", "A" }, { "x", "y", "z" }, { { "Disc 1", "Disc 1", "Disc 1" } });
	CHECK(LabelLines(model, 0) == 0);
	CHECK(LabelLines(model, 1) == 0);
	CHECK(LabelLines(model, 2) == 0);
	CHECK(!model.IsLevelShown(0, 0));
}

void ALabelThatChangesInsideTheAlbumStays() {
	const GroupModel model = BuildHidingUniform({ "A", "A", "A" }, { "x", "x", "x" }, { { "Disc 1", "Disc 1", "Disc 2" } });
	CHECK(LabelLines(model, 0) == 1);
	CHECK(LabelLines(model, 2) == 1);
	CHECK(model.IsLevelShown(0, 0));
}

void EachAlbumIsJudgedOnItsOwn() {
	const GroupModel model = BuildHidingUniform({ "A", "A", "B", "B" }, { "x", "x", "y", "y" }, { { "Disc 1", "Disc 1", "Disc 1", "Disc 2" } });
	CHECK(LabelLines(model, 0) == 0);
	CHECK(!model.IsLevelShown(1, 0));
	CHECK(LabelLines(model, 2) == 1);
	CHECK(model.IsLevelShown(2, 0));
}

void OnlyTheUniformLevelIsHidden() {
	const GroupModel model = BuildHidingUniform(
		{ "A", "A", "A" }, { "x", "x", "x" },
		{ { "Live", "Live", "Live" }, { "Disc 1", "Disc 1", "Disc 2" } });
	CHECK(!model.IsLevelShown(0, 0));
	CHECK(model.IsLevelShown(0, 1));
	CHECK(LabelLines(model, 0) == 1);
	CHECK(LabelFirstLevel(model, 0) == 0);
	CHECK(LabelLines(model, 2) == 1);
}

void WithoutTheOptionAUniformLabelStays() {
	const GroupModel model = BuildWithSubgroups({ "A", "A" }, { "x", "y" }, { { "Disc 1", "Disc 1" } });
	CHECK(LabelLines(model, 0) == 1);
	CHECK(LabelLines(model, 1) == 1);
	CHECK(model.IsLevelShown(0, 0));
}

} // namespace

void RunGroupModelTests() {
	EmptyListHasNoAlbumsOrBlocks();
	SingleItemIsOneAlbumWithOneBlock();
	AdjacentEqualAlbumKeysShareAnAlbum();
	AnAlbumKeyThatComesBackStartsANewAlbum();
	EveryItemMapsToItsAlbum();
	EmptyAlbumKeysGroupLikeAnyOtherKey();
	ABlockKeyChangeSplitsTheAlbumIntoBlocks();
	AnAlbumChangeStartsABlockEvenWhenTheBlockKeyStays();
	EveryItemMapsToItsBlock();
	BlocksKnowTheirAlbumAndAlbumsKnowTheirBlocks();
	OnlyTheLastBlockOfAnAlbumIsMarkedLast();
	TheLastBlockReachesTheEndOfTheList();
	RebuildReplacesTheOldContent();
	WithoutSubgroupsNoItemHasALabel();
	ABlockStartShowsItsSubgroupText();
	ASubgroupChangeInsideABlockAddsALabel();
	AnEmptySubgroupTextShowsNoLabel();
	AnOuterSubgroupChangeShowsTheInnerLevelsToo();
	ANewBlockShowsItsSubgroupTextEvenWhenUnchanged();
	LabelLinesAreCountedPerBlock();
	ALabelThatIsTheSameForTheWholeAlbumIsHidden();
	ALabelThatChangesInsideTheAlbumStays();
	EachAlbumIsJudgedOnItsOwn();
	OnlyTheUniformLevelIsHidden();
	WithoutTheOptionAUniformLabelStays();
}
