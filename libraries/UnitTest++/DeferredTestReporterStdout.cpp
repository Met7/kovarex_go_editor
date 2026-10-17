#include <CommonUtil/Util.hpp>
#include <DeferredTestReporterStdout.h>
#include <TestDetails.h>
#include <Suite.hpp>
#include <TestReporterStdout.h>
#include <cstdio>

// cstdio doesn't pull in namespace std on VC6, so we do it here.
#if defined(_MSC_VER) && (_MSC_VER == 1200)
  namespace std {}
#endif

namespace UnitTest {

void DeferredTestReporterStdout::reportFailure(const TestDetails& details, char const* failure)
{
    DeferredTestResult& r = m_results.back();
    r.failed = true;
    r.failures.push_back(DeferredTestResult::Failure(details.lineNumber, failure));
    r.failureFile = details.filename;

#if defined(__APPLE__) || defined(__GNUG__)
    std::printf("%s:%d: ", details.filename, details.lineNumber);
#else
    std::printf("%s(%d): ", details.filename, details.lineNumber);
#endif

    if (details.checkId && *details.checkId)
    {
      std::printf("[%s] ", details.checkId);
      r.checkId = " [";
      r.checkId += details.checkId;
      r.checkId += "]";
    }
    else if (!this->tag.empty())
    {
      std::printf("[%s] ", this->tag.c_str());
      r.checkId = " [";
      r.checkId += this->tag;
      r.checkId += "]";
    }

    std::printf("error: Failure in %s: %s\n", details.name.c_str(), failure);
}

void DeferredTestReporterStdout::reportTestStart(TestDetails const& details)
{
  m_results.push_back(DeferredTestResult(details.suite->name, details.name));
  printf("[%s] ", details.fullName().c_str());
  fflush(stdout);
}

void DeferredTestReporterStdout::reportTestFinish(const Test& test, TestTime time)
{
  (void)test;
  DeferredTestResult& r = m_results.back();
  r.time = time;
  TestReporterStdout::reportFinishedTime(time);
}

void DeferredTestReporterStdout::reportSummary(int const totalTestCount, int const failedTestCount,
                                               int const failureCount, TestTime time)
{
  printf("Test time: %.2f seconds.\n", time.secondsElapsed);
  if (failureCount > 0)
  {
    printf("%sFAILURE%s: %d out of %d tests failed (%d failures).\n\n",
           this->outputColors ? Util::colorize(Util::TextColor::Red).c_str() : "",
           this->outputColors ? Util::colorize(Util::TextColor::NoColor).c_str() : "",
           failedTestCount,
           totalTestCount,
           failureCount);
    printf("Failed tests:\n");
    for (const auto& result: GetResults())
      if (result.failed)
      {
        printf("[%s/%s]\n", result.suiteName.c_str(), result.testName.c_str());
        for (const auto& failure: result.failures)
          printf(" %s:%d%s: %s\n", result.failureFile.c_str(), failure.first, result.checkId.c_str(), failure.second.c_str());
      }
  }
  else
    printf("%sSuccess%s: %d tests passed.\n\n",
           this->outputColors ? Util::colorize(Util::TextColor::Green).c_str() : "",
           this->outputColors ? Util::colorize(Util::TextColor::NoColor).c_str() : "",
           totalTestCount);
}

void DeferredTestReporterStdout::setTag(const std::string& tag)
{
  this->tag = tag;
}

DeferredTestReporterStdout::DeferredTestResultList& DeferredTestReporterStdout::GetResults()
{
    return m_results;
}

}
