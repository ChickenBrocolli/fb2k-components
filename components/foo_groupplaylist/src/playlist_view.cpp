#include "stdafx.h"

#include <libPPUI/PaintUtils.h>
#include <uxtheme.h>
#include <vssym32.h>

#pragma comment(lib, "uxtheme.lib")

#include "column_fit.h"
#include "column_store.h"
#include "playlist_view.h"
#include "preferences_pages.h"
#include "preset_store.h"
#include "rating_cell.h"
#include "rating_store.h"
#include "text_parts.h"
#include "ui_language.h"

namespace {

// The first two list columns belong to the blocks. Track columns follow.
constexpr size_t kCoverColumn = 0;
constexpr size_t kInfoColumn = 1;
constexpr size_t kFirstTrackColumn = 2;

std::wstring_view View(const pfc::stringcvt::string_os_from_utf8& text) {
	return std::wstring_view(text.get_ptr(), text.length());
}

// Sizes in pixels at 96 DPI. The cover size is a setting.
constexpr int kCoverInset = 4;
constexpr int kBlockGap = 12;
// The narrowest a track column gets from a drag while the columns fill the panel.
constexpr int kMinFitColumnWidth = 16;

constexpr unsigned kPlaylistCallbacks =
	playlist_callback_single::flag_on_items_added |
	playlist_callback_single::flag_on_items_reordered |
	playlist_callback_single::flag_on_items_removed |
	playlist_callback_single::flag_on_items_selection_change |
	playlist_callback_single::flag_on_item_focus_change |
	playlist_callback_single::flag_on_items_modified |
	playlist_callback_single::flag_on_items_modified_fromplayback |
	playlist_callback_single::flag_on_items_replaced |
	playlist_callback_single::flag_on_item_ensure_visible |
	playlist_callback_single::flag_on_playlist_switch;

constexpr unsigned kPlaybackCallbacks =
	play_callback::flag_on_playback_new_track |
	play_callback::flag_on_playback_stop |
	play_callback::flag_on_playback_pause |
	play_callback::flag_on_playback_time |
	play_callback::flag_on_playback_seek;

// Makes %bitrate% read as missing. Used only for block keys: tracks of one release differ in
// bitrate, and the original does not split a block on it.
class OmitBitrateHook : public titleformat_hook {
public:
	bool process_field(titleformat_text_out*, const char* name, t_size nameLength, bool& found) override {
		if (pfc::stricmp_ascii_ex(name, nameLength, "bitrate", SIZE_MAX) != 0) return false;
		found = false;
		return true;
	}
	bool process_function(titleformat_text_out*, const char*, t_size, titleformat_hook_function_params*, bool&) override {
		return false;
	}
};

DWORD HeaderFormat(ColumnAlignment alignment) {
	return alignment == ColumnAlignment::Right ? HDF_RIGHT : alignment == ColumnAlignment::Center ? HDF_CENTER : HDF_LEFT;
}

std::vector<ColumnDef> AllColumnDefs() {
	std::vector<ColumnDef> defs = SavedBuiltInColumnDefs();
	for (ColumnDef& def : LoadCustomColumnDefs()) defs.push_back(std::move(def));
	return defs;
}

// Every panel that exists, so a change to the shared custom columns reaches all of them.
std::vector<PlaylistView*>& LiveViews() {
	static std::vector<PlaylistView*> views;
	return views;
}

int CountInfoLines(const char* pattern) {
	int lines = 1;
	for (const char* walk = pattern; *walk; ++walk) {
		if (*walk == '|') ++lines;
	}
	return lines;
}

} // namespace

// Both callback bases start with no events. Events are on only while the window exists,
// so no callback has to check for a missing window.
PlaylistView::PlaylistView()
	: playlist_callback_single_impl_base(0), play_callback_impl_base(0),
	  m_columnDefs(AllColumnDefs()),
	  m_presets(LoadGroupPresets()),
	  m_covers([this] { if (m_hWnd) Invalidate(); }) {
	LiveViews().push_back(this);
	if (!SelectPresetByName(kDefaultGroupPresetName)) SelectPreset(0);
}

PlaylistView::~PlaylistView() {
	auto& views = LiveViews();
	views.erase(std::remove(views.begin(), views.end(), this), views.end());
}

void PlaylistView::ApplyCustomColumns(const std::vector<ColumnDef>& customDefs, const std::vector<std::pair<std::string, std::string>>& renames) {
	SaveCustomColumnDefs(customDefs);
	for (PlaylistView* view : LiveViews()) view->OnCustomColumnsChanged(renames);
}

// A renamed column keeps its place in the layout. A deleted column leaves the layout.
void PlaylistView::OnCustomColumnsChanged(const std::vector<std::pair<std::string, std::string>>& renames) {
	ColumnLayout layout = CapturedLayout();
	for (ColumnLayout::Column& column : layout.columns) {
		for (const auto& rename : renames) {
			if (column.name == rename.first) {
				column.name = rename.second;
				break;
			}
		}
	}
	m_columnDefs = AllColumnDefs();
	SetColumnLayout(layout);
}

const ColumnDef* PlaylistView::FindColumnDef(const std::string& name) const {
	for (const ColumnDef& def : m_columnDefs) {
		if (def.name == name) return &def;
	}
	return nullptr;
}

bool PlaylistView::IsColumnShown(const std::string& name) const {
	for (const ColumnLayout::Column& column : m_layout.columns) {
		if (column.name == name) return true;
	}
	return false;
}

// A column that is turned on goes after the last track column with its default width,
// so a cover dragged to the right end stays there.
void PlaylistView::ToggleColumn(const std::string& name) {
	const ColumnDef* def = FindColumnDef(name);
	if (!def) return;
	m_layout = CapturedLayout();
	auto& columns = m_layout.columns;
	const auto shown = std::find_if(columns.begin(), columns.end(), [&](const ColumnLayout::Column& column) { return column.name == name; });
	std::vector<bool> kept(columns.size(), true);
	const bool adding = shown == columns.end();
	if (adding) {
		columns.push_back({ name, def->width });
	} else {
		kept[static_cast<size_t>(shown - columns.begin())] = false;
		columns.erase(shown);
	}
	m_layout.places = PlacesAfterTrackChange(m_layout.places, kept, adding ? 1 : 0);
	if (m_hWnd) RebuildColumns();
}

ColumnLayout PlaylistView::GetColumnLayout() {
	return CapturedLayout();
}

