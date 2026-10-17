#pragma  once
#include <CommonUtil/TimeUtil.hpp>
#include <Test.h>

namespace UnitTest
{
  class TestReporter;
  class TestResults;

  class TestRunner
  {
  public:
    explicit TestRunner(TestReporter& reporter);
    ~TestRunner();

  protected:
    int Finish() const;
    void RunTest(Test* curTest);

    TestReporter* reporter;
    TestResults* result;
    TimeUtil::CpuTimer timer;
  };
}
