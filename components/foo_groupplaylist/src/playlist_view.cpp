#include "stdafx.h"

#include <libPPUI/IDataObjectUtils.h>
#include <libPPUI/PaintUtils.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <chrono>
#include <cmath>
#include <numeric>
#include <utility>

#pragma comment(lib, "uxtheme.lib")

#include "column_fit.h"
#include "column_store.h"
#include "info_lines_popup.h"
#include "playlist_view.h"
#include "preferences_pages.h"
#include "preset_store.h"
#include "rating_cell.h"
#include "rating_store.h"
#include "text_parts.h"
#include "text_rendering.h"
#include "ui_language.h"
#include "view_settings_fields.h"

namespace {

std::wstring_view View(const pfc::stringcvt::string_os_from_utf8& text) {
	return std::wstring_view(text.get_ptr(), text.length());
}

// Sizes in pixels at 96 DPI. The cover size is a setting, and its margin is in view_settings_rules.h.
constexpr int kBlockGap = 12;
// The narrowest a track column gets from a drag while the columns fill the panel.
constexpr int kMinFitColumnWidth = 16;
// How a dragged column meets the next one (ColumnDragFeel): the next column holds out, giving way a quarter of how far
// it is covered (at most 16 pixels at 96 DPI), until 60% of it is covered and a few pixels more; then it gives in
// and slides quickly to its new place, going a little past it (ColumnSlides). Tuned by feel.
constexpr double kColumnSwapDepth = 0.6;
constexpr int kColumnSwapSlack = 6;
constexpr double kColumnResistShare = 0.25;
constexpr int kColumnResistMax = 16;
constexpr double kColumnSlideMs = 120;
constexpr double kColumnSlideOvershoot = 0.9;
// A column title starts to move once it has been pressed this long, or dragged this far at once (96 DPI pixels);
// before that, letting go is a click (ColumnDragStarts). A plain click takes about 100 ms. Tuned by feel.
constexpr unsigned kColumnDragHoldMs = 150;
constexpr int kColumnDragEscape = 12;
// How many column changes Ctrl+Z can undo in a row.
constexpr size_t kColumnUndoSteps = 20;
// The other animations (effects.h), always on: track column widths slide to a fit or as a column is shown or hidden,
// a cover that finishes loading fades in, and a clicked heart or star grows by a third and goes back.
constexpr double kCoverFadeMs = 150;
constexpr double kWidthSlideMs = 120;
constexpr double kPopMs = 180;
constexpr double kPopAmount = 0.3;
// A pushed column edge (PushFeel): it holds for this many pixels of the pointer when a column has given all it may,
// and longer at the wall, past which text columns are cut short (96 DPI pixels). Tuned by feel.
constexpr int kColumnFitCatch = 8;
constexpr int kColumnFitWall = 24;

constexpr unsigned kPlaylistCallbacks =
	playlist_callback_single::flag_on_items_added |
	playlist_callback_single::flag_on_items_reordered |
	playlist_callback_single::flag_on_items_removing |
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
	  m_columnDefs(SavedColumnDefs()),
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
	m_columnDefs = SavedColumnDefs();
	const auto renameIn = [&renames](ColumnLayout& other) {
		for (ColumnLayout::Column& column : other.columns) {
			for (const auto& rename : renames) {
				if (column.name == rename.first) {
					column.name = rename.second;
					break;
				}
			}
		}
	};
	// The base and the layout on entering the playlist get the same renames and lose the same deleted columns as the
	// shown layout, so a change the user made before this, not yet stored with the playlist, still counts as a change.
	if (m_baseLayout) {
		renameIn(*m_baseLayout);
		m_baseLayout = WithKnownColumns(*m_baseLayout);
	}
	if (m_layoutOnEnter) {
		renameIn(*m_layoutOnEnter);
		m_layoutOnEnter = WithKnownColumns(*m_layoutOnEnter);
	}
	// So do the columns kept for Ctrl+Z, or a renamed column would be dropped as unknown when they come back. A deleted
	// column is dropped then (RestoreColumnState makes the columns again, as the set differs).
	for (std::vector<ColumnState>* states : { &m_columnUndo, &m_columnRedo }) {
		for (ColumnState& state : *states) {
			renameIn(state.layout);
			for (std::string& name : state.listNames) {
				for (const auto& rename : renames) {
					if (name == rename.first) {
						name = rename.second;
						break;
					}
				}
			}
		}
	}
	if (!renames.empty()) {
		auto playlists = playlist_manager_v5::get();
		for (t_size playlist = 0; playlist < playlists->get_playlist_count(); ++playlist) {
			std::optional<ColumnLayout> own = PlaylistColumns(playlist);
			if (!own) continue;
			renameIn(*own);
			SetPlaylistColumns(playlist, *own);
		}
	}
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

// A column that is turned on unfolds from nothing, and one turned off folds away before it goes, while the others make
// room or take it: with the columns filling the panel, in their proportions; otherwise the columns beyond it slide.
void PlaylistView::ToggleColumn(const std::string& name) {
	if (!FindColumnDef(name)) return;
	FinishColumnWidthSlide();
	const bool animate = m_hWnd && GetColumnCount() == kFirstTrackColumn + m_layout.columns.size();
	ColumnState columnsBefore = CaptureColumnState();
	if (m_hWnd) SetFocus();
	if (!animate) {
		ApplyColumnToggle(name);
		RememberColumnState(columnsBefore);
		return;
	}
	const std::vector<size_t> onScreen = TrackColumnsOnScreen();
	std::vector<std::pair<std::string, int>> before;
	for (const size_t column : onScreen) before.emplace_back(m_layout.columns[column - kFirstTrackColumn].name, static_cast<int>(GetSubItemWidth(column)));
	const auto shown = std::find_if(before.begin(), before.end(), [&](const auto& column) { return column.first == name; });
	if (shown != before.end()) {
		// Folding: the column goes to nothing, then it is taken out.
		std::vector<int> from;
		std::vector<float> weights;
		for (const auto& column : before) {
			from.push_back(column.second);
			if (column.first != name) weights.push_back(static_cast<float>(column.second));
		}
		std::vector<int> to;
		const int total = std::accumulate(from.begin(), from.end(), 0);
		const std::vector<int> others = m_autoSizeColumns ? FitColumnWidths(weights, total) : std::vector<int>();
		size_t other = 0;
		for (const auto& column : before) {
			if (column.first == name) to.push_back(0);
			else to.push_back(m_autoSizeColumns ? others[other++] : column.second);
		}
		SlideColumnWidths(onScreen, from, to, [this, name, columnsBefore] {
			ApplyColumnToggle(name);
			RememberColumnState(columnsBefore);
		});
		return;
	}
	// Unfolding: the column is put in, then grows from nothing while the others give it room.
	ApplyColumnToggle(name);
	if (!m_hWnd || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) {
		RememberColumnState(columnsBefore);
		return;
	}
	const std::vector<size_t> after = TrackColumnsOnScreen();
	std::vector<int> from;
	std::vector<int> to;
	for (const size_t column : after) {
		const std::string& columnName = m_layout.columns[column - kFirstTrackColumn].name;
		const auto old = std::find_if(before.begin(), before.end(), [&](const auto& walk) { return walk.first == columnName; });
		to.push_back(static_cast<int>(GetSubItemWidth(column)));
		from.push_back(columnName == name || old == before.end() ? 0 : old->second);
	}
	// With the columns filling the panel the old widths fill it too; otherwise the others keep their widths anyway.
	if (m_autoSizeColumns && std::accumulate(from.begin(), from.end(), 0) != std::accumulate(to.begin(), to.end(), 0)) {
		RememberColumnState(columnsBefore);
		return;
	}
	SlideColumnWidths(after, from, to, [this, columnsBefore] {
		CaptureColumnWidths();
		RememberColumnState(columnsBefore);
	});
}

// A column that is turned on goes after the last track column with its default width,
// so a cover dragged to the right end stays there.
void PlaylistView::ApplyColumnToggle(const std::string& name) {
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

// The panel's own layout. With columns remembered per playlist that is the layout of playlists never changed, not
// the one a playlist with its own columns shows now.
ColumnLayout PlaylistView::GetColumnLayout() {
	ColumnLayout layout = CapturedLayout();
	if (m_baseLayout) {
		const WidthAt96Dpi coverWidth = layout.coverWidth;
		layout = *m_baseLayout;
		layout.coverWidth = coverWidth;
	}
	return layout;
}

// The user reorders columns by dragging header titles, the cover and info columns too. The header control keeps
// that order apart from the column indices, so the copy lists the track columns as they appear on screen,
// and CaptureColumnWidths reads where the cover and info columns are.
// A width slide still running ends first: a column folding away is taken out then (ToggleColumn), so the copy holds
// the columns as they end up, and the slide's end does not later land on a layout put in place since.
ColumnLayout PlaylistView::CapturedLayout() {
	FinishColumnWidthSlide();
	CaptureColumnWidths();
	ColumnLayout layout = m_layout;
	if (!m_hWnd || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return layout;
	layout.columns.clear();
	for (const int listColumn : GetColumnOrderArray()) {
		if (listColumn >= static_cast<int>(kFirstTrackColumn)) layout.columns.push_back(m_layout.columns[listColumn - kFirstTrackColumn]);
	}
	return layout;
}

// The panel moves a dragged column itself instead of the header's drag image and drop marker: the columns change
// places while the title is dragged, so the order on screen is the one a drop gives (column_drag.h).
// The header reports a title moved past the system's small drag distance; the column waits until the press was held
// long enough or the move is long enough (WaitForColumnDrag), so a click that slips a little still sorts.
LRESULT PlaylistView::OnHeaderBeginDrag(int, LPNMHDR header, BOOL&) {
	if (!m_settings.dragColumns || m_headerDrag != HeaderDrag::None) return TRUE;
	const int column = reinterpret_cast<NMHEADER*>(header)->iItem;
	if (column < 0 || static_cast<size_t>(column) >= GetColumnCount()) return TRUE;
	CPoint pointer;
	::GetCursorPos(&pointer);
	GetHeaderCtrl().ScreenToClient(&pointer);
	m_waitingColumn = column;
	m_headerDrag = HeaderDrag::Waiting;
	WaitForColumnDrag(pointer.x);
	return TRUE;
}

// Called with the pointer while a title waits to move. The time is that of the mouse message being handled.
// The drag starts from where the title was pressed, so the column moves under the pointer from the first step.
void PlaylistView::WaitForColumnDrag(int x) {
	const DWORD held = static_cast<DWORD>(::GetMessageTime()) - m_headerPressTime;
	if (!ColumnDragStarts(held, std::abs(x - m_headerPressX), kColumnDragHoldMs, Scaled(kColumnDragEscape))) return;
	BeginColumnDrag(m_waitingColumn, m_headerPressX);
	MoveColumnDrag(x);
}

void PlaylistView::BeginColumnDrag(int column, int grabX) {
	if (column < 0 || static_cast<size_t>(column) >= GetColumnCount()) {
		m_headerDrag = HeaderDrag::Ended;
		return;
	}
	std::vector<int> widths(GetColumnCount());
	for (size_t walk = 0; walk < widths.size(); ++walk) widths[walk] = static_cast<int>(GetSubItemWidth(walk));
	// Without animation the columns only change places: the dragged one is not lifted, and the others neither give way
	// nor slide.
	const bool animate = AnimateColumnMoves();
	ColumnDragFeel feel;
	feel.swapDepth = kColumnSwapDepth;
	feel.slack = Scaled(kColumnSwapSlack);
	if (animate) {
		feel.resistShare = kColumnResistShare;
		feel.resistMax = Scaled(kColumnResistMax);
	}
	// The panel takes the keyboard, so Ctrl+Z after the drop reaches it (RunColumnUndoKey).
	SetFocus();
	m_columnsBeforeDrag = CaptureColumnState();
	m_columnDrag.emplace(GetColumnOrderArray(), std::move(widths), column, grabX, feel);
	m_headerDrag = HeaderDrag::Dragging;
	if (animate) m_columnSlides.emplace(GetColumnCount(), kColumnSlideMs, kColumnSlideOvershoot);
	else m_columnSlides.reset();
	DrawColumnFrame();
}

void PlaylistView::MoveColumnDrag(int x) {
	if (!m_columnDrag) return;
	const int count = static_cast<int>(m_columnDrag->Order().size());
	const double now = NowMs();
	std::vector<int> leftBefore(count);
	std::vector<int> givenBefore(count);
	std::vector<double> drawnBefore(count);
	for (int column = 0; column < count; ++column) {
		leftBefore[column] = m_columnDrag->LeftOf(column);
		givenBefore[column] = m_columnDrag->Resistance(column);
		drawnBefore[column] = leftBefore[column] + givenBefore[column] + (m_columnSlides ? m_columnSlides->Offset(column, now) : 0);
	}
	if (!m_columnDrag->MoveTo(x)) {
		if (m_columnSlides) DrawColumnFrame();
		return;
	}
	// Each column that changed places, or now gives way by another amount, goes on being drawn where it was and
	// slides from there.
	if (m_columnSlides) {
		for (int column = 0; column < count; ++column) {
			if (column == m_columnDrag->Dragged()) continue;
			const int left = m_columnDrag->LeftOf(column);
			const int given = m_columnDrag->Resistance(column);
			if (left != leftBefore[column] || given != givenBefore[column]) {
				m_columnSlides->StartFrom(column, drawnBefore[column] - left - given, now);
			}
		}
		StartColumnClock();
	}
	ShowColumnOrder(m_columnDrag->Order());
}

// keep: the order on screen stays (a drop). Otherwise the columns go back to where they were (the mouse lost). A drop
// is undone with Ctrl+Z instead of an Esc during the drag.
// The dragged column slides from under the pointer into its place, and on a cancel every column slides back.
void PlaylistView::EndColumnDrag(bool keep) {
	m_headerDrag = HeaderDrag::Ended;
	if (!m_columnDrag) return;
	const ColumnDrag drag = std::move(*m_columnDrag);
	m_columnDrag.reset();
	const std::vector<int>& start = drag.StartOrder();
	const bool moved = drag.Order() != start;
	if (m_columnSlides) {
		const double now = NowMs();
		const int dragged = drag.Dragged();
		for (int column = 0; column < static_cast<int>(drag.Order().size()); ++column) {
			const int given = column == dragged ? 0 : drag.Resistance(column);
			const double drawn = column == dragged ? drag.DraggedLeft() : drag.LeftOf(column) + given + m_columnSlides->Offset(column, now);
			const int home = keep ? drag.LeftOf(column) : drag.LeftIn(start, column);
			if (column == dragged || given != 0 || !keep) m_columnSlides->StartFrom(column, drawn - home, now);
		}
		StartColumnClock();
	}
	if (!keep && moved) ShowColumnOrder(start);
	else DrawColumnFrame();
	if (std::optional<ColumnState> before = std::exchange(m_columnsBeforeDrag, std::nullopt); before && keep && moved) {
		RememberColumnState(std::move(*before));
	}
}

// Ctrl+Z puts the columns back as they were before the latest change of their order, widths or set; Ctrl+Y makes it
// again. Without such a step the keys go on to the playlist's undo and redo.
bool PlaylistView::RunColumnUndoKey(WPARAM key) {
	if (GetHotkeyModifierFlags() != MOD_CONTROL || m_headerDrag != HeaderDrag::None) return false;
	if (key == 'Z') return StepColumnState(m_columnUndo, m_columnRedo);
	if (key == 'Y') return StepColumnState(m_columnRedo, m_columnUndo);
	return false;
}

PlaylistView::ColumnState PlaylistView::CaptureColumnState() {
	ColumnState state;
	state.layout = CapturedLayout();
	for (const ColumnLayout::Column& column : m_layout.columns) state.listNames.push_back(column.name);
	if (m_hWnd) {
		state.order = GetColumnOrderArray();
		for (size_t column = 0; column < GetColumnCount(); ++column) state.widths.push_back(static_cast<int>(GetSubItemWidth(column)));
	}
	return state;
}

// Called after a change of the columns with how they were before it. A change that left them as they were is not kept.
void PlaylistView::RememberColumnState(ColumnState before) {
	if (!m_hWnd) return;
	const ColumnState now = CaptureColumnState();
	if (before.listNames == now.listNames && before.order == now.order && before.widths == now.widths) return;
	m_columnUndo.push_back(std::move(before));
	if (m_columnUndo.size() > kColumnUndoSteps) m_columnUndo.erase(m_columnUndo.begin());
	m_columnRedo.clear();
}

bool PlaylistView::StepColumnState(std::vector<ColumnState>& from, std::vector<ColumnState>& to) {
	if (from.empty() || !m_hWnd) return false;
	FinishColumnWidthSlide();
	const ColumnState state = std::move(from.back());
	from.pop_back();
	to.push_back(CaptureColumnState());
	RestoreColumnState(state);
	return true;
}

// The same columns slide back to their order and widths. A column shown or hidden since needs the columns made again.
void PlaylistView::RestoreColumnState(const ColumnState& state) {
	std::vector<std::string> listNames;
	for (const ColumnLayout::Column& column : m_layout.columns) listNames.push_back(column.name);
	if (state.listNames != listNames || state.order.size() != GetColumnCount() || state.widths.size() != GetColumnCount()) {
		// The cover column is as wide as the album art, a setting of all panels that may have changed since, so it keeps
		// its width now (as EnterPlaylistColumns does).
		ColumnLayout layout = state.layout;
		layout.coverWidth = CapturedLayout().coverWidth;
		SetColumnLayout(layout);
		return;
	}
	// The saved proportions come back with the order, as the columns fill the panel by them.
	for (ColumnLayout::Column& column : m_layout.columns) {
		for (const ColumnLayout::Column& saved : state.layout.columns) {
			if (saved.name == column.name) column.width = saved.width;
		}
	}
	if (state.order != GetColumnOrderArray()) SlideToColumnOrder(state.order);
	const std::vector<size_t> onScreen = TrackColumnsOnScreen();
	std::vector<int> from;
	std::vector<int> to;
	for (const size_t column : onScreen) {
		from.push_back(static_cast<int>(GetSubItemWidth(column)));
		to.push_back(state.widths[column]);
	}
	SlideColumnWidths(onScreen, from, to, [this] {
		CaptureColumnWidths();
		FitTrackColumns();
	});
}

// The columns slide from where they are to their places in the new order, as after a drop.
void PlaylistView::SlideToColumnOrder(const std::vector<int>& order) {
	const std::vector<int> current = GetColumnOrderArray();
	if (AnimateColumnMoves()) {
		const auto lefts = [&](const std::vector<int>& sequence) {
			std::vector<int> left(sequence.size(), 0);
			int x = 0;
			for (const int column : sequence) {
				if (column < 0 || static_cast<size_t>(column) >= left.size()) continue;
				left[static_cast<size_t>(column)] = x;
				x += static_cast<int>(GetSubItemWidth(static_cast<size_t>(column)));
			}
			return left;
		};
		const std::vector<int> before = lefts(current);
		const std::vector<int> after = lefts(order);
		const double now = NowMs();
		m_columnSlides.emplace(current.size(), kColumnSlideMs, kColumnSlideOvershoot);
		for (size_t column = 0; column < current.size(); ++column) {
			if (before[column] != after[column]) m_columnSlides->StartFrom(column, before[column] - after[column], now);
		}
		StartColumnClock();
	}
	ShowColumnOrder(order);
}

// The same change of order as libPPUI's own drop (CListControlHeaderImpl::OnColumnHeaderDrag), drawn at once,
// because Windows does not paint while the mouse keeps moving.
void PlaylistView::ShowColumnOrder(const std::vector<int>& order) {
	GetHeaderCtrl().SetOrderArray(static_cast<int>(order.size()), const_cast<int*>(order.data()));
	OnColumnsChanged();
	DrawColumnFrame();
}

double PlaylistView::NowMs() {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The panel's own setting decides, not Windows' "Animation effects", which is off on many computers that want
// windows to open at once.
bool PlaylistView::AnimateColumnMoves() const {
	return m_settings.animateColumnMoves;
}

// Whether the columns are drawn anywhere but in their places: a lifted column, or columns still sliding.
bool PlaylistView::ColumnsMoving() const {
	if (!m_columnSlides) return false;
	return m_columnDrag.has_value() || m_columnSlides->Running(m_columnFrameMs);
}

std::optional<size_t> PlaylistView::LiftedColumn() const {
	if (!m_columnDrag || !m_columnSlides) return std::nullopt;
	return static_cast<size_t>(m_columnDrag->Dragged());
}

// How far from its place a column is drawn in the frame being drawn.
int PlaylistView::ColumnOffset(size_t column) const {
	if (!m_columnSlides) return 0;
	if (m_columnDrag && static_cast<int>(column) == m_columnDrag->Dragged()) {
		return m_columnDrag->DraggedLeft() - m_columnDrag->LeftOf(static_cast<int>(column));
	}
	const int given = m_columnDrag ? m_columnDrag->Resistance(static_cast<int>(column)) : 0;
	return given + static_cast<int>(std::lround(m_columnSlides->Offset(column, m_columnFrameMs)));
}

// Draws the frame at once, as the cover column drag does (Windows does not paint while the mouse keeps moving).
// After the last frame of a slide the clock stops and the columns are drawn as usual again.
void PlaylistView::DrawColumnFrame() {
	m_columnFrameMs = NowMs();
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	if (!ColumnsMoving()) {
		m_columnClock.Pause();
		if (!m_columnDrag) m_columnSlides.reset();
	}
}

// The slides are drawn once per display refresh, on the thread that times smooth scrolling (smooth_scroll.h).
void PlaylistView::StartColumnClock() {
	const HWND wnd = m_hWnd;
	m_columnClock.Start([wnd] { SendMessageTimeout(wnd, kColumnSlideMessage, 0, 0, SMTO_BLOCK, 50, nullptr); });
}

LRESULT PlaylistView::OnColumnSlideFrame(UINT, WPARAM, LPARAM, BOOL&) {
	DrawColumnFrame();
	return 0;
}

void PlaylistView::StartEffectClock() {
	const HWND wnd = m_hWnd;
	if (wnd) m_effectClock.Start([wnd] { SendMessageTimeout(wnd, kEffectFrameMessage, 0, 0, SMTO_BLOCK, 50, nullptr); });
}

// One frame of every animation that runs. Painting a fading cover or a popping cell asks for the next frame again
// (RenderCover, PopScaleOf), so the clock stops once a frame has nothing left to move.
LRESULT PlaylistView::OnEffectFrame(UINT, WPARAM, LPARAM, BOOL&) {
	const double now = NowMs();
	bool running = false;
	if (m_columnWidthSlide) {
		if (m_columnWidthSlide->slide.Done(now)) {
			FinishColumnWidthSlide();
		} else {
			ShowTrackWidths(m_columnWidthSlide->columns, m_columnWidthSlide->slide.At(now));
			running = true;
		}
	}
	if (m_coversFading) {
		m_coversFading = false;
		Invalidate(FALSE);
		running = true;
	}
	if (m_pop) {
		if (m_pop->item < GetItemCount()) InvalidateRect(GetSubItemRect(m_pop->item, m_pop->subItem), FALSE);
		if (now - m_pop->startMs >= kPopMs) m_pop.reset();
		running = true;
	}
	if (running) UpdateWindow();
	else m_effectClock.Pause();
	return 0;
}

// Slides the widths of these list columns from from to to, then runs done. With nothing to slide, or no window, the
// widths are set and done runs at once. A slide still running finishes first.
void PlaylistView::SlideColumnWidths(const std::vector<size_t>& columns, const std::vector<int>& from, const std::vector<int>& to, std::function<void()> done) {
	FinishColumnWidthSlide();
	if (from == to || !m_hWnd) {
		ShowTrackWidths(columns, to);
		if (done) done();
		return;
	}
	m_columnWidthSlide.emplace(ColumnWidthSlide{ columns, WidthSlide(from, to, NowMs(), kWidthSlideMs), std::move(done) });
	ShowTrackWidths(columns, from);
	StartEffectClock();
}

// Jumps a running width slide to its end. Anything that changes the columns another way calls this first.
void PlaylistView::FinishColumnWidthSlide() {
	if (!m_columnWidthSlide) return;
	ColumnWidthSlide slide = std::move(*m_columnWidthSlide);
	m_columnWidthSlide.reset();
	if (m_hWnd && GetColumnCount() > 0) ShowTrackWidths(slide.columns, slide.slide.To());
	if (slide.done) slide.done();
}

// Draws with draw on a copy of what is under rect, then lays that over at the given opacity, so what is under it shows
// through. draw gets a DC with the same coordinates, font and colours as dc, clipped to rect. rect is only the part
// being painted: a lifted cover column is as tall as its whole group, which can be far more than the screen.
template <typename Draw>
static void DrawSeeThrough(CDCHandle dc, const CRect& rect, BYTE opacity, Draw&& draw) {
	CDC memory;
	memory.CreateCompatibleDC(dc);
	CBitmap bitmap;
	if (memory) bitmap.CreateCompatibleBitmap(dc, rect.Width(), rect.Height());
	if (!memory || !bitmap) {
		draw(dc);
		return;
	}
	{
		SelectObjectScope bitmapScope(memory, bitmap);
		memory.SetViewportOrg(-rect.left, -rect.top);
		memory.BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), dc, rect.left, rect.top, SRCCOPY);
		memory.IntersectClipRect(rect);
		SelectObjectScope fontScope(memory, dc.GetCurrentFont());
		memory.SetTextColor(dc.GetTextColor());
		memory.SetBkColor(dc.GetBkColor());
		memory.SetBkMode(dc.GetBkMode());
		draw(CDCHandle(memory));
		memory.SetViewportOrg(0, 0);
		const BLENDFUNCTION blend = { AC_SRC_OVER, 0, opacity, 0 };
		dc.AlphaBlend(rect.left, rect.top, rect.Width(), rect.Height(), memory, 0, 0, rect.Width(), rect.Height(), blend);
	}
}

// How much of the lifted column covers what is under it, out of 255.
constexpr BYTE kLiftedColumnOpacity = 180;

// While columns move, each column of a row is drawn where its column is drawn (ColumnOffset), the lifted one last,
// see-through, on a slightly darker strip with an edge on each side, so it reads as picked up. Where it came from
// stays empty.
void PlaylistView::RenderItemText(t_size item, const CRect& itemRect, const CRect& updateRect, CDCHandle dc, bool allowColors) {
	if (!ColumnsMoving()) {
		__super::RenderItemText(item, itemRect, updateRect, dc, allowColors);
		return;
	}
	SelectObjectScope fontScope(dc, GetFont());
	const std::optional<size_t> lifted = LiftedColumn();
	const auto drawColumn = [&](size_t column, int left) {
		const CRect rect(left, itemRect.top, left + static_cast<int>(GetSubItemWidth(column)), itemRect.bottom);
		CRect update;
		if (rect.Width() <= 0 || !update.IntersectRect(rect, updateRect)) return;
		DCStateScope scope(dc);
		if (dc.IntersectClipRect(rect) == NULLREGION) return;
		if (!lifted || column != *lifted) {
			RenderSubItemText(item, column, rect, update, dc, allowColors);
			return;
		}
		DrawSeeThrough(dc, update, kLiftedColumnOpacity, [&](CDCHandle target) {
			// BlendColor's percentage is the share of its second colour.
			const COLORREF fill = target.GetBkColor();
			const COLORREF text = GetSysColorHook(colorText);
			target.FillSolidRect(rect, PaintUtils::BlendColor(fill, text, 6));
			const COLORREF edge = PaintUtils::BlendColor(fill, text, 30);
			target.FillSolidRect(rect.left, rect.top, 1, rect.Height(), edge);
			target.FillSolidRect(rect.right - 1, rect.top, 1, rect.Height(), edge);
			RenderSubItemText(item, column, rect, update, target, allowColors);
		});
	};
	int x = itemRect.left;
	int liftedPlace = x;
	for (const int column : GetColumnOrderArray()) {
		if (lifted && static_cast<size_t>(column) == *lifted) liftedPlace = x;
		else drawColumn(static_cast<size_t>(column), x + ColumnOffset(static_cast<size_t>(column)));
		x += static_cast<int>(GetSubItemWidth(static_cast<size_t>(column)));
	}
	if (lifted) drawColumn(*lifted, liftedPlace + ColumnOffset(*lifted));
}

// The covers and their text move with their columns as the rows do (RenderItemText).
void PlaylistView::RenderGroupOverlay(size_t baseItem, const CRect& groupWhole, const CRect& updateRect, CDCHandle dc) {
	if (!ColumnsMoving()) {
		__super::RenderGroupOverlay(baseItem, groupWhole, updateRect, dc);
		return;
	}
	const std::optional<size_t> lifted = LiftedColumn();
	const auto drawColumn = [&](size_t column, int left) {
		const CRect rect(left, groupWhole.top, left + static_cast<int>(GetSubItemWidth(column)), groupWhole.bottom);
		CRect update;
		if (rect.Width() <= 0 || !update.IntersectRect(rect, updateRect)) return;
		DCStateScope scope(dc);
		if (dc.IntersectClipRect(rect) == NULLREGION) return;
		if (lifted && column == *lifted) {
			DrawSeeThrough(dc, update, kLiftedColumnOpacity, [&](CDCHandle target) { RenderGroupOverlayColumn(baseItem, column, rect, update, target); });
		} else {
			RenderGroupOverlayColumn(baseItem, column, rect, update, dc);
		}
	};
	int x = groupWhole.left;
	int liftedPlace = x;
	for (const int column : GetColumnOrderArray()) {
		if (lifted && static_cast<size_t>(column) == *lifted) liftedPlace = x;
		else drawColumn(static_cast<size_t>(column), x + ColumnOffset(static_cast<size_t>(column)));
		x += static_cast<int>(GetSubItemWidth(static_cast<size_t>(column)));
	}
	if (lifted) drawColumn(*lifted, liftedPlace + ColumnOffset(*lifted));
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

// "Show column headers" off: the header control stays, because it holds the column widths and order, and is only
// hidden. The list control lays the header out by asking it (HDM_LAYOUT) and hides it and the line below it when
// the answer is no, on every resize, scroll, font and theme change. So the answer is given here, in one place.
LRESULT CALLBACK PlaylistView::HeaderProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR view) {
	PlaylistView* const owner = reinterpret_cast<PlaylistView*>(view);
	if (message == HDM_LAYOUT && !owner->m_settings.showColumnHeaders) return FALSE;
	if (owner->ColumnsMoving()) {
		if (message == WM_PAINT) {
			owner->PaintMovingHeader(window);
			return 0;
		}
		if (message == WM_ERASEBKGND) return TRUE;
	}
	// A title waiting to move (WaitForColumnDrag) counts from the press.
	if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
		owner->m_headerPressTime = static_cast<DWORD>(::GetMessageTime());
		owner->m_headerPressX = static_cast<short>(LOWORD(lParam));
	}
	// A column drag the panel took over (OnHeaderBeginDrag). The header keeps the mouse until the button comes up.
	if (owner->m_headerDrag != HeaderDrag::None) {
		switch (message) {
		case WM_MOUSEMOVE:
			if (owner->m_headerDrag == HeaderDrag::Waiting) owner->WaitForColumnDrag(static_cast<short>(LOWORD(lParam)));
			else if (owner->m_headerDrag == HeaderDrag::Dragging) owner->MoveColumnDrag(static_cast<short>(LOWORD(lParam)));
			return 0;
		case WM_LBUTTONUP: {
			// Let go before the column moved: the header reports the click, which sorts as any click does.
			if (owner->m_headerDrag == HeaderDrag::Waiting) {
				owner->m_headerDrag = HeaderDrag::None;
				break;
			}
			if (owner->m_headerDrag == HeaderDrag::Dragging) owner->EndColumnDrag(true);
			// The header lets go of the mouse and reports a click, which OnColumnHeaderClick ignores while this is set.
			const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
			owner->m_headerDrag = HeaderDrag::None;
			return result;
		}
		case WM_CAPTURECHANGED:
			if (owner->m_headerDrag == HeaderDrag::Dragging) owner->EndColumnDrag(false);
			owner->m_headerDrag = HeaderDrag::None;
			break;
		}
	}
	if (message == WM_NCDESTROY) RemoveWindowSubclass(window, HeaderProc, id);
	return DefSubclassProc(window, message, wParam, lParam);
}

// The list control leaves room for the header by its size even while it is hidden, so a hidden header gives none.
CRect PlaylistView::GetClientRectHook() const {
	return m_settings.showColumnHeaders ? __super::GetClientRectHook() : CListControlImpl::GetClientRectHook();
}

// The layout without the names that have no column definition, so a layout saved with a column that no longer exists
// still loads. The cover and info columns stay beside the same track columns.
ColumnLayout PlaylistView::WithKnownColumns(ColumnLayout layout) const {
	auto& columns = layout.columns;
	std::vector<bool> kept;
	for (const ColumnLayout::Column& column : columns) kept.push_back(FindColumnDef(column.name) != nullptr);
	columns.erase(std::remove_if(columns.begin(), columns.end(), [this](const ColumnLayout::Column& column) { return !FindColumnDef(column.name); }), columns.end());
	// Normalized as the header will show them (RebuildColumns), so the result compares equal to the layout shown.
	layout.places = NormalizePlaces(columns.size(), PlacesAfterTrackChange(layout.places, kept, 0));
	return layout;
}

// A width slide still running ends on the columns it began on, before the new layout takes their place.
void PlaylistView::SetColumnLayout(const ColumnLayout& layout) {
	FinishColumnWidthSlide();
	m_layout = WithKnownColumns(layout);
	if (m_hWnd) RebuildColumns();
}

// While the window exists the header control owns the widths, because the user drags them there.
// While the columns fill the panel, their saved widths are the proportions, and the screen shows them
// rounded to the panel's width. Reading them back on every resize would let the rounding pile up.
void PlaylistView::CaptureColumnWidths() {
	CaptureColumnWidths(!m_autoSizeColumns);
}

void PlaylistView::CaptureColumnWidths(bool readTrackWidths) {
	if (!m_hWnd || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return;
	m_layout.places = PlacesFromOrder(GetColumnOrderArray());
	// Without groups the cover and info columns are hidden, and their widths are kept for when groups come back.
	if (!m_ungrouped) {
		m_layout.coverWidth = ColumnWidthAt96Dpi(kCoverColumn);
		if (m_blockLineCount > 0) m_layout.infoWidth = ColumnWidthAt96Dpi(kInfoColumn);
	}
	if (readTrackWidths) SaveTrackColumnWeights();
}

void PlaylistView::SaveTrackColumnWeights() {
	for (size_t index = 0; index < m_layout.columns.size(); ++index) {
		m_layout.columns[index].width = ColumnWidthAt96Dpi(kFirstTrackColumn + index);
	}
}

void PlaylistView::SetAutoSizeColumns(bool on) {
	if (on == m_autoSizeColumns) return;
	// Either way the widths on screen are kept: as the proportions when it turns on, as they are when it turns off.
	CaptureColumnWidths(true);
	m_autoSizeColumns = on;
	FitTrackColumns();
}

std::vector<size_t> PlaylistView::TrackColumnsOnScreen() const {
	std::vector<size_t> columns;
	for (const int listColumn : GetColumnOrderArray()) {
		if (listColumn >= static_cast<int>(kFirstTrackColumn)) columns.push_back(static_cast<size_t>(listColumn));
	}
	return columns;
}

// The space beside the cover and info columns goes to the track columns in proportion to their saved widths.
// A panel narrower than the cover and info columns leaves the track columns no width until it is widened again.
// Returns whether a width changed. Without updateView the caller updates the view.
bool PlaylistView::FitTrackColumns(bool updateView) {
	if (!m_autoSizeColumns || !m_hWnd || m_layout.columns.empty() || GetColumnCount() != kFirstTrackColumn + m_layout.columns.size()) return false;
	const int available = GetClientRectHook().Width() - static_cast<int>(GetSubItemWidth(kCoverColumn) + GetSubItemWidth(kInfoColumn));
	std::vector<float> weights;
	for (const ColumnLayout::Column& column : m_layout.columns) weights.push_back(column.width.value);
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
	if (changed && updateView) OnColumnsChanged();
	return changed;
}

void PlaylistView::RebuildColumns() {
	FinishColumnWidthSlide();
	// The star preview and the pop name their cell by column number, which may now belong to another column or to none.
	m_starHover.MoveTo(std::nullopt);
	m_pop.reset();
	ResetColumns(false);
	m_listColumns.clear();
	auto compiler = titleformat_compiler::get();

	AddColumnF("", m_ungrouped ? 0.f : m_layout.coverWidth.value, HDF_LEFT, false);
	AddColumnF("", !m_ungrouped && m_blockLineCount > 0 ? m_layout.infoWidth.value : 0.f, HDF_LEFT, false);
	m_listColumns.resize(kFirstTrackColumn);
	for (const ColumnLayout::Column& column : m_layout.columns) {
		const ColumnDef* def = FindColumnDef(column.name);
		AddColumnF(ColumnTitle(*def).c_str(), column.width.value, HeaderFormat(def->alignment), false);
		ListColumn listColumn;
		listColumn.kind = def->kind;
		if (def->kind == ColumnKind::Rating) {
			compiler->compile_safe(listColumn.script, RatingReadPattern(GetRatingStorage()));
		} else if (def->kind == ColumnKind::Like) {
			compiler->compile_safe(listColumn.script, LikeReadPattern(GetLikeStorage()));
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

const char* PlaylistView::SortPattern(const ColumnDef& def) {
	switch (def.kind) {
	case ColumnKind::Rating: return RatingReadPattern(GetRatingStorage());
	case ColumnKind::Like: return LikeReadPattern(GetLikeStorage());
	default: return def.pattern.c_str();
	}
}

// A click on a track column header sorts the playlist by that column. A second click on the same
// column reverses the order. A click on the leftmost header, above the covers, puts the playlist back
// in the order of the active preset, as in the original.
void PlaylistView::OnColumnHeaderClick(t_size listColumn) {
	// The header reports a drag the panel took over as a click when the button comes up (OnHeaderBeginDrag).
	if (m_headerDrag != HeaderDrag::None) return;
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
	SortPlaylist(SortPattern(*def), descending);
	// The arrow shows which way the click sorted, then goes: the playlist is not kept sorted, so a lasting arrow would mislead.
	SetSortIndicator(listColumn, !descending);
	SetTimer(kSortIndicatorTimer, kSortIndicatorMs);
}

LRESULT PlaylistView::OnSortIndicatorTimer(UINT, WPARAM timer, LPARAM, BOOL& handled) {
	if (timer != kSortIndicatorTimer) {
		handled = FALSE;
		return 0;
	}
	HideSortIndicator();
	return 0;
}

void PlaylistView::HideSortIndicator() {
	if (!m_hWnd) return;
	KillTimer(kSortIndicatorTimer);
	if (IsHeaderEnabled()) ClearSortIndicator();
}

// The playlist is no longer in the order of the last header click, so its arrow goes too.
void PlaylistView::ForgetHeaderSort() {
	m_headerSort.Forget();
	HideSortIndicator();
}

namespace {

// Properties kept with each playlist, so they follow the playlist across restarts.
constexpr GUID guid_playlist_preset = { 0x41e6b601, 0x93da, 0x4c7d, { 0xa9, 0xa8, 0x22, 0x31, 0xb2, 0x2b, 0x3b, 0x7f } };
constexpr GUID guid_playlist_scroll = { 0x3a4e2d8c, 0x430b, 0x4a2d, { 0xa1, 0xdc, 0xb8, 0xc2, 0x23, 0xe8, 0xd6, 0x11 } };
constexpr GUID guid_playlist_columns = { 0x8d3f1b62, 0x5e47, 0x4c0a, { 0xb9, 0x21, 0x6a, 0xe4, 0x0c, 0x97, 0x3d, 0x58 } };

} // namespace

// The columns a playlist remembers (SerializeColumnLayout), or nothing for one that keeps the panel's.
std::optional<ColumnLayout> PlaylistView::PlaylistColumns(t_size playlist) {
	pfc::array_t<t_uint8> data;
	if (!playlist_manager_v2::get()->playlist_get_property(playlist, guid_playlist_columns, data)) return std::nullopt;
	return ParseColumnLayout(std::string(reinterpret_cast<const char*>(data.get_ptr()), data.get_size()));
}

void PlaylistView::SetPlaylistColumns(t_size playlist, const ColumnLayout& layout) {
	const std::string text = SerializeColumnLayout(layout);
	pfc::array_t<t_uint8> data;
	data.set_data_fromptr(reinterpret_cast<const t_uint8*>(text.data()), text.size());
	playlist_manager_v2::get()->playlist_set_property(playlist, guid_playlist_columns, data);
}

// Columns per playlist (ViewSettings::columnsPerPlaylist). The panel's layout is the base, which playlists never
// changed show. A playlist that remembers its own columns shows them. What the user changes while a playlist is
// shown belongs to that playlist alone, and is stored with it when the panel leaves it (LeavePlaylistColumns).
void PlaylistView::EnterPlaylistColumns() {
	m_layoutOnEnter.reset();
	if (!m_settings.columnsPerPlaylist || !m_hWnd) return;
	auto playlists = playlist_manager_v5::get();
	const t_size playlist = playlists->get_active_playlist();
	if (playlist == pfc_infinite) return;
	if (!m_baseLayout) m_baseLayout = CapturedLayout();
	const std::optional<ColumnLayout> own = PlaylistColumns(playlist);
	ColumnLayout wanted = own ? *own : *m_baseLayout;
	// The cover column is as wide as the album art, a setting of all panels, so it does not go with a playlist.
	const ColumnLayout shown = CapturedLayout();
	wanted.coverWidth = shown.coverWidth;
	if (!SameColumnLayout(shown, wanted)) SetColumnLayout(wanted);
	m_columnsPlaylist = playlists->playlist_get_guid(playlist);
	m_layoutOnEnter = CapturedLayout();
}

// A change since the panel came to the playlist is stored with it. An untouched layout stores nothing, so a
// playlist only looked at goes on following the panel's columns.
void PlaylistView::LeavePlaylistColumns() {
	const std::optional<ColumnLayout> entered = std::exchange(m_layoutOnEnter, std::nullopt);
	if (!entered || !m_hWnd) return;
	ColumnLayout now = CapturedLayout();
	now.coverWidth = entered->coverWidth;
	if (SameColumnLayout(now, *entered)) return;
	const t_size playlist = playlist_manager_v5::get()->find_playlist_by_guid(m_columnsPlaylist);
	if (playlist != pfc_infinite) SetPlaylistColumns(playlist, now);
}

// The setting turned on: the shown layout becomes the base. Turned off: a pending change is stored, and the panel
// goes on with the columns it shows.
void PlaylistView::SyncColumnsPerPlaylist() {
	if (m_settings.columnsPerPlaylist == m_layoutOnEnter.has_value()) return;
	if (m_settings.columnsPerPlaylist) {
		m_baseLayout.reset();
		EnterPlaylistColumns();
	} else {
		LeavePlaylistColumns();
		m_baseLayout.reset();
	}
}

void PlaylistView::ChoosePreset(size_t index) {
	if (index >= m_presets.size()) return;
	SelectPreset(index);
	RememberPlaylistGrouping(m_presets[index].name);
	if (m_headerSort.SortedByColumn(ActivePlaylistGuid())) SortByActivePreset();
}

void PlaylistView::ChooseUngrouped() {
	SetUngrouped();
	RememberPlaylistGrouping(kUngroupedPresetMark);
}

// The playlist property holds the preset name, or kUngroupedPresetMark for "None".
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
	if (name == kUngroupedPresetMark) {
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
	return BlockInColumnAt(clientPoint, kCoverColumn);
}

std::optional<size_t> PlaylistView::InfoBlockAt(CPoint clientPoint) const {
	if (m_blockLineCount == 0) return std::nullopt;
	return BlockInColumnAt(clientPoint, kInfoColumn);
}

std::optional<size_t> PlaylistView::BlockInColumnAt(CPoint clientPoint, size_t column) const {
	size_t header = 0;
	if (GroupHeaderFromPoint2(clientPoint, header) || m_groups.ItemCount() == 0) return std::nullopt;
	const int y = PointClientToAbs(clientPoint).y;
	const size_t item = IndexFromPointAbs(y);
	if (item >= m_groups.ItemCount() || y < GetItemOffsetAbs(item)) return std::nullopt;
	const CRect cell = GetSubItemRect(item, column);
	if (clientPoint.x < cell.left || clientPoint.x >= cell.right) return std::nullopt;
	return m_groups.BlockIndexOf(item);
}

// Selects the tracks of a cover block and puts the focus on its first track, which it returns.
size_t PlaylistView::SelectCoverBlock(size_t block) {
	const size_t first = m_groups.BlockAt(block).firstItem;
	SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, m_groups.BlockSize(block)));
	SetFocusItem(first);
	return first;
}

// With Ctrl, a click on a label row or a cover takes its tracks into the selection or out of it; with Shift it
// adds them. An album header does the same in the list control. False without either key.
bool PlaylistView::SelectPartWithModifiers(WPARAM keys, size_t first, size_t count) {
	if ((keys & (MK_CONTROL | MK_SHIFT)) == 0 || count == 0) return false;
	const pfc::bit_array_range range(first, count);
	if ((keys & MK_CONTROL) != 0) SetSelection(range, pfc::bit_array_val(GetSelectedCount(range, 1) == 0));
	else SetSelection(range, pfc::bit_array_true());
	SetFocusItem(first);
	return true;
}

// A press on a cover selects the tracks under that cover, and a drag from there moves them (OnCoverMouseMove).
// The list control cannot start that drag: much of a cover lies below its track rows, where it sees no row.
// A selection that already holds the cover's tracks stays for the drag, so several covers move together.
LRESULT PlaylistView::OnCoverButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled) {
	const CPoint point(position);
	const std::optional<size_t> block = CoverBlockAt(point);
	if (!block) {
		handled = FALSE;
		return 0;
	}
	SetFocus();
	const size_t first = m_groups.BlockAt(*block).firstItem;
	const size_t count = m_groups.BlockSize(*block);
	if (SelectPartWithModifiers(keys, first, count)) return 0;
	SelectForMenu(first, count);
	SetFocusItem(first);
	m_coverPress = CoverPress{ point, first, count };
	SetCapture();
	return 0;
}

// Released without a drag: a click, which leaves only this cover's tracks selected.
LRESULT PlaylistView::OnCoverButtonUp(UINT, WPARAM, LPARAM, BOOL& handled) {
	handled = FALSE;
	if (!m_coverPress) return 0;
	const CoverPress press = *m_coverPress;
	m_coverPress.reset();
	if (GetCapture() == m_hWnd) ReleaseCapture();
	if (press.first + press.count <= GetItemCount()) SetSelection(pfc::bit_array_true(), pfc::bit_array_range(press.first, press.count));
	return 0;
}

LRESULT PlaylistView::OnCoverMouseMove(UINT, WPARAM buttons, LPARAM position, BOOL& handled) {
	handled = FALSE;
	if (!m_coverPress) return 0;
	if ((buttons & MK_LBUTTON) == 0) {
		m_coverPress.reset();
		return 0;
	}
	const CPoint point(position);
	const CPoint origin = m_coverPress->point;
	if (std::abs(point.x - origin.x) <= GetSystemMetrics(SM_CXDRAG) && std::abs(point.y - origin.y) <= GetSystemMetrics(SM_CYDRAG)) return 0;
	m_coverPress.reset();
	if (GetCapture() == m_hWnd) ReleaseCapture();
	m_coverDragActive = true;
	RunDragDrop(PointClientToAbs(origin), false);
	m_coverDragActive = false;
	handled = TRUE;
	return 0;
}

// For a cover that changed on disk, or one that failed to load.
void PlaylistView::ReloadCover(size_t block) {
	metadb_handle_ptr track;
	if (block >= m_groups.BlockCount() || !m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return;
	m_covers.Forget(track);
	Invalidate();
}

bool PlaylistView::BlockHasNoCover(size_t block) const {
	metadb_handle_ptr track;
	if (block >= m_groups.BlockCount() || !m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return false;
	return m_covers.HasNoCover(track);
}

void PlaylistView::ReloadAllCovers() {
	m_covers.Clear();
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
	ForgetHeaderSort();
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
	LoadInfoLines();
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
		if (coverShown) m_layout.coverWidth = ColumnWidthAt96Dpi(kCoverColumn);
		ResizeColumn(kCoverColumn, m_ungrouped ? 0 : ScreenWidth(m_layout.coverWidth));
	}
	ShowInfoColumn(!m_ungrouped && m_blockLineCount > 0);
	FitTrackColumns();
}

void PlaylistView::ShowInfoColumn(bool show) {
	const bool shown = GetSubItemWidth(kInfoColumn) > 0;
	if (show == shown) return;
	if (shown) m_layout.infoWidth = ColumnWidthAt96Dpi(kInfoColumn);
	ResizeColumn(kInfoColumn, show ? ScreenWidth(m_layout.infoWidth) : 0);
}

bool PlaylistView::SelectPresetByName(const char* name) {
	const std::optional<size_t> index = FindPresetByName(m_presets, name);
	if (index) SelectPreset(*index);
	return index.has_value();
}

void PlaylistView::ApplyGroupPresets(const std::vector<GroupPreset>& presets, const std::vector<std::pair<std::string, std::string>>& renames) {
	SaveGroupPresets(presets);
	SaveInfoLineChoices(InfoLinesAfterPresetChange(LoadInfoLineChoices(), presets, renames));
	for (PlaylistView* view : LiveViews()) view->OnGroupPresetsChanged(renames);
}

void PlaylistView::SetPresetInfoLines(const std::string& presetName, const std::optional<std::vector<std::string>>& lines) {
	InfoLineChoices choices = LoadInfoLineChoices();
	SetPickedInfoLines(choices, presetName, lines);
	ApplyInfoLineChoices(choices);
}

void PlaylistView::ApplyInfoLineChoices(const InfoLineChoices& choices) {
	SaveInfoLineChoices(choices);
	for (PlaylistView* view : LiveViews()) view->OnInfoLinesChanged();
}

// The lines beside the covers: those picked in the info popup for the active preset, or else its cover pattern's text.
void PlaylistView::LoadInfoLines() {
	m_infoLines.clear();
	const GroupPreset& preset = m_presets[m_activePreset];
	const std::optional<std::vector<std::string>> picked = PickedInfoLines(LoadInfoLineChoices(), preset.name);
	m_infoLinesPicked = picked.has_value();
	if (!picked) {
		m_blockLineCount = preset.showCoverText ? CountInfoLines(preset.coverPattern.c_str()) : 0;
		return;
	}
	const std::vector<std::string> patternLines = PatternLines(preset.coverPattern);
	auto compiler = titleformat_compiler::get();
	for (const std::string& entry : *picked) {
		InfoLine line;
		if (const InfoItem* item = InfoItemOfEntry(entry)) {
			if (item->labelled) line.label = Tr(item->name);
			compiler->compile_safe(line.script, item->pattern);
		} else {
			// A line taken out of the cover pattern since is left out.
			const auto at = std::find(patternLines.begin(), patternLines.end(), entry);
			if (at == patternLines.end()) continue;
			// On its own, so it does not take the place of another line when one before it gives nothing (WholePatternOf).
			if (const std::optional<std::string> whole = WholePatternOf(entry)) compiler->compile_safe(line.script, whole->c_str());
			else line.patternLine = static_cast<int>(at - patternLines.begin());
		}
		m_infoLines.push_back(std::move(line));
	}
	m_blockLineCount = static_cast<int>(m_infoLines.size());
}

// Only the blocks' heights and the info column change, so the row at the top of the view stays there.
void PlaylistView::OnInfoLinesChanged() {
	if (m_presets.empty() || m_activePreset >= m_presets.size()) return;
	const int before = m_blockLineCount;
	LoadInfoLines();
	if (!m_hWnd || m_ungrouped) return;
	const TopRow top = CurrentTopRow();
	ShowInfoColumn(m_blockLineCount > 0);
	FitTrackColumns();
	if (m_blockLineCount != before) ResizeBlocks(top);
	Invalidate();
}

std::string PlaylistView::InfoValue(size_t block, const std::string& entry) const {
	if (m_ungrouped || block >= m_groups.BlockCount() || m_activePreset >= m_presets.size()) return {};
	const size_t item = m_groups.BlockAt(block).firstItem;
	pfc::string8_fastalloc text;
	if (const InfoItem* infoItem = InfoItemOfEntry(entry)) {
		titleformat_object::ptr script;
		titleformat_compiler::get()->compile_safe(script, infoItem->pattern);
		m_playlists->activeplaylist_item_format_title(item, nullptr, text, script, nullptr, playback_control::display_level_none);
		return text.c_str();
	}
	const std::vector<std::string> lines = PatternLines(m_presets[m_activePreset].coverPattern);
	const auto at = std::find(lines.begin(), lines.end(), entry);
	if (at == lines.end()) return {};
	// As the panel draws it (LoadInfoLines): on its own when it can stand alone, else by its place in the cover text.
	if (const std::optional<std::string> whole = WholePatternOf(entry)) {
		titleformat_object::ptr script;
		titleformat_compiler::get()->compile_safe(script, whole->c_str());
		m_playlists->activeplaylist_item_format_title(item, nullptr, text, script, nullptr, playback_control::display_level_none);
		return text.c_str();
	}
	m_playlists->activeplaylist_item_format_title(item, nullptr, text, m_blockScript, nullptr, playback_control::display_level_none);
	const std::vector<std::string> parts = PatternLines(text.c_str());
	const size_t index = static_cast<size_t>(at - lines.begin());
	return index < parts.size() ? parts[index] : std::string();
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
	SetWindowSubclass(GetHeaderCtrl(), HeaderProc, 0, reinterpret_cast<DWORD_PTR>(this));
	UpdateHeaderLayout();
	RebuildColumns();
	set_callback_flags(kPlaylistCallbacks);
	play_callback_reregister(kPlaybackCallbacks);
	ApplyPlaylistPreset();
	EnterPlaylistColumns();
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
		text += " · ";
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
// The row is found from the full row heights, as in CoverBlockAt: the part of a cover below its track rows
// belongs to the block's last row, though the list control sees no row there.
size_t PlaylistView::DropIndexAt(CPoint clientPoint, DragUnit unit, size_t level) const {
	size_t item = 0;
	if (GroupHeaderFromPoint2(clientPoint, item)) return item;
	item = IndexFromPointAbs(PointClientToAbs(clientPoint).y);
	if (item >= m_groups.ItemCount()) return GetItemCount();
	std::pair<size_t, size_t> range;
	switch (unit) {
	case DragUnit::Section: range = SectionRangeOf(item, level); break;
	case DragUnit::Cover: {
		const size_t block = m_groups.BlockIndexOf(item);
		const size_t blockFirst = m_groups.BlockAt(block).firstItem;
		range = { blockFirst, blockFirst + m_groups.BlockSize(block) };
		break;
	}
	default: range = AlbumRangeOf(item); break;
	}
	const auto [first, end] = range;
	const int top = GetItemOffsetAbs(first);
	const int bottom = GetItemOffsetAbs(end - 1) + GetItemHeight2(end - 1);
	return PointClientToAbs(clientPoint).y < (top + bottom) / 2 ? first : end;
}

// The list shows a tooltip over text that does not fit. The Duration cell of the playing track changes
// every second, and a tooltip would freeze its old text over the cell, so that cell gets none.
bool PlaylistView::GetTooltipData(CPoint pointAbs, pfc::string_base& text, CRect& rect, CFontHandle& font) const {
	size_t item = 0;
	size_t subItem = 0;
	if (GetItemAtPointAbsEx(pointAbs, item, subItem) && IsPlayingDurationCell(item, subItem)) return false;
	if (!__super::GetTooltipData(pointAbs, text, rect, font)) return false;
	m_tooltipText = pfc::stringcvt::string_wide_from_utf8(text.c_str()).get_ptr();
	return true;
}

// With DirectWrite the header's own title text is cleared, and each title is drawn after Windows has drawn the item's
// background, hover and sort arrow, so titles go through the same painter as the rest of the panel and show colour
// emoji. The list control keeps its own copy of every title (GetColumnText) and never reads the header's text back.
// With GDI the header writes its own titles. Under Wine the drawing over the item does not happen, and cleared
// titles stayed blank.
void PlaylistView::ClearHeaderTitles() {
	CHeaderCtrl header = GetHeaderCtrl();
	const bool painterDraws = UseDirectWrite();
	const int count = header.GetItemCount();
	for (int index = 0; index < count; ++index) {
		pfc::string8 title;
		if (!painterDraws && static_cast<size_t>(index) < GetColumnCount()) GetColumnText(static_cast<size_t>(index), title);
		const pfc::stringcvt::string_os_from_utf8 wide(title);
		HDITEM item = {};
		item.mask = HDI_TEXT;
		item.pszText = const_cast<LPWSTR>(wide.get_ptr());
		header.SetItem(index, &item);
	}
}

LRESULT PlaylistView::OnChildCustomDraw(int, LPNMHDR header, BOOL& handled) {
	// With GDI the header and the tooltip draw their text themselves (ClearHeaderTitles).
	if (!UseDirectWrite()) {
		handled = FALSE;
		return 0;
	}
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
		DrawHeaderTitle(CDCHandle(draw.hdc), column, draw.rc);
		return CDRF_DODEFAULT;
	}
	default:
		return CDRF_DODEFAULT;
	}
}

void PlaylistView::DrawHeaderTitle(CDCHandle dc, size_t column, const CRect& itemRect) {
	pfc::string8 title;
	GetColumnText(column, title);
	if (title.is_empty()) return;
	CHeaderCtrl header = GetHeaderCtrl();
	// The colour the header itself would use: the dark scheme's (as the list control sets it), else the theme's.
	COLORREF color = GetSysColor(COLOR_BTNTEXT);
	if (GetDarkMode()) {
		color = 0xdedede;
	} else if (HTHEME theme = OpenThemeData(header, L"Header")) {
		GetThemeColor(theme, HP_HEADERITEM, HIS_NORMAL, TMT_TEXTCOLOR, &color);
		CloseThemeData(theme);
	}
	DCStateScope state(dc);
	dc.SelectFont(header.GetFont());
	dc.SetTextColor(color);
	dc.SetBkMode(TRANSPARENT);
	// Windows puts a header title three edge widths in from each side.
	const int margin = 3 * GetSystemMetrics(SM_CXEDGE);
	CRect textRect(itemRect);
	textRect.DeflateRect(margin, 0);
	const pfc::stringcvt::string_os_from_utf8 wide(title);
	m_text.Draw(dc, View(wide), textRect, AlignOfColumnFormat(GetColumnFormat(column)), true);
}

// While columns move, the header window is painted here in full, each title where its column is drawn: the header
// control itself only knows the columns' places. Its custom draw notice cannot do this, because the header paints
// over whatever that draws. The items are drawn with the header's own theme (dark or light), through a buffer.
void PlaylistView::PaintMovingHeader(HWND window) {
	CHeaderCtrl header = window;
	CPaintDC paint(window);
	CRect client;
	header.GetClientRect(client);
	CMemoryDC buffer(paint.m_hDC, client);
	CDCHandle dc(buffer.m_hDC);
	const HTHEME theme = OpenThemeData(header, L"Header");
	const auto drawItem = [&](size_t column, int left, int state) {
		CRect rect;
		if (!header.GetItemRect(static_cast<int>(column), rect) || rect.Width() <= 0) return;
		rect.MoveToX(left);
		const auto draw = [&](CDCHandle target) {
			if (theme) {
				DrawThemeBackground(theme, target, HP_HEADERITEM, state, rect, nullptr);
			} else {
				target.FillSolidRect(rect, GetSysColor(COLOR_BTNFACE));
				target.DrawEdge(rect, state == HIS_PRESSED ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
			}
			DrawHeaderTitle(target, column, rect);
		};
		// The lifted title is see-through, as its column below.
		if (state == HIS_PRESSED) DrawSeeThrough(dc, rect, kLiftedColumnOpacity, draw);
		else draw(dc);
	};
	// Right of the last column the header shows an empty item.
	if (theme) DrawThemeBackground(theme, dc, HP_HEADERITEM, HIS_NORMAL, client, nullptr);
	else dc.FillSolidRect(client, GetSysColor(COLOR_BTNFACE));
	const std::optional<size_t> lifted = LiftedColumn();
	for (const int column : GetColumnOrderArray()) {
		if (lifted && static_cast<size_t>(column) == *lifted) continue;
		CRect rect;
		header.GetItemRect(column, rect);
		drawItem(static_cast<size_t>(column), rect.left + ColumnOffset(static_cast<size_t>(column)), HIS_NORMAL);
	}
	if (lifted) {
		CRect rect;
		header.GetItemRect(static_cast<int>(*lifted), rect);
		drawItem(*lifted, rect.left + ColumnOffset(*lifted), HIS_PRESSED);
	}
	if (theme) CloseThemeData(theme);
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

bool PlaylistView::IsPlayingDurationCell(size_t item, size_t subItem) const {
	return subItem < m_listColumns.size() && m_listColumns[subItem].kind == ColumnKind::Duration && IsPlayingItem(item);
}

// A click on the Duration cell of the playing track switches between time played and time left.
// The click still goes on to the list control, so it also selects the row as usual.
LRESULT PlaylistView::OnDurationButtonDown(UINT, WPARAM, LPARAM position, BOOL& handled) {
	handled = FALSE;
	size_t item = 0;
	size_t subItem = 0;
	if (!GetItemAtPointAbsEx(PointClientToAbs(CPoint(position)), item, subItem)) return 0;
	if (!IsPlayingDurationCell(item, subItem)) return 0;
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
	// The playlist changed after any column move, so Ctrl+Z undoes the playlist again.
	m_columnUndo.clear();
	m_columnRedo.clear();
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
	CRect client;
	GetClientRect(client);
	// A slide sets widths for the old panel width. A scroll bar coming and going during the slide changes only the height.
	if (client.Width() != m_lastClientWidth) FinishColumnWidthSlide();
	FitTrackColumns();
	if (client.Width() != m_lastClientWidth) {
		m_lastClientWidth = client.Width();
		Invalidate();
	}
}

int PlaylistView::ScreenWidth(WidthAt96Dpi width) const {
	return MulDiv(static_cast<int>(width.value + 0.5f), m_dpi.cx, 96);
}

int PlaylistView::WidthAt96DpiOf(int screenWidth) const {
	return MulDiv(screenWidth, 96, m_dpi.cx);
}

WidthAt96Dpi PlaylistView::ColumnWidthAt96Dpi(size_t column) const {
	return { GetColumnWidthF(column) };
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
	const int minimum = (std::max)(Scaled(CoverColumnWidth(CoverSize())), m_blockLineCount * GetItemHeight());
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
	// A block's last row always reports its height, even a plain one. Its height changes with the cover size, and the
	// list keeps an updated height of -1 instead of dropping it (ReloadItems), which would break the row offsets.
	return lastInBlock || height > rowHeight ? height : -1;
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
	const int minimum = (std::max)(Scaled(CoverColumnWidth(CoverSize())), m_blockLineCount * GetItemHeight());
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
	if (message == WM_RBUTTONDOWN) SelectForMenu(first, count);
	else if (count > 0) SetSelection(pfc::bit_array_true(), pfc::bit_array_range(first, count));
	if (message == WM_LBUTTONDBLCLK && count > 0) ExecuteDefaultActionGroup(first, count);
	return 0;
}

// A right click selects what is under it for the menu. When the selection already holds all of it, the selection
// stays, so the menu can act on several albums, sections or covers at once, as on several selected tracks.
void PlaylistView::SelectForMenu(size_t first, size_t count) {
	if (count == 0) return;
	const pfc::bit_array_range range(first, count);
	if (GetSelectedCount(range) != count) SetSelection(pfc::bit_array_true(), range);
}

// A right click on a label row takes its section and one on a cover takes the cover's tracks, as a left click
// does. Left to the list control, the click would select only the track under the pointer. A right click on an
// album header is the list control's own and selects the album.
LRESULT PlaylistView::OnPartRightButtonDown(UINT, WPARAM, LPARAM position, BOOL& handled) {
	const CPoint point(position);
	size_t first = 0;
	size_t count = 0;
	size_t item = 0;
	if (const std::optional<size_t> block = CoverBlockAt(point)) {
		first = m_groups.BlockAt(*block).firstItem;
		count = m_groups.BlockSize(*block);
	} else if (const std::optional<size_t> level = LabelLevelAtPoint(point, item)) {
		const auto [sectionFirst, sectionEnd] = SectionRangeOf(item, *level);
		first = sectionFirst;
		count = sectionEnd - sectionFirst;
	} else {
		handled = FALSE;
		return 0;
	}
	SetFocus();
	SelectForMenu(first, count);
	SetFocusItem(first);
	return 0;
}

// The caller clips drawing to the column, so a cover wider than the column is cut off, not squeezed.
// During a drag of the cover column the covers of the saved size are stretched to the dragged size, so no file is read
// again for each new width. After the drag, a cover still loading at the new size is stretched from its old size.
void PlaylistView::RenderCover(size_t block, const CRect& blockRect, CDCHandle dc) {
	const int inset = Scaled(kCoverInset);
	const int size = Scaled(CoverSize());
	const CPoint origin(blockRect.left + inset, blockRect.top + inset);

	metadb_handle_ptr track;
	if (!m_playlists->activeplaylist_get_item_handle(track, m_groups.BlockAt(block).firstItem)) return;
	// A block without a cover shows nothing there, as the original SimPlaylist did, or foobar2000's stub image when
	// chosen. One whose cover is still loading shows nothing.
	m_covers.DrawCover(track, Scaled(m_settings.coverSize), m_settings.stubForMissingArt, [&](HBITMAP cover, int madeForPixels, double arrivedMs) {
		BITMAP dimensions = {};
		if (!GetObject(cover, sizeof(dimensions), &dimensions) || madeForPixels <= 0) return;
		CDC memory;
		memory.CreateCompatibleDC(dc);
		SelectObjectScope bitmapScope(memory, cover);
		const int width = MulDiv(dimensions.bmWidth, size, madeForPixels);
		const int height = MulDiv(dimensions.bmHeight, size, madeForPixels);
		// A cover that just arrived fades in; the effect clock draws it again until it is fully there.
		const int opacity = FadeInOpacity(arrivedMs, NowMs(), kCoverFadeMs);
		if (opacity < 255) {
			m_coversFading = true;
			StartEffectClock();
		}
		// The cover keeps its transparency, so a cover with a see-through background sits on the row colour.
		const BLENDFUNCTION blend = { AC_SRC_OVER, 0, static_cast<BYTE>(opacity), AC_SRC_ALPHA };
		dc.AlphaBlend(origin.x, origin.y, width, height, memory, 0, 0, dimensions.bmWidth, dimensions.bmHeight, blend);
	});
}

void PlaylistView::RenderInfoLines(size_t block, const CRect& blockRect, CDCHandle dc) {
	if (m_infoLinesPicked) {
		RenderPickedInfoLines(block, blockRect, dc);
		return;
	}
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

// Picked lines: a name in a paler colour, and the values lined up after the longest name. A line that gives nothing for
// this cover is left out, so the lines below move up instead of leaving a gap.
void PlaylistView::RenderPickedInfoLines(size_t block, const CRect& blockRect, CDCHandle dc) {
	const size_t item = m_groups.BlockAt(block).firstItem;
	pfc::string8_fastalloc text;
	std::vector<std::string> patternParts;
	bool patternFormatted = false;
	struct Shown {
		std::wstring label;
		std::wstring value;
	};
	std::vector<Shown> shown;
	const auto wide = [](const std::string& utf8) { return std::wstring(pfc::stringcvt::string_os_from_utf8(utf8.c_str()).get_ptr()); };
	for (const InfoLine& line : m_infoLines) {
		std::string value;
		if (line.script.is_valid()) {
			m_playlists->activeplaylist_item_format_title(item, nullptr, text, line.script, nullptr, playback_control::display_level_none);
			value = text.c_str();
		} else {
			if (!patternFormatted) {
				m_playlists->activeplaylist_item_format_title(item, nullptr, text, m_blockScript, nullptr, playback_control::display_level_none);
				patternParts = PatternLines(text.c_str());
				patternFormatted = true;
			}
			if (line.patternLine >= 0 && static_cast<size_t>(line.patternLine) < patternParts.size()) value = patternParts[static_cast<size_t>(line.patternLine)];
		}
		if (!value.empty()) shown.push_back({ wide(line.label), wide(value) });
	}

	SelectObjectScope fontScope(dc, static_cast<HGDIOBJ>(GetFont()));
	dc.SetBkMode(TRANSPARENT);
	const COLORREF textColor = GetSysColorHook(colorText);
	const COLORREF labelColor = PaintUtils::BlendColor(textColor, GetSysColorHook(colorBackground), 45);
	int labelWidth = 0;
	for (const Shown& line : shown) {
		if (!line.label.empty()) labelWidth = (std::max)(labelWidth, m_text.Width(dc, line.label));
	}
	const int rowHeight = GetItemHeight();
	const int padding = static_cast<int>(GetColumnSpacing());
	const int valueOffset = labelWidth > 0 ? labelWidth + 2 * padding : 0;
	CRect lineRect(blockRect.left + padding, blockRect.top, blockRect.right - padding, blockRect.top + rowHeight);
	for (const Shown& line : shown) {
		if (lineRect.top >= blockRect.bottom) break;
		CRect valueRect = lineRect;
		if (!line.label.empty()) {
			dc.SetTextColor(labelColor);
			m_text.Draw(dc, line.label, lineRect, TextPainter::Align::Left, true);
			valueRect.left += valueOffset;
		}
		dc.SetTextColor(textColor);
		m_text.Draw(dc, line.value, valueRect, TextPainter::Align::Left, true);
		lineRect.OffsetRect(0, rowHeight);
	}
}

void PlaylistView::OnDestroy() {
	// The timing threads send frames to this window, so they stop before the window goes away.
	StopSmoothScroll();
	m_columnClock.Stop();
	m_effectClock.Stop();
	CloseInfoLinesPopupOf(this);
	SaveScrollPosition();
	LeavePlaylistColumns();
	ClaimEditContext(false);
	ClaimSelection(false);
	set_callback_flags(0);
	play_callback_reregister(0);
	SetMsgHandled(FALSE);
}

LRESULT PlaylistView::OnFocusChange(UINT message, WPARAM, LPARAM, BOOL& handled) {
	ClaimEditContext(message == WM_SETFOCUS);
	ClaimSelection(message == WM_SETFOCUS);
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

// The shared selection, as the SDK asks of a window with selectable items: taken with the focus, set to track
// the playlist selection, and let go with the focus (ui.h, ui_selection_holder).
void PlaylistView::ClaimSelection(bool claim) {
	m_selectionHolder.release();
	if (!claim) return;
	m_selectionHolder = ui_selection_manager::get()->acquire();
	m_selectionHolder->set_playlist_selection_tracking();
}

// User-defined keyboard shortcuts take priority over the built-in keys, as in the default playlist view.
LRESULT PlaylistView::OnShortcutKey(UINT, WPARAM key, LPARAM, BOOL& handled) {
	// Before the keyboard shortcuts, where Ctrl+Z is the playlist's undo.
	if (RunColumnUndoKey(key)) return 0;
	handled = (keyboard_shortcut_manager::get()->on_keydown_auto_playlist(key) || RunEditKey(key) || RunPageKey(key) || RunRefreshKey(key)) ? TRUE : FALSE;
	return 0;
}

// F5 reads every cover again, as other playlist views do, for covers added or changed on disk.
// A user who bound F5 to a command in Preferences > Keyboard Shortcuts keeps it: those run first.
bool PlaylistView::RunRefreshKey(WPARAM key) {
	if (key != VK_F5 || GetHotkeyModifierFlags() != 0) return false;
	ReloadAllCovers();
	return true;
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

// Draws with draw at scale times its size around the middle of rect, through a world transform, for a heart or star
// that pops. At scale 1 it just draws.
template <typename Draw>
static void DrawScaled(CDCHandle dc, const CRect& rect, double scale, Draw&& draw) {
	if (scale == 1) {
		draw();
		return;
	}
	const int mode = ::SetGraphicsMode(dc, GM_ADVANCED);
	XFORM saved = {};
	GetWorldTransform(dc, &saved);
	const FLOAT middleX = (rect.left + rect.right) / 2.f;
	const FLOAT middleY = (rect.top + rect.bottom) / 2.f;
	const FLOAT factor = static_cast<FLOAT>(scale);
	const XFORM grow = { factor, 0, 0, factor, middleX - factor * middleX, middleY - factor * middleY };
	ModifyWorldTransform(dc, &grow, MWT_LEFTMULTIPLY);
	draw();
	SetWorldTransform(dc, &saved);
	::SetGraphicsMode(dc, mode);
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
		if (GetSubItemText(item, subItem, text) && strcmp(text.c_str(), "⏸") == 0) {
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
		DrawScaled(dc, starRect, PopScaleOf(item, subItem, star), [&] {
			dc.DrawText(star <= rating ? filledGlyph : emptyGlyph, 1, starRect, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_CENTER);
		});
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
	const double scale = PopScaleOf(item, subItem, 0);
	if (liked && m_settings.likeStyle == LikeStyle::Emoji) {
		DrawScaled(dc, rowRect, scale, [&] { m_text.Draw(dc, L"\x2764\xFE0F", rowRect, TextPainter::Align::Center, false); });
		return;
	}
	UpdateStarFont();
	if (m_starFontIsIcons) dc.SelectFont(m_starFont);
	// Segoe Fluent Icons and Segoe MDL2 Assets: HeartFill and Heart. Without them, the row font's heart characters.
	const wchar_t* const glyph = liked ? (m_starFontIsIcons ? L"\xEB52" : L"\x2665") : (m_starFontIsIcons ? L"\xEB51" : L"\x2661");
	dc.SetTextColor(liked ? LikeFillColor() : PaintUtils::BlendColor(dc.GetTextColor(), GetSysColorHook(colorBackground), 55));
	DrawScaled(dc, rowRect, scale, [&] { dc.DrawText(glyph, 1, rowRect, DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | DT_CENTER); });
}

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

// A click on a star, or on the gap before the first star, rates the track (RatingAfterClick) and goes no further, so it neither changes the selection
// nor starts a drag. The second press of a double click is dropped, because passed on it would start playback.
LRESULT PlaylistView::OnStarButtonDown(UINT message, WPARAM, LPARAM position, BOOL& handled) {
	const std::optional<StarPosition> hit = StarAtPoint(CPoint(position));
	if (!hit) {
		handled = FALSE;
		return 0;
	}
	SetFocus();
	if (message == WM_LBUTTONDOWN) {
		if (IsLikeColumn(hit->subItem)) {
			WriteLikeOf(hit->item, hit->rating != 0);
			if (hit->rating != 0) StartPop(hit->item, hit->subItem, 0);
		} else {
			const int rating = RatingAfterClick(hit->rating, RatingOf(hit->item, hit->subItem));
			WriteRatingOf(hit->item, rating);
			if (rating > 0) StartPop(hit->item, hit->subItem, rating);
		}
		// The cell now shows the new value; the pointer previews the next click only after it moves.
		MoveStarHover(std::nullopt);
	}
	return 0;
}

// A heart turned on, or the star clicked, grows a little and goes back (PopScale).
void PlaylistView::StartPop(size_t item, size_t subItem, int star) {
	m_pop = Pop{ item, subItem, star, NowMs() };
	StartEffectClock();
}

// How large to draw a star (1 to 5) or the heart (0) of a cell in this frame.
double PlaylistView::PopScaleOf(size_t item, size_t subItem, int star) const {
	if (!m_pop || m_pop->item != item || m_pop->subItem != subItem || m_pop->star != star) return 1;
	return PopScale(m_pop->startMs, NowMs(), kPopMs, kPopAmount);
}

void PlaylistView::WriteLikeOf(size_t item, bool liked) {
	metadb_handle_ptr track;
	if (!m_playlists->activeplaylist_get_item_handle(track, item)) return;
	metadb_handle_list tracks;
	tracks.add_item(track);
	WriteLikes(tracks, liked, GetLikeStorage());
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
	const int saved = LoadViewSettings().coverSize;
	const int size = ClampCoverSize(saved + notches * 8);
	if (size != saved) ApplyCoverSize(size);
	return 0;
}

// While the columns fill the panel, dragging a track column's divider moves only the border between it
// and the next column on screen, so the right edge stays at the panel's edge. The header's own change
// is refused and both widths are set here. The last column's divider is the panel's edge and does not move.
// A hidden column has no width, so it sits on the divider of the column to its left, and the header hands a drag
// there to the hidden one. The drag is meant for what the user sees.
bool PlaylistView::IsHiddenColumn(size_t column) const {
	if (column == kCoverColumn) return m_ungrouped;
	if (column == kInfoColumn) return m_ungrouped || m_blockLineCount == 0;
	return false;
}

// Where the hidden info column sits right after the cover, a drag of their shared divider is meant for the cover.
bool PlaylistView::InfoFollowsCover() const {
	const std::vector<int> order = GetColumnOrderArray();
	const auto cover = std::find(order.begin(), order.end(), kCoverOrderColumn);
	return cover != order.end() && cover + 1 != order.end() && *(cover + 1) == kInfoOrderColumn;
}

// A drag of the cover's divider, or of the hidden info column's divider on top of it, sets the cover size.
bool PlaylistView::IsCoverDrag(size_t column) const {
	if (m_ungrouped) return false;
	return column == kCoverColumn || (column == kInfoColumn && IsHiddenColumn(kInfoColumn) && InfoFollowsCover());
}

int PlaylistView::CoverSize() const {
	return m_dragCoverSize.value_or(m_settings.coverSize);
}

// The cover column stops at the smallest and the largest cover. While the columns fill the panel, the track columns give
// or take the difference in the same step, so the columns are never wider than the panel. Otherwise the list would show
// a horizontal scroll bar for a moment on every step, and fitting the columns to the panel would hide it again.
void PlaylistView::DragCoverColumn(int width) {
	const int clamped = std::clamp(width, Scaled(CoverColumnWidth(kMinCoverSize)), Scaled(CoverColumnWidth(kMaxCoverSize)));
	if (clamped == static_cast<int>(GetSubItemWidth(kCoverColumn))) return;
	// The header draws once, after every width is set.
	CHeaderCtrl headerCtrl = GetHeaderCtrl();
	headerCtrl.SetRedraw(FALSE);
	{
		pfc::vartoggle_t<bool> fitting(m_fittingColumns, true);
		ResizeColumn(kCoverColumn, static_cast<t_uint32>(clamped), false);
	}
	FitTrackColumns(false);
	headerCtrl.SetRedraw(TRUE);
	headerCtrl.Invalidate();
	const int size = CoverSizeForColumnWidth(WidthAt96DpiOf(clamped));
	if (size != CoverSize()) {
		m_dragCoverSize = size;
		// This lays the view out again, which the new widths need as well.
		ResizeBlocks(CurrentTopRow());
	} else {
		OnColumnsChanged();
	}
	// Windows paints only when no input is waiting, so while the mouse keeps moving the list would not be drawn until
	// the hand slows down. Each step is drawn at once, as libPPUI's own scrolling does (MoveViewOriginNoClip).
	RedrawWindow(nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
}

PlaylistView::TopRow PlaylistView::CurrentTopRow() const {
	const size_t count = GetItemCount();
	if (count == 0) return { 0, 0 };
	const int viewTop = GetViewOrigin().y;
	const size_t row = (std::min)(IndexFromPointAbs(viewTop), count - 1);
	return { row, GetItemOffsetAbs(row) - viewTop };
}

// Only the blocks' last rows change height with the cover. The rest of the list's layout is kept, unlike ReloadData,
// which forgets every height and would let the view jump and the scroll bar come and go while it lays the rows out again.
// Every last row is measured, so the rows above the view have their true heights, and the row that was at the top of
// the view goes back there.
void PlaylistView::ResizeBlocks(TopRow top) {
	const size_t count = GetItemCount();
	if (m_ungrouped || count == 0 || count != m_groups.ItemCount()) {
		OnViewAreaChanged();
		return;
	}
	pfc::bit_array_bittable lastRows(count);
	for (size_t block = 0; block < m_groups.BlockCount(); ++block) {
		lastRows.set(m_groups.BlockAt(block).firstItem + m_groups.BlockSize(block) - 1, true);
	}
	ReloadItems(lastRows);
	const size_t row = (std::min)(top.row, count - 1);
	const int into = std::clamp(-top.offset, 0, (std::max)(0, GetItemHeightCached(row) - 1));
	OnViewAreaChanged(CPoint(GetViewOrigin().x, GetItemOffsetAbs(row) + into));
}

LRESULT PlaylistView::OnHeaderItemChanging(int, LPNMHDR header, BOOL& handled) {
	handled = FALSE;
	const NMHEADER* change = reinterpret_cast<NMHEADER*>(header);
	// Only the user's drag is taken over here: the header holds the mouse while the user drags a divider. The view itself
	// sets these widths too, which also arrives here.
	const bool userDrag = ::GetCapture() == GetHeaderCtrl().m_hWnd;
	const bool widthChange = change->pitem && (change->pitem->mask & HDI_WIDTH);
	if (userDrag && !m_fittingColumns) FinishColumnWidthSlide();
	const size_t column = static_cast<size_t>(change->iItem);
	if (userDrag && !m_fittingColumns && widthChange && IsCoverDrag(column)) {
		handled = TRUE;
		int width = change->pitem->cxy;
		if (column != kCoverColumn) {
			// The cover's right edge follows the pointer, as when its own divider is dragged.
			CHeaderCtrl headerCtrl = GetHeaderCtrl();
			CPoint pointer;
			::GetCursorPos(&pointer);
			headerCtrl.ScreenToClient(&pointer);
			CRect coverRect;
			headerCtrl.GetItemRect(static_cast<int>(kCoverColumn), &coverRect);
			width = pointer.x - coverRect.left;
		}
		DragCoverColumn(width);
		return TRUE;
	}
	if (userDrag && !m_fittingColumns && widthChange && IsHiddenColumn(column)) {
		handled = TRUE;
		return TRUE;
	}
	if (!m_autoSizeColumns || m_fittingColumns || !change->pitem || !(change->pitem->mask & HDI_WIDTH)) return FALSE;
	const std::vector<size_t> onScreen = TrackColumnsOnScreen();
	const auto dragged = std::find(onScreen.begin(), onScreen.end(), static_cast<size_t>(change->iItem));
	if (dragged == onScreen.end()) return FALSE;
	handled = TRUE;
	if (dragged + 1 == onScreen.end()) return TRUE;
	PushTrackColumns(onScreen, static_cast<size_t>(dragged - onScreen.begin()), change->pitem->cxy);
	return TRUE;
}

// A track column's edge pushes the columns beyond it as books along a shelf (PushDivider): each gives down to the width
// its content needs, the nearest first. The widths at the start of the drag and the content widths are taken once, and
// the edge follows the pointer from where the drag started: the header's own width for the dragged column would drift
// when the columns on its left get narrower.
void PlaylistView::PushTrackColumns(const std::vector<size_t>& onScreen, size_t divider, int requestedWidth) {
	CHeaderCtrl headerCtrl = GetHeaderCtrl();
	CPoint pointer;
	::GetCursorPos(&pointer);
	headerCtrl.ScreenToClient(&pointer);
	if (!m_dividerDrag || m_dividerDrag->divider != divider || m_dividerDrag->columns != onScreen) {
		DividerDrag drag;
		drag.columns = onScreen;
		drag.divider = divider;
		// The header asks for the first step already: the drag started that far back.
		drag.startX = pointer.x - (requestedWidth - static_cast<int>(GetSubItemWidth(onScreen[divider])));
		for (const size_t column : onScreen) {
			drag.widths.push_back(static_cast<int>(GetSubItemWidth(column)));
			drag.fits.push_back(ColumnContentWidth(column));
			drag.fixedContent.push_back(IsRatingColumn(column) || IsLikeColumn(column));
		}
		m_dividerDrag = std::move(drag);
	}
	const DividerDrag& drag = *m_dividerDrag;
	PushFeel feel;
	feel.minimum = Scaled(kMinFitColumnWidth);
	feel.catchPixels = Scaled(kColumnFitCatch);
	feel.wallPixels = Scaled(kColumnFitWall);
	feel.beyondWall = true;
	ShowTrackWidths(onScreen, PushDivider(drag.widths, drag.fits, drag.fixedContent, drag.divider, pointer.x - drag.startX, feel));
	// Drawn at once, as the cover column drag is: Windows does not paint while the mouse keeps moving.
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

// Sets the widths of the track columns on screen (in that order) in one go, the header drawn once.
void PlaylistView::ShowTrackWidths(const std::vector<size_t>& onScreen, const std::vector<int>& widths) {
	CHeaderCtrl headerCtrl = GetHeaderCtrl();
	headerCtrl.SetRedraw(FALSE);
	{
		pfc::vartoggle_t<bool> fitting(m_fittingColumns, true);
		for (size_t index = 0; index < onScreen.size() && index < widths.size(); ++index) {
			if (static_cast<int>(GetSubItemWidth(onScreen[index])) != widths[index]) ResizeColumn(onScreen[index], static_cast<t_uint32>(widths[index]), false);
		}
	}
	headerCtrl.SetRedraw(TRUE);
	headerCtrl.Invalidate();
	OnColumnsChanged();
}

// A double-click on a track column's right edge fits the column to its content, as in a spreadsheet: as if the edge
// were dragged to where the content just fits. While the columns fill the panel, a column that needs more takes only
// the room the columns on its right have spare, and cuts no one's text; one that needs less gives the rest to the next
// column. The panel's right edge does not move.
void PlaylistView::FitColumnToContent(size_t column) {
	const int fit = ColumnContentWidth(column);
	const int width = static_cast<int>(GetSubItemWidth(column));
	if (fit == width) return;
	FinishColumnWidthSlide();
	SetFocus();
	ColumnState before = CaptureColumnState();
	if (!m_autoSizeColumns) {
		SlideColumnWidths({ column }, { width }, { fit }, [this, before] {
			CaptureColumnWidths();
			RememberColumnState(before);
		});
		return;
	}
	const std::vector<size_t> onScreen = TrackColumnsOnScreen();
	const auto at = std::find(onScreen.begin(), onScreen.end(), column);
	if (at == onScreen.end() || at + 1 == onScreen.end()) return;
	std::vector<int> widths;
	std::vector<int> fits;
	for (const size_t walk : onScreen) {
		widths.push_back(static_cast<int>(GetSubItemWidth(walk)));
		fits.push_back(walk == column ? fit : ColumnContentWidth(walk));
	}
	const std::vector<int> fitted = PushDivider(widths, fits, static_cast<size_t>(at - onScreen.begin()), fit - width, Scaled(kMinFitColumnWidth));
	SlideColumnWidths(onScreen, widths, fitted, [this, before] {
		SaveTrackColumnWeights();
		CaptureColumnWidths();
		FitTrackColumns();
		RememberColumnState(before);
	});
}

// The width a cell needs to show all of its content, measured as RenderSubItemText draws it.
int PlaylistView::CellContentWidth(size_t item, size_t column, CDCHandle dc) {
	const int spacing = static_cast<int>(GetColumnSpacing());
	const int slot = StarSlotWidth();
	if (IsLikeColumn(column)) return slot + 2 * spacing;
	if (IsRatingColumn(column)) return RatingGapWidth(slot, spacing) + kMaxRating * slot + spacing;
	pfc::string8 text;
	if (!GetSubItemText(item, column, text) || text.is_empty()) return 0;
	const bool bold = m_settings.boldPlaying && m_playingFont && IsPlayingItem(item);
	SelectObjectScope fontScope(dc, bold ? static_cast<HGDIOBJ>(static_cast<HFONT>(m_playingFont)) : static_cast<HGDIOBJ>(GetFont()));
	const pfc::stringcvt::string_os_from_utf8 wide(text.c_str());
	return m_text.Width(dc, View(wide)) + 2 * spacing;
}

// The width a column needs so that nothing in it is cut: its title, and every cell of the rows on screen. Only those
// rows are measured, so a drag starts at once in a playlist of thousands of tracks.
int PlaylistView::ColumnContentWidth(size_t column) {
	CClientDC dc(m_hWnd);
	int width = 0;
	if (m_settings.showColumnHeaders) {
		pfc::string8 title;
		GetColumnText(column, title);
		if (!title.is_empty()) {
			SelectObjectScope fontScope(dc, static_cast<HGDIOBJ>(GetHeaderCtrl().GetFont()));
			const pfc::stringcvt::string_os_from_utf8 wide(title);
			// Windows puts a header title three edge widths in from each side (DrawHeaderTitle).
			width = m_text.Width(CDCHandle(dc), View(wide)) + 6 * GetSystemMetrics(SM_CXEDGE);
		}
	}
	size_t base = 0;
	size_t count = 0;
	if (GetItemRange(GetClientRectHook(), base, count)) {
		for (size_t item = base; item < base + count && item < GetItemCount(); ++item) {
			width = (std::max)(width, CellContentWidth(item, column, CDCHandle(dc)));
		}
	}
	return width;
}

// The work waits for a posted message, because rebuilding the columns inside the header's notice would
// pull the columns out from under the header while it still handles the drag.
// An edge drag of a track column can be undone. The cover column's edge sets the album art size of every panel instead.
LRESULT PlaylistView::OnHeaderBeginTrack(int, LPNMHDR header, BOOL& handled) {
	handled = FALSE;
	const size_t column = static_cast<size_t>(reinterpret_cast<NMHEADER*>(header)->iItem);
	if (column >= kFirstTrackColumn) {
		// The panel takes the keyboard, as for a column drag, so Ctrl+Z reaches it.
		SetFocus();
		m_columnsBeforeDrag = CaptureColumnState();
	}
	return 0;
}

LRESULT PlaylistView::OnHeaderEndTrack(int, LPNMHDR header, BOOL& handled) {
	handled = FALSE;
	m_dividerDrag.reset();
	PostMessage(kColumnDragEndedMessage, static_cast<WPARAM>(reinterpret_cast<NMHEADER*>(header)->iItem));
	return 0;
}

// The list would size the cover column to its text, which is empty. While the columns fill the panel,
// sizing one column to its text would leave them wider or narrower than the panel. So a double-click keeps the width.
LRESULT PlaylistView::OnHeaderDividerDoubleClick(int, LPNMHDR header, BOOL& handled) {
	const int column = reinterpret_cast<NMHEADER*>(header)->iItem;
	handled = TRUE;
	if (column >= static_cast<int>(kFirstTrackColumn) && static_cast<size_t>(column) < GetColumnCount()) FitColumnToContent(static_cast<size_t>(column));
	return 0;
}

LRESULT PlaylistView::OnColumnDragEnded(UINT, WPARAM column, LPARAM, BOOL&) {
	// The dragged size is the one saved now, so the blocks keep their heights and nothing on screen moves.
	m_dragCoverSize.reset();
	if (IsCoverDrag(column)) {
		ApplyCoverColumnWidth();
	} else if (m_autoSizeColumns) {
		// A track column's drag took its width from its neighbor, so the widths on screen become the proportions.
		if (column >= kFirstTrackColumn) SaveTrackColumnWeights();
		CaptureColumnWidths();
		FitTrackColumns();
	}
	if (std::optional<ColumnState> before = std::exchange(m_columnsBeforeDrag, std::nullopt); before && column >= kFirstTrackColumn) {
		RememberColumnState(std::move(*before));
	}
	return 0;
}

// Dragging the cover column's divider sets the cover size, as in the Default UI playlist. The size is set when
// the drag ends, because each new size reads every cover again. The column snaps to the cover,
// so a width outside the allowed cover sizes springs back to the nearest one.
void PlaylistView::ApplyCoverColumnWidth() {
	const int width = WidthAt96DpiOf(GetSubItemWidth(kCoverColumn));
	const int size = CoverSizeForColumnWidth(width);
	if (size == m_settings.coverSize && width == CoverColumnWidth(size)) return;
	ApplyCoverSize(size);
}

// The wheel without Ctrl or Shift, while smooth scrolling is on, starts or extends an animation (smooth_scroll.h).
// A precision touchpad, a held mouse button (a drag), and a list that does not scroll go on to the list control.
LRESULT PlaylistView::OnSmoothWheel(UINT, WPARAM wheel, LPARAM point, BOOL& handled) {
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
				// The list control shifts the old pixels and leaves the uncovered rows erased, to be painted before
				// its next move. That is a frame later here, so they would show as a blank band for a whole frame.
				// They are painted now, within this frame.
				UpdateWindow();
			});
	}
	const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wheel);
	if (!m_smoothScroll->ShouldAnimateWheel(wheelDelta)) {
		// A touchpad's many small messages already move smoothly. With smooth scrolling on, the list control would
		// add its own 50 ms glide to each one, and it waits out that glide every time: the view falls behind the
		// fingers and the animation frames wait too. So the list control scrolls this message without it.
		pfc::vartoggle_t<bool> plain(m_plainWheel, true);
		LRESULT result = 0;
		CListControlWithSelectionBase::ProcessWindowMessage(m_hWnd, WM_MOUSEWHEEL, wheel, point, result);
		handled = TRUE;
		return result;
	}
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

// A drag of the cover column or Ctrl with the wheel changes the cover size and nothing else.
void PlaylistView::ApplyCoverSize(int size) {
	ViewSettings settings = LoadViewSettings();
	settings.coverSize = size;
	SaveViewSettings(settings);
	PanelSettingsChanged();
	ShowPreferencesPreview(PreviewChange::CoverSizeOnly);
}

void PlaylistView::ShowViewSettings(const ViewSettings& settings, PreviewChange change) {
	for (PlaylistView* view : LiveViews()) {
		// A panel showing the Preferences page's other unapplied edits still needs them laid out.
		ViewSettings withOldCover = settings;
		withOldCover.coverSize = view->m_settings.coverSize;
		const bool onlyCoverSize = change == PreviewChange::CoverSizeOnly && SameViewSettings(withOldCover, view->m_settings);
		view->m_settings = settings;
		if (!view->m_settings.smoothScroll) view->StopSmoothScroll();
		if (!view->m_hWnd) continue;
		if (onlyCoverSize) {
			view->ShowCoverSize();
			continue;
		}
		view->SyncColumnsPerPlaylist();
		view->m_layout = view->CapturedLayout();
		view->m_layout.coverWidth = { static_cast<float>(CoverColumnWidth(view->m_settings.coverSize)) };
		view->ApplyFonts();
		view->ApplyHeaderDragging();
		view->ListHandleResize();
		view->RebuildColumns();
		view->OnPlaylistContentChanged();
		view->Invalidate();
	}
}

// A new cover size changes the cover column's width and the blocks' heights and nothing else. Building the columns
// and the groups again, as for other settings, would let the columns add up wider than the panel for a moment:
// the horizontal scroll bar would come and go, and Windows would clear the panel to white each time.
void PlaylistView::ShowCoverSize() {
	const TopRow top = CurrentTopRow();
	m_layout = CapturedLayout();
	m_layout.coverWidth = { static_cast<float>(CoverColumnWidth(m_settings.coverSize)) };
	if (!m_ungrouped) {
		CHeaderCtrl headerCtrl = GetHeaderCtrl();
		headerCtrl.SetRedraw(FALSE);
		{
			pfc::vartoggle_t<bool> fitting(m_fittingColumns, true);
			ResizeColumn(kCoverColumn, static_cast<t_uint32>(ScreenWidth(m_layout.coverWidth)), false);
		}
		FitTrackColumns(false);
		headerCtrl.SetRedraw(TRUE);
		headerCtrl.Invalidate();
	}
	ResizeBlocks(top);
}
void PlaylistView::ApplyRatingStorage(RatingStorage ratings, LikeStorage likes) {
	SetStorageChoices(ratings, likes);
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
// the section here first lets one press and drag move it. With Ctrl or Shift an album header is the list control's
// own, and a label row takes its whole section the same way (SelectPartWithModifiers).
LRESULT PlaylistView::OnHeaderButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled) {
	handled = FALSE;
	m_pressedSection.reset();
	const CPoint point(position);
	size_t item = 0;
	if ((keys & (MK_CONTROL | MK_SHIFT)) != 0) {
		if (GroupHeaderFromPoint2(point, item)) return 0;
		const std::optional<size_t> level = LabelLevelAtPoint(point, item);
		if (!level) return 0;
		const auto [first, end] = SectionRangeOf(item, *level);
		SetFocus();
		SelectPartWithModifiers(keys, first, end - first);
		handled = TRUE;
		return 0;
	}
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

namespace {
// The list control's own drag source takes a release anywhere inside the panel's rectangle as a reorder,
// even over a window on top of the panel (an Explorer window, a floating window), which then never gets
// the drop. This one reorders only when the window under the pointer is the panel or one of its children.
class DragSource : public ImplementCOMRefCounter<IDropSource> {
public:
	explicit DragSource(HWND panel) : m_panel(panel) {}

	std::optional<CPoint> releasedOnPanel;

	COM_QI_BEGIN()
		COM_QI_ENTRY(IUnknown)
		COM_QI_ENTRY(IDropSource)
	COM_QI_END()

	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD effect) override {
		m_effect = effect;
		return DRAGDROP_S_USEDEFAULTCURSORS;
	}

	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keys) override {
		if (escapePressed || (keys & MK_RBUTTON) != 0) return DRAGDROP_S_CANCEL;
		if ((keys & MK_LBUTTON) != 0) return S_OK;
		if (m_effect == DROPEFFECT_NONE) return DRAGDROP_S_CANCEL;
		CPoint point;
		if (!GetCursorPos(&point)) return DRAGDROP_S_CANCEL;
		const HWND under = WindowFromPoint(point);
		if (under == m_panel || IsChild(m_panel, under)) {
			releasedOnPanel = point;
			return DRAGDROP_S_CANCEL;
		}
		return DRAGDROP_S_DROP;
	}

private:
	HWND m_panel;
	DWORD m_effect = DROPEFFECT_NONE;
};
} // namespace

// A drag that starts on an album header moves the album, one that starts on a label row moves that section,
// and one that starts on a cover moves the cover's tracks. Each is dropped only at boundaries of its own kind.
// The list control passes the origin in list coordinates (PointClientToAbs), not client coordinates.
void PlaylistView::RunDragDrop(const CPoint& origin, bool isRightClick) {
	if (isRightClick) {
		__super::RunDragDrop(origin, isRightClick);
		return;
	}
	size_t item = 0;
	m_pressedSection.reset();
	m_dragUnit = DragUnit::Tracks;
	const CPoint clientOrigin = PointAbsToClient(origin);
	if (GroupHeaderFromPoint2(clientOrigin, item)) {
		m_dragUnit = DragUnit::Album;
	} else if (CoverBlockAt(clientOrigin)) {
		m_dragUnit = DragUnit::Cover;
	} else if (const std::optional<size_t> level = LabelLevelAtPoint(clientOrigin, item)) {
		m_dragUnit = DragUnit::Section;
		m_dragLevel = *level;
	}
	const pfc::com_ptr_t<DragSource> source = new DragSource(m_hWnd);
	DWORD effect = DROPEFFECT_NONE;
	DoDragDrop(MakeDataObject().get_ptr(), source.get_ptr(), DragDropSourceEffects(), &effect);
	if (source->releasedOnPanel) {
		CPoint point = *source->releasedOnPanel;
		ScreenToClient(&point);
		pfc::array_t<size_t> order;
		if (MakeDropReorderPermutation(order, point)) RequestReorder(order.get_ptr(), order.get_size());
	}
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
	if (m_coverDragActive) {
		accept.dwEFfect = DROPEFFECT_MOVE;
		accept.showDropMark = true;
		return accept;
	}
	// A locked playlist (an autoplaylist, for one) that takes no added tracks shows the no-drop cursor,
	// as the right-click menu greys out Paste for it (track_menu.cpp). The drop would add nothing.
	if ((m_playlists->activeplaylist_lock_get_filter_mask() & playlist_lock::filter_add) != 0) return accept;
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

// Selected tracks dragged out to another panel or to Explorer carry the same data as Copy (CopySelection).
// A drop inside this panel only reorders and never reads it. The list control allows a move so that reorder
// works, so Explorer is told to copy by default. A drag to a folder on the same drive then does not take the
// files away from the library.
pfc::com_ptr_t<IDataObject> PlaylistView::MakeDataObject() {
	metadb_handle_list items;
	m_playlists->activeplaylist_get_selected_items(items);
	auto data = ole_interaction::get()->create_dataobject(items);
	static const UINT preferredEffect = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
	IDataObjectUtils::SetDataObjectDWORD(data, preferredEffect, DROPEFFECT_COPY);
	return data;
}

void PlaylistView::ExecuteDefaultAction(t_size index) {
	m_playlists->activeplaylist_execute_default_action(index);
}

// Any change of order this view did not make itself (a drag, another sort, added or removed tracks)
// ends the order of the header click. Ctrl+Z arrives as a playlist switch (on_playlist_switch).
void PlaylistView::on_items_added(t_size, metadb_handle_list_cref, const bit_array&) {
	if (!m_sorting) ForgetHeaderSort();
	OnRowsMoved();
}

void PlaylistView::on_items_reordered(const t_size*, t_size) {
	if (!m_sorting) ForgetHeaderSort();
	OnRowsMoved();
}

// The row at the top of the view, while the rows are still those it was measured in.
void PlaylistView::on_items_removing(const bit_array&, t_size oldCount, t_size) {
	m_topBeforeRemoval.reset();
	if (!m_hWnd || oldCount == 0 || oldCount != GetItemCount()) return;
	m_topBeforeRemoval = CurrentTopRow();
}

void PlaylistView::on_items_removed(const bit_array& removed, t_size oldCount, t_size) {
	if (!m_sorting) ForgetHeaderSort();
	OnRowsMoved();
	if (const std::optional<TopRow> before = std::exchange(m_topBeforeRemoval, std::nullopt)) KeepTopRow(*before, removed, oldCount);
}

// After rows were removed the view keeps what was in it: the row that was at the top stays where it was.
// When that row went too, the first remaining row after it comes to the top with its album header, or the top of the
// playlist when nothing after it remains. Without this the list control only clamps the old position. A library viewer
// that narrows its playlist to an album removes the other tracks and moves neither the focus nor the view, so the
// album was shown from its end, often past it.
void PlaylistView::KeepTopRow(const TopRow& before, const bit_array& removed, size_t oldCount) {
	if (!m_hWnd || GetItemCount() == 0) return;
	size_t removedBefore = 0;
	for (size_t row = 0; row < before.row && row < oldCount; ++row) if (removed[row]) ++removedBefore;
	if (!removed[before.row]) {
		const size_t row = before.row - removedBefore;
		MoveViewOrigin(CPoint(GetViewOrigin().x, GetItemOffsetAbs(row) - before.offset));
		return;
	}
	size_t next = before.row;
	while (next < oldCount && removed[next]) {
		++removedBefore;
		++next;
	}
	if (next >= oldCount) {
		MoveViewOrigin(CPoint(GetViewOrigin().x, 0));
		return;
	}
	const size_t row = next - removedBefore;
	int top = GetItemOffsetAbs(row);
	if (row == 0 || GetItemGroup(row) != GetItemGroup(row - 1)) top -= GetGroupHeaderHeight();
	MoveViewOrigin(CPoint(GetViewOrigin().x, top));
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
	ForgetHeaderSort();
	SaveScrollPosition();
	LeavePlaylistColumns();
	m_selectionStart = SIZE_MAX;
	ApplyPlaylistPreset();
	EnterPlaylistColumns();
	OnRowsMoved();
	RestoreScrollPosition();
}
