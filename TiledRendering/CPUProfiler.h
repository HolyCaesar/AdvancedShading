#pragma once

#include <unordered_map>
#include <Windows.h>
#include <winnt.h>
#include "psapi.h"
#include "TCHAR.h"
#include "pdh.h"


using namespace std;

class SystemTime
{
public:

	// Query the performance counter frequency
	static void Initialize(void);

	// Query the current value of the performance counter
	static int64_t GetCurrentTick(void);

	static void BusyLoopSleep(float SleepTime);

	static inline double TicksToSeconds(int64_t TickCount)
	{
		return TickCount * sm_CpuTickDelta;
	}

	static inline double TicksToMillisecs(int64_t TickCount)
	{
		return TickCount * sm_CpuTickDelta * 1000.0;
	}

	static inline double TimeBetweenTicks(int64_t tick1, int64_t tick2)
	{
		return TicksToSeconds(tick2 - tick1);
	}

private:

	// The amount of time that elapses between ticks of the performance counter
	static double sm_CpuTickDelta;
};

class CpuTimer
{
public:
	CpuTimer()
	{
		m_StartTick = 0ll;
		m_ElapsedTicks = 0ll;
	}

	void Start()
	{
		if (m_StartTick == 0ll)
			m_StartTick = SystemTime::GetCurrentTick();
	}

	void Stop()
	{
		if (m_StartTick != 0ll)
		{
			m_ElapsedTicks += SystemTime::GetCurrentTick() - m_StartTick;
			m_StartTick = 0ll;
		}
	}

	void Reset()
	{
		m_ElapsedTicks = 0ll;
		m_StartTick = 0ll;
	}

	double GetTime() const
	{
		return SystemTime::TicksToSeconds(m_ElapsedTicks);
	}

private:

	int64_t m_StartTick;
	int64_t m_ElapsedTicks;
};

// Get memory and cpu usage information in windows
// https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-globalmemorystatusex
// Other references for other operation system.
// https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
typedef
enum MEM_USAGE_UNIT
{
	BYTES = 0,
	BYTES_TO_KB = (BYTES + 1),
	BYTES_TO_MB = (BYTES_TO_KB + 1),
	BYTES_TO_GB = (BYTES_TO_MB + 1)
} MEM_USAGE_UNIT;

class MemoryInfoReader
{
public:
	MemoryInfoReader()
	{
		bytesToKB = 1.0 / 1024;
		bytesToMB = bytesToKB / 1024;
		bytesToGB = bytesToMB / 1024;
	}
	~MemoryInfoReader() {}

	inline double GetTotalPhysicalMemory(MEM_USAGE_UNIT memoUint = MEM_USAGE_UNIT::BYTES)
	{
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		DWORDLONG totalPhysMem = memInfo.ullTotalPhys;

		return ConvertToUnit(totalPhysMem, memoUint);
	}

	inline double GetCurTotalUsedPhysicalMemory(MEM_USAGE_UNIT memoUint = MEM_USAGE_UNIT::BYTES)
	{
		MEMORYSTATUSEX memInfo;
		memInfo.dwLength = sizeof(MEMORYSTATUSEX);
		GlobalMemoryStatusEx(&memInfo);
		DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
		return ConvertToUnit(physMemUsed, memoUint);
	}

	inline double GetCurProcUsedPhysicalMemory(MEM_USAGE_UNIT memoUint = MEM_USAGE_UNIT::BYTES)
	{
		PROCESS_MEMORY_COUNTERS_EX pmc;
		GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
		SIZE_T physMemUsedByCurProc = pmc.WorkingSetSize;
		return ConvertToUnit(physMemUsedByCurProc, memoUint);
	}

private:
	inline double ConvertToUnit(DWORDLONG val, MEM_USAGE_UNIT memoUint = MEM_USAGE_UNIT::BYTES)
	{
		if (memoUint == MEM_USAGE_UNIT::BYTES_TO_KB) return val * bytesToKB;
		if (memoUint == MEM_USAGE_UNIT::BYTES_TO_MB) return val * bytesToMB;
		if (memoUint == MEM_USAGE_UNIT::BYTES_TO_GB) return val * bytesToGB;
		return val;
	}

private:
	double bytesToKB;
	double bytesToMB;
	double bytesToGB;
};