// The user reorders columns by dragging header titles, the cover and info columns too. The header control keeps
// that order apart from the column indices, so the copy lists the track columns as they appear on screen,
// and CaptureColumnWidths reads where the cover and info columns are.
ColumnLayout PlaylistView::CapturedLayout() {
	CaptureColumnWidths();
	ColumnLayout layout = m_layout;
	if (!m_hWnd || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return layout;
	layout.columns.clear();
	for (const int listColumn : GetColumnOrderArray()) {
		if (listColumn >= static_cast<int>(kFirstTrackColumn)) layout.columns.push_back(m_layout.columns[listColumn - kFirstTrackColumn]);
	}
	return layout;
}

// With column dragging off the header does not start a drag (ApplyHeaderDragging); this refuses one that starts anyway.
bool PlaylistView::OnColumnHeaderDrag(t_size listColumn, t_size newPosition) {
	if (!m_settings.dragColumns) return true;
	return __super::OnColumnHeaderDrag(listColumn, newPosition);
}

// The header's own drag style follows the setting, so with dragging off a column title does not even lift.
void PlaylistView::ApplyHeaderDragging() {
	if (!IsHeaderEnabled()) return;
	GetHeaderCtrl().ModifyStyle(m_settings.dragColumns ? 0 : HDS_DRAGDROP, m_settings.dragColumns ? HDS_DRAGDROP : 0);
}

// Names with no column definition are dropped, so a layout saved with a column that no longer exists still loads.
void PlaylistView::SetColumnLayout(const ColumnLayout& layout) {
	m_layout = layout;
	auto& columns = m_layout.columns;
	std::vector<bool> kept;
	for (const ColumnLayout::Column& column : columns) kept.push_back(FindColumnDef(column.name) != nullptr);
	columns.erase(std::remove_if(columns.begin(), columns.end(), [this](const ColumnLayout::Column& column) { return !FindColumnDef(column.name); }), columns.end());
	m_layout.places = PlacesAfterTrackChange(m_layout.places, kept, 0);
	if (m_hWnd) RebuildColumns();
}

// While the window exists the header control owns the widths, because the user drags them there.
void PlaylistView::CaptureColumnWidths() {
	if (!m_hWnd || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return;
	m_layout.places = PlacesFromOrder(GetColumnOrderArray());
	// Without groups the cover and info columns are hidden, and their widths are kept for when groups come back.
	if (!m_ungrouped) {
		m_layout.coverWidth = GetColumnWidthF(kCoverColumn);
		if (m_blockLineCount > 0) m_layout.infoWidth = GetColumnWidthF(kInfoColumn);
	}
	// While the columns fill the panel, their saved widths are the proportions, and the screen shows them
	// rounded to the panel's width. Reading them back on every resize would let the rounding pile up.
	if (!m_autoSizeColumns) SaveTrackColumnWeights();
}

void PlaylistView::SaveTrackColumnWeights() {
	for (size_t index = 0; index < m_layout.columns.size(); ++index) {
		m_layout.columns[index].width = GetColumnWidthF(kFirstTrackColumn + index);
	}
}

void PlaylistView::SetAutoSizeColumns(bool on) {
	if (on == m_autoSizeColumns) return;
	// Either way the widths on screen are kept: as the proportions when it turns on, as they are when it turns off.
	m_autoSizeColumns = false;
	CaptureColumnWidths();
	m_autoSizeColumns = on;
	FitTrackColumns();
}

// The track columns as they appear on screen, left to right. The user can drag them into another order.
std::vector<size_t> PlaylistView::TrackColumnsOnScreen() const {
	std::vector<size_t> columns;
	for (const int listColumn : GetColumnOrderArray()) {
		if (listColumn >= static_cast<int>(kFirstTrackColumn)) columns.push_back(static_cast<size_t>(listColumn));
	}
	return columns;
}

// The space beside the cover and info columns goes to the track columns in proportion to their saved widths.
// A panel narrower than the cover and info columns leaves the track columns no width until it is widened again.
void PlaylistView::FitTrackColumns() {
	if (!m_autoSizeColumns || !m_hWnd || m_layout.columns.empty() || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return;
	const int available = GetClientRectHook().Width() - static_cast<int>(GetSubItemWidth(kCoverColumn) + GetSubItemWidth(kInfoColumn));
	std::vector<float> weights;
	for (const ColumnLayout::Column& column : m_layout.columns) weights.push_back(column.width);
	const std::vector<int> widths = FitColumnWidths(weights, available);
	bool changed = false;
	{
		pfc::vartoggle_t<bool> fitting(m_fittingColumns, true);
		for (size_t index = 0; index < widths.size(); ++index) {
			if (static_cast<int>(GetSubItemWidth(kFirstTrackColumn + index)) == widths[index]) continue;
			ResizeColumn(kFirstTrackColumn + index, static_cast<t_uint32>(widths[index]), false);
			changed = true;
		}
	}
	if (changed) OnColumnsChanged();
}

void PlaylistView::RebuildColumns() {
	ResetColumns(false);
	m_listColumns.clear();
	auto compiler = titleformat_compiler::get();

	AddColumnF("", m_ungrouped ? 0.f : m_layout.coverWidth, HDF_LEFT, false);
	AddColumnF("", !m_ungrouped && m_blockLineCount > 0 ? m_layout.infoWidth : 0.f, HDF_LEFT, false);
	m_listColumns.resize(kFirstTrackColumn);
	for (const ColumnLayout::Column& column : m_layout.columns) {
		const ColumnDef* def = FindColumnDef(column.name);
		AddColumnF(ColumnTitle(*def).c_str(), column.width, HeaderFormat(def->alignment), false);
		ListColumn listColumn;
		listColumn.kind = def->kind;
		if (def->kind == ColumnKind::Rating) {
			compiler->compile_safe(listColumn.script, RatingReadPattern(GetRatingStorage()));
		} else if (def->kind == ColumnKind::Like) {
			compiler->compile_safe(listColumn.script, LikeReadPattern(GetRatingStorage()));
		} else if (def->kind == ColumnKind::Duration) {
			// On the playing track the original shows the time played, or the time left after a click.
			compiler->compile_safe(listColumn.script, "$if(%isplaying%,%playback_time% / %length%,%length%)");
			compiler->compile_safe(listColumn.countdownScript, "$if(%isplaying%,-%playback_time_remaining% / %length%,%length%)");
		} else {
			compiler->compile_safe(listColumn.script, def->pattern.c_str());
		}
		m_listColumns.push_back(listColumn);
	}
	// The columns were added in list order; the header shows the cover and info columns where the user put them.
	std::vector<int> order = ScreenOrder(m_layout.columns.size(), m_layout.places);
	m_layout.places = PlacesFromOrder(order);
	if (IsHeaderEnabled()) {
		GetHeaderCtrl().SetOrderArray(static_cast<int>(order.size()), order.data());
		ClearHeaderTitles();
		OnColumnsChanged();
	}
	FitTrackColumns();
	ReloadData();
}

// Sorts the whole active playlist, from the largest value down when descending. Ctrl+Z undoes it.
void PlaylistView::SortPlaylist(const char* pattern, bool descending) {
	m_sorting = true;
	m_playlists->activeplaylist_undo_backup();
	if (descending) {
		// The SDK's sort keeps tracks with the same value in their playlist order in either direction.
		metadb_handle_list tracks;
		m_playlists->activeplaylist_get_all_items(tracks);
		titleformat_object::ptr script;
		titleformat_compiler::get()->compile_safe(script, pattern);
		std::vector<t_size> order(tracks.get_count());
		metadb_handle_list_helper::sort_by_format_get_order(tracks, order.data(), script, nullptr, -1);
		m_playlists->activeplaylist_reorder_items(order.data(), order.size());
	} else {
		m_playlists->activeplaylist_sort_by_format(pattern, false);
	}
	m_sorting = false;
}

GUID PlaylistView::ActivePlaylistGuid() const {
	auto playlists = playlist_manager_v5::get();
	const t_size playlist = playlists->get_active_playlist();
	return playlist == pfc_infinite ? pfc::guid_null : playlists->playlist_get_guid(playlist);
}

// A click on a track column header sorts the playlist by that column. A second click on the same
// column reverses the order. A click on the leftmost header, above the covers, puts the playlist back
// in the order of the active preset, as in the original.
void PlaylistView::OnColumnHeaderClick(t_size listColumn) {
	if (listColumn == kCoverColumn) {
		SortByActivePreset();
		return;
	}
	if (!m_settings.sortOnHeaderClick || listColumn < kFirstTrackColumn) return;
	const size_t position = listColumn - kFirstTrackColumn;
	if (position >= m_layout.columns.size()) return;
	const ColumnDef* def = FindColumnDef(m_layout.columns[position].name);
	if (!def) return;
	const bool descending = m_headerSort.Click(def->name, ActivePlaylistGuid());
	const char* pattern = def->kind == ColumnKind::Rating ? RatingReadPattern(GetRatingStorage())
		: def->kind == ColumnKind::Like ? LikeReadPattern(GetRatingStorage()) : def->pattern.c_str();
	SortPlaylist(pattern, descending);
}

namespace {

// Properties kept with each playlist, so they follow the playlist across restarts.
constexpr GUID guid_playlist_preset = { 0x41e6b601, 0x93da, 0x4c7d, { 0xa9, 0xa8, 0x22, 0x31, 0xb2, 0x2b, 0x3b, 0x7f } };
// Stored in place of a preset name for "None". A preset name never holds this character.
constexpr char kUngroupedMark[] = "\x01";
constexpr GUID guid_playlist_scroll = { 0x3a4e2d8c, 0x430b, 0x4a2d, { 0xa1, 0xdc, 0xb8, 0xc2, 0x23, 0xe8, 0xd6, 0x11 } };

} // namespace

void PlaylistView::ChoosePreset(size_t index) {
	if (index >= m_presets.size()) return;
	SelectPreset(index);
	RememberPlaylistGrouping(m_presets[index].name);
	if (m_headerSort.SortedByColumn(ActivePlaylistGuid())) SortByActivePreset();
}

void PlaylistView::ChooseUngrouped() {
	SetUngrouped();
	RememberPlaylistGrouping(kUngroupedMark);
}

// The playlist property holds the preset name, or kUngroupedMark for "None".
void PlaylistView::SetPlaylistPreset(t_size playlist, const std::string& name) {
	pfc::array_t<t_uint8> data;
	data.set_data_fromptr(reinterpret_cast<const t_uint8*>(name.data()), name.size());
	playlist_manager_v2::get()->playlist_set_property(playlist, guid_playlist_preset, data);
}

void PlaylistView::RememberPlaylistGrouping(const std::string& name) {
	if (!m_settings.presetPerPlaylist) return;
	const t_size playlist = m_playlists->get_active_playlist();
	if (playlist == pfc_infinite) return;
	SetPlaylistPreset(playlist, name);
}

// A playlist that had a preset chosen for it shows that preset. Other playlists keep the panel's preset.
void PlaylistView::ApplyPlaylistPreset() {
	if (!m_settings.presetPerPlaylist) return;
	const t_size playlist = m_playlists->get_active_playlist();
	if (playlist == pfc_infinite) return;
	pfc::array_t<t_uint8> data;
	if (!playlist_manager_v2::get()->playlist_get_property(playlist, guid_playlist_preset, data)) return;
	const std::string name(reinterpret_cast<const char*>(data.get_ptr()), data.get_size());
	if (name == kUngroupedMark) {
		SetUngrouped();
		return;
	}
	const std::optional<size_t> index = FindPresetByName(m_presets, name);
	if (index && (*index != m_activePreset || m_ungrouped)) SelectPreset(*index);
}

// The top of the view, in pixels from the top of the list, is kept with the playlist the view showed.
void PlaylistView::SaveScrollPosition() {
	if (!m_settings.scrollPerPlaylist || m_shownPlaylist == pfc::guid_null || !m_hWnd) return;
	auto playlists = playlist_manager_v5::get();
	const t_size playlist = playlists->find_playlist_by_guid(m_shownPlaylist);
	if (playlist == pfc_infinite) return;
	playlists->playlist_set_property_int(playlist, guid_playlist_scroll, static_cast<t_int32>(GetViewOrigin().y));
}

// Shows the active playlist where it was left. Without a saved place the focused track is brought into view.
void PlaylistView::RestoreScrollPosition() {
	auto playlists = playlist_manager_v5::get();
	const t_size playlist = playlists->get_active_playlist();
	m_shownPlaylist = playlist == pfc_infinite ? pfc::guid_null : playlists->playlist_get_guid(playlist);
	t_int32 top = 0;
	if (m_settings.scrollPerPlaylist && playlist != pfc_infinite && playlists->playlist_get_property_int(playlist, guid_playlist_scroll, top)) {
		MoveViewOrigin(CPoint(GetViewOrigin().x, top));
		return;
	}
	const t_size focus = GetFocusItem();
	if (focus < GetItemCount()) EnsureItemVisible(focus);
}

// The cover column of a block reaches below its track rows: the last row is made taller to fit the
// cover. The list control does not count that extra space as part of any row, so the block is found
// from the full row heights here. Otherwise a block with one track could hardly be clicked at all.
std::optional<size_t> PlaylistView::CoverBlockAt(CPoint clientPoint) const {
	size_t header = 0;
	if (GroupHeaderFromPoint2(clientPoint, header) || m_groups.ItemCount() == 0) return std::nullopt;
	const int y = PointClientToAbs(clientPoint).y;
	const size_t item = IndexFromPointAbs(y);
	if (item >= m_groups.ItemCount() || y < GetItemOffsetAbs(item)) return std::nullopt;
	const CRect cover = GetSubItemRect(item, kCoverColumn);
	if (clientPoint.x < cover.left || clientPoint.x >= cover.right) return std::nullopt;
	return m_groups.BlockIndexOf(item);
}

// Selects the tracks of a cover block and puts the focus on its first track, which it returns.
size_t PlaylistView::SelectCoverBlock(size_t block) {
	const size_t first = m_groups.BlockAt(block).firstItem;
	SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, m_groups.BlockSize(block)));
	SetFocusItem(first);
	return first;
}

// A plain click on a cover selects the tracks under that cover. Ctrl and Shift clicks go to the list control.
LRESULT PlaylistView::OnCoverButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled) {
	const std::optional<size_t> block = (keys & (MK_CONTROL | MK_SHIFT)) == 0 ? CoverBlockAt(CPoint(position)) : std::nullopt;
	if (!block) {
		handled = FALSE;
		return 0;
	}
	SetFocus();
	SelectCoverBlock(*block);
	return 0;
}

