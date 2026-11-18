#include "stdafx.h"

#include "rating_store.h"

#include <string>
#include <vector>

namespace {

constexpr GUID guid_rating_storage = { 0x613e1a94, 0xeb91, 0x4f0b, { 0xbc, 0xd3, 0x47, 0x52, 0x4d, 0x79, 0x3d, 0x33 } };

cfg_int cfg_ratingStorage(guid_rating_storage, static_cast<int>(RatingStorage::FileTags));

void WriteRatingTag(metadb_handle_ptr track, int rating) {
	metadb_handle_list tracks;
	tracks.add_item(track);
	const std::string value = rating > 0 ? std::to_string(rating) : std::string();
	auto filter = file_info_filter::create([value](trackRef, t_filestats, file_info& info) {
		const char* current = info.meta_get("RATING", 0);
		if (value == (current ? current : "")) return false;
		if (value.empty()) info.meta_remove_field("RATING");
		else info.meta_set("RATING", value.c_str());
		return true;
	});
	metadb_io_v2::get()->update_info_async(tracks, filter, core_api::get_main_window(), metadb_io_v2::op_flag_delay_ui, nullptr);
}

// Menu names can carry '&' to mark the access key. The names are compared without it.
bool SameMenuName(const char* menuName, const char* wanted) {
	std::string plain;
	for (const char* walk = menuName; *walk; ++walk) {
		if (*walk != '&') plain += *walk;
	}
	return stricmp_utf8(plain.c_str(), wanted) == 0;
}

// Runs a command of the track's context menu, found by the names along its menu path.
// The Playback Statistics component offers no programming interface for ratings, only these commands.
bool RunContextCommand(metadb_handle_ptr track, const std::vector<const char*>& path) {
	metadb_handle_list tracks;
	tracks.add_item(track);
	contextmenu_manager::ptr manager;
	contextmenu_manager::g_create(manager);
	manager->init_context(tracks, 0);
	contextmenu_node* node = manager->get_root();
	for (const char* name : path) {
		if (!node) return false;
		contextmenu_node* found = nullptr;
		for (t_size index = 0; index < node->get_num_children() && !found; ++index) {
			contextmenu_node* child = node->get_child(index);
			if (child && SameMenuName(child->get_name(), name)) found = child;
		}
		node = found;
	}
	if (!node || node->get_type() != contextmenu_item_node::TYPE_COMMAND) return false;
	node->execute();
	return true;
}

} // namespace

RatingStorage GetRatingStorage() {
	switch (cfg_ratingStorage) {
	case static_cast<int>(RatingStorage::PlaybackStatistics): return RatingStorage::PlaybackStatistics;
	case static_cast<int>(RatingStorage::Both): return RatingStorage::Both;
	default: return RatingStorage::FileTags;
	}
}

void SetRatingStorage(RatingStorage storage) {
	cfg_ratingStorage = static_cast<int>(storage);
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

// The Playback Statistics field %rating% is empty when the database has no rating. It does not fall back to the tag.
const char* RatingReadPattern(RatingStorage storage) {
	switch (storage) {
	case RatingStorage::PlaybackStatistics: return "%rating%";
	case RatingStorage::Both: return "$if2(%rating%,$meta(rating))";
	default: return "$meta(rating)";
	}
}

bool WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage) {
	if (storage != RatingStorage::PlaybackStatistics) WriteRatingTag(track, rating);
	if (!NeedsPlaybackStatistics(storage)) return true;
	const std::string value = rating > 0 ? std::to_string(rating) : std::string("<not set>");
	return RunContextCommand(track, { "Playback Statistics", "Rating", value.c_str() });
}