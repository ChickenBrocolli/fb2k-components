#include "stdafx.h"

// Library > GroupPlaylist: commands that make autoplaylists from GroupPlaylist's own store (track_store.h).
// An autoplaylist keeps its query, so tracks come and go as they are played. The playlists are made with
// add_client_simple, so foobar2000 keeps them even without GroupPlaylist. Each comes with its order, and with a group
// preset for its panels while "Remember a preset per playlist" is on.

#include "group_preset.h"
#include "playlist_view.h"
#include "rating_store.h"
#include "ui_language.h"

namespace {

constexpr GUID guid_menu_group = { 0x6b2f4c10, 0x93d1, 0x4e8a, { 0xa5, 0x77, 0x2c, 0x41, 0x0e, 0xd9, 0x5b, 0x13 } };

// Albums in order: the usual library order.
#define LIBRARY_ORDER "%album artist% - %date% - %album% - %discnumber% - %tracknumber%"

struct AutoplaylistCommand {
	GUID guid;
	const char* name;         // English; the menu shows it in the current language
	const char* description;  // English
	const char* query;        // nullptr for the liked tracks, whose query follows where likes are kept
	// Newest first is the query syntax's own "SORT DESCENDING BY", which the autoplaylist's properties show as written.
	// The field is in brackets: a missing field reads "?", which would sort above every date.
	// Otherwise the sort pattern orders the list.
	const char* sortClause;
	const char* sort;
	// The group preset the playlist is shown with. Lists sorted by count or date start a group at nearly every track,
	// which reads best as artist and cover, like a viewing history. nullptr keeps the panel's preset.
	const char* preset;
};

const AutoplaylistCommand kCommands[] = {
	// Many tracks share a play count, so the count is turned around in the pattern and ties keep the album order.
	{ { 0x1d7e5a3c, 0x40b2, 0x4f6e, { 0x8c, 0x19, 0x7a, 0x03, 0xe4, 0x52, 0x9b, 0x61 } },
	  "Played more than 10 times", // ui-text
	  "Makes an autoplaylist of the tracks played more than 10 times, most played first.", // ui-text
	  "%gp_play_count% GREATER 10", "", "$num($sub(9999999,%gp_play_count%),7) - " LIBRARY_ORDER, kArtistCoverPresetName },
	{ { 0x58c0e9b4, 0x2a7d, 0x4c31, { 0x96, 0x4e, 0x0f, 0xb8, 0x13, 0x6d, 0xa2, 0x75 } },
	  "Played in the last year", // ui-text
	  "Makes an autoplaylist of the tracks played in the last 52 weeks, most recently played first.", // ui-text
	  "%gp_last_played% DURING LAST 52 WEEKS", " SORT DESCENDING BY [%gp_last_played%]", "", kArtistCoverPresetName },
	{ { 0x9f41b7d2, 0x6e08, 0x4a5c, { 0xb3, 0x2a, 0x55, 0xc7, 0x90, 0x1e, 0x4d, 0x86 } },
	  "Never played", // ui-text
	  "Makes an autoplaylist of the library tracks that have never been played.", // ui-text
	  "%gp_play_count% MISSING", "", LIBRARY_ORDER, nullptr },
	// Tracks liked before the like times were kept, or liked in file tags only, have no time and come last.
	{ { 0x3a6e0c95, 0xd148, 0x47b2, { 0x8f, 0x5b, 0x61, 0x2c, 0xa7, 0x9d, 0x0e, 0x34 } },
	  "Liked tracks", // ui-text
	  "Makes an autoplaylist of the liked tracks, most recently liked first.", // ui-text
	  nullptr, " SORT DESCENDING BY [%gp_liked_date%]", "", kArtistCoverPresetName },
};

#undef LIBRARY_ORDER

mainmenu_group_popup_factory g_menuGroup(guid_menu_group, mainmenu_groups::library, mainmenu_commands::sort_priority_dontcare, "GroupPlaylist");

class AutoplaylistMenu : public mainmenu_commands {
public:
	t_uint32 get_command_count() override { return static_cast<t_uint32>(std::size(kCommands)); }
	GUID get_command(t_uint32 index) override { return kCommands[index].guid; }
	void get_name(t_uint32 index, pfc::string_base& out) override { out = Tr(kCommands[index].name); }
	bool get_description(t_uint32 index, pfc::string_base& out) override {
		out = Tr(kCommands[index].description);
		return true;
	}
	GUID get_parent() override { return guid_menu_group; }

	void execute(t_uint32 index, service_ptr_t<service_base>) override {
		const AutoplaylistCommand& command = kCommands[index];
		auto playlists = playlist_manager::get();
		const char* name = Tr(command.name);
		const t_size playlist = playlists->create_playlist(name, pfc_infinite, pfc_infinite);
		if (playlist == pfc_infinite) return;
		try {
			const std::string query = std::string(command.query ? command.query : LikedTracksQuery(GetRatingStorage())) + command.sortClause;
			autoplaylist_manager::get()->add_client_simple(query.c_str(), command.sort, playlist, autoplaylist_flag_sort);
			if (command.preset) PlaylistView::SetPlaylistPreset(playlist, command.preset);
		} catch (std::exception const& e) {
			playlists->remove_playlist(playlist);
			popup_message::g_complain(Tr("The autoplaylist could not be made."), e);
			return;
		}
		playlists->set_active_playlist(playlist);
	}
};

FB2K_SERVICE_FACTORY(AutoplaylistMenu);

} // namespace
