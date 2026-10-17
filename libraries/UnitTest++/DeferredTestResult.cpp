#include "DeferredTestResult.h"
#include "Config.h"

namespace UnitTest
{
  DeferredTestResult::DeferredTestResult(const std::string& suiteName, const std::string& testName)
    : suiteName(suiteName)
    , testName(testName)
  {}
}
