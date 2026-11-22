#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

// Keeps front covers scaled to one size, loaded in the background.
// Every method runs on the main thread. A cover is read and scaled on a worker thread,
// then handed back to the main thread, where onCoverReady tells the owner to repaint.
class CoverCache {
public:
	explicit CoverCache(std::function<void()> onCoverReady);
	~CoverCache();

	CoverCache(const CoverCache&) = delete;
	void operator=(const CoverCache&) = delete;

	// Calls draw(bitmap, madeForPixels, arrivedMs) with the cover of the track's file, fitted inside a square of
	// madeForPixels. While the cover loads at sizePixels, the same file's cover at the nearest other size stands in, and
	// draw scales it by sizePixels / madeForPixels. arrivedMs is when the cover finished loading (SteadyNowMs), so it can
	// fade in, or 0 when it took the place of another size that was already shown. The bitmap is valid only during the
	// call, and draw must not use the cache. When the file has none, foobar2000's stub image is drawn with stubWhenNone,
	// and nothing without. Nothing is drawn either while it loads with no other size to stand in.
	template <typename Draw>
	void DrawCover(const metadb_handle_ptr& track, int sizePixels, bool stubWhenNone, Draw&& draw) {
		if (const Found found = Get(track, sizePixels, stubWhenNone); found.bitmap) draw(found.bitmap, found.sizePixels, found.arrivedMs);
	}

	// Whether the track's file was read and has no cover. False while it has not been read yet.
	bool HasNoCover(const metadb_handle_ptr& track) const { return KnownWithoutCover(track->get_path()); }

	// Drops the covers of the track's file at every size, so the next Get reads the file again.
	void Forget(const metadb_handle_ptr& track);
	// Drops every cover, so each is read from its file again when it is next drawn.
	void Clear();

private:
	struct Found {
		HBITMAP bitmap = NULL;
		int sizePixels = 0;
		double arrivedMs = 0;
	};
	// No bitmap when the file has none (unless stubWhenNone), or while it loads with no other size loaded. A later call
	// may drop the bitmap.
	Found Get(const metadb_handle_ptr& track, int sizePixels, bool stubWhenNone);
	// The entry of path at sizePixels, made and loaded on first use. A track gives the file's cover, no track the stub image.
	Found Lookup(const std::string& path, const metadb_handle_ptr& track, int sizePixels);

	struct Entry {
		CBitmap bitmap;
		int sizePixels = 0;
		bool loading = true;
		uint64_t lastUse = 0;
		double arrivedMs = 0;
	};

	Found NearestLoadedSize(const std::string& path, int sizePixels);
	bool KnownWithoutCover(const std::string& path) const;

	void StartLoad(const std::string& key, const metadb_handle_ptr& track, int sizePixels);
	void FinishLoad(const std::string& key, HBITMAP bitmap);
	void DropLeastRecentlyUsed();

	std::function<void()> m_onCoverReady;
	std::map<std::string, Entry> m_entries;
	uint64_t m_useCounter = 0;
	// Set to false by the destructor. A load that finishes later sees it and drops its result.
	std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
