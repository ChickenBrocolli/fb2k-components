#include "stdafx.h"

// GroupPlaylist > Like and Unlike in foobar2000's track menu, so tracks can be liked from any playlist or library view,
// several at once. The like is kept where the ratings are (rating_store.h).

#include "rating_store.h"
#include "ui_language.h"

namespace {

constexpr GUID guid_like_group = { 0x2e8b6d41, 0x7c3a, 0x4f95, { 0x8d, 0x12, 0x6a, 0xf0, 0x3b, 0x9e, 0x54, 0xc7 } };

contextmenu_group_popup_factory g_likeGroup(guid_like_group, contextmenu_groups::root, "GroupPlaylist", 0);

class LikeCommands : public contextmenu_item_simple {
public:
	enum { kLike, kUnlike, kCount };

	GUID get_parent() override { return guid_like_group; }
	unsigned get_num_items() override { return kCount; }

	void get_item_name(unsigned index, pfc::string_base& out) override {
		out = index == kLike ? Tr("Like") : Tr("Unlike");
	}

	GUID get_item_guid(unsigned index) override {
		static const GUID like = { 0x6f1d93a2, 0x0b47, 0x4e8c, { 0xa9, 0x35, 0x12, 0xd8, 0x7e, 0x60, 0xc4, 0x2b } };
		static const GUID unlike = { 0x81c4e57b, 0x3d29, 0x4a06, { 0xb6, 0x7f, 0x4e, 0x95, 0x0a, 0xd1, 0x28, 0x3c } };
		return index == kLike ? like : unlike;
	}

	bool get_item_description(unsigned index, pfc::string_base& out) override {
		out = index == kLike ? Tr("Likes the selected tracks.") : Tr("Removes the like from the selected tracks.");
		return true;
	}

	void context_command(unsigned index, metadb_handle_list_cref tracks, const GUID&) override {
		WriteLikes(tracks, index == kLike, GetRatingStorage());
	}
};

FB2K_SERVICE_FACTORY(LikeCommands);

} // namespace
