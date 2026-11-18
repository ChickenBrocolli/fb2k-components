#pragma once

#include <optional>
#include <vector>

#include <libPPUI/CListControlWithSelection.h>

#include "column_def.h"
#include "cover_cache.h"
#include "group_model.h"
#include "group_preset.h"
#include "rating_cell.h"
#include "rating_store.h"
#include "smooth_scroll.h"
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
	void ChoosePreset(size_t index);
	// "None" in the Groups menu: a plain track list with no group headers, covers or labels.
	// The active preset is kept, so choosing it again brings the groups back as they were.
	bool IsUngrouped() const { return m_ungrouped; }
	void SetUngrouped();
	// SetUngrouped for a choice of the user, remembered for the active playlist like ChoosePreset.
	void ChooseUngrouped();
	// The cover block under a point in client coordinates, if the point is on a cover.
	std::optional<size_t> CoverBlockAt(CPoint clientPoint) const;
	void ReloadCover(size_t block);
	void OpenCoverFolder(size_t block);
	// Sorts the playlist by the sort pattern of the active preset.
	void SortByActivePreset();
	// Keeps the current preset and returns false when no preset has this name.
	bool SelectPresetByName(const char* name);
	// Saves where ratings are kept and makes every panel read them from there.
	static void ApplyRatingStorage(RatingStorage storage);
	// Saves the look set on the Preferences page and redraws every panel with it.
	static void ApplyViewSettings(const ViewSettings& settings);
	// Redraws every panel with these settings without saving them, so the Scrolling page can show a change at once.
	// ApplyViewSettings, or this with LoadViewSettings(), puts the saved ones back.
	static void ShowViewSettings(const ViewSettings& settings);
	// The playlist font of the host. The panel uses it unless the Preferences page sets another font.
	void SetHostFont(HFONT font);
	// Saves the shared presets and updates every panel. renames pairs an old name with its new name.
	static void ApplyGroupPresets(const std::vector<GroupPreset>& presets, const std::vector<std::pair<std::string, std::string>>& renames);

	const std::vector<ColumnDef>& ColumnDefs() const { return m_columnDefs; }
	bool IsColumnShown(const std::string& name) const;
	void ToggleColumn(const std::string& name);
	// Reads the widths the user dragged, so call it when saving.
	ColumnLayout GetColumnLayout();
	void SetColumnLayout(const ColumnLayout& layout);
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
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLabelButtonUp)
		MESSAGE_HANDLER(kSelectSectionMessage, OnSelectPressedSection)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnPinnedHeaderButton)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnStarButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnCoverDoubleClick)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnCoverWheel)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnStarMouseMove)
		MESSAGE_HANDLER(WM_MOUSEWHEEL, OnSmoothWheel)
		MESSAGE_HANDLER(kSmoothScrollMessage, OnSmoothScrollFrame)
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
	// animation is left out of those moves. Keys and scroll bar arrows still use it.
	bool UserEnabledSmoothScroll() const override { return m_settings.smoothScroll && !m_animatingWheel; }
	void RenderGroupHeaderText2(size_t baseItem, const CRect& headerRect, const CRect& updateRect, CDCHandle dc) override;
	// The track counts at the right end of the headers and labels follow the window's width,
	// so a change of width repaints the whole list, not only the uncovered strip.
	void ListHandleResize() noexcept override;
	int GetItemHeight2(size_t item) const override;
	int GetItemHeight2Content(size_t item, int height) const override;
	void RenderItem(t_size item, const CRect& itemRect, const CRect& updateRect, CDCHandle dc) override;
	void RenderSubItemText(t_size item, t_size subItem, const CRect& subItemRect, const CRect& updateRect, CDCHandle dc, bool allowColors) override;
	void RenderGroupOverlayColumn(size_t baseItem, size_t subItem, const CRect& albumColumnRect, const CRect& updateRect, CDCHandle dc) override;
	void RenderOverlay2(const CRect& updateRect, CDCHandle dc) override;
	void OnViewOriginChange(CPoint delta) override;
	void EnsureVisibleRectAbs(const CRect& rect) override;
	bool OnColumnHeaderDrag(t_size listColumn, t_size newPosition) override;
	bool GetTooltipData(CPoint pointAbs, pfc::string_base& text, CRect& rect, CFontHandle& font) const override;
	void OnColumnHeaderClick(t_size listColumn) override;

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
	int StarSlotWidth() const;
	void RenderDimmedBrackets(const char* text, const CRect& textRect, CDCHandle dc);
	void SortPlaylist(const char* pattern, bool descending);
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
	bool RunEditKey(WPARAM key);
	bool RunPageKey(WPARAM key);
	void MoveFocusWithModifiers(t_size index);
	void CopySelection();
	void PasteClipboard();
	void RebuildGroups();
	void OnPlaylistContentChanged();
	void OnRowsMoved();
	void ClearGroupFocus();
	int Scaled(int pixelsAt96Dpi) const;
	int m_lastClientWidth = -1;
	void ApplyFonts();
	LRESULT OnCoverDoubleClick(UINT, WPARAM, LPARAM position, BOOL& handled);
	LRESULT OnCoverButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled);
	size_t SelectCoverBlock(size_t block);
	LRESULT OnHeaderButtonDown(UINT, WPARAM keys, LPARAM position, BOOL& handled);
	LRESULT OnLabelButtonUp(UINT, WPARAM, LPARAM, BOOL& handled);
	LRESULT OnSelectPressedSection(UINT, WPARAM, LPARAM, BOOL&);
	// What a drag moves: the selected tracks, a whole album (started on an album header), or one
	// section under a label row such as "Disc 2" (started on that label).
	enum class DragUnit { Tracks, Album, Section };
	std::pair<size_t, size_t> AlbumRangeOf(size_t item) const;
	std::pair<size_t, size_t> SectionRangeOf(size_t item, size_t level) const;
	std::optional<size_t> LabelLevelAtPoint(CPoint clientPoint, size_t& item) const;
	size_t DropIndexAt(CPoint clientPoint, DragUnit unit, size_t level) const;
	LRESULT OnCoverWheel(UINT, WPARAM wheel, LPARAM, BOOL& handled);
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
	ColumnLayout CapturedLayout();
	void RebuildColumns();
	void OnCustomColumnsChanged(const std::vector<std::pair<std::string, std::string>>& renames);
	void OnGroupPresetsChanged(const std::vector<std::pair<std::string, std::string>>& renames);
	void RenderSubgroupLabels(size_t item, const CRect& labelRect, CDCHandle dc);
	std::string SectionSummary(size_t item, size_t level) const;
	void RenderCover(size_t block, const CRect& blockRect, CDCHandle dc);
	void RenderInfoLines(size_t block, const CRect& blockRect, CDCHandle dc);

	void on_items_added(t_size base, metadb_handle_list_cref items, const bit_array& selection) override;
	void on_items_reordered(const t_size* order, t_size count) override;
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
	titleformat_object::ptr m_albumScript;
	titleformat_object::ptr m_blockScript;
	std::vector<titleformat_object::ptr> m_subgroupScripts;
	// Lines of text shown beside each cover. Zero hides the info column.
	int m_blockLineCount = 0;
	GroupModel m_groups;
	CoverCache m_covers;
	ViewSettings m_settings = LoadViewSettings();
	HFONT m_hostFont = NULL;
	CFont m_trackFont;
	CFont m_headerFont;
	CFont m_playingFont;
	// The unit of the drag that is running, and for a section drag the label level it started on.
	DragUnit m_dragUnit = DragUnit::Tracks;
	size_t m_dragLevel = 0;
	// The column the playlist was last sorted by from its header, and whether that sort was reversed.
	std::string m_sortColumn;
	bool m_sortDescending = false;
	// The playlist this view shows, so its scroll position can be saved after the switch to another one.
	GUID m_shownPlaylist = pfc::guid_null;
	// The section under the label row the left button went down on, until the button comes up or a drag starts.
	std::optional<std::pair<size_t, size_t>> m_pressedSection;
	static constexpr UINT kSelectSectionMessage = WM_APP + 41;
	// Smooth wheel scrolling (smooth_scroll.h). The helper is made on the first animated wheel turn.
	static constexpr UINT kSmoothScrollMessage = WM_APP + 42;
	static constexpr UINT_PTR kSmoothScrollTimer = 0x47504c53;
	std::optional<SmoothScrollHelper> m_smoothScroll;
	// True while the helper moves the view, so that move does not end the animation.
	bool m_animatingWheel = false;
	LRESULT OnSmoothWheel(UINT, WPARAM wheel, LPARAM, BOOL& handled);
	LRESULT OnSmoothScrollFrame(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSmoothScrollTimer(UINT, WPARAM timer, LPARAM, BOOL& handled);
	void StopSmoothScroll();
	// Total length in seconds of each album, in album order.
	std::vector<double> m_albumSeconds;
	// Total length of the items before each item, one more entry than items, so a range sums in one step.
	std::vector<double> m_secondsBefore;
	// Item whose group header has keyboard focus, or SIZE_MAX. While set, no track row has focus in this view.
	size_t m_groupFocus = SIZE_MAX;
	t_size m_selectionStart = SIZE_MAX;
	std::optional<t_uint32> m_editContext;
};
