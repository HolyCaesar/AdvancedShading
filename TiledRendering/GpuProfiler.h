// A reference to the GPU profiler : https://github.com/TheRealMJP/DeferredTexturing/blob/experimental/SampleFramework12/v1.01/Graphics/Profiler.cpp

#pragma once

#include "stdafx.h"
#include <unordered_map>
#include "Utility.h"

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
public:
	GpuProfiler()
	{
	}

	~GpuProfiler()
	{
		GpuTimeCore::Shutdown();
	}

	void Initialize()
	{
		GpuTimeCore::Initialize();
	}

	void Start(string name, CommandContext& Context)
	{
		m_gpuTimerMap[name].Start(Context);
	}

	void Stop(string name, CommandContext& Context)
	{
		ASSERT(m_gpuTimerMap.find(name) != m_gpuTimerMap.end(), "No such time stamp of this name before.");
		m_gpuTimerMap[name].Stop(Context);
	}

	void Update()
	{
		GpuTimeCore::BeginReadBack();
		GpuTimeCore::EndReadBack();
	}

	float GetTime(string name)
	{
		return m_gpuTimerMap[name].GetTime();
	}

	unordered_map<string, GpuTimer>* GetGpuTimes()
	{
		return &m_gpuTimerMap;
	}
private:
	unordered_map<string, GpuTimer> m_gpuTimerMap;
};
