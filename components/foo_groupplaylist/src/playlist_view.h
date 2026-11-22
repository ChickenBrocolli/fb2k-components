#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <libPPUI/CListControlWithSelection.h>

#include "column_def.h"
#include "column_drag.h"
#include "cover_cache.h"
#include "effects.h"
#include "group_model.h"
#include "group_preset.h"
#include "header_sort.h"
#include "info_lines.h"
#include "rating_cell.h"
#include "rating_store.h"
#include "smooth_scroll.h"
#include "text_painter.h"
#include "view_settings.h"

// List control that shows the active playlist.
// The playlist owns the items, the selection, and the focus. This view keeps no copy:
// it reads them from playlist_manager, writes user input back to playlist_manager,
// and repaints from the playlist callbacks. A change made in another panel and a change
// made here reach the screen through the same path.
class PlaylistView : public CListControlWithSelectionBase,
                     private playlist_callback_single_impl_base,
                     private play_callback_impl_base {
public:
	PlaylistView();
	~PlaylistView();

	// Call once, after the window exists.
	void AttachToPlaylist();

	const std::vector<GroupPreset>& Presets() const { return m_presets; }
	size_t ActivePreset() const { return m_activePreset; }
	void SelectPreset(size_t index);
	// Selects a preset the user picked, and remembers it for the active playlist when that setting is on.
	// When the playlist is in the order of a column header click, it is sorted by the preset again.
	void ChoosePreset(size_t index);
	// "None" in the Groups menu: a plain track list with no group headers, covers or labels.
	// The active preset is kept, so choosing it again brings the groups back as they were.
	bool IsUngrouped() const { return m_ungrouped; }
	void SetUngrouped();
	// SetUngrouped for a choice of the user, remembered for the active playlist like ChoosePreset.
	void ChooseUngrouped();
	// The cover block under a point in client coordinates, if the point is on a cover.
	std::optional<size_t> CoverBlockAt(CPoint clientPoint) const;
	// The cover block whose info lines are under a point in client coordinates.
	std::optional<size_t> InfoBlockAt(CPoint clientPoint) const;
	// The text an info line entry (info_lines.h) gives for a cover block, for the info popup. Empty when it gives none.
	std::string InfoValue(size_t block, const std::string& entry) const;
	// Saves the info lines picked for a preset, or with none given drops its choice, and updates every panel.
	static void SetPresetInfoLines(const std::string& presetName, const std::optional<std::vector<std::string>>& lines);
	// Saves the info lines picked for every preset and updates every panel.
	static void ApplyInfoLineChoices(const InfoLineChoices& choices);
	void ReloadCover(size_t block);
	// Whether the block's track was looked at and has no album art. False while its art still loads.
	bool BlockHasNoCover(size_t block) const;
	void ReloadAllCovers();
	void OpenCoverFolder(size_t block);
	void SortByActivePreset();
	// Keeps the current preset and returns false when no preset has this name.
	bool SelectPresetByName(const char* name);
	// Saves where ratings are kept and makes every panel read them from there.
	static void ApplyRatingStorage(RatingStorage ratings, LikeStorage likes);
	// Saves the look set on the Preferences page and redraws every panel with it.
	static void ApplyViewSettings(const ViewSettings& settings);
	// Redraws every panel with these settings without saving them, so the Scrolling page can show a change at once.
	// ApplyViewSettings, or this with LoadViewSettings(), puts the saved ones back.
	static void ShowViewSettings(const ViewSettings& settings, PreviewChange change = PreviewChange::Any);
	// Saves a new cover size and shows it in every panel.
	static void ApplyCoverSize(int size);
	// The playlist font of the host. The panel uses it unless the Preferences page sets another font.
	void SetHostFont(HFONT font);
	// Saves the shared presets and updates every panel. renames pairs an old name with its new name.
	static void ApplyGroupPresets(const std::vector<GroupPreset>& presets, const std::vector<std::pair<std::string, std::string>>& renames);
	// Gives a playlist the preset its panels show it with, as if it had been picked from the Groups menu there.
	// It counts only while "Remember a preset per playlist" is on; without a preset of that name the panel's stays.
	static void SetPlaylistPreset(t_size playlist, const std::string& name);

	const std::vector<ColumnDef>& ColumnDefs() const { return m_columnDefs; }
	bool IsColumnShown(const std::string& name) const;
	void ToggleColumn(const std::string& name);
	// Reads the widths the user dragged, so call it when saving.
	ColumnLayout GetColumnLayout();
	void SetColumnLayout(const ColumnLayout& layout);
	ColumnLayout WithKnownColumns(ColumnLayout layout) const;
	// "Auto-size columns": the track columns fill the panel beside the cover and info columns,
	// in the proportions of their saved widths. On for a new panel; a panel saved before it existed keeps it off.
	bool IsAutoSizeColumns() const { return m_autoSizeColumns; }
	void SetAutoSizeColumns(bool on);
	// Saves the shared custom columns and updates every panel. renames pairs an old name with its new name.
	static void ApplyCustomColumns(const std::vector<ColumnDef>& customDefs, const std::vector<std::pair<std::string, std::string>>& renames);

	BEGIN_MSG_MAP_EX(PlaylistView)
		MSG_WM_DESTROY(OnDestroy)
		MESSAGE_HANDLER(WM_SETFOCUS, OnFocusChange)
		MESSAGE_HANDLER(WM_KILLFOCUS, OnFocusChange)
		MESSAGE_HANDLER(WM_KEYDOWN, OnShortcutKey)
		MESSAGE_HANDLER(WM_SYSKEYDOWN, OnShortcutKey)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnPinnedHeaderButton)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnStarButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnHeaderButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnCoverButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnDurationButtonDown)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, OnPinnedHeaderButton)
		MESSAGE_HANDLER(WM_RBUTTONDOWN, OnPartRightButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnCoverButtonUp)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLabelButtonUp)
		MESSAGE_HANDLER(kSelectSectionMessage, OnSelectPressedSection)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnPinnedHeaderButton)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnStarButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnCoverDoubleClick)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnCoverWheel)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnCoverMouseMove)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnSmoothWheel)
		MESSAGE_HANDLER(kSmoothScrollMessage, OnSmoothScrollFrame)
		NOTIFY_CODE_HANDLER(HDN_BEGINDRAG, OnHeaderBeginDrag)
		NOTIFY_CODE_HANDLER(HDN_ITEMCHANGING, OnHeaderItemChanging)
		NOTIFY_CODE_HANDLER(HDN_BEGINTRACK, OnHeaderBeginTrack)
		NOTIFY_CODE_HANDLER(HDN_ENDTRACK, OnHeaderEndTrack)
		NOTIFY_CODE_HANDLER(HDN_DIVIDERDBLCLICK, OnHeaderDividerDoubleClick)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnChildCustomDraw)
		MESSAGE_HANDLER(kColumnDragEndedMessage, OnColumnDragEnded)
		MESSAGE_HANDLER(kColumnSlideMessage, OnColumnSlideFrame)
		MESSAGE_HANDLER(kEffectFrameMessage, OnEffectFrame)
		MESSAGE_HANDLER(WM_TIMER, OnSortIndicatorTimer)
		MESSAGE_HANDLER(WM_TIMER, OnSmoothScrollTimer)
		CHAIN_MSG_MAP(CListControlWithSelectionBase)
	END_MSG_MAP()

