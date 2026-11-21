#pragma once

#include <memory>
#include <string_view>

// Draws and measures one line of text in the font and text colour selected into the DC.
// Every piece of text the playlist draws goes through here, so how text is drawn is decided in one place.
//
// Text is laid out and drawn with DirectWrite, which shows colour emoji and picks fallback fonts for any script.
// DirectWrite draws straight into a GDI bitmap (IDWriteBitmapRenderTarget), the way ui_helpers does for Columns UI,
// and laid-out lines are kept for reuse, so drawing costs about what GDI's did. The layout uses GDI-compatible
// metrics and GDI's rendering, so text lines up and looks as GDI text did. When DirectWrite cannot be used
// (it fails to start, or the font has no DirectWrite counterpart), the text is drawn with GDI.
// The "Text rendering" setting can ask for GDI (text_rendering.h). Then DirectWrite is not even started, since
// starting it is what Wine is suspected of crashing in.
class TextPainter {
public:
	enum class Align { Left, Right, Center };

	TextPainter();
	~TextPainter();
	TextPainter(const TextPainter&) = delete;
	void operator=(const TextPainter&) = delete;

	// Draws the text on one line, centred top to bottom in the rectangle and clipped to it.
	// With ellipsis, text that does not fit ends in "…".
	void Draw(CDCHandle dc, std::wstring_view text, const CRect& rect, Align align, bool ellipsis);

	// The width the text takes when drawn.
	int Width(CDCHandle dc, std::wstring_view text);

private:
	struct DirectWrite;
	// The DirectWrite drawer when the setting asks for it, made the first time it does. Null for GDI.
	DirectWrite* Drawer();
	std::unique_ptr<DirectWrite> m_directWrite;
};

// The alignment of a list column (HDF_LEFT, HDF_RIGHT or HDF_CENTER).
TextPainter::Align AlignOfColumnFormat(DWORD format);