// Need to read this: https://ladydebug.com/blog/2019/07/01/cpu-usage-programmatically/
// The code is here: https://ladydebug.com/blog/codes/cpuusage_win.htm
// TODO: Need to put this one in another thread
#define SystemProcessorPerformanceInformation 0x8
#define SystemBasicInformation    0x0
class CPUInfoReader
{
	typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
	{
		LARGE_INTEGER IdleTime;
		LARGE_INTEGER KernelTime;
		LARGE_INTEGER UserTime;
		LARGE_INTEGER Reserved1[2];
		ULONG Reserved2;
	} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;
	typedef struct _SYSTEM_BASIC_INFORMATION {
		ULONG Reserved;
		ULONG TimerResolution;
		ULONG PageSize;
		ULONG NumberOfPhysicalPages;
		ULONG LowestPhysicalPageNumber;
		ULONG HighestPhysicalPageNumber;
		ULONG AllocationGranularity;
		ULONG_PTR MinimumUserModeAddress;
		ULONG_PTR MaximumUserModeAddress;
		KAFFINITY ActiveProcessorsAffinityMask;
		CCHAR NumberOfProcessors;
	} SYSTEM_BASIC_INFORMATION, * PSYSTEM_BASIC_INFORMATION;

public:
	CPUInfoReader() 
	{
		m_waitSeconds = 3;
	}
	~CPUInfoReader() {}

	inline double GetCPUTotalUsage()
	{
		typedef DWORD(WINAPI* PNTQUERYSYSYTEMINFORMATION)(DWORD info_class, void* out, DWORD size, DWORD* out_size);
		PNTQUERYSYSYTEMINFORMATION pNtQuerySystemInformation = NULL;

		pNtQuerySystemInformation = (PNTQUERYSYSYTEMINFORMATION)GetProcAddress(GetModuleHandle(L"NTDLL.DLL"), "NtQuerySystemInformation");
		SYSTEM_BASIC_INFORMATION sbi;
		SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION* spi;

		DWORD returnlength;
		DWORD status = pNtQuerySystemInformation(SystemBasicInformation, &sbi,
			sizeof(SYSTEM_BASIC_INFORMATION), &returnlength);

		spi = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[sbi.NumberOfProcessors];

		memset(spi, 0, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * sbi.NumberOfProcessors);

		status = pNtQuerySystemInformation(SystemProcessorPerformanceInformation, spi,
			(sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * sbi.NumberOfProcessors), &returnlength);
		int numberOfCores = returnlength / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);

		printf("Number of cores: %d\n", numberOfCores);
		static ULARGE_INTEGER  ul_sys_idle_old[32];
		static ULARGE_INTEGER  ul_sys_kernel_old[32];
		static ULARGE_INTEGER  ul_sys_user_old[32];

		float          usage = 0;
		float          usageAccum = 0;

		printf("\n\nWait for %d seconds\n", m_waitSeconds);
		Sleep(m_waitSeconds * 1000);
		status = pNtQuerySystemInformation(SystemProcessorPerformanceInformation, spi,
			(sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * numberOfCores), &returnlength);

		usageAccum = 0;
		for (int ii = 0; ii < numberOfCores; ii++)
		{
			ULARGE_INTEGER        ul_sys_idle;
			ULARGE_INTEGER        ul_sys_kernel;
			ULARGE_INTEGER        ul_sys_user;

			ul_sys_idle.QuadPart = spi[ii].IdleTime.QuadPart;
			ul_sys_kernel.QuadPart = spi[ii].KernelTime.QuadPart;
			ul_sys_user.QuadPart = spi[ii].UserTime.QuadPart;

			ULONGLONG kernelTime = (ul_sys_kernel.QuadPart - ul_sys_kernel_old[ii].QuadPart);
			ULONGLONG usertime = (ul_sys_user.QuadPart - ul_sys_user_old[ii].QuadPart);
			ULONGLONG idletime = (ul_sys_idle.QuadPart - ul_sys_idle_old[ii].QuadPart);

			ULONGLONG proctime = kernelTime + usertime - idletime;

			ULONGLONG totaltime = kernelTime + usertime;

			usage = (float)(proctime * 100) / totaltime;
			usageAccum += usage;
			printf("Core        : %u: Usage      : %f%%\n", ii + 1, usage);
		}
		usageAccum /= numberOfCores;
		printf("----------------\nAverage for the last %d seconds: %f", m_waitSeconds, usageAccum);
		delete[] spi;
		return 0;
	}

	double GetCPUCurProcUsage() {}

private:
	uint64_t m_waitSeconds; // Waiting interval in seconds for reporting avg CPU usage
};


class CPUProfiler
{
public:
	CPUProfiler();
	~CPUProfiler();

	void AddTimeStamp(string name);
	void EndTimeStamp(string name);

	double GetTime(string name)
	{
		return m_timeStampMap[name].GetTime();
	}

	unordered_map<string, CpuTimer>* GetCpuTimes()
	{
		return &m_timeStampMap;
	}

	MemoryInfoReader m_memReader;
	CPUInfoReader m_cpuReader;

private:
	unordered_map<string, CpuTimer> m_timeStampMap;
};
