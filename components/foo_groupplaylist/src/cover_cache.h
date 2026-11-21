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

	// Returns the cover of the track's file, fitted inside a square of sizePixels.
	// Returns NULL while the cover is loading and when the file has none.
	// The bitmap stays valid until the next call to Get.
	HBITMAP Get(const metadb_handle_ptr& track, int sizePixels);

	// Drops the covers of the track's file at every size, so the next Get reads the file again.
	void Forget(const metadb_handle_ptr& track);
	// Drops every cover, so each is read from its file again when it is next drawn.
	void Clear();

private:
	struct Entry {
		CBitmap bitmap;
		bool loading = true;
		uint64_t lastUse = 0;
	};

	void StartLoad(const std::string& key, const metadb_handle_ptr& track, int sizePixels);
	void FinishLoad(const std::string& key, HBITMAP bitmap);
	void DropLeastRecentlyUsed();

	std::function<void()> m_onCoverReady;
	std::map<std::string, Entry> m_entries;
	uint64_t m_useCounter = 0;
	// Set to false by the destructor. A load that finishes later sees it and drops its result.
	std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
