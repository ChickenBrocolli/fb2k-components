#pragma once

#include <functional>
#include <string>

// Helpers for dialogs where the user writes a title formatting pattern.

// Calls back when the track a preview reads may have changed: the focus moved, another playlist became active, or
// the active playlist's tracks changed. A page that shows a preview keeps one, so the preview follows the track the
// user clicks while the page stays open.
class PreviewTrackWatcher : private playlist_callback_single_impl_base {
public:
	explicit PreviewTrackWatcher(std::function<void()> changed)
		: playlist_callback_single_impl_base(flag_on_item_focus_change | flag_on_playlist_switch | flag_on_items_added |
			flag_on_items_removed | flag_on_items_modified | flag_on_items_replaced),
		  m_changed(std::move(changed)) {}

private:
	void on_item_focus_change(t_size, t_size) override { m_changed(); }
	void on_playlist_switch() override { m_changed(); }
	void on_items_added(t_size, metadb_handle_list_cref, const bit_array&) override { m_changed(); }
	void on_items_removed(const bit_array&, t_size, t_size) override { m_changed(); }
	void on_items_modified(const bit_array&) override { m_changed(); }
	void on_items_replaced(const bit_array&, const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>&) override { m_changed(); }

	std::function<void()> m_changed;
};

// Shows a menu of common fields under the button. Inserts the chosen field at the caret of the edit control.
void RunInsertFieldMenu(CWindow button, CEdit patternEdit);

// What the pattern produces for the focused track of the active playlist, or why there is nothing to show.
std::string PatternPreviewValue(const char* pattern);

// One line for a preview control: "Preview:", a tab, and the value (PatternPreviewValue).
std::string DescribePatternPreview(const char* pattern);

class TextPainter;

// Draws an owner-drawn preview control (SS_OWNERDRAW) in the dialog's colours with the painter, so colour emoji show
// as in the playlist; a plain text control draws them in outline. Its text is split into lines at '\n', and a '\t'
// splits a line into a name and a value: the values line up after the first line's name.
void DrawPreviewText(HWND dialog, const DRAWITEMSTRUCT& item, TextPainter& painter);