// For a cover that changed on disk, or one that failed to load.
void PlaylistView::ReloadCover(size_t block) {
	metadb_handle_ptr track;
	if (block >= m_groups.BlockCount() || !m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return;
	m_covers.Forget(track);
	Invalidate();
}

// Opens the folder of the block's first track in Explorer, with that file selected.
void PlaylistView::OpenCoverFolder(size_t block) {
	metadb_handle_ptr track;
	if (block >= m_groups.BlockCount() || !m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return;
	pfc::string8 path;
	filesystem::g_get_display_path(track->get_path(), path);
	const std::wstring arguments = L"/select,\"" + std::wstring(pfc::stringcvt::string_os_from_utf8(path).get_ptr()) + L"\"";
	ShellExecuteW(m_hWnd, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

void PlaylistView::SortByActivePreset() {
	m_headerSort.Forget();
	SortPlaylist(m_presets[m_activePreset].sortPattern.c_str(), false);
}

void PlaylistView::SelectPreset(size_t index) {
	if (index >= m_presets.size()) return;
	CaptureColumnWidths();
	m_activePreset = index;
	m_ungrouped = false;
	const GroupPreset& preset = m_presets[index];
	auto compiler = titleformat_compiler::get();
	compiler->compile_safe(m_albumScript, preset.headerPattern.c_str());
	compiler->compile_safe(m_blockScript, preset.coverPattern.c_str());
	m_subgroupScripts.clear();
	for (const std::string& pattern : preset.subgroupPatterns) {
		titleformat_object::ptr script;
		compiler->compile_safe(script, pattern.c_str());
		m_subgroupScripts.push_back(script);
	}
	m_blockLineCount = preset.showCoverText ? CountInfoLines(preset.coverPattern.c_str()) : 0;
	if (m_hWnd) {
		ShowGroupingColumns();
		OnPlaylistContentChanged();
	}
}

void PlaylistView::SetUngrouped() {
	if (m_ungrouped) return;
	CaptureColumnWidths();
	m_ungrouped = true;
	if (m_hWnd) {
		ShowGroupingColumns();
		OnPlaylistContentChanged();
	}
}

// The cover column shows when there are groups. The info column also needs a preset with cover text.
void PlaylistView::ShowGroupingColumns() {
	const bool coverShown = GetSubItemWidth(kCoverColumn) > 0;
	if (coverShown == m_ungrouped) {
		if (coverShown) m_layout.coverWidth = GetColumnWidthF(kCoverColumn);
		ResizeColumn(kCoverColumn, m_ungrouped ? 0 : MulDiv(static_cast<int>(m_layout.coverWidth + 0.5f), m_dpi.cx, 96));
	}
	ShowInfoColumn(!m_ungrouped && m_blockLineCount > 0);
	FitTrackColumns();
}

void PlaylistView::ShowInfoColumn(bool show) {
	const bool shown = GetSubItemWidth(kInfoColumn) > 0;
	if (show == shown) return;
	if (shown) m_layout.infoWidth = GetColumnWidthF(kInfoColumn);
	ResizeColumn(kInfoColumn, show ? MulDiv(static_cast<int>(m_layout.infoWidth + 0.5f), m_dpi.cx, 96) : 0);
}

bool PlaylistView::SelectPresetByName(const char* name) {
	const std::optional<size_t> index = FindPresetByName(m_presets, name);
	if (index) SelectPreset(*index);
	return index.has_value();
}

void PlaylistView::ApplyGroupPresets(const std::vector<GroupPreset>& presets, const std::vector<std::pair<std::string, std::string>>& renames) {
	SaveGroupPresets(presets);
	for (PlaylistView* view : LiveViews()) view->OnGroupPresetsChanged(renames);
}

// A panel keeps its preset through a rename. A panel whose preset was deleted shows the first preset.
void PlaylistView::OnGroupPresetsChanged(const std::vector<std::pair<std::string, std::string>>& renames) {
	std::string activeName = m_presets[m_activePreset].name;
	for (const auto& rename : renames) {
		if (SamePresetName(activeName, rename.first)) {
			activeName = rename.second;
			break;
		}
	}
	m_presets = LoadGroupPresets();
	if (!SelectPresetByName(activeName.c_str())) SelectPreset(0);
}

void PlaylistView::AttachToPlaylist() {
	SetPlaylistStyle();
	// HDS_BUTTONS makes the header report clicks, which sort the playlist (OnColumnHeaderClick).
	InitializeHeaderCtrl(HDS_FULLDRAG | (m_settings.dragColumns ? HDS_DRAGDROP : 0) | HDS_BUTTONS);
	RebuildColumns();
	set_callback_flags(kPlaylistCallbacks);
	play_callback_reregister(kPlaybackCallbacks);
	ApplyPlaylistPreset();
	OnPlaylistContentChanged();
	RestoreScrollPosition();
}

// Keys ignore playback state, so starting or stopping a track never moves a boundary.
void PlaylistView::RebuildGroups() {
	const t_size count = m_playlists->activeplaylist_get_item_count();
	std::vector<std::string> albumKeys(count), blockKeys(count);
	OmitBitrateHook omitBitrate;
	pfc::string8_fastalloc formatted;
	// Without groups every key is empty: one group, one block, no labels. Nothing is formatted.
	for (t_size item = 0; item < count && !m_ungrouped; ++item) {
		m_playlists->activeplaylist_item_format_title(item, nullptr, formatted, m_albumScript, nullptr, playback_control::display_level_none);
		albumKeys[item] = formatted.c_str();
		m_playlists->activeplaylist_item_format_title(item, &omitBitrate, formatted, m_blockScript, nullptr, playback_control::display_level_none);
		blockKeys[item] = formatted.c_str();
	}
	std::vector<GroupModel::Keys> subgroupTexts(m_ungrouped ? 0 : m_subgroupScripts.size(), GroupModel::Keys(count));
	for (size_t level = 0; level < subgroupTexts.size(); ++level) {
		for (t_size item = 0; item < count; ++item) {
			m_playlists->activeplaylist_item_format_title(item, nullptr, formatted, m_subgroupScripts[level], nullptr, playback_control::display_level_none);
			subgroupTexts[level][item] = formatted.c_str();
		}
	}
	m_groups.Rebuild(albumKeys, blockKeys, subgroupTexts, m_presets[m_activePreset].hideUniformLabels);

	metadb_handle_list tracks;
	m_playlists->activeplaylist_get_all_items(tracks);
	m_albumSeconds.assign(m_groups.AlbumCount(), 0.0);
	m_secondsBefore.assign(m_groups.ItemCount() + 1, 0.0);
	for (t_size item = 0; item < m_groups.ItemCount(); ++item) {
		const double length = item < tracks.get_count() ? tracks[item]->get_length() : 0;
		m_secondsBefore[item + 1] = m_secondsBefore[item] + (length > 0 ? length : 0);
		if (length > 0) m_albumSeconds[m_groups.AlbumIndexOf(item)] += length;
	}
}

namespace {

// "12 tracks · 48:31". The length is left out when no track knows its length.
std::string TrackSummary(size_t tracks, double seconds) {
	std::string text = TrackCount(tracks);
	if (seconds > 0) {
		text += " \xC2\xB7 ";
		text += pfc::format_time(static_cast<t_int64>(seconds + 0.5)).c_str();
	}
	return text;
}

} // namespace

// Tracks and length of the section a label line opens, such as one disc of an album.
// Shown even when the section holds every track under the header: in the artist presets the header is
// the artist and the label is the album, and every album shows its own count.
std::string PlaylistView::SectionSummary(size_t item, size_t level) const {
	const auto [first, end] = SectionRangeOf(item, level);
	if (end >= m_secondsBefore.size()) return {};
	return TrackSummary(end - first, m_secondsBefore[end] - m_secondsBefore[first]);
}

// Tracks and total length of the album that starts at baseItem, as "12 tracks · 48:31".
std::string PlaylistView::AlbumSummary(size_t baseItem) const {
	if (baseItem >= m_groups.ItemCount()) return {};
	const size_t album = m_groups.AlbumIndexOf(baseItem);
	const size_t first = m_groups.AlbumAt(album).firstItem;
	const size_t end = album + 1 < m_groups.AlbumCount() ? m_groups.AlbumAt(album + 1).firstItem : m_groups.ItemCount();
	return TrackSummary(end - first, album < m_albumSeconds.size() ? m_albumSeconds[album] : 0);
}

bool PlaylistView::IsPlayingItem(size_t item) const {
	t_size playlist = 0;
	t_size index = 0;
	return m_playlists->get_playing_item_location(&playlist, &index) && playlist == m_playlists->get_active_playlist() && index == item;
}

// A double click on a cover plays the tracks under that cover from the first one. In the album presets
// that is the album. In the artist presets the header is the artist and each cover is one album.
LRESULT PlaylistView::OnCoverDoubleClick(UINT, WPARAM, LPARAM position, BOOL& handled) {
	const std::optional<size_t> block = CoverBlockAt(CPoint(position));
	if (!block) {
		handled = FALSE;
		return 0;
	}
	// The cover's tracks are selected first, as a double click on an album header selects the album.
	m_playlists->activeplaylist_execute_default_action(SelectCoverBlock(*block));
	return 0;
}

// First item and end of the album that holds item.
std::pair<size_t, size_t> PlaylistView::AlbumRangeOf(size_t item) const {
	const size_t album = m_groups.AlbumIndexOf(item);
	const size_t first = m_groups.AlbumAt(album).firstItem;
	const size_t end = album + 1 < m_groups.AlbumCount() ? m_groups.AlbumAt(album + 1).firstItem : m_groups.ItemCount();
	return { first, end };
}

// The section at a label level that holds item. A section starts at a label row that shows that level
// or an outer one, runs to the next such row, and never leaves its cover block. A block without such
// rows is one section.
std::pair<size_t, size_t> PlaylistView::SectionRangeOf(size_t item, size_t level) const {
	const size_t block = m_groups.BlockIndexOf(item);
	const size_t blockFirst = m_groups.BlockAt(block).firstItem;
	const size_t blockEnd = blockFirst + m_groups.BlockSize(block);
	const auto opensSection = [&](size_t at) {
		const GroupModel::Label* label = m_groups.LabelAt(at);
		return label && label->firstLevel <= level;
	};
	size_t first = item;
	while (first > blockFirst && !opensSection(first)) --first;
	size_t end = item + 1;
	while (end < blockEnd && !opensSection(end)) ++end;
	return { first, end };
}

// The label level of the label line under the point, with the item that carries the label.
// Empty label texts take no line, as in RenderSubgroupLabels.
std::optional<size_t> PlaylistView::LabelLevelAtPoint(CPoint clientPoint, size_t& item) const {
	if (!ItemFromPoint(clientPoint, item) || item >= m_groups.ItemCount()) return std::nullopt;
	const GroupModel::Label* label = m_groups.LabelAt(item);
	if (!label) return std::nullopt;
	const int rowHeight = GetItemHeight();
	const int offset = clientPoint.y - GetItemRect(item).top;
	if (offset < 0 || offset >= LabelLinesAbove(item) * rowHeight) return std::nullopt;
	int line = offset / rowHeight;
	pfc::string8_fastalloc text;
	for (size_t level = label->firstLevel; level < m_subgroupScripts.size(); ++level) {
		if (!m_groups.IsLevelShown(item, level)) continue;
		m_playlists->activeplaylist_item_format_title(item, nullptr, text, m_subgroupScripts[level], nullptr, playback_control::display_level_none);
		if (text.is_empty()) continue;
		if (line-- == 0) return level;
	}
	return std::nullopt;
}

// Where an album or a section goes: before the album or section under the point when the point is in
// its upper half, after it otherwise. A point on an album header counts as the album's top.
size_t PlaylistView::DropIndexAt(CPoint clientPoint, DragUnit unit, size_t level) const {
	size_t item = 0;
	if (GroupHeaderFromPoint2(clientPoint, item)) return item;
	if (!ItemFromPoint(clientPoint, item) || item >= m_groups.ItemCount()) return GetItemCount();
	const auto [first, end] = unit == DragUnit::Section ? SectionRangeOf(item, level) : AlbumRangeOf(item);
	const int top = GetItemOffsetAbs(first);
	const int bottom = GetItemOffsetAbs(end - 1) + GetItemHeight2(end - 1);
	return PointClientToAbs(clientPoint).y < (top + bottom) / 2 ? first : end;
}

// The list shows a tooltip over text that does not fit. The Duration cell of the playing track changes
// every second, and a tooltip would freeze its old text over the cell, so that cell gets none.
bool PlaylistView::GetTooltipData(CPoint pointAbs, pfc::string_base& text, CRect& rect, CFontHandle& font) const {
	size_t item = 0;
	size_t subItem = 0;
	if (GetItemAtPointAbsEx(pointAbs, item, subItem) && subItem < m_listColumns.size() &&
		m_listColumns[subItem].kind == ColumnKind::Duration && IsPlayingItem(item)) {
		return false;
	}
	if (!__super::GetTooltipData(pointAbs, text, rect, font)) return false;
	m_tooltipText = pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
	return true;
}

// The header's own title text is cleared, and each title is drawn after Windows has drawn the item's background,
// hover and sort arrow, so titles go through the same painter as the rest of the panel and show colour emoji.
// The list control keeps its own copy of every title (GetColumnText) and never reads the header's text back.
void PlaylistView::ClearHeaderTitles() {
	CHeaderCtrl header = GetHeaderCtrl();
	const int count = header.GetItemCount();
	for (int index = 0; index < count; ++index) {
		HDITEM item = {};
		item.mask = HDI_TEXT;
		item.pszText = const_cast<LPWSTR>(L"");
		header.SetItem(index, &item);
	}
}

LRESULT PlaylistView::OnChildCustomDraw(int, LPNMHDR header, BOOL& handled) {
	if (IsHeaderEnabled() && header->hwndFrom == GetHeaderCtrl().m_hWnd) return DrawHeaderItem(*reinterpret_cast<NMCUSTOMDRAW*>(header));
	wchar_t className[32] = {};
	if (GetClassNameW(header->hwndFrom, className, 32) && wcscmp(className, TOOLTIPS_CLASSW) == 0) {
		return DrawTooltip(*reinterpret_cast<NMTTCUSTOMDRAW*>(header));
	}
	handled = FALSE;
	return 0;
}

LRESULT PlaylistView::DrawHeaderItem(NMCUSTOMDRAW& draw) {
	switch (draw.dwDrawStage) {
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
	case CDDS_ITEMPREPAINT:
		return CDRF_NOTIFYPOSTPAINT;
	case CDDS_ITEMPOSTPAINT: {
		const size_t column = static_cast<size_t>(draw.dwItemSpec);
		if (column >= GetColumnCount()) return CDRF_DODEFAULT;
		pfc::string8 title;
		GetColumnText(column, title);
		if (title.is_empty()) return CDRF_DODEFAULT;

		CHeaderCtrl header = GetHeaderCtrl();
		// The colour the header itself would use: the dark scheme's (as the list control sets it), else the theme's.
		COLORREF color = GetSysColor(COLOR_BTNTEXT);
		if (GetDarkMode()) {
			color = 0xdedede;
		} else if (HTHEME theme = OpenThemeData(header, L"Header")) {
			GetThemeColor(theme, HP_HEADERITEM, HIS_NORMAL, TMT_TEXTCOLOR, &color);
			CloseThemeData(theme);
		}
		CDCHandle dc(draw.hdc);
		DCStateScope state(dc);
		dc.SelectFont(header.GetFont());
		dc.SetTextColor(color);
		dc.SetBkMode(TRANSPARENT);
		// Windows puts a header title three edge widths in from each side.
		const int margin = 3 * GetSystemMetrics(SM_CXEDGE);
		CRect textRect(draw.rc);
		textRect.DeflateRect(margin, 0);
		const pfc::stringcvt::string_os_from_utf8 wide(title);
		m_text.Draw(dc, View(wide), textRect, AlignOfColumnFormat(GetColumnFormat(column)), true);
		return CDRF_DODEFAULT;
	}
	default:
		return CDRF_DODEFAULT;
	}
}

// The tooltip draws itself as usual. Then its text area is covered with its own background again and the text is
// drawn through the painter, in the tooltip's own colours, so it matches the cell it repeats.
LRESULT PlaylistView::DrawTooltip(NMTTCUSTOMDRAW& draw) {
	switch (draw.nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		return CDRF_NOTIFYPOSTPAINT;
	case CDDS_POSTPAINT: {
		if (m_tooltipText.empty()) return CDRF_DODEFAULT;
		CWindow tooltip(draw.nmcd.hdr.hwndFrom);
		CRect textRect;
		tooltip.GetWindowRect(textRect);
		tooltip.SendMessage(TTM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&textRect));
		tooltip.ScreenToClient(textRect);
		CRect client;
		tooltip.GetClientRect(client);

		CDCHandle dc(draw.nmcd.hdc);
		DCStateScope state(dc);
		COLORREF color = static_cast<COLORREF>(tooltip.SendMessage(TTM_GETTIPTEXTCOLOR));
		if (HTHEME theme = OpenThemeData(tooltip, L"Tooltip")) {
			DrawThemeBackground(theme, dc, TTP_STANDARD, TTSS_NORMAL, client, textRect);
			GetThemeColor(theme, TTP_STANDARD, TTSS_NORMAL, TMT_TEXTCOLOR, &color);
			CloseThemeData(theme);
		} else {
			dc.FillSolidRect(textRect, static_cast<COLORREF>(tooltip.SendMessage(TTM_GETTIPBKCOLOR)));
		}
		dc.SelectFont(reinterpret_cast<HFONT>(tooltip.SendMessage(WM_GETFONT)));
		dc.SetTextColor(color);
		dc.SetBkMode(TRANSPARENT);
		m_text.Draw(dc, m_tooltipText, textRect, TextPainter::Align::Left, false);
		return CDRF_DODEFAULT;
	}
	default:
		return CDRF_DODEFAULT;
	}
}

// The time shown on the playing track changes every second.
void PlaylistView::RefreshPlayingRow() {
	t_size playlist = 0;
	t_size item = 0;
	if (!m_hWnd || !m_playlists->get_playing_item_location(&playlist, &item)) return;
	if (playlist == m_playlists->get_active_playlist() && item < GetItemCount()) UpdateItem(item);
}

// A click on the Duration cell of the playing track switches between time played and time left.
// The click still goes on to the list control, so it also selects the row as usual.
LRESULT PlaylistView::OnDurationButtonDown(UINT, WPARAM, LPARAM position, BOOL& handled) {
	handled = FALSE;
	size_t item = 0;
	size_t subItem = 0;
	if (!GetItemAtPointAbsEx(PointClientToAbs(CPoint(position)), item, subItem)) return 0;
	if (subItem >= m_listColumns.size() || m_listColumns[subItem].kind != ColumnKind::Duration || !IsPlayingItem(item)) return 0;
	SetDurationCountsDown(!GetDurationCountsDown());
	UpdateItem(item);
	return 0;
}

// A double click on an album header plays the album from its first track.
void PlaylistView::ExecuteDefaultActionGroup(t_size base, t_size count) {
	if (count > 0) m_playlists->activeplaylist_execute_default_action(base);
}

// Rows moved, so the star preview may point at another track. A tag change moves no rows and keeps
// the preview, so a click that is still pausing it stays paused when the new rating arrives.
void PlaylistView::OnRowsMoved() {
	m_starHover.MoveTo(std::nullopt);
	OnPlaylistContentChanged();
}

void PlaylistView::OnPlaylistContentChanged() {
	m_groupFocus = SIZE_MAX;
	RebuildGroups();
	ReloadData();
}

// The list control knows one group level. An album is that group. Blocks inside an album
// are laid out here: the last row of a block is made taller to hold the cover and the gap.
PlaylistView::groupID_t PlaylistView::GetItemGroup(t_size item) const {
	// Group 0 everywhere draws no headers at all.
	if (m_ungrouped || item >= m_groups.ItemCount()) return 0;
	return m_groups.AlbumIndexOf(item) + 1;
}

bool PlaylistView::GetGroupHeaderText2(size_t baseItem, pfc::string_base& out) const {
	if (baseItem >= m_groups.ItemCount()) return false;
	out = m_groups.AlbumAt(m_groups.AlbumIndexOf(baseItem)).key.c_str();
	return true;
}

// The list control spaces rows by the font height plus a fixed margin. The setting adds to that margin.
int PlaylistView::GetItemHeight() const {
	return __super::GetItemHeight() + Scaled(m_settings.rowPadding);
}

int PlaylistView::GetGroupHeaderHeight() const {
	const int textHeight = m_settings.customHeaderFont && m_headerFont
		? static_cast<int>(GetFontHeight(m_headerFont)) + Scaled(4)
		: __super::GetGroupHeaderHeight();
	return textHeight + Scaled(kBlockGap);
}

HFONT PlaylistView::HeaderFont() const {
	return m_settings.customHeaderFont && m_headerFont ? static_cast<HFONT>(m_headerFont) : static_cast<HFONT>(GetGroupHeaderFont());
}

// Drawn here in place of the list control's own header text, because that one always uses the list control's header font.
void PlaylistView::RenderGroupHeaderText2(size_t baseItem, const CRect& headerRect, const CRect&, CDCHandle dc) {
	pfc::string_formatter label;
	if (!GetGroupHeaderText2(baseItem, label)) return;
	CRect textRect = headerRect;
	textRect.top += Scaled(kBlockGap);
	textRect = GetItemTextRect(textRect);
	SelectObjectScope fontScope(dc, HeaderFont());
	dc.SetTextColor(HeaderTextColor());

	// The summary sits at the right end in the track font. The album name and its rule stop before it.
	const pfc::stringcvt::string_os_from_utf8 summary(ShowsHeaderSummary(m_settings.showTrackSummary, m_settings.trackSummaryPlace) ? AlbumSummary(baseItem).c_str() : "");
	if (summary.length() > 0) {
		SelectObjectScope summaryFont(dc, static_cast<HFONT>(GetFont()));
		const int summaryWidth = m_text.Width(dc, View(summary));
		// The header spans every column, which can run past the window. The summary stays in sight.
		CRect client;
		GetClientRect(client);
		CRect summaryRect = textRect;
		summaryRect.right = (std::min)(textRect.right, client.right - Scaled(8));
		summaryRect.left = (std::max)(textRect.left, summaryRect.right - summaryWidth);
		m_text.Draw(dc, View(summary), summaryRect, TextPainter::Align::Right, false);
		textRect.right = summaryRect.left - Scaled(12);
	}
	pfc::stringcvt::string_os_from_utf8 text(label);
	m_text.Draw(dc, View(text), textRect, TextPainter::Align::Left, true);
	const int textWidth = m_text.Width(dc, View(text));
	const int ruleGap = textRect.Height() / 2;
	if (textWidth + ruleGap < textRect.Width()) {
		const int y = textRect.CenterPoint().y;
		const COLORREF ruleColor = PaintUtils::BlendColor(HeaderTextColor(), GetSysColorHook(colorBackground), 25);
		PaintUtils::DrawSmoothedLine(dc, CPoint(textRect.left + textWidth + ruleGap, y), CPoint(textRect.right, y), ruleColor, 1.0 * m_dpi.cy / 96.0);
	}
}

void PlaylistView::ListHandleResize() noexcept {
	__super::ListHandleResize();
	FitTrackColumns();
	CRect client;
	GetClientRect(client);
	if (client.Width() != m_lastClientWidth) {
		m_lastClientWidth = client.Width();
		Invalidate();
	}
}

int PlaylistView::Scaled(int pixelsAt96Dpi) const {
	return MulDiv(pixelsAt96Dpi, m_dpi.cy, 96);
}

int PlaylistView::LabelLinesAbove(size_t item) const {
	const GroupModel::Label* label = m_groups.LabelAt(item);
	return label ? static_cast<int>(label->lineCount) : 0;
}

// Height of the rows a block holds: one per track, one per subgroup label.
int PlaylistView::BlockRowsHeight(size_t block) const {
	return static_cast<int>(m_groups.BlockSize(block) + m_groups.LabelLinesInBlock(block)) * GetItemHeight();
}

int PlaylistView::BlockHeight(size_t block) const {
	const int minimum = (std::max)(Scaled(m_settings.coverSize + 2 * kCoverInset), m_blockLineCount * GetItemHeight());
	const int gap = m_groups.IsLastBlockOfAlbum(block) ? 0 : Scaled(kBlockGap);
	return (std::max)(BlockRowsHeight(block), minimum) + gap;
}

// A row grows upward to hold subgroup labels, and the last row of a block grows downward
// to reach the height of the cover and to leave the gap before the next block.
int PlaylistView::GetItemHeight2(size_t item) const {
	if (m_ungrouped || item >= m_groups.ItemCount()) return -1;
	const int rowHeight = GetItemHeight();
	int height = (LabelLinesAbove(item) + 1) * rowHeight;
	const size_t block = m_groups.BlockIndexOf(item);
	const bool lastInBlock = item + 1 == m_groups.BlockAt(block).firstItem + m_groups.BlockSize(block);
	if (lastInBlock) height += BlockHeight(block) - BlockRowsHeight(block);
	return height > rowHeight ? height : -1;
}

// The labels belong to the row's clickable area. The space below a block's last row does not.
int PlaylistView::GetItemHeight2Content(size_t item, int) const {
	return (LabelLinesAbove(item) + 1) * GetItemHeight();
}

void PlaylistView::RenderItem(t_size item, const CRect& itemRect, const CRect& updateRect, CDCHandle dc) {
	const int labelHeight = item < m_groups.ItemCount() ? LabelLinesAbove(item) * GetItemHeight() : 0;
	if (labelHeight == 0) {
		RenderTrackRow(item, itemRect, updateRect, dc);
		return;
	}
	CRect labelRect = itemRect;
	labelRect.bottom = labelRect.top + labelHeight;
	RenderSubgroupLabels(item, labelRect, dc);

	CRect rowRect = itemRect;
	rowRect.top = labelRect.bottom;
	RenderTrackRow(item, rowRect, updateRect, dc);
}

// As the list control draws a row, except for the selection. The list control draws a selected row in the style of
// Windows' own lists, which frames every row and so puts a dark line between selected rows. Here a selected row is
// filled flat with a pale tint, so a run of selected tracks reads as one block, as in the original.
void PlaylistView::RenderTrackRow(t_size item, const CRect& rowRect, const CRect& updateRect, CDCHandle dc) {
	const bool weHaveFocus = ::GetFocus() == m_hWnd;
	const COLORREF fill = IsItemSelected(item) ? SelectionFill(weHaveFocus) : GetSysColorHook(colorBackground);
	CRect selectionRect = rowRect;
	AdjustSelectionRect(item, selectionRect);
	RenderItemBackground(dc, selectionRect, item, fill);
	{
		DCStateScope state(dc);
		dc.SetBkMode(TRANSPARENT);
		dc.SetBkColor(fill);
		dc.SetTextColor(GetSysColorHook(colorText));
		RenderItemText(item, rowRect, updateRect, dc, true);
	}
	if (weHaveFocus && IsItemFocused(item)) PaintUtils::FocusRect2(dc, selectionRect, fill);
}

// A tint of the selection colour (the Default UI colour, or Windows' accent) over the background. The tint is
// stronger on a dark background, where a pale one would hardly show, and weaker while the panel has no focus.
COLORREF PlaylistView::HeaderTextColor() const {
	return m_settings.customHeaderColor ? m_settings.headerColor : GetSysColorHook(colorHighlight);
}

COLORREF PlaylistView::LabelTextColor() const {
	return m_settings.customLabelColor ? m_settings.labelColor : GetSysColorHook(colorHighlight);
}

COLORREF PlaylistView::StarFillColor() const {
	return m_settings.customStarColor ? m_settings.starColor : GetSysColorHook(colorHighlight);
}

COLORREF PlaylistView::SelectionFill(bool focused) const {
	const int percent = GetDarkMode() ? (focused ? 45 : 30) : (focused ? 22 : 13);
	return PaintUtils::BlendColor(GetSysColorHook(colorBackground), GetSysColorHook(colorSelection), percent);
}

// Labels sit over the track columns. The outermost subgroup level gets a rule to its right, as in the original.
void PlaylistView::RenderSubgroupLabels(size_t item, const CRect& labelRect, CDCHandle dc) {
	const GroupModel::Label* label = m_groups.LabelAt(item);
	if (!label) return;
	DCStateScope state(dc);
	dc.FillSolidRect(labelRect, GetSysColorHook(colorBackground));
	SelectObjectScope fontScope(dc, static_cast<HGDIOBJ>(GetFont()));
	dc.SetTextColor(LabelTextColor());
	dc.SetBkMode(TRANSPARENT);

	const int rowHeight = GetItemHeight();
	const int padding = static_cast<int>(GetColumnSpacing());
	CRect client;
	GetClientRect(client);
	// The labels go over the widest run of track columns side by side, clear of the cover and info columns wherever
	// the user put them. With those at one end, that is all the track columns.
	const std::vector<int> order = GetColumnOrderArray();
	std::vector<int> widths;
	for (const int column : order) widths.push_back(static_cast<int>(GetSubItemWidth(static_cast<size_t>(column))));
	const std::optional<std::pair<size_t, size_t>> run = WidestTrackRun(order, widths);
	if (!run) return;
	const int left = GetSubItemRect(item, static_cast<size_t>(order[run->first])).left;
	const int right = GetSubItemRect(item, static_cast<size_t>(order[run->second])).right;
	CRect lineRect(left + padding, labelRect.top, right - padding, labelRect.top + rowHeight);
	pfc::string8_fastalloc text;
	for (size_t level = label->firstLevel; level < m_subgroupScripts.size(); ++level) {
		if (!m_groups.IsLevelShown(item, level)) continue;
		m_playlists->activeplaylist_item_format_title(item, nullptr, text, m_subgroupScripts[level], nullptr, playback_control::display_level_none);
		if (text.is_empty()) continue;

		// The section's tracks and length sit at the right end, as the album's do on the header.
		// The label and its rule stop before them. The right end stays in sight when the columns run past the window.
		CRect textRect = lineRect;
		const pfc::stringcvt::string_os_from_utf8 summary(ShowsLabelSummary(m_settings.showTrackSummary, m_settings.trackSummaryPlace) ? SectionSummary(item, level).c_str() : "");
		if (summary.length() > 0) {
			const int summaryWidth = m_text.Width(dc, View(summary));
			CRect summaryRect = lineRect;
			summaryRect.right = (std::min)(lineRect.right, client.right - Scaled(8));
			summaryRect.left = (std::max)(lineRect.left, summaryRect.right - summaryWidth);
			m_text.Draw(dc, View(summary), summaryRect, TextPainter::Align::Right, false);
			textRect.right = summaryRect.left - Scaled(12);
		}

		pfc::stringcvt::string_os_from_utf8 wide(text);
		m_text.Draw(dc, View(wide), textRect, TextPainter::Align::Left, true);
		const int ruleGap = rowHeight / 2;
		const int textWidth = level == 0 ? m_text.Width(dc, View(wide)) : 0;
		if (level == 0 && textWidth + ruleGap < textRect.Width()) {
			const int y = textRect.CenterPoint().y;
			const COLORREF ruleColor = PaintUtils::BlendColor(LabelTextColor(), GetSysColorHook(colorBackground), 25);
			PaintUtils::DrawSmoothedLine(dc, CPoint(textRect.left + textWidth + ruleGap, y), CPoint(textRect.right, y), ruleColor, 1.0 * m_dpi.cy / 96.0);
		}
		lineRect.OffsetRect(0, rowHeight);
	}
}

void PlaylistView::RenderGroupOverlayColumn(size_t baseItem, size_t subItem, const CRect& albumColumnRect, const CRect& updateRect, CDCHandle dc) {
	if (subItem >= kFirstTrackColumn || baseItem >= m_groups.ItemCount()) return;

	const size_t album = m_groups.AlbumIndexOf(baseItem);
	int top = albumColumnRect.top;
	for (size_t block = m_groups.AlbumAt(album).firstBlock; block < m_groups.AlbumBlockEnd(album); ++block) {
		const CRect blockRect(albumColumnRect.left, top, albumColumnRect.right, top + BlockHeight(block));
		top = blockRect.bottom;
		if (blockRect.bottom <= updateRect.top) continue;
		if (blockRect.top >= updateRect.bottom) break;

		dc.FillSolidRect(blockRect, GetSysColorHook(colorBackground));
		const CRect contentRect = CoverBlockContentRect(block, blockRect);
		if (subItem == kCoverColumn) {
			RenderCover(block, contentRect, dc);
		} else {
			RenderInfoLines(block, contentRect, dc);
		}
	}
}

// Where the cover and its text go inside the block. With pinned headers they stay in view below the
// pinned header while the block's tracks scroll, until the end of the block pushes them up.
CRect PlaylistView::CoverBlockContentRect(size_t block, const CRect& blockRect) const {
	if (!m_settings.pinHeaders) return blockRect;
	const int gap = m_groups.IsLastBlockOfAlbum(block) ? 0 : Scaled(kBlockGap);
	const int minimum = (std::max)(Scaled(m_settings.coverSize + 2 * kCoverInset), m_blockLineCount * GetItemHeight());
	const int height = (std::min)(minimum, blockRect.Height() - gap);
	const int lowest = static_cast<int>(blockRect.bottom) - gap - height;
	const int wanted = GetClientRectHook().top + GetGroupHeaderHeight();
	const int top = (std::max)(static_cast<int>(blockRect.top), (std::min)(wanted, lowest));
	return CRect(blockRect.left, top, blockRect.right, top + height);
}

// The group at the top of the view, when its own header has scrolled out above.
std::optional<PlaylistView::PinnedHeader> PlaylistView::FindPinnedHeader() const {
	const size_t count = m_groups.ItemCount();
	if (!m_settings.pinHeaders || count == 0 || GetItemCount() != count) return std::nullopt;
	const CRect visible = GetVisibleRectAbs();
	const size_t album = m_groups.AlbumIndexOf((std::min)(IndexFromPointAbs(visible.top), count - 1));
	const size_t first = m_groups.AlbumAt(album).firstItem;
	CRect header;
	if (!GetGroupHeaderRectAbs2(first, header) || header.top >= visible.top) return std::nullopt;
	int bottom = visible.top + header.Height();
	CRect nextHeader;
	if (album + 1 < m_groups.AlbumCount() && GetGroupHeaderRectAbs2(m_groups.AlbumAt(album + 1).firstItem, nextHeader)) {
		bottom = (std::min)(bottom, static_cast<int>(nextHeader.top));
	}
	if (bottom <= visible.top) return std::nullopt;
	const CRect pinned(header.left, bottom - header.Height(), header.right, bottom);
	return PinnedHeader{ first, RectAbsToClient(pinned) };
}

bool PlaylistView::IsOnPinnedHeader(CPoint clientPoint) const {
	const std::optional<PinnedHeader> pinned = FindPinnedHeader();
	return pinned && clientPoint.y >= GetClientRectHook().top && clientPoint.y < pinned->rect.bottom;
}

// Drawn over the rows after them, as the list control draws its drop marks.
void PlaylistView::RenderOverlay2(const CRect& updateRect, CDCHandle dc) {
	if (const std::optional<PinnedHeader> pinned = FindPinnedHeader()) {
		const CRect client = GetClientRectHook();
		CRect band(client.left, (std::max)(pinned->rect.top, client.top), client.right, pinned->rect.bottom);
		CRect area;
		if (area.IntersectRect(band, updateRect)) {
			DCStateScope state(dc);
			dc.IntersectClipRect(band);
			dc.FillSolidRect(band, GetSysColorHook(colorBackground));
			RenderGroupHeader2(pinned->firstItem, pinned->rect, area, dc);
			// A faint line under the band shows that rows pass beneath it.
			const COLORREF line = PaintUtils::BlendColor(GetSysColorHook(colorText), GetSysColorHook(colorBackground), 15);
			dc.FillSolidRect(CRect(band.left, band.bottom - 1, band.right, band.bottom), line);
		}
	}
	__super::RenderOverlay2(updateRect, dc);
}

// The window scrolls by moving its pixels, which moves the pinned header and the covers with the rows.
// They are drawn again in their place.
void PlaylistView::OnViewOriginChange(CPoint delta) {
	__super::OnViewOriginChange(delta);
	// Any other move (scroll bar, keys, a jump to the playing track, another playlist) ends a wheel animation where it is.
	if (!m_animatingWheel && m_smoothScroll && m_smoothScroll->IsAnimating()) m_smoothScroll->AbandonAnimation();
	if (!m_settings.pinHeaders || delta.y == 0 || !m_hWnd) return;
	const CRect client = GetClientRectHook();
	CRect band = client;
	band.bottom = (std::min)(client.bottom, client.top + GetGroupHeaderHeight() + std::abs(delta.y));
	InvalidateRect(band, FALSE);
	if (GetItemCount() == 0 || GetColumnCount() < kFirstTrackColumn) return;
	for (size_t column = 0; column < kFirstTrackColumn; ++column) {
		const CRect cell = GetSubItemRect(0, column);
		InvalidateRect(CRect(cell.left, client.top, cell.right, client.bottom), FALSE);
	}
}

// With pinned headers the top of the view is covered by a header, so a row brought into view
// (by the arrow keys, a search, or the playing track) asks for that much room above it.
void PlaylistView::EnsureVisibleRectAbs(const CRect& rect) {
	if (!m_settings.pinHeaders || m_ungrouped) {
		__super::EnsureVisibleRectAbs(rect);
		return;
	}
	CRect withRoom = rect;
	withRoom.top -= GetGroupHeaderHeight();
	__super::EnsureVisibleRectAbs(withRoom);
}

// A click on the pinned header acts on its group, as on the group's own header,
// and never on the row hidden under it.
LRESULT PlaylistView::OnPinnedHeaderButton(UINT message, WPARAM, LPARAM position, BOOL& handled) {
	const std::optional<PinnedHeader> pinned = FindPinnedHeader();
	const CPoint point(position);
	if (!pinned || !IsOnPinnedHeader(point)) {
		handled = FALSE;
		return 0;
	}
	const size_t first = pinned->firstItem;
	const size_t count = ResolveGroupRange2(first);
	SetFocus();
	if (count > 0) SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, count));
	if (message == WM_LBUTTONDBLCLK && count > 0) ExecuteDefaultActionGroup(first, count);
	return 0;
}

// The caller clips drawing to the column, so a cover wider than the column is cut off, not squeezed.
void PlaylistView::RenderCover(size_t block, const CRect& blockRect, CDCHandle dc) {
	const int inset = Scaled(kCoverInset);
	const int size = Scaled(m_settings.coverSize);
	const CPoint origin(blockRect.left + inset, blockRect.top + inset);

	metadb_handle_ptr track;
	if (!m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return;
	const HBITMAP cover = m_covers.Get(track, size);
	if (!cover) {
		dc.FillSolidRect(CRect(origin, CSize(size, size)), PaintUtils::BlendColor(GetSysColorHook(colorText), GetSysColorHook(colorBackground), 8));
		return;
	}
	BITMAP dimensions = {};
	if (!GetObject(cover, sizeof(dimensions), &dimensions)) return;
	CDC memory;
	memory.CreateCompatibleDC(dc);
	SelectObjectScope bitmapScope(memory, cover);
	// The cover keeps its transparency, so a cover with a see-through background sits on the row colour.
	const BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	dc.AlphaBlend(origin.x, origin.y, dimensions.bmWidth, dimensions.bmHeight, memory, 0, 0, dimensions.bmWidth, dimensions.bmHeight, blend);
}

void PlaylistView::RenderInfoLines(size_t block, const CRect& blockRect, CDCHandle dc) {
	pfc::string8_fastalloc text;
	m_playlists->activeplaylist_item_format_title(m_groups.BlockAt(block).firstItem, nullptr, text, m_blockScript, nullptr, playback_control::display_level_none);

	SelectObjectScope fontScope(dc, static_cast<HGDIOBJ>(GetFont()));
	dc.SetTextColor(GetSysColorHook(colorText));
	dc.SetBkMode(TRANSPARENT);

	const int rowHeight = GetItemHeight();
	const int padding = static_cast<int>(GetColumnSpacing());
	CRect lineRect(blockRect.left + padding, blockRect.top, blockRect.right - padding, blockRect.top + rowHeight);
	const char* line = text.c_str();
	while (lineRect.top < blockRect.bottom) {
		const char* lineEnd = strchr(line, '|');
		const t_size length = lineEnd ? static_cast<t_size>(lineEnd - line) : strlen(line);
		pfc::stringcvt::string_os_from_utf8 wide(line, length);
		m_text.Draw(dc, View(wide), lineRect, TextPainter::Align::Left, true);
		if (!lineEnd) break;
		line = lineEnd + 1;
		lineRect.OffsetRect(0, rowHeight);
	}
}

void PlaylistView::OnDestroy() {
	// The timing thread sends frames to this window, so it stops before the window goes away.
	StopSmoothScroll();
	SaveScrollPosition();
	ClaimEditContext(false);
	set_callback_flags(0);
	play_callback_reregister(0);
	SetMsgHandled(FALSE);
}

LRESULT PlaylistView::OnFocusChange(UINT message, WPARAM, LPARAM, BOOL& handled) {
	ClaimEditContext(message == WM_SETFOCUS);
	handled = FALSE;
	return 0;
}

// The Edit menu and its shortcuts (Undo, Select all, Copy, Paste) act on the edit context
// of the focused panel. Without a context they stay disabled.
void PlaylistView::ClaimEditContext(bool claim) {
	auto manager = ui_edit_context_manager::get();
	if (m_editContext) {
		manager->unset_context(*m_editContext);
		m_editContext.reset();
	}
	if (claim) m_editContext = manager->set_context_active_playlist();
}

// User-defined keyboard shortcuts take priority over the built-in keys, as in the default playlist view.
LRESULT PlaylistView::OnShortcutKey(UINT, WPARAM key, LPARAM, BOOL& handled) {
	handled = (keyboard_shortcut_manager::get()->on_keydown_auto_playlist(key) || RunEditKey(key) || RunPageKey(key)) ? TRUE : FALSE;
	return 0;
}

// Page Up and Page Down move the focus by one window height in pixels.
// The base class moves by "rows that fit in the window", which assumes every row has the same
// height. Blocks make most rows taller, so that jumps several screens at once.
bool PlaylistView::RunPageKey(WPARAM key) {
	if (key != VK_PRIOR && key != VK_NEXT) return false;
	const t_size count = GetItemCount();
	if (count == 0) return true;

	t_size current = GetGroupFocus2();
	if (current == SIZE_MAX) current = GetFocusItem();
	if (current >= count) current = 0;

	const bool down = key == VK_NEXT;
	const int targetY = GetItemOffsetAbs(current) + (down ? GetVisibleHeight() : -GetVisibleHeight());
	t_size target = targetY <= 0 ? 0 : (std::min)(IndexFromPointAbs(targetY), count - 1);
	if (target == current) {
		if (down && current + 1 < count) ++target;
		if (!down && current > 0) --target;
	}
	MoveFocusWithModifiers(target);
	return true;
}

// Same rules as the base class uses for arrow keys: Ctrl moves the focus only,
// Shift extends the selection from its anchor, no modifier selects the one row.
void PlaylistView::MoveFocusWithModifiers(t_size index) {
	if (IsKeyPressed(VK_CONTROL)) {
		SetFocusItem(index);
	} else if (IsKeyPressed(VK_SHIFT)) {
		t_size anchor = m_selectionStart;
		if (anchor == SIZE_MAX) anchor = GetFocusItem();
		if (anchor == SIZE_MAX) anchor = index;
		const t_size first = (std::min)(anchor, index);
		SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, (std::max)(anchor, index) + 1 - first));
		SetFocusItem(index);
		m_selectionStart = anchor;
	} else {
		SetFocusItem(index);
		SetSelection(pfc::bit_array_true(), pfc::bit_array_one(index));
	}
}