protected:
	t_size GetItemCount() const override;
	bool GetSubItemText(t_size item, t_size subItem, pfc::string_base& out) const override;

	groupID_t GetItemGroup(t_size item) const override;
	bool GetGroupHeaderText2(size_t baseItem, pfc::string_base& out) const override;
	int GetGroupHeaderHeight() const override;
	int GetItemHeight() const override;
	// A wheel animation moves the view itself, one display frame at a time, so the list control's own short
	// animation is left out of those moves, and out of touchpad wheel moves too. Keys and scroll bar arrows still use it.
	bool UserEnabledSmoothScroll() const override { return m_settings.smoothScroll && !m_animatingWheel && !m_plainWheel; }
	void RenderGroupHeaderText2(size_t baseItem, const CRect& headerRect, const CRect& updateRect, CDCHandle dc) override;
	// The track counts at the right end of the headers and labels follow the window's width,
	// so a change of width repaints the whole list, not only the uncovered strip.
	void ListHandleResize() noexcept override;
	int GetItemHeight2(size_t item) const override;
	int GetItemHeight2Content(size_t item, int height) const override;
	void RenderItem(t_size item, const CRect& itemRect, const CRect& updateRect, CDCHandle dc) override;
	void RenderTrackRow(t_size item, const CRect& rowRect, const CRect& updateRect, CDCHandle dc);
	COLORREF SelectionFill(bool focused) const;
	// The user's own colour, or foobar2000's highlight colour so the text follows the colour scheme.
	COLORREF HeaderTextColor() const;
	COLORREF LabelTextColor() const;
	// The filled rating stars: their own colour, or foobar2000's highlight colour like the header and label text.
	COLORREF StarFillColor() const;
	// The filled hearts of the Like column: their own colour, or the star colour.
	COLORREF LikeFillColor() const;
	void RenderSubItemText(t_size item, t_size subItem, const CRect& subItemRect, const CRect& updateRect, CDCHandle dc, bool allowColors) override;
	void RenderGroupOverlayColumn(size_t baseItem, size_t subItem, const CRect& albumColumnRect, const CRect& updateRect, CDCHandle dc) override;
	void RenderOverlay2(const CRect& updateRect, CDCHandle dc) override;
	void RenderItemText(t_size item, const CRect& itemRect, const CRect& updateRect, CDCHandle dc, bool allowColors) override;
	void RenderGroupOverlay(size_t baseItem, const CRect& groupWhole, const CRect& updateRect, CDCHandle dc) override;
	void OnViewOriginChange(CPoint delta) override;
	void EnsureVisibleRectAbs(const CRect& rect) override;
	bool GetTooltipData(CPoint pointAbs, pfc::string_base& text, CRect& rect, CFontHandle& font) const override;
	void OnColumnHeaderClick(t_size listColumn) override;
	bool OnColumnHeaderDrag(t_size listColumn, t_size newPosition) override;

	t_size GetFocusItem() const override;
	void SetFocusItem(t_size index) override;
	void SetGroupFocusByItem(t_size item) override;
	size_t GetGroupFocus2() const override;
	bool IsItemSelected(t_size index) const override;
	void SetSelection(pfc::bit_array const& affected, pfc::bit_array const& status) override;
	t_size GetSelectionStart() const override { return m_selectionStart; }
	void SetSelectionStart(t_size index) override { m_selectionStart = index; }

	void RequestReorder(size_t const* order, size_t count) override;
	void RunDragDrop(const CPoint& origin, bool isRightClick) override;
	t_size InsertIndexFromPointEx(const CPoint& point, bool& inside) const override;
	void RequestRemoveSelection() override;
	void ExecuteDefaultAction(t_size index) override;
	void ExecuteDefaultActionGroup(t_size base, t_size count) override;
	uint32_t QueryDragDropTypes() const override { return dragDrop_reorder | dragDrop_external; }
	dragDropAccept_t DragDropAccept2(IDataObject* data) override;
	void OnDrop(IDataObject* data, CPoint screenPoint) override;
	pfc::com_ptr_t<IDataObject> MakeDataObject() override;
	// Ctrl+C / Ctrl+V and the Copy and Paste items of the track menu.
	void CopySelection();
	void PasteClipboard();

