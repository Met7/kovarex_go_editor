#pragma once
#include "Config.h"
#include <TestTime.h>
#include <string>
#include <vector>

namespace UnitTest
{
  struct DeferredTestResult
  {
    DeferredTestResult() = default;
    DeferredTestResult(const std::string& suiteName, const std::string& testName);

    using Failure = std::pair<int, std::string>;
    using FailureVec = std::vector<Failure>;

    std::string suiteName;
    std::string testName;
    std::string failureFile;
    std::string checkId;

    FailureVec failures;
    TestTime time {};
    bool failed = false;
  };
}
