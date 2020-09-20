#pragma once

#include "stdafx.h"

using namespace std;

class CommandContext;

// Code from Mini Engine
namespace GpuTimeCore
{
	void Initialize(uint32_t MaxNumTimers = 4096);
	void Shutdown(void);

	// Reserve a unique timer index
	uint32_t NewTimer(void);

	// Write start and stop time stamps on the GPU timeline
	void StartTimer(CommandContext& Context, uint32_t TimerIdx);
	void StopTimer(CommandContext& context, uint32_t TimerIdx);

	// Bookend all calls to GetTime() with Begin/End which correspond to Map/Unmap.
	// This needs to happen either at the very start or very end of a frame.
	void BeginReadBack(void);
	void EndReadBack(void);

	// Returns the time in milliseconds between start and stop queries
	float GetTime(uint32_t TimerIdx);
};

class GpuTimer
{
public:
	GpuTimer()
	{
		m_timerIdx = GpuTimeCore::NewTimer();
	}

	void Start(CommandContext& Context)
	{
		GpuTimeCore::StartTimer(Context, m_timerIdx);
	}

	void Stop(CommandContext& Context)
	{
		GpuTimeCore::StopTimer(Context, m_timerIdx);
	}

	float GetTime(void)
	{
		return GpuTimeCore::GetTime(m_timerIdx);
	}

	uint32_t GetTimerIndex(void)
	{
		return m_timerIdx;
	}

private:
	uint32_t m_timerIdx;
};

class GpuProfiler
{

};