// foobar2000 binds no shortcut to undo, copy, cut, or paste by default. A playlist view handles these keys itself.
bool PlaylistView::RunEditKey(WPARAM key) {
	if (GetHotkeyModifierFlags() != MOD_CONTROL) return false;
	switch (key) {
	case 'Z': m_playlists->activeplaylist_undo_restore(); return true;
	case 'Y': m_playlists->activeplaylist_redo_restore(); return true;
	case 'C': CopySelection(); return true;
	case 'X': CopySelection(); RequestRemoveSelection(); return true;
	case 'V': PasteClipboard(); return true;
	default: return false;
	}
}

void PlaylistView::CopySelection() {
	metadb_handle_list items;
	m_playlists->activeplaylist_get_selected_items(items);
	if (items.get_count() == 0) return;
	OleSetClipboard(ole_interaction::get()->create_dataobject(items).get_ptr());
}

// Pastes tracks that foobar2000 put on the clipboard. Files copied in Explorer are not handled yet.
void PlaylistView::PasteClipboard() {
	pfc::com_ptr_t<IDataObject> clipboard;
	if (FAILED(OleGetClipboard(clipboard.receive_ptr()))) return;
	metadb_handle_list items;
	if (FAILED(ole_interaction::get()->parse_dataobject_immediate(clipboard, items)) || items.get_count() == 0) return;
	const t_size insertAt = GetPasteTarget();
	m_playlists->activeplaylist_undo_backup();
	m_playlists->activeplaylist_clear_selection();
	m_playlists->activeplaylist_insert_items(insertAt, items, pfc::bit_array_true());
}

