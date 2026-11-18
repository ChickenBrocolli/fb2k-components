#include "stdafx.h"

// Adapted from smooth_scroll.cpp of ui_helpers (https://github.com/reupen/ui_helpers),
// Copyright (c) Reupen Shah, BSD-3-Clause. The license text is in THIRD-PARTY-NOTICES.txt.

#include "smooth_scroll.h"

#include <array>
#include <chrono>
#include <limits>

#include <dwmapi.h>
#include <dxgi.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {

double NowMs() {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Average time the last frames took to paint.
class FrameTimeAverager {
public:
	void Add(double frameTime) {
		m_sum -= m_times[m_index];
		m_times[m_index] = frameTime;
		m_sum += frameTime;
		m_index = (m_index + 1) % m_times.size();
		if (m_count < m_times.size()) ++m_count;
	}
	double Average() const { return m_count == 0 ? 0.0 : m_sum / m_count; }

private:
	std::array<double, 25> m_times{};
	double m_sum = 0;
	size_t m_index = 0;
	size_t m_count = 0;
};

// Shortest of the last times between display refreshes.
class FrameTimeMinimum {
public:
	void Add(double frameTime) {
		const double evicted = m_times[m_index];
		m_times[m_index] = frameTime;
		m_index = (m_index + 1) % m_times.size();
		if (m_count < m_times.size()) ++m_count;
		if (frameTime <= m_minimum) {
			m_minimum = frameTime;
		} else if (evicted == m_minimum) {
			m_minimum = m_times[0];
			for (size_t i = 1; i < m_count; ++i) m_minimum = (std::min)(m_minimum, m_times[i]);
		}
	}
	double Minimum() const { return m_minimum; }

private:
	std::array<double, 10> m_times{};
	double m_minimum = (std::numeric_limits<double>::max)();
	size_t m_index = 0;
	size_t m_count = 0;
};

// The primary display output, whose refreshes the thread waits for. Null when DXGI is not available.
CComPtr<IDXGIOutput> PrimaryOutput() {
	CComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return nullptr;
	CComPtr<IDXGIAdapter1> adapter;
	if (FAILED(factory->EnumAdapters1(0, &adapter))) return nullptr;
	CComPtr<IDXGIOutput> output;
	if (FAILED(adapter->EnumOutputs(0, &output))) return nullptr;
	return output;
}

// Names the thread for a debugger. SetThreadDescription is looked up, not linked: it exists only from Windows 10 1607,
// and a linked call would stop the whole component from loading on Windows 7 and 8, which 64-bit foobar2000 supports.
void NameThisThread(const wchar_t* name) {
	using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
	const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
	if (!kernel) return;
	const auto setDescription = reinterpret_cast<SetThreadDescriptionFunc>(GetProcAddress(kernel, "SetThreadDescription"));
	if (setDescription) setDescription(GetCurrentThread(), name);
}

} // namespace

void SmoothScrollTimingThread::Start(std::function<void()> onTick) {
	m_active.store(true, std::memory_order_release);
	if (m_thread) return;

	m_thread.emplace([this, onTick = std::move(onTick)](std::stop_token stop) {
		NameThisThread(L"GroupPlaylist smooth scroll");
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		{
			CComPtr<IDXGIOutput> output = PrimaryOutput();
			FrameTimeAverager paintTimes;
			FrameTimeMinimum refreshTimes;
			double lastRefresh = NowMs() - 16.0;
			double refreshTime = 0;

			const auto waitForRefresh = [&] {
				if (!output || FAILED(output->WaitForVBlank())) DwmFlush();
				const double now = NowMs();
				refreshTime = now - lastRefresh;
				if (refreshTime > 1.0) refreshTimes.Add(refreshTime);
				lastRefresh = now;
			};

			while (!stop.stop_requested()) {
				waitForRefresh();
				// The display may be off or the session remote, and then no wait happens.
				const bool needToSleep = refreshTime < 1.0;
				if (stop.stop_requested()) break;

				if (m_active.load(std::memory_order_acquire)) {
					if (!needToSleep) {
						// When painting takes more than half a refresh, every other refresh (or more) is skipped.
						const int skip = (std::max)(0, static_cast<int>(paintTimes.Average() * 2 / refreshTimes.Minimum()));
						for (int i = 0; i < skip && !stop.stop_requested(); ++i) waitForRefresh();
					}
					const double frameStart = NowMs();
					onTick();
					paintTimes.Add(NowMs() - frameStart);
				}
				if (needToSleep) Sleep(15);
			}
		}
		CoUninitialize();
	});
}

