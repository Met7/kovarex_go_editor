#pragma once
#include <TestTime.h>
#include <string>

namespace UnitTest
{
  class Test;
  class TestDetails;
  class TestReporter
  {
  public:
    virtual ~TestReporter();

    virtual void reportTestStart(TestDetails const& test) = 0;
    virtual void reportFailure(const TestDetails& test, char const* failure) = 0;
    virtual void reportTestFinish(const Test& test, TestTime time) = 0;
    virtual void reportSummary(int totalTestCount, int failedTestCount, int failureCount, TestTime time) = 0;
    virtual void setTag(const std::string& tag) = 0;
  };
}
