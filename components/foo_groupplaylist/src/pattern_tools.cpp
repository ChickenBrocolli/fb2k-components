#include "stdafx.h"

#include "pattern_tools.h"
#include "text_painter.h"
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

// The fields of the Playback Statistics component. In brackets, so a column shows nothing, not "?", without it.
constexpr FieldEntry kStatisticsFields[] = {
	{ "Play count", "[%play_count%]" },
	{ "Rating (number)", "[%rating%]" },
	{ "Rating (stars)", "$if(%rating%,$repeat($char(9733),%rating%)$repeat($char(9734),$sub(5,%rating%)),)" },
	{ "First played", "[%first_played%]" },
	{ "Last played", "[%last_played%]" },
	{ "Added to library", "[%added%]" },
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

std::string PatternPreviewValue(const char* pattern) {
	auto playlists = playlist_manager::get();
	const t_size count = playlists->activeplaylist_get_item_count();
	if (count == 0) return Tr("(the playlist is empty)");
	t_size item = playlists->activeplaylist_get_focus_item();
	if (item >= count) item = 0;

	titleformat_object::ptr script;
	if (!titleformat_compiler::get()->compile(script, pattern)) return Tr("(the pattern has a syntax error)");
	pfc::string8 value;
	playlists->activeplaylist_item_format_title(item, nullptr, value, script, nullptr, playback_control::display_level_all);
	if (value.is_empty()) return Tr("(empty for the focused track)");
	return value.c_str();
}

std::string DescribePatternPreview(const char* pattern) {
	return std::string(Tr("Preview:")) + '\t' + PatternPreviewValue(pattern);
}

void DrawPreviewText(HWND dialog, const DRAWITEMSTRUCT& item, TextPainter& painter) {
	CDCHandle dc(item.hDC);
	const CWindow control(item.hwndItem);
	// The dialog's own colours for a text control, so the preview matches the page in light and dark mode.
	dc.SetTextColor(GetSysColor(COLOR_WINDOWTEXT));
	dc.SetBkColor(GetSysColor(COLOR_BTNFACE));
	HBRUSH background = reinterpret_cast<HBRUSH>(::SendMessage(dialog, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(item.hDC), reinterpret_cast<LPARAM>(item.hwndItem)));
	if (background) dc.FillRect(&item.rcItem, background);
	else dc.FillSolidRect(&item.rcItem, GetSysColor(COLOR_BTNFACE));

	const HFONT oldFont = dc.SelectFont(control.GetFont());
	TEXTMETRIC metrics = {};
	dc.GetTextMetrics(&metrics);
	const int lineHeight = metrics.tmHeight + metrics.tmExternalLeading;

	const std::wstring text(pfc::stringcvt::string_os_from_utf8(uGetWindowText(control).c_str()));
	// Lines are split at '\n'. A '\t' splits a line into its name and its value; the values of all lines start at
	// the same place, after the first line's name.
	int valueLeft = item.rcItem.left;
	const size_t firstTab = text.find(L'\t');
	const size_t firstEnd = text.find(L'\n');
	if (firstTab != std::wstring::npos && firstTab < firstEnd) {
		valueLeft += painter.Width(dc, std::wstring_view(text).substr(0, firstTab)) + metrics.tmAveCharWidth;
	}
	int top = item.rcItem.top;
	for (size_t start = 0; start <= text.size() && top < item.rcItem.bottom;) {
		size_t end = text.find(L'\n', start);
		if (end == std::wstring::npos) end = text.size();
		const std::wstring_view line = std::wstring_view(text).substr(start, end - start);
		const size_t tab = line.find(L'\t');
		const CRect lineRect(item.rcItem.left, top, item.rcItem.right, top + lineHeight);
		if (tab == std::wstring_view::npos && painter.Width(dc, line) > lineRect.Width()) {
			// A sentence too long for one line, such as the hint for the Rating column, wraps as in a text control.
			CRect rest(item.rcItem.left, top, item.rcItem.right, item.rcItem.bottom);
			dc.SetBkMode(TRANSPARENT);
			dc.DrawText(line.data(), static_cast<int>(line.size()), &rest, DT_WORDBREAK | DT_NOPREFIX);
			break;
		} else if (tab == std::wstring_view::npos) {
			painter.Draw(dc, line, lineRect, TextPainter::Align::Left, true);
		} else {
			if (tab > 0) painter.Draw(dc, line.substr(0, tab), CRect(lineRect.left, lineRect.top, valueLeft, lineRect.bottom), TextPainter::Align::Left, true);
			painter.Draw(dc, line.substr(tab + 1), CRect(valueLeft, lineRect.top, lineRect.right, lineRect.bottom), TextPainter::Align::Left, true);
		}
		top += lineHeight;
		start = end + 1;
	}
	dc.SelectFont(oldFont);
}
