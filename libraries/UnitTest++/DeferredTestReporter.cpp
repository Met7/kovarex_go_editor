#include "DeferredTestReporter.h"
#include <Suite.hpp>
#include "TestDetails.h"

using namespace UnitTest;

void DeferredTestReporter::reportTestStart(TestDetails const& details)
{
  m_results.push_back(DeferredTestResult(details.suite->name, details.name));
}

void DeferredTestReporter::reportFailure(TestDetails const& details, char const* failure)
{
  DeferredTestResult& r = m_results.back();
  r.failed = true;
  r.failures.push_back(DeferredTestResult::Failure(details.lineNumber, failure));
  r.failureFile = details.filename;
}

void DeferredTestReporter::reportTestFinish(const Test&, TestTime time)
{
  DeferredTestResult& r = m_results.back();
  r.time = time;
}

void DeferredTestReporter::setTag(const std::string& tag)
{
  this->tag = tag;
}

DeferredTestReporter::DeferredTestResultList& DeferredTestReporter::GetResults()
{
  return m_results;
}
