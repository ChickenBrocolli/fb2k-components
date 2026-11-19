#include "stdafx.h"

// The GDI text renderer, the layout cache and the emoji font fallback are adapted from ui_helpers (direct_write.cpp,
// direct_write_cache.h, direct_write_emoji.cpp) by Reupen Shah, BSD 3-Clause. The licence is in THIRD-PARTY-NOTICES.txt.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <dwrite_2.h>

#include "emoji_table.h"
#include "text_painter.h"

#pragma comment(lib, "dwrite.lib")

namespace {

UINT AlignFlag(TextPainter::Align align) {
	switch (align) {
	case TextPainter::Align::Right: return DT_RIGHT;
	case TextPainter::Align::Center: return DT_CENTER;
	default: return DT_LEFT;
	}
}

DWRITE_TEXT_ALIGNMENT DirectWriteAlign(TextPainter::Align align) {
	switch (align) {
	case TextPainter::Align::Right: return DWRITE_TEXT_ALIGNMENT_TRAILING;
	case TextPainter::Align::Center: return DWRITE_TEXT_ALIGNMENT_CENTER;
	default: return DWRITE_TEXT_ALIGNMENT_LEADING;
	}
}

void DrawWithGdi(CDCHandle dc, std::wstring_view text, const CRect& rect, TextPainter::Align align, bool ellipsis) {
	CRect drawRect = rect;
	const UINT flags = DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER | AlignFlag(align) | (ellipsis ? DT_END_ELLIPSIS : 0);
	dc.DrawText(text.data(), static_cast<int>(text.size()), drawRect, flags);
}

int WidthWithGdi(CDCHandle dc, std::wstring_view text) {
	SIZE size = {};
	if (!dc.GetTextExtent(text.data(), static_cast<int>(text.size()), &size)) return 0;
	return size.cx;
}

COLORREF ToColorRef(const DWRITE_COLOR_F& color) {
	const auto channel = [](float value) { return static_cast<BYTE>(std::clamp(std::round(value * 255.0f), 0.0f, 255.0f)); };
	return RGB(channel(color.r), channel(color.g), channel(color.b));
}

CComPtr<IDWriteFontFamily> FindFamily(IDWriteFontCollection* collection, const wchar_t* name) {
	UINT32 index = 0;
	BOOL exists = FALSE;
	CComPtr<IDWriteFontFamily> family;
	if (SUCCEEDED(collection->FindFamilyName(name, &index, &exists)) && exists) collection->GetFontFamily(index, &family.p);
	return family;
}

// Picks the font for emoji by the Unicode rules, which DirectWrite's own fallback does not follow:
// a character that can be either text or emoji is drawn as text after U+FE0E and as emoji after U+FE0F, and
// otherwise as its default presentation says. Characters the text's own font has (©, ®, ™, digits, #) keep that
// font. Emoji go to Segoe UI Emoji, text presentation to Segoe UI Symbol, and everything else to the system fallback.
class EmojiFontFallback : public IDWriteFontFallback {
public:
	EmojiFontFallback(IDWriteFontCollection* collection, IDWriteFontFallback* baseFallback)
		: m_baseFallback(baseFallback) {
		m_emojiFamily = FindFamily(collection, L"Segoe UI Emoji");
		m_textFamily = FindFamily(collection, L"Segoe UI Symbol");
		if (m_textFamily) m_textFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &m_textFont.p);
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** object) noexcept override {
		if (riid == __uuidof(IDWriteFontFallback) || riid == __uuidof(IUnknown)) {
			AddRef();
			*object = static_cast<IDWriteFontFallback*>(this);
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++m_refCount; }
	ULONG STDMETHODCALLTYPE Release() noexcept override {
		const ULONG count = --m_refCount;
		if (count == 0) delete this;
		return count;
	}

	HRESULT STDMETHODCALLTYPE MapCharacters(IDWriteTextAnalysisSource* source, UINT32 position, UINT32 length,
		IDWriteFontCollection* baseCollection, const wchar_t* baseFamilyName, DWRITE_FONT_WEIGHT baseWeight,
		DWRITE_FONT_STYLE baseStyle, DWRITE_FONT_STRETCH baseStretch, UINT32* mappedLength, IDWriteFont** mappedFont,
		FLOAT* scale) noexcept override {
		const wchar_t* text = nullptr;
		UINT32 available = 0;
		if (FAILED(source->GetTextAtPosition(position, &text, &available))) return E_FAIL;
		const std::wstring_view view(text, (std::min)(available, length));
		if (view.empty()) return m_baseFallback->MapCharacters(source, position, length, baseCollection, baseFamilyName, baseWeight, baseStyle, baseStretch, mappedLength, mappedFont, scale);

		CComPtr<IDWriteFont> baseFont;
		if (baseCollection && baseFamilyName) {
			if (const auto family = FindFamily(baseCollection, baseFamilyName)) family->GetFirstMatchingFont(baseWeight, baseStretch, baseStyle, &baseFont.p);
		}

		// A run of characters that all go to the same kind of font is mapped at once.
		const MapResult first = MapChar(view, baseFont, std::nullopt);
		size_t runLength = first.skipLength;
		while (runLength < view.size()) {
			const MapResult next = MapChar(view.substr(runLength), baseFont, first.type);
			if (next.type != first.type) break;
			runLength += next.skipLength;
		}

		if (first.type == EmojiType::NotEmoji) {
			return m_baseFallback->MapCharacters(source, position, static_cast<UINT32>(runLength), baseCollection, baseFamilyName,
				baseWeight, baseStyle, baseStretch, mappedLength, mappedFont, scale);
		}
		*mappedLength = static_cast<UINT32>(runLength);
		*scale = 1.0f;
		*mappedFont = nullptr;
		if (first.type == EmojiType::UseBaseFont) {
			if (baseFont) *mappedFont = baseFont.Detach();
			return S_OK;
		}
		const auto& family = first.type == EmojiType::Emoji ? m_emojiFamily : m_textFamily;
		if (family) family->GetFirstMatchingFont(baseWeight, baseStretch, baseStyle, mappedFont);
		return S_OK;
	}

private:
	enum class EmojiType { Emoji, Text, NotEmoji, UseBaseFont };
	struct MapResult {
		EmojiType type;
		size_t skipLength;
	};

	static size_t DecodeChar(std::wstring_view text, char32_t* decoded) {
		const wchar_t first = text[0];
		if (first >= 0xD800 && first <= 0xDBFF && text.size() > 1 && text[1] >= 0xDC00 && text[1] <= 0xDFFF) {
			*decoded = 0x10000 + ((static_cast<char32_t>(first) - 0xD800) << 10) + (static_cast<char32_t>(text[1]) - 0xDC00);
			return 2;
		}
		*decoded = first;
		return 1;
	}

	MapResult MapChar(std::wstring_view text, IDWriteFont* baseFont, std::optional<EmojiType> previousType) const {
		char32_t decoded = 0;
		const size_t decodedLength = DecodeChar(text, &decoded);
		const auto found = emoji_table::emojis.find(decoded);
		const auto props = found == emoji_table::emojis.end() ? std::nullopt : std::make_optional(found->second);

		const bool isEmoji = props && props->emoji;
		const bool isEmojiPresentation = props && props->emoji_presentation;
		const bool isComponent = props && props->component;
		const bool hasVariation = props && props->has_variation;
		const bool inEmojiSequence = previousType && *previousType == EmojiType::Emoji;

		const std::optional<wchar_t> nextChar = decodedLength < text.size() ? std::make_optional(text[decodedLength]) : std::nullopt;
		const bool nextIsTextSelector = nextChar && *nextChar == 0xFE0E;
		const bool nextIsEmojiSelector = nextChar && *nextChar == 0xFE0F;

		const auto baseHasCharacter = [&] {
			BOOL exists = FALSE;
			if (baseFont) baseFont->HasCharacter(decoded, &exists);
			return exists == TRUE;
		};

		const EmojiType type = [&] {
			if (!(isEmoji || isComponent)) return baseHasCharacter() ? EmojiType::UseBaseFont : EmojiType::NotEmoji;
			if (!inEmojiSequence && !isEmoji) return baseHasCharacter() ? EmojiType::UseBaseFont : EmojiType::NotEmoji;
			if (hasVariation && nextIsEmojiSelector) return EmojiType::Emoji;
			if (hasVariation && nextIsTextSelector) return EmojiType::Text;
			if (isEmoji && baseHasCharacter()) return EmojiType::UseBaseFont;
			if (isComponent && inEmojiSequence && !hasVariation) return EmojiType::Emoji;
			if (!isEmoji && isComponent && previousType && *previousType == EmojiType::UseBaseFont && !hasVariation) return EmojiType::UseBaseFont;
			if (isComponent && !isEmojiPresentation) return EmojiType::NotEmoji;
			if (!isEmojiPresentation) {
				BOOL exists = FALSE;
				if (m_textFont) m_textFont->HasCharacter(decoded, &exists);
				if (exists) return EmojiType::Text;
			}
			return isEmoji ? EmojiType::Emoji : EmojiType::NotEmoji;
		}();
		return MapResult{ type, decodedLength + ((nextIsTextSelector || nextIsEmojiSelector) ? 1 : 0) };
	}

	std::atomic<ULONG> m_refCount{ 1 };
	CComPtr<IDWriteFontFallback> m_baseFallback;
	CComPtr<IDWriteFontFamily> m_emojiFamily;
	CComPtr<IDWriteFontFamily> m_textFamily;
	CComPtr<IDWriteFont> m_textFont;
};

// The factories are shared by every panel. They are made once, on the main thread, where all drawing happens.
struct Factories {
	CComPtr<IDWriteFactory> dwrite;
	CComPtr<IDWriteFactory2> dwrite2;  // Colour glyphs. Null before Windows 8.1, where text is drawn in one colour.
	CComPtr<IDWriteGdiInterop> gdiInterop;
	CComPtr<IDWriteRenderingParams> gdiStyle;
	CComPtr<IDWriteFontFallback> emojiFallback;  // Null before Windows 8.1, where DirectWrite's own fallback is used.
	bool ok = false;

