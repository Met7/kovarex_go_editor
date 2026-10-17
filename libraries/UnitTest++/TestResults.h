#pragma once
#include <TestTime.h>
#include <string>

namespace UnitTest
{
  class TestReporter;
  class Test;
  class TestDetails;

  class TestResults
  {
  public:
    explicit TestResults(TestReporter* reporter = 0);
    TestResults(const TestResults&) = delete;

    void onTestStart(TestDetails const& test);
    void onTestFailure(const TestDetails& test, char const* failure);
    void onTestFinish(const Test& test, TestTime time);

    int getTotalTestCount() const;
    int getFailedTestCount() const;
    int getFailureCount() const;
    bool isCurrentTestFailed() const { return this->currentTestFailed; }
    TestReporter* getTestReporter() const { return this->testReporter; }

    TestResults& operator=(const TestResults&) = delete;
  private:
    TestReporter* testReporter;
    int totalTestCount = 0;
    int failedTestCount = 0;
    int failureCount = 0;
    bool currentTestFailed = false;
  };
}
