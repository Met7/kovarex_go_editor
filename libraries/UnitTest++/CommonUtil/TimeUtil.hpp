#pragma once
#include <chrono>
#include <cstdint>

namespace TimeUtil
{
  class CpuTimer
  {
  public:

    using Clock = std::chrono::high_resolution_clock;
    using Duration = Clock::duration;

    CpuTimer()
      : startTime(Clock::now())
    {}

    double getSecondsPassed() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double, std::chrono::seconds::period>>(this->elapsed()).count();
    }

    double getMilliSecondsPassed() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double, std::chrono::milliseconds::period>>(this->elapsed()).count();
    }

    double getMicroSecondsPassed() const
    {
      return std::chrono::duration_cast<std::chrono::duration<double, std::chrono::microseconds::period>>(this->elapsed()).count();
    }

    int64_t getNanoSecondsPassedAsInteger() const
    {
      return std::chrono::duration_cast<std::chrono::duration<int64_t, std::chrono::nanoseconds::period>>(this->elapsed()).count();
    }

    Duration elapsed() const
    {
      return Clock::now() - this->startTime;
    }

    Clock::time_point getStartTime() const { return this->startTime; }

    void reset()
    {
      this->startTime = Clock::now();
    }

  private:
    Clock::time_point startTime;
  };
}
