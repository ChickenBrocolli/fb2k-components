#pragma once

// Where the rating column reads and writes ratings. One choice for every panel, kept in the foobar2000 configuration.
//
// FileTags            The RATING tag inside the music file. Travels with the file. Every click rewrites the file.
// PlaybackStatistics  The database of the Playback Statistics component (foo_playcount). Leaves the file alone.
// Both                Writes both. Shows the Playback Statistics rating, or the tag when the database has none.
enum class RatingStorage { FileTags = 0, PlaybackStatistics = 1, Both = 2 };

RatingStorage GetRatingStorage();
void SetRatingStorage(RatingStorage storage);

// True when the storage needs the Playback Statistics component.
inline bool NeedsPlaybackStatistics(RatingStorage storage) {
	return storage != RatingStorage::FileTags;
}

// Whether the Playback Statistics component (foo_playcount) is loaded.
bool IsPlaybackStatisticsInstalled();

// The title formatting pattern that reads the rating from the storage.
const char* RatingReadPattern(RatingStorage storage);

// Sets the rating of one track, 1 to 5, or removes it with 0. The write finishes later, and the
// playlist reports the change then. Returns false when the Playback Statistics part could not be
// written because the component is missing. The tag part of Both is written either way.
bool WriteRating(metadb_handle_ptr track, int rating, RatingStorage storage);
