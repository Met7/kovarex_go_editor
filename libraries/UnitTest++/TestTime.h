#pragma once
#include <cstdint>

namespace UnitTest
{
  struct TestTime
  {
    double secondsElapsed = 0;
    int64_t nanoSecondsElapsed = 0;
  };
}
