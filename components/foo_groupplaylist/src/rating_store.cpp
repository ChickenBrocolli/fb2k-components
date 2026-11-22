#include "stdafx.h"

#include "rating_store.h"

#include <string>
#include <vector>

#include "track_store.h"
#include "view_settings_rules.h"

namespace {

constexpr GUID guid_rating_storage = { 0x613e1a94, 0xeb91, 0x4f0b, { 0xbc, 0xd3, 0x47, 0x52, 0x4d, 0x79, 0x3d, 0x33 } };

cfg_int cfg_ratingStorage(guid_rating_storage, static_cast<int>(RatingStorage::Statistics));

// kNeverSaved until the user picks where likes go: until then they go where the rating choice sends them.
constexpr GUID guid_like_storage = { 0x4a92d6c7, 0x1e58, 0x4b3f, { 0x87, 0x0d, 0x5c, 0x61, 0xb4, 0x2e, 0x9f, 0x13 } };
cfg_int cfg_likeStorage(guid_like_storage, kNeverSaved);

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

// Menu names can carry '&' to mark the access key. The names are compared without it.
bool SameMenuName(const char* menuName, const char* wanted) {
	std::string plain;
	for (const char* walk = menuName; *walk; ++walk) {
		if (*walk != '&') plain += *walk;
	}
	return stricmp_utf8(plain.c_str(), wanted) == 0;
}

contextmenu_node* ChildNamed(contextmenu_node* node, const char* name) {
	for (t_size index = 0; index < node->get_num_children(); ++index) {
		contextmenu_node* child = node->get_child(index);
		if (child && SameMenuName(child->get_name(), name)) return child;
	}
	return nullptr;
}

// The command at the end of the path, starting from this node or from any submenu below it. The user can move a
// component's menu into another submenu (Preferences > Display > Context Menu), so the path may not start at the top.
contextmenu_node* FindCommand(contextmenu_node* node, const std::vector<const char*>& path) {
	if (!node) return nullptr;
	contextmenu_node* walk = node;
	for (const char* name : path) {
		walk = walk ? ChildNamed(walk, name) : nullptr;
	}
	if (walk && walk->get_type() == contextmenu_item_node::TYPE_COMMAND) return walk;
	for (t_size index = 0; index < node->get_num_children(); ++index) {
		contextmenu_node* child = node->get_child(index);
		if (!child || child->get_type() != contextmenu_item_node::TYPE_POPUP) continue;
		if (contextmenu_node* found = FindCommand(child, path)) return found;
	}
	return nullptr;
}

// Runs a command of the track's context menu, found by the names along its menu path.
bool RunContextCommand(metadb_handle_ptr track, const std::vector<const char*>& path) {
	metadb_handle_list tracks;
	tracks.add_item(track);
	contextmenu_manager::ptr manager;
	contextmenu_manager::g_create(manager);
	// The full menu, so a command the user hid from the short one (shown only with Shift) is found too.
	manager->init_context(tracks, contextmenu_manager::flag_view_full);
	contextmenu_node* node = FindCommand(manager->get_root(), path);
	if (!node) return false;
	node->execute();
	return true;
}

// The Playback Statistics component offers no programming interface for ratings, only its context menu commands.
// Script playlists (JS Smooth Playlist, JSPlaylist) rate through the same commands.
void WriteRatingToPlaybackStatistics(metadb_handle_ptr track, int rating) {
	const std::string value = rating > 0 ? std::to_string(rating) : std::string("<not set>");
	if (RunContextCommand(track, { "Playback Statistics", "Rating", value.c_str() })) return;
	FB2K_console_formatter() << "GroupPlaylist: the Playback Statistics rating command was not found, so the rating was not saved.";
}

} // namespace

RatingStorage GetRatingStorage() {
	switch (cfg_ratingStorage) {
	case static_cast<int>(RatingStorage::FileTags): return RatingStorage::FileTags;
	case static_cast<int>(RatingStorage::Both): return RatingStorage::Both;
	default: return RatingStorage::Statistics;
	}
}

LikeStorage GetLikeStorage() {
	switch (cfg_likeStorage) {
	case static_cast<int>(LikeStorage::Store): return LikeStorage::Store;
	case static_cast<int>(LikeStorage::FileTags): return LikeStorage::FileTags;
	case static_cast<int>(LikeStorage::Both): return LikeStorage::Both;
	}
	return LikeStorageFor(GetRatingStorage());
}

LikeStorage LikeStorageFor(RatingStorage ratings) {
	switch (ratings) {
	case RatingStorage::FileTags: return LikeStorage::FileTags;
	case RatingStorage::Both: return LikeStorage::Both;
	default: return LikeStorage::Store;
	}
}

void SetStorageChoices(RatingStorage ratings, LikeStorage likes) {
	cfg_likeStorage = static_cast<int>(likes);
	cfg_ratingStorage = static_cast<int>(ratings);
}

// foobar2000 saves a setting only when it differs from its default. Before the store, the default was File tags, so a
// profile that rated in file tags has no saved choice, and would read the new default. A panel saved by an earlier
// GroupPlaylist shows that the profile is such a one; then File tags is saved as its choice. Decided once.
void SettleRatingStorage(bool panelSavedBefore) {
	if (cfg_ratingStorageSettled) return;
	cfg_ratingStorageSettled = true;
	const pfc::string8 name = cfg_var_modern::cfg_var_common::formatVarName(guid_rating_storage);
	const bool chosen = fb2k::configStore::get()->getConfigInt(name, kNeverSaved) != kNeverSaved;
	if (!chosen && panelSavedBefore) cfg_ratingStorage = static_cast<int>(RatingStorage::FileTags);
}

// A component reports its file name without the extension. Components do not come or go while foobar2000 runs.
bool IsPlaybackStatisticsInstalled() {
	static const bool installed = [] {
		pfc::string8 fileName;
		for (auto component : componentversion::enumerate()) {
			component->get_file_name(fileName);
			if (stricmp_utf8(fileName, "foo_playcount") == 0) return true;
		}
		return false;
	}();
	return installed;
}

// Ratings need Playback Statistics for anything but the file's tag.
static RatingStorage RatingStorageInUse(RatingStorage storage) {
	return IsPlaybackStatisticsInstalled() ? storage : RatingStorage::FileTags;
}

// %rating% is the Playback Statistics component's: its rating, and nothing when it has none. The bracket keeps a
// missing rating empty rather than "?".
const char* RatingReadPattern(RatingStorage storage) {
	switch (RatingStorageInUse(storage)) {
	case RatingStorage::FileTags: return "$meta(rating)";
	case RatingStorage::Both: return "$if2(%rating%,$meta(rating))";
	default: return "[%rating%]";
	}
}

void WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage) {
	storage = RatingStorageInUse(storage);
	if (storage != RatingStorage::FileTags) WriteRatingToPlaybackStatistics(track, rating);
	if (storage != RatingStorage::Statistics) WriteRatingTag(track, rating);
}

const char* LikeReadPattern(LikeStorage storage) {
	return storage == LikeStorage::FileTags ? "$meta(loved)" : "$if(%gp_liked_set%,[%gp_liked%],$meta(loved))";
}

void WriteLikes(metadb_handle_list_cref tracks, bool liked, LikeStorage storage) {
	if (tracks.get_count() == 0) return;
	if (storage != LikeStorage::FileTags) {
		const FileTime now = track_store::Now();
		track_store::Update(tracks, [liked, now](TrackRecord& record) { return SetLiked(record, liked, now); });
	}
	if (storage != LikeStorage::Store) WriteTag(tracks, "LOVED", liked ? "1" : "");
}
