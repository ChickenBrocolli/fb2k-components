#include "stdafx.h"

#include <algorithm>
#include <cstdlib>

#include <SDK/album_art.h>
#include <SDK/threadsLite.h>
#include <libPPUI/WindowsImaging.h>

#include "cover_cache.h"
#include "effects.h"

namespace {

constexpr size_t kMaxEntries = 256;
// The path of the stub image's entries. No file path starts with a control character.
const std::string kStubPath = "stub";

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

// Runs on a worker thread. Returns NULL when the file has no front cover or it cannot be read. With no track, loads
// foobar2000's stub image, the picture its preferences set for tracks without album art.
HBITMAP LoadCover(const metadb_handle_ptr& track, int sizePixels) {
	try {
		ComScope com;
		const auto manager = album_art_manager_v2::get();
		const auto extractor = track.is_valid()
			? manager->open(pfc::list_single_ref_t<metadb_handle_ptr>(track), pfc::list_single_ref_t<GUID>(album_art_ids::cover_front), fb2k::noAbort)
			: manager->open_stub(fb2k::noAbort);
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

CoverCache::Found CoverCache::Get(const metadb_handle_ptr& track, int sizePixels, bool stubWhenNone) {
	const std::string path = track->get_path();
	const Found found = Lookup(path, track, sizePixels);
	if (found.bitmap || !stubWhenNone || !KnownWithoutCover(path)) return found;
	return Lookup(kStubPath, metadb_handle_ptr(), sizePixels);
}

// Whether a load of the file at some size finished without a cover. One at the new size may still be loading, as
// while the cover size changes; the stub stays meanwhile.
bool CoverCache::KnownWithoutCover(const std::string& path) const {
	const std::string prefix = path + '|';
	for (auto entry = m_entries.lower_bound(prefix); entry != m_entries.end() && entry->first.compare(0, prefix.size(), prefix) == 0; ++entry) {
		if (!entry->second.loading && !entry->second.bitmap) return true;
	}
	return false;
}

CoverCache::Found CoverCache::Lookup(const std::string& path, const metadb_handle_ptr& track, int sizePixels) {
	// The size is part of the key, so a new cover size loads new images and the old ones age out.
	const std::string key = path + '|' + std::to_string(sizePixels);
	auto found = m_entries.find(key);
	if (found == m_entries.end()) {
		if (m_entries.size() >= kMaxEntries) DropLeastRecentlyUsed();
		found = m_entries.emplace(key, Entry()).first;
		found->second.sizePixels = sizePixels;
		StartLoad(key, track, sizePixels);
	}
	found->second.lastUse = ++m_useCounter;
	if (found->second.loading) return NearestLoadedSize(path, sizePixels);
	return { found->second.bitmap, sizePixels, found->second.arrivedMs };
}

// A finished entry without a bitmap means the file has no cover, and stands in for nothing.
CoverCache::Found CoverCache::NearestLoadedSize(const std::string& path, int sizePixels) {
	const std::string prefix = path + '|';
	Entry* nearest = nullptr;
	for (auto entry = m_entries.lower_bound(prefix); entry != m_entries.end() && entry->first.compare(0, prefix.size(), prefix) == 0; ++entry) {
		if (entry->second.loading || !entry->second.bitmap) continue;
		if (!nearest || std::abs(entry->second.sizePixels - sizePixels) < std::abs(nearest->sizePixels - sizePixels)) nearest = &entry->second;
	}
	if (!nearest) return {};
	nearest->lastUse = ++m_useCounter;
	return { nearest->bitmap, nearest->sizePixels };
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

// Loads still running finish into no entry and throw their images away, as after Forget.
void CoverCache::Clear() {
	m_entries.clear();
}

void CoverCache::FinishLoad(const std::string& key, HBITMAP bitmap) {
	const auto found = m_entries.find(key);
	if (found == m_entries.end()) {
		if (bitmap) DeleteObject(bitmap);
		return;
	}
	// A cover that nothing stood in for arrives on an empty place, and fades in there.
	const std::string path = key.substr(0, key.rfind('|'));
	if (!NearestLoadedSize(path, found->second.sizePixels).bitmap) found->second.arrivedMs = SteadyNowMs();
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