	Factories() {
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwrite.p)))) return;
		dwrite2 = dwrite;
		if (FAILED(dwrite->GetGdiInterop(&gdiInterop.p))) return;
		// GDI's own rendering, with the system's ClearType settings, so the text looks as it did before.
		CComPtr<IDWriteRenderingParams> system;
		if (FAILED(dwrite->CreateRenderingParams(&system.p))) return;
		if (FAILED(dwrite->CreateCustomRenderingParams(system->GetGamma(), system->GetEnhancedContrast(), system->GetClearTypeLevel(),
			system->GetPixelGeometry(), DWRITE_RENDERING_MODE_GDI_CLASSIC, &gdiStyle.p))) return;
		CComPtr<IDWriteFontFallback> systemFallback;
		CComPtr<IDWriteFontCollection> systemFonts;
		if (dwrite2 && SUCCEEDED(dwrite2->GetSystemFontFallback(&systemFallback.p)) && SUCCEEDED(dwrite->GetSystemFontCollection(&systemFonts.p))) {
			emojiFallback.Attach(new EmojiFontFallback(systemFonts, systemFallback));
		}
		ok = true;
	}

	static Factories& Get() {
		static Factories factories;
		return factories;
	}
};

// Draws the glyph runs of a text layout into the memory DC of a bitmap render target. A glyph run of a colour font
// (such as Segoe UI Emoji) is split into one run per colour layer, each drawn in its own colour.
class GdiTextRenderer : public IDWriteTextRenderer {
public:
	GdiTextRenderer(IDWriteBitmapRenderTarget* target, IDWriteRenderingParams* params, COLORREF color)
		: m_target(target), m_params(params), m_color(color) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** object) noexcept override {
		if (riid == __uuidof(IDWriteTextRenderer) || riid == __uuidof(IDWritePixelSnapping) || riid == __uuidof(IUnknown)) {
			AddRef();
			*object = static_cast<IDWriteTextRenderer*>(this);
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() noexcept override { return ++m_refCount; }
	ULONG STDMETHODCALLTYPE Release() noexcept override {
		const ULONG count = --m_refCount;
		if (count == 0) delete this;
		return count;
	}

	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* isDisabled) noexcept override {
		*isDisabled = FALSE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) noexcept override {
		return m_target->GetCurrentTransform(transform);
	}
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) noexcept override {
		*pixelsPerDip = m_target->GetPixelsPerDip();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT originX, FLOAT originY, DWRITE_MEASURING_MODE measuringMode,
		const DWRITE_GLYPH_RUN* glyphRun, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown*) noexcept override {
		CComPtr<IDWriteColorGlyphRunEnumerator> layers;
		if (const auto& factory2 = Factories::Get().dwrite2) {
			DWRITE_MATRIX transform = {};
			m_target->GetCurrentTransform(&transform);
			const HRESULT hr = factory2->TranslateColorGlyphRun(originX, originY, glyphRun, description, measuringMode, &transform, 0, &layers.p);
			if (FAILED(hr) && hr != DWRITE_E_NOCOLOR) return hr;
		}
		if (!layers) return m_target->DrawGlyphRun(originX, originY, measuringMode, glyphRun, m_params, m_color);

		for (;;) {
			BOOL hasRun = FALSE;
			if (FAILED(layers->MoveNext(&hasRun))) return E_FAIL;
			if (!hasRun) return S_OK;
			const DWRITE_COLOR_GLYPH_RUN* layer = nullptr;
			if (FAILED(layers->GetCurrentRun(&layer))) return E_FAIL;
			// Palette index 0xFFFF means the layer takes the text colour.
			const COLORREF color = layer->paletteIndex == 0xFFFF ? m_color : ToColorRef(layer->runColor);
			const HRESULT hr = m_target->DrawGlyphRun(layer->baselineOriginX, layer->baselineOriginY, measuringMode, &layer->glyphRun, m_params, color);
			if (FAILED(hr)) return hr;
		}
	}

	HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept override { return S_OK; }
	HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept override { return S_OK; }

	// The ellipsis at the end of trimmed text is an inline object, which draws itself through this renderer.
	HRESULT STDMETHODCALLTYPE DrawInlineObject(void* context, FLOAT originX, FLOAT originY, IDWriteInlineObject* inlineObject,
		BOOL isSideways, BOOL isRightToLeft, IUnknown* effect) noexcept override {
		return inlineObject->Draw(context, this, originX, originY, isSideways, isRightToLeft, effect);
	}

