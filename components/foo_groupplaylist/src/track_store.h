#pragma once

#include <functional>

#include "track_record.h"

// GroupPlaylist's own store of ratings and play history, kept by foobar2000 in its profile (metadb_index), so files are
// left alone and no other component is needed. Built after the rating database sample in the foobar2000 SDK
// (foo_sample/rating.cpp).
//
// A track is known by its artist, album and title, so its record stays when the file moves or is renamed. Tracks
// with the same three share a record. A track without a title tag is known by its path.
//
// The records show in title formatting as %gp_rating%, %gp_play_count%, %gp_first_played%, %gp_last_played%,
// %gp_added%, %gp_liked% (1 when liked) and %gp_liked_date%. When the Playback Statistics component is not installed, the first five
// also answer to that component's names (%rating%, %play_count%, %first_played%, %last_played%, %added%), so other
// components and skins keep working.
namespace track_store {

// The record of the track. Empty when the track has none or its tags cannot be read.
TrackRecord Get(const metadb_handle_ptr& track);

// Runs change on the record of every track, once per record, and saves the records it reports as changed.
// foobar2000 then refreshes everything that shows them: playlists, autoplaylists, searches. Main thread only.
void Update(metadb_handle_list_cref tracks, const std::function<bool(TrackRecord&)>& change);

// The current time as stored in a record.
uint64_t Now();

} // namespace track_store
