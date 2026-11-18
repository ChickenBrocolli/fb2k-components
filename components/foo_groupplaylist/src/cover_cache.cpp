#include "stdafx.h"

#include <algorithm>

#include <SDK/album_art.h>
#include <SDK/threadsLite.h>
#include <libPPUI/gdiplus_helpers.h>

#include "cover_cache.h"

namespace {

constexpr size_t kMaxEntries = 256;

HBITMAP ScaleToBitmap(const album_art_data_ptr& data, int sizePixels) {
	auto image = GdiplusImageFromMem(data->get_ptr(), data->get_size());
	if (!image || image->GetLastStatus() != Gdiplus::Ok) return NULL;
	const UINT sourceWidth = image->GetWidth(), sourceHeight = image->GetHeight();
	if (sourceWidth == 0 || sourceHeight == 0) return NULL;

	const double scale = (std::min)(static_cast<double>(sizePixels) / sourceWidth, static_cast<double>(sizePixels) / sourceHeight);
	const int width = (std::max)(1, static_cast<int>(sourceWidth * scale + 0.5));
	const int height = (std::max)(1, static_cast<int>(sourceHeight * scale + 0.5));

	Gdiplus::Bitmap scaled(width, height, PixelFormat32bppPARGB);
	{
		Gdiplus::Graphics graphics(&scaled);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
		graphics.DrawImage(image.get(), 0, 0, width, height);
	}
	HBITMAP bitmap = NULL;
	scaled.GetHBITMAP(Gdiplus::Color(255, 255, 255), &bitmap);
	return bitmap;
}

// Runs on a worker thread. Returns NULL when the file has no front cover or it cannot be read.
HBITMAP LoadCover(const metadb_handle_ptr& track, int sizePixels) {
	try {
		GdiplusScope gdiplus;
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
