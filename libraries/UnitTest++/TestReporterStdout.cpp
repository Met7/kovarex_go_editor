#include <CommonUtil/Util.hpp>
#include <TestReporterStdout.h>
#include <cstdio>
#include <Test.h>

// cstdio doesn't pull in namespace std on VC6, so we do it here.
#if defined(_MSC_VER) && (_MSC_VER == 1200)
	namespace std {}
#endif

namespace UnitTest {

void TestReporterStdout::reportFailure(const TestDetails& testDetails, char const* failure)
{
#if defined(__APPLE__) || defined(__GNUG__)
  std::printf("%s:%d: ", testDetails.filename, testDetails.lineNumber);
#else
  std::printf("%s(%d): ", testDetails.filename, testDetails.lineNumber);
#endif

  if (testDetails.checkId && *testDetails.checkId)
      std::printf("[%s] ", testDetails.checkId);
  else if (!this->tag.empty())
      std::printf("[%s] ", this->tag.c_str());

  std::printf("%s%s: %s\n", TestReporterStdout::failurePrefix, testDetails.name.c_str(), failure);
}

void TestReporterStdout::reportTestStart(TestDetails const& test)
{
  TestReporterStdout::reportTestStart(test.fullName());
}

void TestReporterStdout::reportTestFinish(const Test&, TestTime time)
{
  TestReporterStdout::reportFinishedTime(time);
}

void TestReporterStdout::reportSummary(int const totalTestCount, int const failedTestCount,
                                       int const failureCount, TestTime time)
{
  printf("Test time: %.2f seconds.\n", time.secondsElapsed);
  if (failureCount > 0)
    printf("%sFAILURE%s: %d out of %d tests failed (%d failures).\n\n",
           this->outputColors ? Util::colorize(Util::TextColor::Red).c_str() : "",
           this->outputColors ? Util::colorize(Util::TextColor::NoColor).c_str() : "",
           failedTestCount,
           totalTestCount,
           failureCount);
  else
    TestReporterStdout::reportSuccessSummary(this->outputColors, totalTestCount);
}

void TestReporterStdout::setTag(const std::string& tag)
{
    this->tag = tag;
}

void TestReporterStdout::reportTestStart(const std::string& name)
{
  printf("[%s] ", name.c_str());
  fflush(stdout);
}

void TestReporterStdout::reportFinishedTime(TestTime time)
{
  printf("[TIME: %.4fs, %lli]\n", time.secondsElapsed, static_cast<long long int>(time.nanoSecondsElapsed));
}

void TestReporterStdout::reportSuccessSummary(bool outputColors, int totalTestCount)
{
  printf("%sSuccess%s: %d tests passed.\n\n",
         outputColors ? Util::colorize(Util::TextColor::Green).c_str() : "",
         outputColors ? Util::colorize(Util::TextColor::NoColor).c_str() : "",
         totalTestCount);
}

}