t_size PlaylistView::GetItemCount() const {
	return m_playlists->activeplaylist_get_item_count();
}

bool PlaylistView::GetSubItemText(t_size item, t_size subItem, pfc::string_base& out) const {
	if (subItem >= m_listColumns.size() || m_listColumns[subItem].script.is_empty()) return false;
	const ListColumn& column = m_listColumns[subItem];
	const bool countsDown = column.kind == ColumnKind::Duration && column.countdownScript.is_valid() && GetDurationCountsDown();
	m_playlists->activeplaylist_item_format_title(item, nullptr, out, countsDown ? column.countdownScript : column.script, nullptr, playback_control::display_level_all);
	return true;
}

bool PlaylistView::IsRatingColumn(size_t subItem) const {
	return subItem < m_listColumns.size() && m_listColumns[subItem].kind == ColumnKind::Rating;
}

bool PlaylistView::IsLikeColumn(size_t subItem) const {
	return subItem < m_listColumns.size() && m_listColumns[subItem].kind == ColumnKind::Like;
}

int PlaylistView::RatingOf(size_t item, size_t subItem) const {
	pfc::string8 value;
	GetSubItemText(item, subItem, value);
	return ParseRating(value.c_str());
}

// Draws the text left to right, with bracketed parts in a colour halfway to the background, as the
// original did for titles such as "Song (Remastered 2012)". Text that does not fit ends in an ellipsis.
void PlaylistView::RenderDimmedBrackets(const char* text, const CRect& textRect, CDCHandle dc) {
	DCStateScope state(dc);
	dc.SetBkMode(TRANSPARENT);
	const COLORREF plain = dc.GetTextColor();
	const COLORREF dim = PaintUtils::BlendColor(plain, GetSysColorHook(colorBackground), 50);
	int x = textRect.left;
	for (const TextPart& part : SplitBracketParts(text)) {
		if (x >= textRect.right) break;
		const pfc::stringcvt::string_os_from_utf8 wide(part.text.c_str());
		const int width = m_text.Width(dc, View(wide));
		CRect partRect(x, textRect.top, textRect.right, textRect.bottom);
		const bool fits = x + width <= textRect.right;
		dc.SetTextColor(part.dim ? dim : plain);
		m_text.Draw(dc, View(wide), partRect, TextPainter::Align::Left, !fits);
		if (!fits) break;
		x += width;
	}
}

