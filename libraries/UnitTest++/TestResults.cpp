#include "TestResults.h"
#include "TestReporter.h"
#include "TerminateTestException.h"
#include "TestDetails.h"

namespace UnitTest
{

  TestResults::TestResults(TestReporter* testReporter)
      : testReporter(testReporter)
  {}

  void TestResults::onTestStart(TestDetails const& test)
  {
    ++this->totalTestCount;
    this->currentTestFailed = false;
    if (this->testReporter)
      this->testReporter->reportTestStart(test);
  }

  void TestResults::onTestFailure(const TestDetails& testDetails, char const* failure)
  {
    ++this->failureCount;
    if (!this->currentTestFailed)
    {
      ++this->failedTestCount;
      this->currentTestFailed = true;
    }

    if (this->testReporter)
      this->testReporter->reportFailure(testDetails, failure);

    throw TerminateTestException();
  }

  void TestResults::onTestFinish(const Test& test, TestTime time)
  {
    if (testReporter)
      testReporter->reportTestFinish(test, time);
  }

  int TestResults::getTotalTestCount() const
  {
    return totalTestCount;
  }

  int TestResults::getFailedTestCount() const
  {
    return failedTestCount;
  }

  int TestResults::getFailureCount() const
  {
    return failureCount;
  }
}
