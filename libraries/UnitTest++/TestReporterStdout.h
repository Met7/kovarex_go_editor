#pragma once
#include "TestReporter.h"

namespace UnitTest
{
  class TestReporterStdout : public TestReporter
  {
  public:
    TestReporterStdout(bool outputColors) : outputColors(outputColors) {}
  private:
    virtual void reportTestStart(TestDetails const& test) override;
    virtual void reportFailure(const TestDetails& test, char const* failure) override;
    virtual void reportTestFinish(const Test& test, TestTime time) override;
    virtual void reportSummary(int totalTestCount, int failedTestCount, int failureCount, TestTime time) override;
    virtual void setTag(const std::string& tag) override;

    bool outputColors;
    std::string tag;
  public:
    static void reportTestStart(const std::string& name);
    static void reportFinishedTime(TestTime time);
    static void reportSuccessSummary(bool outputColors, int totalTestCount);
    static constexpr const char* failurePrefix = "error: Failure in ";
  };
}
