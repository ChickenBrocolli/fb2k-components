#include "stdafx.h"

#include <algorithm>

#include <SDK/album_art.h>
#include <SDK/threadsLite.h>
#include <libPPUI/WindowsImaging.h>

#include "cover_cache.h"

namespace {

constexpr size_t kMaxEntries = 256;

// Copies a premultiplied BGRA image into a top-down 32-bit DIB section, so AlphaBlend can draw it with its transparency.
HBITMAP ToBitmap(IWICBitmapSource* source) {
	const SIZE size = PP::WIC::bitmapSize(source);
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = size.cx;
	info.bmiHeader.biHeight = -size.cy;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	const HBITMAP bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!bitmap) return NULL;
	const UINT stride = static_cast<UINT>(size.cx) * 4;
	if (FAILED(source->CopyPixels(nullptr, stride, stride * static_cast<UINT>(size.cy), static_cast<BYTE*>(bits)))) {
		DeleteObject(bitmap);
		return NULL;
	}
	return bitmap;
}

// Windows Imaging reads every format Windows has a codec for (JPEG, PNG, GIF, BMP, TIFF, WebP, and HEIF, AVIF or JPEG XL
// when their extensions are installed), turns the picture upright from its EXIF orientation and converts its colour profile to sRGB.
HBITMAP ScaleToBitmap(const album_art_data_ptr& data, int sizePixels) {
	PP::WIC::WICContext wic;
	PP::WIC::WICContext::bitmapSourceArg_t fit;
	fit.wantSize = { sizePixels, sizePixels };
	const auto source = wic.CreateBitmapSource(wic.CreateDecoder(data->get_ptr(), data->get_size()), fit);
	return ToBitmap(wic.ChangePixelFormat(source, GUID_WICPixelFormat32bppPBGRA));
}

// Windows Imaging needs COM on the worker thread. A thread that already has COM in another mode keeps it.
class ComScope {
public:
	ComScope() : m_initialized(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
	~ComScope() { if (m_initialized) CoUninitialize(); }
	ComScope(const ComScope&) = delete;
	void operator=(const ComScope&) = delete;
private:
	const bool m_initialized;
};

// Runs on a worker thread. Returns NULL when the file has no front cover or it cannot be read.
HBITMAP LoadCover(const metadb_handle_ptr& track, int sizePixels) {
	try {
		ComScope com;
		auto extractor = album_art_manager_v2::get()->open(
			pfc::list_single_ref_t<metadb_handle_ptr>(track),
			pfc::list_single_ref_t<GUID>(album_art_ids::cover_front),
			fb2k::noAbort);
		return ScaleToBitmap(extractor->query(album_art_ids::cover_front, fb2k::noAbort), sizePixels);
	} catch (std::exception const&) {
		return NULL;
	}
}

} // namespace

CoverCache::CoverCache(std::function<void()> onCoverReady) : m_onCoverReady(std::move(onCoverReady)) {}

CoverCache::~CoverCache() {
	*m_alive = false;
}

HBITMAP CoverCache::Get(const metadb_handle_ptr& track, int sizePixels) {
	// The size is part of the key, so a new cover size loads new images and the old ones age out.
	const std::string key = std::string(track->get_path()) + '|' + std::to_string(sizePixels);
	auto found = m_entries.find(key);
	if (found == m_entries.end()) {
		if (m_entries.size() >= kMaxEntries) DropLeastRecentlyUsed();
		found = m_entries.emplace(key, Entry()).first;
		StartLoad(key, track, sizePixels);
	}
	found->second.lastUse = ++m_useCounter;
	return found->second.bitmap;
}

void CoverCache::StartLoad(const std::string& key, const metadb_handle_ptr& track, int sizePixels) {
	fb2k::inCpuWorkerThread([this, alive = m_alive, key, track, sizePixels] {
		const HBITMAP bitmap = LoadCover(track, sizePixels);
		fb2k::inMainThread([this, alive, key, bitmap] {
			if (*alive) {
				FinishLoad(key, bitmap);
			} else if (bitmap) {
				DeleteObject(bitmap);
			}
		});
	});
}

// A load still running for a dropped entry finds no entry when it finishes and throws its image away.
void CoverCache::Forget(const metadb_handle_ptr& track) {
	const std::string prefix = std::string(track->get_path()) + '|';
	for (auto entry = m_entries.begin(); entry != m_entries.end();) {
		if (entry->first.compare(0, prefix.size(), prefix) == 0) entry = m_entries.erase(entry);
		else ++entry;
	}
}

void CoverCache::FinishLoad(const std::string& key, HBITMAP bitmap) {
	const auto found = m_entries.find(key);
	if (found == m_entries.end()) {
		if (bitmap) DeleteObject(bitmap);
		return;
	}
	found->second.loading = false;
	if (!bitmap) return;
	found->second.bitmap.Attach(bitmap);
	m_onCoverReady();
}

// Entries still loading are kept, so a finished load always finds its entry or was evicted on purpose.
void CoverCache::DropLeastRecentlyUsed() {
	auto oldest = m_entries.end();
	for (auto walk = m_entries.begin(); walk != m_entries.end(); ++walk) {
		if (walk->second.loading) continue;
		if (oldest == m_entries.end() || walk->second.lastUse < oldest->second.lastUse) oldest = walk;
	}
	if (oldest != m_entries.end()) m_entries.erase(oldest);
}
