#pragma once

// Where ratings are read and written. One choice for every panel, kept in the foobar2000 configuration.
//
// FileTags    The RATING tag inside the music file. Travels with the file. Every click rewrites the file.
// Statistics  The Playback Statistics component (foo_playcount), as in other playlist views. Leaves the file alone.
//             The default.
// Both        Writes both. Shows the statistics' rating, or the tag when there is none.
//
// Without Playback Statistics there is nowhere but the file for a rating, so ratings then use the RATING tag whatever
// the choice.
// The numbers are saved, so each keeps its meaning.
enum class RatingStorage { FileTags = 0, Statistics = 1, Both = 2 };

// Where likes are read and written: a choice of their own, because Playback Statistics has no likes.
//
// Store     GroupPlaylist's own store (track_store.h). Leaves the file alone. The default.
// FileTags  The LOVED tag inside the music file, set to 1.
// Both      Writes both. Shows the store's like, or the tag for a track GroupPlaylist never liked or unliked.
//
// Until 0.10.1 one choice set both ratings and likes. A profile that never picked where likes go keeps what that
// choice meant for them (GetLikeStorage).
enum class LikeStorage { Store = 0, FileTags = 1, Both = 2 };

RatingStorage GetRatingStorage();
LikeStorage GetLikeStorage();
// What the one choice for ratings and likes meant for likes, before likes had a choice of their own.
LikeStorage LikeStorageFor(RatingStorage ratings);
// Saves both choices together: while likes were never picked they follow the rating choice, so a rating choice
// saved alone would move them too.
void SetStorageChoices(RatingStorage ratings, LikeStorage likes);

// Called when a panel loads. The first time, a profile that used an earlier GroupPlaylist without choosing where
// ratings are kept goes on keeping them in file tags, as it did; a new one starts with the store.
void SettleRatingStorage(bool panelSavedBefore);

// Whether the Playback Statistics component (foo_playcount) is loaded. Its fields are %rating%, %play_count%,
// %first_played%, %last_played% and %added%.
bool IsPlaybackStatisticsInstalled();

// The title formatting pattern that reads the rating from the storage: the RATING tag when Playback Statistics is
// not installed.
const char* RatingReadPattern(RatingStorage storage);

// Sets the rating of one track, 1 to 5, or removes it with 0. The tag write finishes later, and the playlist
// reports the change then.
void WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage);

// The title formatting pattern that reads the like (1 when liked) from the storage, falling back to the LOVED tag
// for a track GroupPlaylist never liked or unliked.
const char* LikeReadPattern(LikeStorage storage);

void WriteLikes(metadb_handle_list_cref tracks, bool liked, LikeStorage storage);
