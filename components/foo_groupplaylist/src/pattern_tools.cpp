#include "stdafx.h"

#include "pattern_tools.h"
#include "ui_language.h"

namespace {

// The labels are English; the menu shows them in the current language (ui_text.h).
struct FieldEntry {
	const char* label;
	const char* text;
};

struct FieldGroup {
	const char* label;
	const FieldEntry* entries;
	size_t count;
};

constexpr FieldEntry kTagFields[] = {
	{ "Artist", "%artist%" },
	{ "Album artist", "%album artist%" },
	{ "Album", "%album%" },
	{ "Title", "%title%" },
	{ "Track number", "%tracknumber%" },
	{ "Disc number", "%discnumber%" },
	{ "Date", "%date%" },
	{ "Genre", "%genre%" },
	{ "Composer", "%composer%" },
	{ "Comment", "%comment%" },
};

constexpr FieldEntry kAudioFields[] = {
	{ "Length", "%length%" },
	{ "Codec", "%codec%" },
	{ "Bitrate (kbps)", "%bitrate%" },
	{ "Sample rate (Hz)", "%samplerate%" },
	{ "Channels", "%channels%" },
	{ "Bits per sample", "%__bitspersample%" },
};

constexpr FieldEntry kFileFields[] = {
	{ "File name", "%filename%" },
	{ "File name with extension", "%filename_ext%" },
	{ "Folder name", "%directoryname%" },
	{ "Full path", "%path%" },
	{ "File size", "%filesize_natural%" },
};

// GroupPlaylist's own store (track_store.h).
constexpr FieldEntry kStatisticsFields[] = {
	{ "Play count", "%gp_play_count%" },
	{ "Rating (number)", "%gp_rating%" },
	{ "Rating (stars)", "$repeat($char(9733),%gp_rating%)$repeat($char(9734),$sub(5,%gp_rating%))" },
	{ "First played", "%gp_first_played%" },
	{ "Last played", "%gp_last_played%" },
	{ "Added to library", "%gp_added%" },
};

constexpr FieldEntry kReplayGainFields[] = {
	{ "Track gain", "%replaygain_track_gain%" },
	{ "Album gain", "%replaygain_album_gain%" },
	{ "Track peak", "%replaygain_track_peak%" },
	{ "Album peak", "%replaygain_album_peak%" },
};

constexpr FieldEntry kPlaylistFields[] = {
	{ "Playing mark", "$if(%isplaying%,$char(9654),)" },
	{ "Position in playlist", "%list_index%" },
};

constexpr FieldEntry kSyntaxPieces[] = {
	{ "Hide when empty: [ ... ]", "[]" },
	{ "Line break in a cover text: |", "|" },
};

constexpr FieldGroup kFieldGroups[] = {
	{ "Tags", kTagFields, std::size(kTagFields) },
	{ "Audio", kAudioFields, std::size(kAudioFields) },
	{ "File", kFileFields, std::size(kFileFields) },
	{ "Playback statistics", kStatisticsFields, std::size(kStatisticsFields) },
	{ "ReplayGain", kReplayGainFields, std::size(kReplayGainFields) },
	{ "Playlist", kPlaylistFields, std::size(kPlaylistFields) },
	{ "Syntax", kSyntaxPieces, std::size(kSyntaxPieces) },
};

} // namespace

void RunInsertFieldMenu(CWindow button, CEdit patternEdit) {
	std::vector<const char*> textByCommand = { nullptr };
	CMenu menu;
	menu.CreatePopupMenu();
	for (const FieldGroup& group : kFieldGroups) {
		CMenu submenu;
		submenu.CreatePopupMenu();
		for (size_t index = 0; index < group.count; ++index) {
			submenu.AppendMenu(MF_STRING, static_cast<UINT_PTR>(textByCommand.size()), pfc::stringcvt::string_os_from_utf8(Tr(group.entries[index].label)));
			textByCommand.push_back(group.entries[index].text);
		}
		menu.AppendMenu(MF_POPUP, submenu.Detach(), pfc::stringcvt::string_os_from_utf8(Tr(group.label)));
	}

	CRect buttonRect;
	button.GetWindowRect(buttonRect);
	const int command = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, buttonRect.left, buttonRect.bottom, button.GetParent());
	if (command <= 0 || command >= static_cast<int>(textByCommand.size())) return;

	// An edit control keeps its caret position while another control has focus, so the field lands where the user was typing.
	patternEdit.ReplaceSel(pfc::stringcvt::string_os_from_utf8(textByCommand[command]), TRUE);
	patternEdit.SetFocus();
}

std::string DescribePatternPreview(const char* pattern) {
	auto playlists = playlist_manager::get();
	const t_size count = playlists->activeplaylist_get_item_count();
	if (count == 0) return Tr("Preview: (the playlist is empty)");
	t_size item = playlists->activeplaylist_get_focus_item();
	if (item >= count) item = 0;

	titleformat_object::ptr script;
	if (!titleformat_compiler::get()->compile(script, pattern)) return Tr("Preview: (the pattern has a syntax error)");
	pfc::string8 value;
	playlists->activeplaylist_item_format_title(item, nullptr, value, script, nullptr, playback_control::display_level_all);
	if (value.is_empty()) return Tr("Preview: (empty for the focused track)");
	return std::string(Tr("Preview: ")) + value.c_str();
}
