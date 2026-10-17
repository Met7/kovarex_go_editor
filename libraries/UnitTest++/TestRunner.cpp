#include <CommonUtil/Util.hpp>
#include <Suite.hpp>
#include <TestRunner.h>
#include <TestResults.h>
#include <TestReporter.h>
#include <TestReporterStdout.h>
#include <TestMemoryOutStream.h>
#include <cstring>
#include <CurrentTest.h>

namespace UnitTest
{
  TestRunner::TestRunner(TestReporter& reporter)
    : reporter(&reporter)
    , result(new TestResults(&reporter))
  {}

  TestRunner::~TestRunner()
  {
    delete this->result;
  }

  int TestRunner::Finish() const
  {
    const double timeElapsed = this->timer.getSecondsPassed();
    const int64_t nanosecondsElapsed = this->timer.getNanoSecondsPassedAsInteger();
    TestTime time { timeElapsed, nanosecondsElapsed };
    this->reporter->reportSummary(this->result->getTotalTestCount(),
                                  this->result->getFailedTestCount(),
                                  this->result->getFailureCount(),
                                  time);

    return this->result->getFailureCount();
  }

  void TestRunner::RunTest(Test* curTest)
  {
    CurrentTest::Results() = this->result;

    TimeUtil::CpuTimer testTimer;

    this->result->onTestStart(curTest->details);

    curTest->Run();

    const double timeElapsed = testTimer.getSecondsPassed();
    const int64_t nanosecondsElapsed = testTimer.getNanoSecondsPassedAsInteger();
    TestTime time { timeElapsed, nanosecondsElapsed };

    this->result->onTestFinish(*curTest, time);
  }
}