private:
	std::atomic<ULONG> m_refCount{ 1 };
	IDWriteBitmapRenderTarget* m_target;
	IDWriteRenderingParams* m_params;
	COLORREF m_color;
};

// LOGFONTs compared byte for byte, so each font selected into the DC maps to one text format.
struct LogFontLess {
	bool operator()(const LOGFONTW& a, const LOGFONTW& b) const { return memcmp(&a, &b, sizeof(LOGFONTW)) < 0; }
};

std::wstring UserLocale() {
	wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
	return GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0 ? std::wstring(name) : std::wstring(L"en-us");
}

std::wstring FamilyName(IDWriteFont* font) {
	CComPtr<IDWriteFontFamily> family;
	CComPtr<IDWriteLocalizedStrings> names;
	if (FAILED(font->GetFontFamily(&family.p)) || FAILED(family->GetFamilyNames(&names.p))) return {};
	UINT32 index = 0;
	BOOL exists = FALSE;
	if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || !exists) index = 0;
	UINT32 length = 0;
	if (FAILED(names->GetStringLength(index, &length))) return {};
	std::vector<wchar_t> buffer(length + 1);
	if (FAILED(names->GetString(index, buffer.data(), length + 1))) return {};
	return std::wstring(buffer.data(), length);
}

// A laid-out line and its size. Laying out is the costly part of drawing text, and the same lines are drawn
// again and again while the list scrolls, so the most recently used ones are kept.
struct CachedLayout {
	CComPtr<IDWriteTextLayout> layout;
	DWRITE_TEXT_METRICS metrics = {};
};

