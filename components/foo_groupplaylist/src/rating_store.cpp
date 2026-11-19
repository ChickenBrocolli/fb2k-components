#include "stdafx.h"

#include "rating_store.h"

#include <string>

#include "track_store.h"

namespace {

constexpr GUID guid_rating_storage = { 0x613e1a94, 0xeb91, 0x4f0b, { 0xbc, 0xd3, 0x47, 0x52, 0x4d, 0x79, 0x3d, 0x33 } };

cfg_int cfg_ratingStorage(guid_rating_storage, static_cast<int>(RatingStorage::Store));

// Whether SettleRatingStorage has decided, once, for this foobar2000 profile.
constexpr GUID guid_rating_storage_settled = { 0x0b7d4e29, 0x5f61, 0x4c83, { 0x9a, 0x1e, 0x36, 0xc2, 0x8d, 0x47, 0xf0, 0x5b } };
cfg_bool cfg_ratingStorageSettled(guid_rating_storage_settled, false);

// Sets one tag in the files, or removes it with an empty value. Files that already hold the value are left alone.
void WriteTag(metadb_handle_list_cref tracks, const char* name, const std::string& value) {
	const std::string field = name;
	auto filter = file_info_filter::create([field, value](trackRef, t_filestats, file_info& info) {
		const char* current = info.meta_get(field.c_str(), 0);
		if (value == (current ? current : "")) return false;
		if (value.empty()) info.meta_remove_field(field.c_str());
		else info.meta_set(field.c_str(), value.c_str());
		return true;
	});
	metadb_io_v2::get()->update_info_async(tracks, filter, core_api::get_main_window(), metadb_io_v2::op_flag_delay_ui, nullptr);
}

void WriteRatingTag(metadb_handle_ptr track, int rating) {
	metadb_handle_list tracks;
	tracks.add_item(track);
	WriteTag(tracks, "RATING", rating > 0 ? std::to_string(rating) : std::string());
}

void WriteRatingToStore(metadb_handle_ptr track, int rating) {
	metadb_handle_list tracks;
	tracks.add_item(track);
	track_store::Update(tracks, [rating](TrackRecord& record) { return SetRating(record, static_cast<uint32_t>(rating)); });
}

} // namespace

RatingStorage GetRatingStorage() {
	switch (cfg_ratingStorage) {
	case static_cast<int>(RatingStorage::FileTags): return RatingStorage::FileTags;
	case static_cast<int>(RatingStorage::Both): return RatingStorage::Both;
	default: return RatingStorage::Store;
	}
}

void SetRatingStorage(RatingStorage storage) {
	cfg_ratingStorage = static_cast<int>(storage);
}

// foobar2000 saves a setting only when it differs from its default. Before the store, the default was File tags, so a
// profile that rated in file tags has no saved choice, and would read the new default. A panel saved by an earlier
// GroupPlaylist shows that the profile is such a one; then File tags is saved as its choice. Decided once.
void SettleRatingStorage(bool panelSavedBefore) {
	if (cfg_ratingStorageSettled) return;
	cfg_ratingStorageSettled = true;
	const pfc::string8 name = cfg_var_modern::cfg_var_common::formatVarName(guid_rating_storage);
	const bool chosen = fb2k::configStore::get()->getConfigInt(name, -1) != -1;
	if (!chosen && panelSavedBefore) cfg_ratingStorage = static_cast<int>(RatingStorage::FileTags);
}

// A component reports its file name without the extension.
bool IsPlaybackStatisticsInstalled() {
	pfc::string8 fileName;
	for (auto component : componentversion::enumerate()) {
		component->get_file_name(fileName);
		if (stricmp_utf8(fileName, "foo_playcount") == 0) return true;
	}
	return false;
}

// The store's rating once GroupPlaylist rated the track or took its rating away, else the RATING tag. The bracket
// keeps a taken-away rating empty rather than "?".
const char* RatingReadPattern(RatingStorage storage) {
	return storage == RatingStorage::FileTags ? "$meta(rating)" : "$if(%gp_rating_set%,[%gp_rating%],$meta(rating))";
}

void WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage) {
	if (storage != RatingStorage::FileTags) WriteRatingToStore(track, rating);
	if (storage != RatingStorage::Store) WriteRatingTag(track, rating);
}

const char* LikeReadPattern(RatingStorage storage) {
	return storage == RatingStorage::FileTags ? "$meta(loved)" : "$if(%gp_liked_set%,[%gp_liked%],$meta(loved))";
}

const char* LikedTracksQuery(RatingStorage storage) {
	return storage == RatingStorage::FileTags ? "LOVED IS 1" : "%gp_liked% IS 1 OR (LOVED IS 1 AND %gp_liked_set% MISSING)";
}

void WriteLikes(metadb_handle_list_cref tracks, bool liked, RatingStorage storage) {
	if (tracks.get_count() == 0) return;
	if (storage != RatingStorage::FileTags) {
		const uint64_t now = track_store::Now();
		track_store::Update(tracks, [liked, now](TrackRecord& record) { return SetLiked(record, liked, now); });
	}
	if (storage != RatingStorage::Store) WriteTag(tracks, "LOVED", liked ? "1" : "");
}
