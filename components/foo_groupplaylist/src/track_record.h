#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

// What GroupPlaylist keeps about one track in its own store (track_store.h): whether it is liked. Ratings and play
// history are the Playback Statistics component's. Times are Windows file times (100-nanosecond steps since 1601),
// 0 when there is none.
struct TrackRecord {
	// What 0.10.0 kept here in place of Playback Statistics. No longer filled or shown; the fields stay so that
	// saved records read and write as before.
	uint32_t rating = 0;
	uint32_t playCount = 0;
	uint64_t firstPlayed = 0;
	uint64_t lastPlayed = 0;
	uint64_t added = 0;
	bool ratingSet = false;

	uint32_t liked = 0;  // 1 when the track is liked (version 2)
	uint64_t likedAt = 0;  // when it was liked, 0 when not liked or liked before version 3
	// Set when the like was given or taken away in GroupPlaylist (version 4). Then the store's value stands even
	// when it is "not liked", and the file's LOVED tag no longer shows in its place.
	bool likeSet = false;

	bool operator==(const TrackRecord&) const = default;
	bool IsEmpty() const { return *this == TrackRecord{}; }

	// Whether the store decides the like. A record saved before version 4 decides only when it is liked.
	bool LikeDecided() const { return likeSet || liked != 0; }
};

// The saved form: a version number, then the fields in order, little-endian. A later version only adds fields at
// the end, so a record saved by a newer GroupPlaylist still reads here, and an older one reads with the new fields empty.
constexpr uint32_t kTrackRecordVersion = 4;

namespace track_record_detail {

inline void Put(std::vector<uint8_t>& out, uint64_t value, int bytes) {
	for (int i = 0; i < bytes; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

class Reader {
public:
	Reader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}
	bool Has(int bytes) const { return m_pos + static_cast<size_t>(bytes) <= m_size; }
	uint64_t Take(int bytes) {
		uint64_t value = 0;
		for (int i = 0; i < bytes; ++i) value |= static_cast<uint64_t>(m_data[m_pos + i]) << (8 * i);
		m_pos += static_cast<size_t>(bytes);
		return value;
	}

private:
	const uint8_t* m_data;
	size_t m_size;
	size_t m_pos = 0;
};

} // namespace track_record_detail

inline std::vector<uint8_t> SerializeTrackRecord(const TrackRecord& record) {
	using track_record_detail::Put;
	std::vector<uint8_t> out;
	Put(out, kTrackRecordVersion, 4);
	Put(out, record.rating, 4);
	Put(out, record.playCount, 4);
	Put(out, record.firstPlayed, 8);
	Put(out, record.lastPlayed, 8);
	Put(out, record.added, 8);
	Put(out, record.liked, 4);
	Put(out, record.likedAt, 8);
	Put(out, (record.ratingSet ? 1u : 0u) | (record.likeSet ? 2u : 0u), 4);
	return out;
}

// Empty data is an empty record. Data too short for the fields its version promises is damaged: nullopt.
inline std::optional<TrackRecord> ParseTrackRecord(const uint8_t* data, size_t size) {
	TrackRecord record;
	if (size == 0) return record;
	track_record_detail::Reader reader(data, size);
	if (!reader.Has(4)) return std::nullopt;
	const uint64_t version = reader.Take(4);
	if (version < 1) return std::nullopt;
	if (!reader.Has(4 + 4 + 8 + 8 + 8)) return std::nullopt;
	record.rating = static_cast<uint32_t>((std::min)(reader.Take(4), uint64_t{ 5 }));
	record.playCount = static_cast<uint32_t>(reader.Take(4));
	record.firstPlayed = reader.Take(8);
	record.lastPlayed = reader.Take(8);
	record.added = reader.Take(8);
	if (version >= 2) {
		if (!reader.Has(4)) return std::nullopt;
		record.liked = reader.Take(4) != 0 ? 1 : 0;
	}
	if (version >= 3) {
		if (!reader.Has(8)) return std::nullopt;
		record.likedAt = reader.Take(8);
	}
	if (version >= 4) {
		if (!reader.Has(4)) return std::nullopt;
		const uint64_t flags = reader.Take(4);
		record.ratingSet = (flags & 1) != 0;
		record.likeSet = (flags & 2) != 0;
	}
	return record;
}

// Likes or unlikes the track at the given time, and reports whether that changed the record. Liking an already liked
// track keeps the time it was first liked, so "Liked tracks" keeps its order.
inline bool SetLiked(TrackRecord& record, bool liked, uint64_t now) {
	if (record.likeSet && (record.liked != 0) == liked) return false;
	if ((record.liked != 0) != liked) record.likedAt = liked ? now : 0;
	record.liked = liked ? 1 : 0;
	record.likeSet = true;
	return true;
}
