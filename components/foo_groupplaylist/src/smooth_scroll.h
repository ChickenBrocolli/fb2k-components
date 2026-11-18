#pragma once

// Smooth wheel scrolling timed to the display.
// Adapted from smooth_scroll.h/.cpp of ui_helpers (https://github.com/reupen/ui_helpers),
// Copyright (c) Reupen Shah, BSD-3-Clause. The license text is in THIRD-PARTY-NOTICES.txt.
// Changes: vertical scrolling only; no wil, mmh or DirectComposition; the original eased animation is replaced by
// inertia (smooth_scroll_math.h). The display-timed thread, frame skipping and idle shutdown are ui_helpers's.

#include <atomic>
#include <functional>
#include <optional>
#include <thread>

#include "smooth_scroll_math.h"

// A thread that wakes once per display refresh and calls onTick while it is active.
// When painting takes long it skips refreshes, so the window is not flooded with frames.
class SmoothScrollTimingThread {
public:
	~SmoothScrollTimingThread() { Stop(); }

	void Start(std::function<void()> onTick);
	void Pause() { m_active.store(false, std::memory_order_release); }
	void Stop();
	bool HasThread() const { return m_thread.has_value(); }
	bool IsActive() const { return m_active.load(std::memory_order_acquire); }

private:
	std::atomic<bool> m_active{};
	std::optional<std::jthread> m_thread;
};

// Animates the vertical position of a window. The timing thread sends messageId to the window once per frame,
// and the window calls OnMessage. While idle the thread is paused, and timerId stops it after 2.5 seconds.
class SmoothScrollHelper {
public:
	using CurrentPositionFunc = std::function<int()>;
	using ClampPositionFunc = std::function<int(int position)>;
	using HandleScrollFunc = std::function<void(int newPosition)>;

	SmoothScrollHelper(HWND wnd, UINT messageId, UINT_PTR timerId, CurrentPositionFunc currentPosition,
		ClampPositionFunc clampPosition, HandleScrollFunc handleScroll);
	~SmoothScrollHelper() { ShutDown(); }

	// Pushes the view by delta pixels with these numbers, adding to the coasting that is running.
	void DeltaScroll(int delta, const smooth_scroll::InertiaParams& inertia);
	// Stops the coasting where the view is.
	void AbandonAnimation();
	bool IsAnimating() const { return m_momentum.has_value(); }
	// Whether a wheel message should be animated: a mouse wheel yes, a precision touchpad no.
	bool ShouldAnimateWheel(int wheelDelta);

	void OnMessage();
	void OnTimer();
	void ShutDown();

private:
	void Coast();
	void EndAnimation();
	void StartTimerThread();
	void PauseTimerThread();
	void StartShutdownTimer();
	void StopShutdownTimer();

	HWND m_wnd;
	UINT m_messageId;
	UINT_PTR m_timerId;
	CurrentPositionFunc m_currentPosition;
	ClampPositionFunc m_clampPosition;
	HandleScrollFunc m_handleScroll;
	std::optional<smooth_scroll::Momentum> m_momentum;
	smooth_scroll::WheelHistory m_wheelHistory;
	bool m_shutdownTimerActive = false;
	SmoothScrollTimingThread m_timingThread;
};
