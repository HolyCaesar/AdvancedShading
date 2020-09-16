#include "CPUProfiler.h"
#include "Utility.h"

CPUProfiler::CPUProfiler()
{
    SystemTime::Initialize();
}

CPUProfiler::~CPUProfiler()
{

}

void CPUProfiler::AddTimeStamp(string timeStampName)
{
    auto it = m_timeStampMap.find(timeStampName);
    if (it == m_timeStampMap.end())
    {
        auto test = m_timeStampMap.size();
        test += 1;
    }
    ASSERT(it == m_timeStampMap.end(), "The time stamp is existed");

    m_timeStampMap[timeStampName].Reset();
    m_timeStampMap[timeStampName].Start();
}

void CPUProfiler::EndTimeStamp(string timeStampName)
{
    auto it = m_timeStampMap.find(timeStampName);
    ASSERT(it != m_timeStampMap.end());

    m_timeStampMap[timeStampName].Stop();
}

/*****************************/
/*   System time definition  */
/*****************************/
double SystemTime::sm_CpuTickDelta = 0.0;

// Query the performance counter frequency
void SystemTime::Initialize(void)
{
    LARGE_INTEGER frequency;
    ASSERT(TRUE == QueryPerformanceFrequency(&frequency), "Unable to query performance counter frequency");
    sm_CpuTickDelta = 1.0 / static_cast<double>(frequency.QuadPart);
}

// Query the current value of the performance counter
int64_t SystemTime::GetCurrentTick(void)
{
    LARGE_INTEGER currentTick;
    ASSERT(TRUE == QueryPerformanceCounter(&currentTick), "Unable to query performance counter value");
    return static_cast<int64_t>(currentTick.QuadPart);
}

void SystemTime::BusyLoopSleep(float SleepTime)
{
    int64_t finalTick = (int64_t)((double)SleepTime / sm_CpuTickDelta) + GetCurrentTick();
    while (GetCurrentTick() < finalTick);
}

PDH_HQUERY CPUInfoReader::cpuQuery;
PDH_HCOUNTER CPUInfoReader::cpuTotal;
