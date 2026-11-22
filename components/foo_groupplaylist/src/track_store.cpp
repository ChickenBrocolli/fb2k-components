#include "stdafx.h"

// Built after foo_sample/rating.cpp in the foobar2000 SDK (Peter Pawlowski, the SDK's licence in THIRD-PARTY-NOTICES.txt).

#include <set>
#include <string>
#include <vector>

#include <pfc/filetimetools.h>

#include "track_store.h"

namespace {

// Make a new GUID if the way a track is turned into its key ever changes, or old records would land on the wrong tracks.
constexpr GUID guid_track_index = { 0x3c1f7a52, 0x8d0e, 0x4b6a, { 0x9e, 0x27, 0x41, 0xd5, 0x0c, 0x6b, 0xf3, 0x88 } };

// Artist, album and title when the track has a title tag, else its location.
constexpr const char* kTrackKey = "$if($meta(title),$lower($meta(artist)|$meta(album)|$meta(title)),%path%|%subsong%)";

// Records of tracks no longer in the library are kept this long, in case the files come back.
const t_filetimestamp kRetention = system_time_periods::week * 52;

class TrackKey : public metadb_index_client {
public:
	TrackKey() { titleformat_compiler::get()->compile_force(m_key, kTrackKey); }

	metadb_index_hash transform(const file_info& info, const playable_location& location) override {
		pfc::string_formatter text;
		m_key->run_simple(location, &info, text);
		return hasher_md5::get()->process_single_string(text).xorHalve();
	}

private:
	titleformat_object::ptr m_key;
};

TrackKey* Key() {
	// Never released: foobar2000 keeps it until the process ends.
	static TrackKey* key = new service_impl_single_t<TrackKey>();
	return key;
}

metadb_index_manager::ptr Index() {
	static metadb_index_manager* index = metadb_index_manager::get().detach();
	return index;
}

bool g_ready = false;

class StoreInit : public init_stage_callback {
public:
	void on_init_stage(t_uint32 stage) override {
		if (stage != init_stages::before_config_read) return;
		try {
			Index()->add(Key(), guid_track_index, kRetention);
			g_ready = true;
		} catch (std::exception const& e) {
			Index()->remove(guid_track_index);
			FB2K_console_formatter() << "GroupPlaylist: the store of likes could not start: " << e;
			return;
		}
		Index()->dispatch_global_refresh();
	}
};

FB2K_SERVICE_FACTORY(StoreInit);

TrackRecord Read(metadb_index_hash hash) {
	if (!g_ready) return {};
	mem_block_container_impl data;
	Index()->get_user_data(guid_track_index, hash, data);
	const auto record = ParseTrackRecord(static_cast<const uint8_t*>(data.get_ptr()), data.get_size());
	return record ? *record : TrackRecord{};
}

// One transaction holds every write of a batch, as the SDK asks for many updates in a row (metadb_index.h).
void Write(metadb_index_transaction::ptr transaction, metadb_index_hash hash, const TrackRecord& record) {
	if (record.IsEmpty()) {
		transaction->set_user_data(guid_track_index, hash, nullptr, 0);
		return;
	}
	const std::vector<uint8_t> data = SerializeTrackRecord(record);
	transaction->set_user_data(guid_track_index, hash, data.data(), data.size());
}

// The fields, in the order of the field indexes: %gp_liked% (1 when liked), %gp_liked_date% and %gp_liked_set%
// (1 when the store decides the like, so the read pattern shows the file's LOVED tag only for a track GroupPlaylist
// never liked or unliked, rating_store.cpp). A field with no value reports false, so "%gp_liked% MISSING" finds the
// tracks that are not liked.
enum class Field { Liked, LikedDate, LikeSet };
constexpr const char* kFieldNames[] = { "gp_liked", "gp_liked_date", "gp_liked_set" };

class StoreFields : public metadb_display_field_provider_v2 {
public:
	t_uint32 get_field_count() override { return static_cast<t_uint32>(std::size(kFieldNames)); }

	void get_field_name(t_uint32 index, pfc::string_base& out) override { out = kFieldNames[index]; }

	bool process_field(t_uint32 index, metadb_handle* handle, titleformat_text_out* out) override {
		return process_field_v2(index, handle, handle->query_v2_(), out);
	}

	bool process_field_v2(t_uint32 index, metadb_handle* handle, metadb_v2::rec_t const& rec, titleformat_text_out* out) override {
		if (!g_ready || !rec.info.is_valid()) return false;
		const TrackRecord record = Read(Key()->transform(rec.info->info(), handle->get_location()));
		switch (static_cast<Field>(index)) {
		case Field::Liked: return WriteFlag(record.liked != 0, out);
		case Field::LikedDate: return WriteTime(record.likedAt, out);
		case Field::LikeSet: return WriteFlag(record.LikeDecided(), out);
		}
		return false;
	}

private:
	static bool WriteFlag(bool on, titleformat_text_out* out) {
		if (!on) return false;
		out->write_int(titleformat_inputtypes::meta, 1);
		return true;
	}

	// "YYYY-MM-DD hh:mm:ss" in local time, which the query syntax reads for "DURING LAST 2 WEEKS" and the like.
	static bool WriteTime(FileTime time, titleformat_text_out* out) {
		if (time == 0) return false;
		out->write(titleformat_inputtypes::meta, pfc::format_filetimestamp(time));
		return true;
	}
};

FB2K_SERVICE_FACTORY(StoreFields);

} // namespace

namespace track_store {

TrackRecord Get(const metadb_handle_ptr& track) {
	metadb_index_hash hash = 0;
	if (!g_ready || !Key()->hashHandle(track, hash)) return {};
	return Read(hash);
}

void Update(metadb_handle_list_cref tracks, const std::function<bool(TrackRecord&)>& change) {
	if (!g_ready) return;
	std::set<metadb_index_hash> seen;
	pfc::list_t<metadb_index_hash> changed;
	metadb_index_transaction::ptr transaction;
	for (size_t i = 0; i < tracks.get_count(); ++i) {
		metadb_index_hash hash = 0;
		if (!Key()->hashHandle(tracks[i], hash) || !seen.insert(hash).second) continue;
		TrackRecord record = Read(hash);
		if (!change(record)) continue;
		if (transaction.is_empty()) transaction = metadb_index_manager_v2::get()->begin_transaction();
		Write(transaction, hash, record);
		changed += hash;
	}
	if (transaction.is_empty()) return;
	transaction->commit();
	Index()->dispatch_refresh(guid_track_index, changed);
}

FileTime Now() {
	return filetimestamp_from_system_timer();
}

} // namespace track_store