class LayoutCache {
public:
	struct Key {
		std::wstring text;
		IDWriteTextFormat* format;
		int width;  // -1 when measuring
		TextPainter::Align align;
		bool ellipsis;
		bool operator==(const Key& other) const {
			return format == other.format && width == other.width && align == other.align && ellipsis == other.ellipsis && text == other.text;
		}
	};

	const CachedLayout* Find(const Key& key) {
		const auto found = m_map.find(key);
		if (found == m_map.end()) return nullptr;
		m_list.splice(m_list.begin(), m_list, found->second);
		return &found->second->second;
	}

	const CachedLayout* Add(Key key, CachedLayout layout) {
		m_list.emplace_front(std::move(key), std::move(layout));
		m_map.emplace(m_list.front().first, m_list.begin());
		if (m_list.size() > kMaxSize) {
			m_map.erase(m_list.back().first);
			m_list.pop_back();
		}
		return &m_list.front().second;
	}

private:
	static constexpr size_t kMaxSize = 2000;
	struct KeyHash {
		size_t operator()(const Key& key) const {
			size_t hash = std::hash<std::wstring>()(key.text);
			hash ^= std::hash<const void*>()(key.format) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			hash ^= std::hash<int>()(key.width * 8 + static_cast<int>(key.align) * 2 + (key.ellipsis ? 1 : 0)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			return hash;
		}
	};
	std::list<std::pair<Key, CachedLayout>> m_list;
	std::unordered_map<Key, std::list<std::pair<Key, CachedLayout>>::iterator, KeyHash> m_map;
};

} // namespace