void SmoothScrollTimingThread::Stop() {
	m_active.store(false, std::memory_order_release);
	if (!m_thread) return;
	m_thread->request_stop();
	m_thread.reset();  // joins
}

SmoothScrollHelper::SmoothScrollHelper(HWND wnd, UINT messageId, UINT_PTR timerId, CurrentPositionFunc currentPosition,
	ClampPositionFunc clampPosition, HandleScrollFunc handleScroll)
	: m_wnd(wnd), m_messageId(messageId), m_timerId(timerId), m_currentPosition(std::move(currentPosition)),
	m_clampPosition(std::move(clampPosition)), m_handleScroll(std::move(handleScroll)) {}

void SmoothScrollHelper::DeltaScroll(int delta, const smooth_scroll::InertiaParams& inertia) {
	using namespace smooth_scroll;
	const double now = NowMs();
	if (!m_momentum) m_momentum = StartMomentum(m_currentPosition(), now, inertia);
	Push(*m_momentum, delta, now, inertia);
	const bool hadThread = m_timingThread.HasThread();
	StartTimerThread();
	if (!hadThread) OnMessage();
}

void SmoothScrollHelper::AbandonAnimation() {
	if (IsAnimating()) EndAnimation();
}

void SmoothScrollHelper::EndAnimation() {
	PauseTimerThread();
	m_momentum.reset();
}

bool SmoothScrollHelper::ShouldAnimateWheel(int wheelDelta) {
	return smooth_scroll::ShouldAnimateWheel(m_wheelHistory, wheelDelta, GetTickCount64());
}

void SmoothScrollHelper::OnMessage() {
	if (m_timingThread.IsActive() && m_momentum) Coast();
}

// Inertia: coasts to this frame's position. Ends when the coasting is spent or the view reached the top or the bottom.
void SmoothScrollHelper::Coast() {
	smooth_scroll::Advance(*m_momentum, NowMs());
	const int asked = static_cast<int>(std::lround(m_momentum->position));
	m_handleScroll(asked);
	if (!m_momentum) return;  // handleScroll ended it
	if (m_currentPosition() != asked || smooth_scroll::MomentumDone(*m_momentum)) EndAnimation();
}

void SmoothScrollHelper::OnTimer() {
	StopShutdownTimer();
	m_timingThread.Stop();
}

void SmoothScrollHelper::ShutDown() {
	StopShutdownTimer();
	m_timingThread.Stop();
	m_momentum.reset();
}

void SmoothScrollHelper::StartTimerThread() {
	StopShutdownTimer();
	const HWND wnd = m_wnd;
	const UINT message = m_messageId;
	m_timingThread.Start([wnd, message] { SendMessageTimeout(wnd, message, 0, 0, SMTO_BLOCK, 50, nullptr); });
}

void SmoothScrollHelper::PauseTimerThread() {
	m_timingThread.Pause();
	StartShutdownTimer();
}

void SmoothScrollHelper::StartShutdownTimer() {
	SetTimer(m_wnd, m_timerId, 2500, nullptr);
	m_shutdownTimerActive = true;
}

void SmoothScrollHelper::StopShutdownTimer() {
	if (!m_shutdownTimerActive) return;
	m_shutdownTimerActive = false;
	KillTimer(m_wnd, m_timerId);
}