// Tied to the row height so the stars grow with the font.
int PlaylistView::StarSlotWidth() const {
	return GetItemHeight() * 7 / 9;
}

// The stars come from the icon font Windows 11 apps draw theirs with (Segoe Fluent Icons), or its Windows 10
// forerunner (Segoe MDL2 Assets): a filled star and an outline star made as a pair. Without either, the row font's
// star characters stand in. The font follows the star slot, so it is made again when the row height changes.
void PlaylistView::UpdateStarFont() {
	const int height = StarSlotWidth() * 4 / 5;
	// Looked up once per size, also when neither icon font is installed.
	if (height == m_starFontHeight) return;
	if (m_starFont) m_starFont.DeleteObject();
	m_starFontHeight = height;
	m_starFontIsIcons = false;
	CWindowDC screen(NULL);
	for (const wchar_t* face : { L"Segoe Fluent Icons", L"Segoe MDL2 Assets" }) {
		LOGFONTW font = {};
		font.lfHeight = -height;
		font.lfCharSet = DEFAULT_CHARSET;
		// Grey smoothing: ClearType's coloured fringes show on a small coloured shape.
		font.lfQuality = ANTIALIASED_QUALITY;
		wcsncpy_s(font.lfFaceName, face, _TRUNCATE);
		CFont candidate;
		if (!candidate.CreateFontIndirect(&font)) continue;
		// Windows hands back another font when the face is missing; the name it picked tells.
		SelectObjectScope scope(screen.m_hDC, static_cast<HFONT>(candidate));
		wchar_t picked[LF_FACESIZE] = {};
		::GetTextFaceW(screen, LF_FACESIZE, picked);
		if (_wcsicmp(picked, face) != 0) continue;
		m_starFont.Attach(candidate.Detach());
		m_starFontIsIcons = true;
		return;
	}
}

void PlaylistView::RenderSubItemText(t_size item, t_size subItem, const CRect& subItemRect, const CRect& updateRect, CDCHandle dc, bool allowColors) {
	// The playing track is drawn in bold, so it stands out even when the Playing column is hidden.
	// The list control selects its own font for each row, so the bold font goes in here, per cell.
	if (IsLikeColumn(subItem)) {
		RenderLikeCell(item, subItem, subItemRect, dc);
		return;
	}
	if (!IsRatingColumn(subItem)) {
		const bool bold = m_settings.boldPlaying && m_playingFont && IsPlayingItem(item);
		SelectObjectScope fontScope(dc, bold ? static_cast<HGDIOBJ>(static_cast<HFONT>(m_playingFont)) : static_cast<HGDIOBJ>(dc.GetCurrentFont()));
		// The pause sign (U+23F8) is missing from common fonts, so it is drawn as two bars in the text colour.
		pfc::string8 text;
		if (GetSubItemText(item, subItem, text) && strcmp(text.c_str(), "\xE2\x8F\xB8") == 0) {
			const int barHeight = GetItemHeight() / 2;
			const int barWidth = (std::max)(2, barHeight / 4);
			const int gap = barWidth;
			const int top = subItemRect.top + (GetItemHeight() - barHeight) / 2;
			const int left = subItemRect.left + (subItemRect.Width() - (2 * barWidth + gap)) / 2;
			dc.FillSolidRect(CRect(left, top, left + barWidth, top + barHeight), dc.GetTextColor());
			dc.FillSolidRect(CRect(left + barWidth + gap, top, left + 2 * barWidth + gap, top + barHeight), dc.GetTextColor());
			return;
		}
		const bool leftAligned = (GetColumnFormat(subItem) & HDF_JUSTIFYMASK) == HDF_LEFT;
		if (m_settings.dimBrackets && leftAligned && HasBracket(text.c_str())) {
			RenderDimmedBrackets(text.c_str(), GetItemTextRectHook(item, subItem, subItemRect), dc);
			return;
		}
		// Every track cell is a plain text cell (GetCellType is not overridden), drawn here as the list control would,
		// so its text goes through the same painter as the rest of the panel.
		const pfc::stringcvt::string_os_from_utf8 wide(text.c_str());
		m_text.Draw(dc, View(wide), GetItemTextRectHook(item, subItem, subItemRect), AlignOfColumnFormat(GetColumnFormat(subItem)), true);
		return;
	}
	const int savedRating = RatingOf(item, subItem);
	const int rating = m_starHover.ShownRating(item, subItem, savedRating);
	const int slot = StarSlotWidth();
	UpdateStarFont();
	DCStateScope state(dc);
	if (m_starFontIsIcons) dc.SelectFont(m_starFont);
	dc.SetBkMode(TRANSPARENT);
	dc.IntersectClipRect(subItemRect);
	const int left = subItemRect.left + RatingGapWidth(slot, static_cast<int>(GetColumnSpacing()));
	CRect starRect(left, subItemRect.top, left + slot, subItemRect.top + GetItemHeight());
	// Filled stars take the star colour and empty ones fade toward the background, so a rating reads at a glance.
	// While the pointer previews a rating the filled stars are paler, as in the Windows rating control,
	// so the preview does not look saved until the click.
	const COLORREF background = GetSysColorHook(colorBackground);
	const COLORREF filled = rating != savedRating ? PaintUtils::BlendColor(StarFillColor(), background, 45) : StarFillColor();
	const COLORREF empty = PaintUtils::BlendColor(dc.GetTextColor(), background, 55);
	const wchar_t* const filledGlyph = m_starFontIsIcons ? L"\xE735" : L"\x2605";
	const wchar_t* const emptyGlyph = m_starFontIsIcons ? L"\xE734" : L"\x2606";
	for (int star = 1; star <= kMaxRating; ++star) {
		dc.SetTextColor(star <= rating ? filled : empty);
		dc.DrawText(star <= rating ? filledGlyph : emptyGlyph, 1, starRect, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_CENTER);
		starRect.OffsetRect(slot, 0);
	}
}