struct TextPainter::DirectWrite {
	struct Format {
		CComPtr<IDWriteTextFormat> format;
		CComPtr<IDWriteInlineObject> ellipsis;
	};

	CComPtr<IDWriteBitmapRenderTarget> target;
	std::map<LOGFONTW, Format, LogFontLess> formats;
	LayoutCache layouts;
	const std::wstring locale = UserLocale();

	// The format of the font selected into the DC. Null when the font has no DirectWrite counterpart.
	const Format* FormatFor(CDCHandle dc) {
		LOGFONTW logFont = {};
		if (!GetObjectW(dc.GetCurrentFont(), sizeof(logFont), &logFont)) return nullptr;
		auto found = formats.find(logFont);
		if (found == formats.end()) found = formats.emplace(logFont, MakeFormat(logFont)).first;
		return found->second.format ? &found->second : nullptr;
	}

	Format MakeFormat(const LOGFONTW& logFont) {
		Format made;
		auto& factories = Factories::Get();
		CComPtr<IDWriteFont> font;
		if (FAILED(factories.gdiInterop->CreateFontFromLOGFONT(&logFont, &font.p))) return made;
		const std::wstring family = FamilyName(font);
		if (family.empty()) return made;
		// A negative height is the em size; a positive one is the cell height, ascent plus descent.
		float size = 16.0f;
		if (logFont.lfHeight < 0) {
			size = static_cast<float>(-logFont.lfHeight);
		} else if (logFont.lfHeight > 0) {
			DWRITE_FONT_METRICS metrics = {};
			font->GetMetrics(&metrics);
			const float cell = static_cast<float>(metrics.ascent + metrics.descent);
			if (cell > 0) size = logFont.lfHeight * metrics.designUnitsPerEm / cell;
		}
		CComPtr<IDWriteTextFormat> format;
		if (FAILED(factories.dwrite->CreateTextFormat(family.c_str(), nullptr, font->GetWeight(), font->GetStyle(), font->GetStretch(),
			size, locale.c_str(), &format.p))) return made;
		format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
		format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		if (factories.emojiFallback) {
			CComQIPtr<IDWriteTextFormat1> format1(format.p);
			if (format1) format1->SetFontFallback(factories.emojiFallback);
		}
		CComPtr<IDWriteInlineObject> ellipsis;
		if (FAILED(factories.dwrite->CreateEllipsisTrimmingSign(format, &ellipsis.p))) return made;
		made.format = format;
		made.ellipsis = ellipsis;
		return made;
	}

	const CachedLayout* LayoutFor(std::wstring_view text, const Format& format, int width, Align align, bool ellipsis) {
		LayoutCache::Key key{ std::wstring(text), format.format, width, align, ellipsis };
		if (const CachedLayout* cached = layouts.Find(key)) return cached;

		CachedLayout made;
		// GDI-compatible metrics keep text widths whole pixels, as GDI measured them, so columns and rules line up as before.
		const float maxWidth = width < 0 ? 100000.0f : static_cast<float>(width);
		if (FAILED(Factories::Get().dwrite->CreateGdiCompatibleTextLayout(text.data(), static_cast<UINT32>(text.size()), format.format,
			maxWidth, 10000.0f, 1.0f, nullptr, FALSE, &made.layout.p))) return nullptr;
		made.layout->SetTextAlignment(DirectWriteAlign(align));
		if (ellipsis) {
			const DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
			made.layout->SetTrimming(&trimming, format.ellipsis);
		}
		if (FAILED(made.layout->GetMetrics(&made.metrics))) return nullptr;
		return layouts.Add(std::move(key), std::move(made));
	}

