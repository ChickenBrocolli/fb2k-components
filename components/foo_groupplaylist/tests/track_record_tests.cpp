#include "check.h"
#include "track_record.h"

namespace {

TrackRecord Sample() {
	TrackRecord record;
	record.rating = 4;
	record.playCount = 12;
	record.firstPlayed = 133000000000000000ull;
	record.lastPlayed = 134000000000000000ull;
	record.added = 132000000000000000ull;
	record.liked = 1;
	record.likedAt = 134500000000000000ull;
	record.ratingSet = true;
	record.likeSet = true;
	return record;
}

void ARecordReadsBackAsItWasSaved() {
	const auto bytes = SerializeTrackRecord(Sample());
	const auto read = ParseTrackRecord(bytes.data(), bytes.size());
	CHECK(read.has_value());
	CHECK(*read == Sample());
}

void NoDataIsAnEmptyRecord() {
	const auto read = ParseTrackRecord(nullptr, 0);
	CHECK(read.has_value());
	CHECK(read->IsEmpty());
}

void CutOffDataIsRejected() {
	const auto bytes = SerializeTrackRecord(Sample());
	CHECK(!ParseTrackRecord(bytes.data(), 3).has_value());
	CHECK(!ParseTrackRecord(bytes.data(), bytes.size() - 1).has_value());
}

void ARecordFromANewerVersionStillReads() {
	auto bytes = SerializeTrackRecord(Sample());
	bytes[0] = 5;  // a later version with one more field at the end
	bytes.push_back(1);
	bytes.push_back(0);
	bytes.push_back(0);
	bytes.push_back(0);
	const auto read = ParseTrackRecord(bytes.data(), bytes.size());
	CHECK(read.has_value());
	CHECK(*read == Sample());
}

// A version 1 record, saved before likes: everything but the like, which reads as not liked.
void AVersionOneRecordReadsWithoutALike() {
	TrackRecord old = Sample();
	old.liked = 0;
	old.likedAt = 0;
	old.ratingSet = old.likeSet = false;
	auto bytes = SerializeTrackRecord(old);
	bytes[0] = 1;
	bytes.resize(bytes.size() - 4 - 8 - 4);
	const auto read = ParseTrackRecord(bytes.data(), bytes.size());
	CHECK(read.has_value());
	CHECK(*read == old);
}

// A version 2 record, saved before the like times: liked, with no time.
void AVersionTwoRecordReadsWithoutALikeTime() {
	TrackRecord old = Sample();
	old.likedAt = 0;
	old.ratingSet = old.likeSet = false;
	auto bytes = SerializeTrackRecord(old);
	bytes[0] = 2;
	bytes.resize(bytes.size() - 8 - 4);
	const auto read = ParseTrackRecord(bytes.data(), bytes.size());
	CHECK(read.has_value());
	CHECK(*read == old);
}

// A record saved before version 4 decides the rating and the like only when it has them.
void AnOldRecordDecidesOnlyWhatItHas() {
	TrackRecord old = Sample();
	old.ratingSet = old.likeSet = false;
	auto bytes = SerializeTrackRecord(old);
	bytes[0] = 3;
	bytes.resize(bytes.size() - 4);
	const auto read = ParseTrackRecord(bytes.data(), bytes.size());
	CHECK(read.has_value() && *read == old);
	CHECK(read->RatingDecided() && read->LikeDecided());
	CHECK(!TrackRecord{}.RatingDecided() && !TrackRecord{}.LikeDecided());
}

// A track liked or rated only in its file tags: taking the like or the rating away in GroupPlaylist is kept,
// so the tag no longer shows in its place.
void TakingAwayATagLikeOrRatingIsKept() {
	TrackRecord record;
	CHECK(SetLiked(record, false, 100));
	CHECK(record.LikeDecided() && record.liked == 0 && !record.IsEmpty());
	CHECK(!SetLiked(record, false, 200));
	CHECK(SetRating(record, 0));
	CHECK(record.RatingDecided() && record.rating == 0);
	CHECK(!SetRating(record, 0));
	CHECK(SetRating(record, 4));
	CHECK(record.rating == 4);
}

void LikingKeepsTheTimeOfTheFirstLike() {
	TrackRecord record;
	CHECK(SetLiked(record, true, 100));
	CHECK(record.liked == 1 && record.likedAt == 100);
	CHECK(!SetLiked(record, true, 200));
	CHECK(record.likedAt == 100);
	CHECK(SetLiked(record, false, 300));
	CHECK(record.liked == 0 && record.likedAt == 0);
	CHECK(SetLiked(record, true, 400));
	CHECK(record.likedAt == 400);
}

void ARatingAboveFiveReadsAsFive() {
	TrackRecord record = Sample();
	record.rating = 9;
	const auto bytes = SerializeTrackRecord(record);
	CHECK(ParseTrackRecord(bytes.data(), bytes.size())->rating == 5);
}

void CountingAPlaySetsTheDates() {
	TrackRecord record;
	CountPlay(record, 100);
	CHECK(record.playCount == 1 && record.firstPlayed == 100 && record.lastPlayed == 100);
	CountPlay(record, 200);
	CHECK(record.playCount == 2 && record.firstPlayed == 100 && record.lastPlayed == 200);
}

} // namespace

void RunTrackRecordTests() {
	ARecordReadsBackAsItWasSaved();
	NoDataIsAnEmptyRecord();
	CutOffDataIsRejected();
	ARecordFromANewerVersionStillReads();
	AVersionOneRecordReadsWithoutALike();
	AVersionTwoRecordReadsWithoutALikeTime();
	LikingKeepsTheTimeOfTheFirstLike();
	AnOldRecordDecidesOnlyWhatItHas();
	TakingAwayATagLikeOrRatingIsKept();
	ARatingAboveFiveReadsAsFive();
	CountingAPlaySetsTheDates();
}