// One heart, in the middle of the cell, on or off: filled when the track is liked (in the heart colour, or as the colour
// emoji), an outline fading toward the background when not. A click turns it over. Unlike the stars, the pointer
// previews nothing: a heart is a plain switch, as in other players.
void PlaylistView::RenderLikeCell(size_t item, size_t subItem, const CRect& subItemRect, CDCHandle dc) {
	const bool liked = RatingOf(item, subItem) > 0;
	CRect rowRect(subItemRect.left, subItemRect.top, subItemRect.right, subItemRect.top + GetItemHeight());
	DCStateScope state(dc);
	dc.SetBkMode(TRANSPARENT);
	dc.IntersectClipRect(subItemRect);
	// The emoji heart is drawn in the row font, through the painter that shows colour emoji.
	if (liked && m_settings.likeStyle == LikeStyle::Emoji) {
		m_text.Draw(dc, L"\x2764\xFE0F", rowRect, TextPainter::Align::Center, false);
		return;
	}
	UpdateStarFont();
	if (m_starFontIsIcons) dc.SelectFont(m_starFont);
	// Segoe Fluent Icons and Segoe MDL2 Assets: HeartFill and Heart. Without them, the row font's heart characters.
	const wchar_t* const glyph = liked ? (m_starFontIsIcons ? L"\xEB52" : L"\x2665") : (m_starFontIsIcons ? L"\xEB51" : L"\x2661");
	dc.SetTextColor(liked ? LikeFillColor() : PaintUtils::BlendColor(dc.GetTextColor(), GetSysColorHook(colorBackground), 55));
	dc.DrawText(glyph, 1, rowRect, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

// The hearts' own colour, or the star colour.
COLORREF PlaylistView::LikeFillColor() const {
	return m_settings.customLikeColor ? m_settings.likeColor : StarFillColor();
}

// The stars and the heart sit in the track row, below any subgroup labels the row carries. On the heart, the
// position holds the like a click would give: 1 to like, 0 to unlike.
std::optional<StarPosition> PlaylistView::StarAtPoint(CPoint clientPoint) const {
	if (IsOnPinnedHeader(clientPoint)) return std::nullopt;
	size_t item = 0;
	size_t subItem = 0;
	if (!GetItemAtPointAbsEx(PointClientToAbs(clientPoint), item, subItem)) return std::nullopt;
	if (!IsRatingColumn(subItem) && !IsLikeColumn(subItem)) return std::nullopt;
	const CRect cell = GetSubItemRect(item, subItem);
	const int rowTop = cell.top + LabelLinesAbove(item) * GetItemHeight();
	if (clientPoint.y < rowTop || clientPoint.y >= rowTop + GetItemHeight()) return std::nullopt;
	if (IsLikeColumn(subItem)) return StarPosition{ item, subItem, RatingOf(item, subItem) > 0 ? 0 : 1 };
	const int slot = StarSlotWidth();
	const std::optional<int> rating = RatingAtOffset(clientPoint.x - cell.left, RatingGapWidth(slot, static_cast<int>(GetColumnSpacing())), slot);
	if (!rating) return std::nullopt;
	return StarPosition{ item, subItem, *rating };
}

void PlaylistView::InvalidateStarCell(const std::optional<StarPosition>& at) {
	if (m_hWnd && at && at->item < GetItemCount()) InvalidateRect(GetSubItemRect(at->item, at->subItem));
}

void PlaylistView::MoveStarHover(const std::optional<StarPosition>& at) {
	const std::optional<StarPosition> before = m_starHover.Position();
	if (!m_starHover.MoveTo(at)) return;
	InvalidateStarCell(before);
	InvalidateStarCell(at);
}

// The preview follows the pointer while no button is held. A held button means a drag, and the
// wheel moves the rows under a pointer that stays put, so both end the preview.
// Every message goes on to the list control, which uses the same messages for its own work.
LRESULT PlaylistView::OnStarMouseMove(UINT message, WPARAM buttons, LPARAM position, BOOL& handled) {
	handled = FALSE;
	const WPARAM anyButton = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
	if (message != WM_MOUSEMOVE || (buttons & anyButton) != 0) {
		MoveStarHover(std::nullopt);
		return 0;
	}
	std::optional<StarPosition> at = StarAtPoint(CPoint(position));
	// Only the stars preview a click; the heart is a plain switch.
	if (at && IsLikeColumn(at->subItem)) at.reset();
	if (at && !m_starHover.Position()) {
		TRACKMOUSEEVENT leave = { sizeof(leave), TME_LEAVE, m_hWnd, 0 };
		TrackMouseEvent(&leave);
	}
	MoveStarHover(at);
	return 0;
}

// A click on a star, or on the gap before the first star, rates the track and goes no further, so it neither changes the selection
// nor starts a drag. The second press of a double click is dropped, because passed on it would start playback.
LRESULT PlaylistView::OnStarButtonDown(UINT message, WPARAM, LPARAM position, BOOL& handled) {
	const std::optional<StarPosition> hit = StarAtPoint(CPoint(position));
	if (!hit) {
		handled = FALSE;
		return 0;
	}
	SetFocus();
	if (message == WM_LBUTTONDOWN) {
		if (IsLikeColumn(hit->subItem)) WriteLikeOf(hit->item, hit->rating != 0);
		else WriteRatingOf(hit->item, hit->rating);
		// The cell now shows the new value; the pointer previews the next click only after it moves.
		MoveStarHover(std::nullopt);
	}
	return 0;
}

void PlaylistView::WriteLikeOf(size_t item, bool liked) {
	metadb_handle_ptr track;
	if (!m_playlists->activeplaylist_get_item_handle(track, item)) return;
	metadb_handle_list tracks;
	tracks.add_item(track);
	WriteLikes(tracks, liked, GetRatingStorage());
}

// The playlist reports the change when the write is done, and the row repaints then.
void PlaylistView::WriteRatingOf(size_t item, int rating) {
	metadb_handle_ptr track;
	if (!m_playlists->activeplaylist_get_item_handle(track, item)) return;
	WriteRating(track, rating, GetRatingStorage());
}

// Ctrl with the mouse wheel resizes the album art of every panel, the same setting as on the Preferences page.
// Without Ctrl the wheel scrolls as usual.
LRESULT PlaylistView::OnCoverWheel(UINT, WPARAM wheel, LPARAM, BOOL& handled) {
	const bool ctrl = (GET_KEYSTATE_WPARAM(wheel) & MK_CONTROL) != 0 || GetKeyState(VK_CONTROL) < 0;
	if (!ctrl) {
		handled = FALSE;
		return 0;
	}
	const int notches = GET_WHEEL_DELTA_WPARAM(wheel) / WHEEL_DELTA;
	ViewSettings settings = LoadViewSettings();
	const int size = ClampCoverSize(settings.coverSize + notches * 8);
	if (size != settings.coverSize) {
		settings.coverSize = size;
		ApplyViewSettings(settings);
	}
	return 0;
}

// While the columns fill the panel, dragging a track column's divider moves only the border between it
// and the next column on screen, so the right edge stays at the panel's edge. The header's own change
// is refused and both widths are set here. The last column's divider is the panel's edge and does not move.
LRESULT PlaylistView::OnHeaderItemChanging(int, LPNMHDR header, BOOL& handled) {
	handled = FALSE;
	const NMHEADER* change = reinterpret_cast<NMHEADER*>(header);
	if (!m_autoSizeColumns || m_fittingColumns || !change->pitem || !(change->pitem->mask & HDI_WIDTH)) return FALSE;
	const std::vector<size_t> onScreen = TrackColumnsOnScreen();
	const auto dragged = std::find(onScreen.begin(), onScreen.end(), static_cast<size_t>(change->iItem));
	if (dragged == onScreen.end()) return FALSE;
	handled = TRUE;
	if (dragged + 1 == onScreen.end()) return TRUE;
	const size_t neighbor = *(dragged + 1);
	const SharedWidths widths = ShareWithNeighbor(static_cast<int>(GetSubItemWidth(*dragged)), static_cast<int>(GetSubItemWidth(neighbor)),
		change->pitem->cxy, Scaled(kMinFitColumnWidth));
	{
		pfc::vartoggle_t<bool> fitting(m_fittingColumns, true);
		ResizeColumn(*dragged, static_cast<t_uint32>(widths.dragged), false);
		ResizeColumn(neighbor, static_cast<t_uint32>(widths.neighbor), false);
	}
	OnColumnsChanged();
	return TRUE;
}

// The work waits for a posted message, because rebuilding the columns inside the header's notice would
// pull the columns out from under the header while it still handles the drag.
LRESULT PlaylistView::OnHeaderEndTrack(int, LPNMHDR header, BOOL& handled) {
	handled = FALSE;
	PostMessage(kColumnDragEndedMessage, static_cast<WPARAM>(reinterpret_cast<NMHEADER*>(header)->iItem));
	return 0;
}

// The list would size the cover column to its text, which is empty. While the columns fill the panel,
// sizing one column to its text would leave them wider or narrower than the panel. So a double-click keeps the width.
LRESULT PlaylistView::OnHeaderDividerDoubleClick(int, LPNMHDR header, BOOL& handled) {
	handled = m_autoSizeColumns || reinterpret_cast<NMHEADER*>(header)->iItem == static_cast<int>(kCoverColumn);
	return 0;
}

LRESULT PlaylistView::OnColumnDragEnded(UINT, WPARAM column, LPARAM, BOOL&) {
	if (column == kCoverColumn && !m_ungrouped) {
		ApplyCoverColumnWidth();
	} else if (m_autoSizeColumns) {
		// A track column's drag took its width from its neighbor, so the widths on screen become the proportions.
		if (column >= kFirstTrackColumn) SaveTrackColumnWeights();
		CaptureColumnWidths();
		FitTrackColumns();
	}
	return 0;
}

// Dragging the cover column's divider sets the cover size, as in the Default UI playlist. The size is set when
// the drag ends, because each new size reads every cover again. The column snaps to the cover,
// so a width outside the allowed cover sizes springs back to the nearest one.
void PlaylistView::ApplyCoverColumnWidth() {
	const int width = MulDiv(GetSubItemWidth(kCoverColumn), 96, m_dpi.cx);
	ViewSettings settings = LoadViewSettings();
	settings.coverSize = ClampCoverSize(width - 2 * kCoverInset);
	if (settings.coverSize == m_settings.coverSize && width == settings.coverSize + 2 * kCoverInset) return;
	ApplyViewSettings(settings);
}

// The wheel without Ctrl or Shift, while smooth scrolling is on, starts or extends an animation (smooth_scroll.h).
// A precision touchpad, a held mouse button (a drag), and a list that does not scroll go on to the list control.
LRESULT PlaylistView::OnSmoothWheel(UINT, WPARAM wheel, LPARAM, BOOL& handled) {
	handled = FALSE;
	if (!m_settings.smoothScroll) return 0;
	const WORD keys = GET_KEYSTATE_WPARAM(wheel);
	if ((keys & (MK_SHIFT | MK_CONTROL | MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) != 0) return 0;
	const CRect client = GetClientRectHook();
	if (client.Height() >= GetViewAreaRectAbs().Height()) return 0;
	if (!m_smoothScroll) {
		m_smoothScroll.emplace(m_hWnd, kSmoothScrollMessage, kSmoothScrollTimer,
			[this] { return static_cast<int>(GetViewOrigin().y); },
			[this](int position) { return static_cast<int>(ClipViewOrigin(CPoint(GetViewOrigin().x, position)).y); },
			[this](int position) {
				pfc::vartoggle_t<bool> moving(m_animatingWheel, true);
				MoveViewOrigin(CPoint(GetViewOrigin().x, position));
			});
	}
	const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wheel);
	if (!m_smoothScroll->ShouldAnimateWheel(wheelDelta)) return 0;
	UINT lines = 3;
	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
	m_smoothScroll->DeltaScroll(smooth_scroll::WheelPixels(wheelDelta, lines, GetItemHeight(), client.Height()), LiveInertiaParams());
	handled = TRUE;
	return 0;
}

LRESULT PlaylistView::OnSmoothScrollFrame(UINT, WPARAM, LPARAM, BOOL&) {
	if (m_smoothScroll) m_smoothScroll->OnMessage();
	return 0;
}

LRESULT PlaylistView::OnSmoothScrollTimer(UINT, WPARAM timer, LPARAM, BOOL& handled) {
	if (timer != kSmoothScrollTimer) {
		handled = FALSE;
		return 0;
	}
	if (m_smoothScroll) m_smoothScroll->OnTimer();
	else KillTimer(kSmoothScrollTimer);
	return 0;
}

void PlaylistView::StopSmoothScroll() {
	m_smoothScroll.reset();
}

void PlaylistView::SetHostFont(HFONT font) {
	m_hostFont = font;
	ApplyFonts();
}

void PlaylistView::ApplyFonts() {
	m_trackFont.DeleteObject();
	m_headerFont.DeleteObject();
	const LOGFONTW trackFont = FontAtDpi(m_settings.trackFont, m_dpi.cy);
	const LOGFONTW headerFont = FontAtDpi(m_settings.headerFont, m_dpi.cy);
	if (m_settings.customTrackFont) m_trackFont.CreateFontIndirect(&trackFont);
	if (m_settings.customHeaderFont) m_headerFont.CreateFontIndirect(&headerFont);
	const HFONT track = m_trackFont ? static_cast<HFONT>(m_trackFont) : m_hostFont;
	if (m_hWnd && track) SetFont(track);

	m_playingFont.DeleteObject();
	LOGFONTW bold = {};
	if (track && ::GetObjectW(track, sizeof(bold), &bold)) {
		bold.lfWeight = FW_BOLD;
		m_playingFont.CreateFontIndirect(&bold);
	}
}

// The cover column is sized to fit the cover, so a new cover size also sets that column's width.
// An open Preferences page takes what a panel changed (PanelSettingsChanged), so its Apply keeps it,
// and the panels go on showing the page's unapplied edits on top of the new settings.
void PlaylistView::ApplyViewSettings(const ViewSettings& settings) {
	SaveViewSettings(settings);
	PanelSettingsChanged();
	ShowPreferencesPreview();
}

void PlaylistView::ShowViewSettings(const ViewSettings& settings) {
	for (PlaylistView* view : LiveViews()) {
		view->m_settings = settings;
		if (!view->m_settings.smoothScroll) view->StopSmoothScroll();
		if (!view->m_hWnd) continue;
		view->m_layout = view->CapturedLayout();
		view->m_layout.coverWidth = static_cast<float>(view->m_settings.coverSize + 2 * kCoverInset);
		view->ApplyFonts();
		view->ApplyHeaderDragging();
		view->RebuildColumns();
		view->OnPlaylistContentChanged();
		view->Invalidate();
	}
}
void PlaylistView::ApplyRatingStorage(RatingStorage storage) {
	SetRatingStorage(storage);
	for (PlaylistView* view : LiveViews()) {
		if (!view->m_hWnd) continue;
		// The widths and places the user dragged stay.
		view->m_layout = view->CapturedLayout();
		view->RebuildColumns();
	}
}

t_size PlaylistView::GetFocusItem() const {
	if (m_groupFocus != SIZE_MAX) return SIZE_MAX;
	return m_playlists->activeplaylist_get_focus_item();
}

// The playlist sends no focus callback when its focus item stays the same,
// so the row is repainted here to show the focus mark again after a header had it.
void PlaylistView::SetFocusItem(t_size index) {
	ClearGroupFocus();
	m_selectionStart = index;
	m_playlists->activeplaylist_set_focus_item(index);
	if (index < GetItemCount()) UpdateItem(index);
}

void PlaylistView::SetGroupFocusByItem(t_size item) {
	const t_size rowFocus = GetFocusItem();
	ClearGroupFocus();
	m_groupFocus = item;
	if (rowFocus < GetItemCount()) UpdateItem(rowFocus);
	UpdateGroupHeader2(item);
	EnsureHeaderVisible2(item);
	OnFocusChangedGroup2(item);
}

size_t PlaylistView::GetGroupFocus2() const {
	return m_groupFocus < GetItemCount() ? m_groupFocus : SIZE_MAX;
}

void PlaylistView::ClearGroupFocus() {
	const size_t previous = m_groupFocus;
	m_groupFocus = SIZE_MAX;
	if (previous < GetItemCount()) UpdateGroupHeader2(previous);
}

bool PlaylistView::IsItemSelected(t_size index) const {
	return m_playlists->activeplaylist_is_item_selected(index);
}

void PlaylistView::SetSelection(pfc::bit_array const& affected, pfc::bit_array const& status) {
	m_playlists->activeplaylist_set_selection(affected, status);
}

// The list control starts a move only from tracks that are already selected. A press on an album header
// would start a selection box, and a press on a label row would select one track. Selecting the album or
// the section here first lets one press and drag move it. Ctrl and Shift clicks keep their usual meaning.
LRESULT PlaylistView::OnHeaderButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled) {
	handled = FALSE;
	m_pressedSection.reset();
	if ((keys & (MK_CONTROL | MK_SHIFT)) != 0) return 0;
	const CPoint point(position);
	size_t item = 0;
	if (GroupHeaderFromPoint2(point, item)) {
		const size_t count = ResolveGroupRange2(item);
		if (count > 0) SetSelection(pfc::bit_array_true(), pfc::bit_array_range(item, count));
	} else if (const std::optional<size_t> level = LabelLevelAtPoint(point, item)) {
		const auto [first, end] = SectionRangeOf(item, *level);
		SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, end - first));
		m_pressedSection = std::make_pair(first, end);
	}
	return 0;
}