private:
	// What a list column shows. The cover and info columns have no script.
	struct ListColumn {
		titleformat_object::ptr script;
		ColumnKind kind = ColumnKind::Text;
		// Duration columns only. Used in place of script while the playing track counts down.
		titleformat_object::ptr countdownScript;
	};

	void OnDestroy();
	LRESULT OnFocusChange(UINT message, WPARAM, LPARAM, BOOL& handled);
	LRESULT OnShortcutKey(UINT, WPARAM key, LPARAM, BOOL& handled);
	LRESULT OnStarButtonDown(UINT message, WPARAM, LPARAM position, BOOL& handled);

	// A group header drawn at the top while its group's tracks scroll under it ("Pin group headers").
	struct PinnedHeader {
		size_t firstItem;
		// Client coordinates. Its top goes above the view when the next group's header pushes it out.
		CRect rect;
	};
	std::optional<PinnedHeader> FindPinnedHeader() const;
	bool IsOnPinnedHeader(CPoint clientPoint) const;
	CRect CoverBlockContentRect(size_t block, const CRect& blockRect) const;
	LRESULT OnPinnedHeaderButton(UINT message, WPARAM, LPARAM position, BOOL& handled);
	bool IsRatingColumn(size_t subItem) const;
	bool IsLikeColumn(size_t subItem) const;
	// The Duration cell of the playing track, whose time changes every second and switches on a click.
	bool IsPlayingDurationCell(size_t item, size_t subItem) const;
	// What a click on the column's header sorts by: the stars and the heart by where they are kept.
	static const char* SortPattern(const ColumnDef& def);
	void RenderLikeCell(size_t item, size_t subItem, const CRect& subItemRect, CDCHandle dc);
	void WriteLikeOf(size_t item, bool liked);
	int StarSlotWidth() const;
	void UpdateStarFont();
	void RenderDimmedBrackets(const char* text, const CRect& textRect, CDCHandle dc);
	void SortPlaylist(const char* pattern, bool descending);
	GUID ActivePlaylistGuid() const;
	void ApplyPlaylistPreset();
	void SaveScrollPosition();
	void RestoreScrollPosition();
	std::optional<StarPosition> StarAtPoint(CPoint clientPoint) const;
	LRESULT OnStarMouseMove(UINT message, WPARAM buttons, LPARAM position, BOOL& handled);
	void MoveStarHover(const std::optional<StarPosition>& at);
	void InvalidateStarCell(const std::optional<StarPosition>& at);
	int RatingOf(size_t item, size_t subItem) const;
	void WriteRatingOf(size_t item, int rating);
	void ClaimEditContext(bool claim);
	void ClaimSelection(bool claim);
	bool RunEditKey(WPARAM key);
	bool RunPageKey(WPARAM key);
	bool RunRefreshKey(WPARAM key);
	void MoveFocusWithModifiers(t_size index);
	void RebuildGroups();
	void OnPlaylistContentChanged();
	void OnRowsMoved();
	void ClearGroupFocus();
	int Scaled(int pixelsAt96Dpi) const;
	// Column widths between 96 DPI and the screen, rounded as the header rounds them.
	int ScreenWidth(WidthAt96Dpi width) const;
	int WidthAt96DpiOf(int screenWidth) const;
	WidthAt96Dpi ColumnWidthAt96Dpi(size_t column) const;
	int m_lastClientWidth = -1;
	void ApplyFonts();
	LRESULT OnCoverDoubleClick(UINT, WPARAM, LPARAM position, BOOL& handled);
	LRESULT OnCoverButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled);
	size_t SelectCoverBlock(size_t block);
	void SelectForMenu(size_t first, size_t count);
	bool SelectPartWithModifiers(WPARAM keys, size_t first, size_t count);
	LRESULT OnCoverButtonUp(UINT, WPARAM, LPARAM, BOOL& handled);
	LRESULT OnCoverMouseMove(UINT, WPARAM buttons, LPARAM position, BOOL& handled);
	LRESULT OnPartRightButtonDown(UINT, WPARAM, LPARAM position, BOOL& handled);
	LRESULT OnHeaderButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled);
	LRESULT OnLabelButtonUp(UINT, WPARAM, LPARAM, BOOL& handled);
	LRESULT OnSelectPressedSection(UINT, WPARAM, LPARAM, BOOL&);
	// What a drag moves: the selected tracks, a whole album (started on an album header), or one
	// section under a label row such as "Disc 2" (started on that label).
	enum class DragUnit { Tracks, Album, Section, Cover };
	std::pair<size_t, size_t> AlbumRangeOf(size_t item) const;
	std::pair<size_t, size_t> SectionRangeOf(size_t item, size_t level) const;
	std::optional<size_t> LabelLevelAtPoint(CPoint clientPoint, size_t& item) const;
	size_t DropIndexAt(CPoint clientPoint, DragUnit unit, size_t level) const;
	LRESULT OnCoverWheel(UINT, WPARAM wheel, LPARAM, BOOL& handled);
	bool IsHiddenColumn(size_t column) const;
	bool InfoFollowsCover() const;
	bool IsCoverDrag(size_t column) const;
	int CoverSize() const;
	void DragCoverColumn(int width);
	void PushTrackColumns(const std::vector<size_t>& onScreen, size_t divider, int requestedWidth);
	int CellContentWidth(size_t item, size_t column, CDCHandle dc);
	int ColumnContentWidth(size_t column);
	void ShowTrackWidths(const std::vector<size_t>& onScreen, const std::vector<int>& widths);
	void FitColumnToContent(size_t column);
	LRESULT OnHeaderBeginDrag(int, LPNMHDR header, BOOL&);
	void WaitForColumnDrag(int x);
	void BeginColumnDrag(int column, int grabX);
	void MoveColumnDrag(int x);
	void EndColumnDrag(bool keep);
	void ShowColumnOrder(const std::vector<int>& order);
	bool RunColumnUndoKey(WPARAM key);
	// The columns as they are: the layout to rebuild them from, and, for the same columns, their list order, header order
	// and widths in pixels.
	struct ColumnState {
		ColumnLayout layout;
		std::vector<std::string> listNames;
		std::vector<int> order;
		std::vector<int> widths;
	};
	ColumnState CaptureColumnState();
	void RememberColumnState(ColumnState before);
	bool StepColumnState(std::vector<ColumnState>& from, std::vector<ColumnState>& to);
	void RestoreColumnState(const ColumnState& state);
	void SlideToColumnOrder(const std::vector<int>& order);
	LRESULT OnHeaderBeginTrack(int, LPNMHDR header, BOOL& handled);
	static double NowMs();
	bool AnimateColumnMoves() const;
	bool ColumnsMoving() const;
	std::optional<size_t> LiftedColumn() const;
	int ColumnOffset(size_t column) const;
	void DrawColumnFrame();
	void StartColumnClock();
	LRESULT OnColumnSlideFrame(UINT, WPARAM, LPARAM, BOOL&);
	void StartEffectClock();
	LRESULT OnEffectFrame(UINT, WPARAM, LPARAM, BOOL&);
	void SlideColumnWidths(const std::vector<size_t>& columns, const std::vector<int>& from, const std::vector<int>& to, std::function<void()> done);
	void FinishColumnWidthSlide();
	void ApplyColumnToggle(const std::string& name);
	double PopScaleOf(size_t item, size_t subItem, int star) const;
	void StartPop(size_t item, size_t subItem, int star);
	void DrawHeaderTitle(CDCHandle dc, size_t column, const CRect& itemRect);
	void PaintMovingHeader(HWND window);
	void ShowCoverSize();
	// The row at the top of the view, and how far its top is from the view's top (zero or less).
	struct TopRow {
		size_t row;
		int offset;
	};
	TopRow CurrentTopRow() const;
	void ResizeBlocks(TopRow top);
	LRESULT OnHeaderItemChanging(int, LPNMHDR header, BOOL& handled);
	LRESULT OnHeaderEndTrack(int, LPNMHDR header, BOOL& handled);
	LRESULT OnHeaderDividerDoubleClick(int, LPNMHDR header, BOOL& handled);
	LRESULT OnChildCustomDraw(int, LPNMHDR header, BOOL& handled);
	LRESULT DrawHeaderItem(NMCUSTOMDRAW& draw);
	LRESULT DrawTooltip(NMTTCUSTOMDRAW& draw);
	void ClearHeaderTitles();
	LRESULT OnColumnDragEnded(UINT, WPARAM column, LPARAM, BOOL&);
	void ApplyCoverColumnWidth();
	std::vector<size_t> TrackColumnsOnScreen() const;
	void SaveTrackColumnWeights();
	bool FitTrackColumns(bool updateView = true);
	void ApplyHeaderDragging();
	static LRESULT CALLBACK HeaderProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR view);
	CRect GetClientRectHook() const override;
	HFONT HeaderFont() const;
	std::string AlbumSummary(size_t baseItem) const;
	bool IsPlayingItem(size_t item) const;
	void RefreshPlayingRow();
	LRESULT OnDurationButtonDown(UINT, WPARAM, LPARAM position, BOOL& handled);
	int LabelLinesAbove(size_t item) const;
	int BlockRowsHeight(size_t block) const;
	int BlockHeight(size_t block) const;
	void ShowInfoColumn(bool show);
	void ShowGroupingColumns();
	void RememberPlaylistGrouping(const std::string& name);
	const ColumnDef* FindColumnDef(const std::string& name) const;
	void CaptureColumnWidths();
	void CaptureColumnWidths(bool readTrackWidths);
	ColumnLayout CapturedLayout();
	void RebuildColumns();
	void OnCustomColumnsChanged(const std::vector<std::pair<std::string, std::string>>& renames);
	void OnGroupPresetsChanged(const std::vector<std::pair<std::string, std::string>>& renames);
	void RenderSubgroupLabels(size_t item, const CRect& labelRect, CDCHandle dc);
	std::string SectionSummary(size_t item, size_t level) const;
	void RenderCover(size_t block, const CRect& blockRect, CDCHandle dc);
	void RenderInfoLines(size_t block, const CRect& blockRect, CDCHandle dc);
	void RenderPickedInfoLines(size_t block, const CRect& blockRect, CDCHandle dc);
	void LoadInfoLines();
	void OnInfoLinesChanged();
	std::optional<size_t> BlockInColumnAt(CPoint clientPoint, size_t column) const;

	void on_items_added(t_size base, metadb_handle_list_cref items, const bit_array& selection) override;
	void on_items_reordered(const t_size* order, t_size count) override;
	void on_items_removing(const bit_array& mask, t_size oldCount, t_size newCount) override;
	void on_items_removed(const bit_array& mask, t_size oldCount, t_size newCount) override;
	void on_items_selection_change(const bit_array& affected, const bit_array& state) override;
	void on_item_focus_change(t_size from, t_size to) override;
	void on_items_modified(const bit_array& mask) override;
	void on_items_modified_fromplayback(const bit_array& mask, play_control::t_display_level level) override;
	void on_items_replaced(const bit_array& mask, const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>& entries) override;
	void on_item_ensure_visible(t_size index) override;
	void on_playlist_switch() override;

	void on_playback_new_track(metadb_handle_ptr) override { UpdateItemsAll(); }
	void on_playback_stop(play_control::t_stop_reason) override { UpdateItemsAll(); }
	void on_playback_pause(bool) override { UpdateItemsAll(); }
	void on_playback_time(double) override { RefreshPlayingRow(); }
	void on_playback_seek(double) override { RefreshPlayingRow(); }

	const playlist_manager::ptr m_playlists = playlist_manager::get();
	// Built-in columns first, then the shared custom columns.
	std::vector<ColumnDef> m_columnDefs;
	ColumnLayout m_layout = DefaultColumnLayout();
	// One entry per list column, in list column order.
	std::vector<ListColumn> m_listColumns;
	StarHover m_starHover;
	// The shared presets. Never empty.
	std::vector<GroupPreset> m_presets;
	size_t m_activePreset = 0;
	bool m_ungrouped = false;
	bool m_autoSizeColumns = true;
	// True while this view sets column widths itself, so the header's notices of them are not taken for a drag.
	bool m_fittingColumns = false;
	// While the user drags the cover column's divider, the cover follows the width at the size held here (96 DPI).
	// The setting is saved, and every panel follows, when the drag ends.
	std::optional<int> m_dragCoverSize;
	// A track column's edge dragged while the columns fill the panel (PushTrackColumns): the track columns on screen,
	// the edge (right of columns[divider]), the pointer and the widths when the drag started, and the widths their
	// content needs.
	struct DividerDrag {
		std::vector<size_t> columns;
		size_t divider = 0;
		int startX = 0;
		std::vector<int> widths;
		std::vector<int> fits;
		std::vector<bool> fixedContent;
	};
	std::optional<DividerDrag> m_dividerDrag;
	// A column title dragged to move its column (OnHeaderBeginDrag). Waiting: the title was pressed and moved a little,
	// but not yet held long or moved far enough to move its column (ColumnDragStarts), so letting go is still a click.
	// Ended lasts until the mouse button comes up, so the header's click and its further drag notices are ignored.
	enum class HeaderDrag { None, Waiting, Dragging, Ended };
	HeaderDrag m_headerDrag = HeaderDrag::None;
	// When and where the header was last pressed (header client x), and the column whose title waits to move.
	DWORD m_headerPressTime = 0;
	int m_headerPressX = 0;
	int m_waitingColumn = -1;
	std::optional<ColumnDrag> m_columnDrag;
	// The columns before each change of their order, widths or set, the latest last, and the changes undone, for Ctrl+Z
	// and Ctrl+Y. They last while those changes are the latest in the panel: a change of the playlist drops them, and
	// Ctrl+Z undoes the playlist again.
	std::vector<ColumnState> m_columnUndo;
	std::vector<ColumnState> m_columnRedo;
	// The columns when a column drag or an edge drag started, kept until it ends.
	std::optional<ColumnState> m_columnsBeforeDrag;
	// Columns sliding to new places during and after a column drag; none without animation. m_columnFrameMs is the
	// time of the frame being drawn, so every part of one frame puts a column at the same place.
	std::optional<ColumnSlides> m_columnSlides;
	SmoothScrollTimingThread m_columnClock;
	double m_columnFrameMs = 0;
	static constexpr UINT kColumnSlideMessage = WM_APP + 44;
	// The panel's other animations (effects.h), drawn once per display refresh while any runs: covers fading in, track
	// column widths sliding (then done runs), and a clicked heart or star popping (star 0 is the heart).
	SmoothScrollTimingThread m_effectClock;
	static constexpr UINT kEffectFrameMessage = WM_APP + 45;
	struct ColumnWidthSlide {
		std::vector<size_t> columns;
		WidthSlide slide;
		std::function<void()> done;
	};
	std::optional<ColumnWidthSlide> m_columnWidthSlide;
	bool m_coversFading = false;
	struct Pop {
		size_t item;
		size_t subItem;
		int star;
		double startMs;
	};
	std::optional<Pop> m_pop;
	titleformat_object::ptr m_albumScript;
	titleformat_object::ptr m_blockScript;
	std::vector<titleformat_object::ptr> m_subgroupScripts;
	// Lines of text shown beside each cover. Zero hides the info column.
	int m_blockLineCount = 0;
	// The lines picked in the info popup for the active preset (info_lines.h), drawn instead of the cover pattern's text.
	// An item has its translated name, empty when the name is not shown, and its script. A line of the cover pattern
	// that is no item has a script of its own (WholePatternOf), or when it cannot stand alone the index of its part in
	// the cover pattern's text.
	struct InfoLine {
		std::string label;
		titleformat_object::ptr script;
		int patternLine = -1;
	};
	bool m_infoLinesPicked = false;
	std::vector<InfoLine> m_infoLines;
	GroupModel m_groups;
	CoverCache m_covers;
	TextPainter m_text;
	// The text of the tooltip on screen, which the tooltip is drawn with (see DrawTooltip).
	mutable std::wstring m_tooltipText;
	ViewSettings m_settings = LoadViewSettings();
	HFONT m_hostFont = NULL;
	CFont m_trackFont;
	CFont m_headerFont;
	CFont m_playingFont;
	// The rating stars, from a Windows icon font when one is installed (StarFont).
	CFont m_starFont;
	int m_starFontHeight = 0;
	bool m_starFontIsIcons = false;
	// The unit of the drag that is running, and for a section drag the label level it started on.
	DragUnit m_dragUnit = DragUnit::Tracks;
	size_t m_dragLevel = 0;
	HeaderSortMemory<GUID> m_headerSort;
	// True while this view sorts the playlist itself, so its own reorder does not count as someone else's.
	bool m_sorting = false;
	// The playlist this view shows, so its scroll position can be saved after the switch to another one.
	GUID m_shownPlaylist = pfc::guid_null;
	// The section under the label row the left button went down on, until the button comes up or a drag starts.
	std::optional<std::pair<size_t, size_t>> m_pressedSection;
	// A left press on a cover that has not yet become a click or a drag: where it was, and the cover's tracks.
	struct CoverPress { CPoint point; size_t first; size_t count; };
	std::optional<CoverPress> m_coverPress;
	// True while a drag that started on a cover runs. The list control does not know of it (its own flag for a
	// drag in progress is private), so DragDropAccept2 says the drop is a move inside the panel.
	bool m_coverDragActive = false;
	static constexpr UINT kSelectSectionMessage = WM_APP + 41;
	// Smooth wheel scrolling (smooth_scroll.h). The helper is made on the first animated wheel turn.
	static constexpr UINT kSmoothScrollMessage = WM_APP + 42;
	static constexpr UINT_PTR kSmoothScrollTimer = 0x47504c53;
	// Posted when the user lets go of a column divider, with the column in wParam,
	// so the columns are rebuilt or refitted after the header is done with the drag.
	static constexpr UINT kColumnDragEndedMessage = WM_APP + 43;
	std::optional<SmoothScrollHelper> m_smoothScroll;
	// True while the helper moves the view, so that move does not end the animation.
	bool m_animatingWheel = false;
	// True while the list control scrolls for a touchpad wheel message, so it adds no glide of its own (OnSmoothWheel).
	bool m_plainWheel = false;
	LRESULT OnSmoothWheel(UINT, WPARAM wheel, LPARAM point, BOOL& handled);
	LRESULT OnSmoothScrollFrame(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSmoothScrollTimer(UINT, WPARAM timer, LPARAM, BOOL& handled);
	void StopSmoothScroll();
	// The sort arrow a header click puts on its column goes away after this long (OnColumnHeaderClick).
	static constexpr UINT_PTR kSortIndicatorTimer = 0x47505341;
	static constexpr UINT kSortIndicatorMs = 3000;
	LRESULT OnSortIndicatorTimer(UINT, WPARAM timer, LPARAM, BOOL& handled);
	void HideSortIndicator();
	void ForgetHeaderSort();
	// Total length in seconds of each album, in album order.
	std::vector<double> m_albumSeconds;
	// Total length of the items before each item, one more entry than items, so a range sums in one step.
	std::vector<double> m_secondsBefore;
	// Item whose group header has keyboard focus, or SIZE_MAX. While set, no track row has focus in this view.
	size_t m_groupFocus = SIZE_MAX;
	t_size m_selectionStart = SIZE_MAX;
	std::optional<t_uint32> m_editContext;
	// Held while the panel has the focus, so panels that show the selected tracks (Selection Properties,
	// Album Art set to the selection) follow this playlist's selection.
	ui_selection_holder::ptr m_selectionHolder;
	// The row at the top of the view before rows are removed (on_items_removing, KeepTopRow).
	std::optional<TopRow> m_topBeforeRemoval;
	// Columns per playlist (EnterPlaylistColumns). The base is the panel's layout, set while the setting is on.
	// The layout on entering is set while a playlist is shown with the setting on, and tells a change apart.
	std::optional<ColumnLayout> m_baseLayout;
	std::optional<ColumnLayout> m_layoutOnEnter;
	GUID m_columnsPlaylist = pfc::guid_null;
	static std::optional<ColumnLayout> PlaylistColumns(t_size playlist);
	static void SetPlaylistColumns(t_size playlist, const ColumnLayout& layout);
	void EnterPlaylistColumns();
	void LeavePlaylistColumns();
	void SyncColumnsPerPlaylist();
	void KeepTopRow(const TopRow& before, const bit_array& removed, size_t oldCount);
};