	bool EnsureTarget(CDCHandle dc, int width, int height) {
		if (!target) {
			if (FAILED(Factories::Get().gdiInterop->CreateBitmapRenderTarget(dc, width, height, &target.p))) return false;
			target->SetPixelsPerDip(1.0f);
			return true;
		}
		SIZE size = {};
		if (FAILED(target->GetSize(&size))) return false;
		if (width <= size.cx && height <= size.cy) return true;
		return SUCCEEDED(target->Resize((std::max)(width, static_cast<int>(size.cx)), (std::max)(height, static_cast<int>(size.cy))));
	}

	// Returns false when nothing was drawn, so the caller draws with GDI instead.
	bool Draw(CDCHandle dc, std::wstring_view text, const CRect& rect, Align align, bool ellipsis) {
		if (rect.IsRectEmpty()) return true;
		const Format* format = FormatFor(dc);
		if (!format) return false;
		const CachedLayout* line = LayoutFor(text, *format, rect.Width(), align, ellipsis);
		if (!line) return false;

		// Centred top to bottom on a whole pixel, as GDI's DT_VCENTER does.
		const int lineTop = rect.top + (rect.Height() - static_cast<int>(std::lround(line->metrics.height))) / 2;

		// Only the pixels the text covers are copied to the bitmap and back: the text's own box with a little room
		// for glyphs that reach past it, within the rectangle and the part of the DC that can be seen.
		CRect box(rect.left + static_cast<int>(std::floor(line->metrics.left)) - 2, rect.top,
			rect.left + static_cast<int>(std::ceil(line->metrics.left + line->metrics.width)) + 2, rect.bottom);
		CRect clip;
		if (dc.GetClipBox(clip) == ERROR) return false;
		if (!box.IntersectRect(box, rect) || !box.IntersectRect(box, clip)) return true;
		if (!EnsureTarget(dc, box.Width(), box.Height())) return false;

		// The text is drawn over a copy of what is already there, so ClearType blends with the real background.
		const HDC memory = target->GetMemoryDC();
		if (!BitBlt(memory, 0, 0, box.Width(), box.Height(), dc, box.left, box.top, SRCCOPY)) return false;
		CComPtr<IDWriteTextRenderer> renderer;
		renderer.Attach(new GdiTextRenderer(target, Factories::Get().gdiStyle, dc.GetTextColor()));
		if (FAILED(line->layout->Draw(nullptr, renderer, static_cast<float>(rect.left - box.left), static_cast<float>(lineTop - box.top)))) return false;
		return BitBlt(dc, box.left, box.top, box.Width(), box.Height(), memory, 0, 0, SRCCOPY) != FALSE;
	}

	// Returns -1 when the text cannot be measured here, so the caller measures with GDI instead.
	int Width(CDCHandle dc, std::wstring_view text) {
		const Format* format = FormatFor(dc);
		if (!format) return -1;
		const CachedLayout* line = LayoutFor(text, *format, -1, Align::Left, false);
		if (!line) return -1;
		return static_cast<int>(std::ceil(line->metrics.widthIncludingTrailingWhitespace));
	}
};

TextPainter::TextPainter() {
	if (Factories::Get().ok) m_directWrite = std::make_unique<DirectWrite>();
}

TextPainter::~TextPainter() = default;

void TextPainter::Draw(CDCHandle dc, std::wstring_view text, const CRect& rect, Align align, bool ellipsis) {
	if (m_directWrite && m_directWrite->Draw(dc, text, rect, align, ellipsis)) return;
	DrawWithGdi(dc, text, rect, align, ellipsis);
}

int TextPainter::Width(CDCHandle dc, std::wstring_view text) {
	if (m_directWrite) {
		const int width = m_directWrite->Width(dc, text);
		if (width >= 0) return width;
	}
	return WidthWithGdi(dc, text);
}

TextPainter::Align AlignOfColumnFormat(DWORD format) {
	switch (format & HDF_JUSTIFYMASK) {
	case HDF_RIGHT: return TextPainter::Align::Right;
	case HDF_CENTER: return TextPainter::Align::Center;
	default: return TextPainter::Align::Left;
	}
}
