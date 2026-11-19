#pragma once

// Where ratings and likes are read and written. One choice for every panel, kept in the foobar2000 configuration.
//
// FileTags  The RATING and LOVED tags inside the music file. Travels with the file. Every click rewrites the file.
// Store     GroupPlaylist's own store (track_store.h). Leaves the file alone. The default.
// Both      Writes both. Shows the store's rating, or the tag when the store has none.
//
// The numbers are saved. 1 and 2 meant the Playback Statistics component before GroupPlaylist had its own store,
// so a panel set up then now uses the store in its place.
enum class RatingStorage { FileTags = 0, Store = 1, Both = 2 };

RatingStorage GetRatingStorage();
void SetRatingStorage(RatingStorage storage);

// Called when a panel loads. The first time, a profile that used an earlier GroupPlaylist without choosing where
// ratings are kept goes on keeping them in file tags, as it did; a new one starts with the store.
void SettleRatingStorage(bool panelSavedBefore);

// Whether the Playback Statistics component (foo_playcount) is loaded. GroupPlaylist does not use it; its own store
// leaves that component's field names to it when it is there.
bool IsPlaybackStatisticsInstalled();

// The title formatting pattern that reads the rating from the storage. The store falls back to the RATING tag,
// so ratings already in the files show until GroupPlaylist rates the track or takes the rating away.
const char* RatingReadPattern(RatingStorage storage);

// Sets the rating of one track, 1 to 5, or removes it with 0. The tag write finishes later, and the playlist
// reports the change then.
void WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage);

// The title formatting pattern that reads the like (1 when liked) from the storage, falling back to the LOVED tag
// for a track GroupPlaylist never liked or unliked.
const char* LikeReadPattern(RatingStorage storage);

// The condition of an autoplaylist that holds the liked tracks of the storage.
const char* LikedTracksQuery(RatingStorage storage);

// Likes or unlikes the tracks. A like is the LOVED tag set to 1 in the files.
void WriteLikes(metadb_handle_list_cref tracks, bool liked, RatingStorage storage);
