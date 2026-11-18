#pragma once

// Smooth wheel scrolling timed to the display.
// Adapted from smooth_scroll.h/.cpp of ui_helpers (https://github.com/reupen/ui_helpers),
// Copyright (c) Reupen Shah, BSD-3-Clause. The license text is in THIRD-PARTY-NOTICES.txt.
// Changes: vertical scrolling only; no wil, mmh or DirectComposition; the arithmetic lives in smooth_scroll_math.h.

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

	// Moves the view by delta pixels, adding to an animation that is running.
	void DeltaScroll(int delta);
	// Ends the animation. With updatePosition the view jumps to where the animation would have ended.
	void AbandonAnimation(bool updatePosition);
	bool IsAnimating() const { return m_animation.has_value(); }
	// Whether a wheel message should be animated: a mouse wheel yes, a precision touchpad no.
	bool ShouldAnimateWheel(int wheelDelta);

	void OnMessage();
	void OnTimer();
	void ShutDown();

private:
	void Scroll();
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
	std::optional<smooth_scroll::Animation> m_animation;
	smooth_scroll::WheelHistory m_wheelHistory;
	bool m_shutdownTimerActive = false;
	SmoothScrollTimingThread m_timingThread;
};
