#pragma once
#include "TestReporter.h"
#include "DeferredTestResult.h"
#include "Config.h"
#include <vector>

namespace UnitTest
{

  class DeferredTestReporterStdout : public TestReporter
  {
  public:
    DeferredTestReporterStdout(bool outputColors) : outputColors(outputColors) {}
  protected:
    virtual void reportTestStart(TestDetails const& test) override;
    virtual void reportFailure(TestDetails const& test, char const* failure) override;
    virtual void reportTestFinish(const Test& test, TestTime time) override;
    virtual void reportSummary(int totalTestCount, int failedTestCount, int failureCount, TestTime time) override;
    virtual void setTag(const std::string& tag) override;

    typedef std::vector< DeferredTestResult > DeferredTestResultList;
    DeferredTestResultList& GetResults();

  private:
    bool outputColors;
    std::string tag;
    DeferredTestResultList m_results;
  };
}
