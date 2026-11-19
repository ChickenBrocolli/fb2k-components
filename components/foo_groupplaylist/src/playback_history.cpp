#include "stdafx.h"

// Fills the play history in GroupPlaylist's store (track_store.h): plays, first and last played, and when a track
// came into the library.

#include "track_store.h"

namespace {

// foobar2000 itself decides when a track counts as played: after 60 seconds, or at its end when at least a third
// of it was heard. The Playback Statistics component counts on the same notice.
class PlayCounter : public playback_statistics_collector {
public:
	void on_item_played(metadb_handle_ptr track) override {
		metadb_handle_list tracks;
		tracks.add_item(track);
		const uint64_t now = track_store::Now();
		track_store::Update(tracks, [now](TrackRecord& record) {
			CountPlay(record, now);
			return true;
		});
	}
};

FB2K_SERVICE_FACTORY(PlayCounter);

void MarkAdded(metadb_handle_list_cref tracks) {
	const uint64_t now = track_store::Now();
	track_store::Update(tracks, [now](TrackRecord& record) {
		if (record.added != 0) return false;
		record.added = now;
		return true;
	});
}

// A track gets its added time when it comes into the library. When the library has loaded, every track still
// without one gets it too, so a library that was there before GroupPlaylist starts out dated to that day.
class LibraryWatcher : public library_callback_v2 {
public:
	void on_items_added(metadb_handle_list_cref items) override { MarkAdded(items); }
	void on_items_removed(metadb_handle_list_cref) override {}
	void on_items_modified(metadb_handle_list_cref) override {}
	void on_items_modified_v2(metadb_handle_list_cref, metadb_io_callback_v2_data&) override {}
	void on_library_initialized() override {
		metadb_handle_list items;
		library_manager::get()->get_all_items(items);
		MarkAdded(items);
	}
};

library_callback_factory_t<LibraryWatcher> g_libraryWatcher;

} // namespace