// On a click without a drag the list control selects only the track under the label row when the
// button comes up. The section is selected again once the list control is done with the message.
LRESULT PlaylistView::OnLabelButtonUp(UINT, WPARAM, LPARAM, BOOL& handled) {
	handled = FALSE;
	if (m_pressedSection) PostMessage(kSelectSectionMessage);
	return 0;
}

LRESULT PlaylistView::OnSelectPressedSection(UINT, WPARAM, LPARAM, BOOL&) {
	if (!m_pressedSection) return 0;
	const auto [first, end] = *m_pressedSection;
	m_pressedSection.reset();
	if (end <= GetItemCount() && first < end) SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, end - first));
	return 0;
}

// A drag that starts on an album header moves the album, and one that starts on a label row moves that
// section. Both are dropped only at boundaries of the same kind.
// The list control passes the origin in list coordinates (PointClientToAbs), not client coordinates.
void PlaylistView::RunDragDrop(const CPoint& origin, bool isRightClick) {
	size_t item = 0;
	m_pressedSection.reset();
	m_dragUnit = DragUnit::Tracks;
	if (!isRightClick) {
		const CPoint clientOrigin = PointAbsToClient(origin);
		if (GroupHeaderFromPoint2(clientOrigin, item)) {
			m_dragUnit = DragUnit::Album;
		} else if (const std::optional<size_t> level = LabelLevelAtPoint(clientOrigin, item)) {
			m_dragUnit = DragUnit::Section;
			m_dragLevel = *level;
		}
	}
	__super::RunDragDrop(origin, isRightClick);
	m_dragUnit = DragUnit::Tracks;
}

// The list control asks this both for the drop line it draws while dragging and for the place the
// dragged tracks go. During an album or section drag both snap to the nearest boundary of that kind.
t_size PlaylistView::InsertIndexFromPointEx(const CPoint& point, bool& inside) const {
	if (m_dragUnit == DragUnit::Tracks) return __super::InsertIndexFromPointEx(point, inside);
	inside = false;
	return DropIndexAt(point, m_dragUnit, m_dragLevel);
}

void PlaylistView::RequestReorder(size_t const* order, size_t count) {
	m_playlists->activeplaylist_undo_backup();
	m_playlists->activeplaylist_reorder_items(order, count);
}

void PlaylistView::RequestRemoveSelection() {
	m_playlists->activeplaylist_undo_backup();
	m_playlists->activeplaylist_remove_selection();
}

namespace {

// True when the dragged things are files from Explorer and at least one of them is a folder.
bool DropHoldsFolder(IDataObject* data) {
	FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM medium = {};
	if (FAILED(data->GetData(&format, &medium))) return false;
	bool folder = false;
	const HDROP drop = static_cast<HDROP>(medium.hGlobal);
	const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
	for (UINT index = 0; index < count && !folder; ++index) {
		wchar_t path[MAX_PATH * 4] = {};
		if (DragQueryFileW(drop, index, path, static_cast<UINT>(std::size(path))) == 0) continue;
		const DWORD attributes = GetFileAttributesW(path);
		folder = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}
	ReleaseStgMedium(&medium);
	return folder;
}

} // namespace

// Files and folders dragged in from Explorer, or anything else foobar2000 can read as tracks.
// Dropped files go before or after the row under the point. Dropped folders, usually whole albums,
// go before or after the album under the point (DropIndexAt).
PlaylistView::dragDropAccept_t PlaylistView::DragDropAccept2(IDataObject* data) {
	dragDropAccept_t accept;
	if (playlist_incoming_item_filter::get()->process_dropped_files_check(data)) {
		accept.dwEFfect = DROPEFFECT_COPY;
		accept.showDropMark = true;
	}
	return accept;
}

// The tracks go in before the row under the drop point, or after it when the point is in the row's lower half.
// Reading the files can take a while, so they are inserted when foobar2000 has them, into the playlist
// that was active at the drop.
void PlaylistView::OnDrop(IDataObject* data, CPoint screenPoint) {
	CPoint point = screenPoint;
	ScreenToClient(&point);
	size_t index = GetItemCount();
	size_t item = 0;
	if (DropHoldsFolder(data)) {
		index = DropIndexAt(point, DragUnit::Album, 0);
	} else if (ItemFromPoint(point, item)) {
		const CRect row = GetItemRect(item);
		index = point.y < row.top + (row.Height() / 2) ? item : item + 1;
	}
	const t_size playlist = m_playlists->get_active_playlist();
	auto playlists = m_playlists;
	auto notify = process_locations_notify::create([playlists, playlist, index](metadb_handle_list_cref tracks) {
		if (playlist >= playlists->get_playlist_count()) return;
		const t_size at = (std::min)(index, playlists->playlist_get_item_count(playlist));
		playlists->playlist_clear_selection(playlist);
		playlists->playlist_insert_items(playlist, at, tracks, pfc::bit_array_true());
	});
	playlist_incoming_item_filter_v2::get()->process_dropped_files_async(data, playlist_incoming_item_filter_v2::op_flag_delay_ui, core_api::get_main_window(), notify);
}

void PlaylistView::ExecuteDefaultAction(t_size index) {
	m_playlists->activeplaylist_execute_default_action(index);
}

// Any change of order this view did not make itself (a drag, another sort, added or removed tracks)
// ends the order of the header click. Ctrl+Z arrives as a playlist switch (on_playlist_switch).
void PlaylistView::on_items_added(t_size, metadb_handle_list_cref, const bit_array&) {
	if (!m_sorting) m_headerSort.Forget();
	OnRowsMoved();
}

void PlaylistView::on_items_reordered(const t_size*, t_size) {
	if (!m_sorting) m_headerSort.Forget();
	OnRowsMoved();
}

void PlaylistView::on_items_removed(const bit_array&, t_size, t_size) {
	if (!m_sorting) m_headerSort.Forget();
	OnRowsMoved();
}

void PlaylistView::on_items_selection_change(const bit_array& affected, const bit_array& state) {
	UpdateItems(affected);
	OnSelectionChanged(affected, state);
}

void PlaylistView::on_item_focus_change(t_size from, t_size to) {
	ClearGroupFocus();
	const t_size count = GetItemCount();
	if (from < count) UpdateItem(from);
	if (to < count) {
		UpdateItem(to);
		EnsureItemVisible(to);
	}
	OnFocusChanged(from, to);
}

// A tag edit can change an album name, which moves group boundaries.
void PlaylistView::on_items_modified(const bit_array&) {
	OnPlaylistContentChanged();
}

void PlaylistView::on_items_modified_fromplayback(const bit_array& mask, play_control::t_display_level) {
	ReloadItems(mask);
}

void PlaylistView::on_items_replaced(const bit_array&, const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>&) {
	OnPlaylistContentChanged();
}

void PlaylistView::on_item_ensure_visible(t_size index) {
	EnsureItemVisible(index);
}

// The view still shows the old playlist's rows here, so its scroll position is saved first.
// Undo and redo also arrive here, and they end the order of a header click as well.
void PlaylistView::on_playlist_switch() {
	m_headerSort.Forget();
	SaveScrollPosition();
	m_selectionStart = SIZE_MAX;
	ApplyPlaylistPreset();
	OnRowsMoved();
	RestoreScrollPosition();
}
