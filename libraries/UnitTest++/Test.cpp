#include "Test.h"
#include "TestList.h"
#include "TestResults.h"
#include "AssertException.h"
#include <Suite.hpp>
#include "TerminateTestException.h"
#include "TestMemoryOutStream.h"
#include "CurrentTest.h"
#include <cstring>
#include <memory>

namespace UnitTest
{
  Test::Test(char const* testName, Suite* suite, char const* filename, int lineNumber)
    : details(testName, suite, filename, lineNumber)
  {
    suite->tests.push_back(this);
  }

  void Test::Run() const
  {
    CurrentTest::Details() = &details;

    FixtureHelperBase* fixtureHelper = nullptr;

    try
    {
        //UNITTEST_THROW_SIGNALS
        fixtureHelper = CreateFixtureHelper();
    }
    catch (...)
    {
        HandleExceptions("creating fixture");
        return;
    }

    try
    {
        //UNITTEST_THROW_SIGNALS
        fixtureHelper->Run();
    }
    catch (...)
    {
      HandleExceptions("running test");
    }

    try
    {
        //UNITTEST_THROW_SIGNALS
        delete fixtureHelper;
    }
    catch (...)
    {
        HandleExceptions("tearing down fixture");
    }
  }

  bool Test::operator<(const Test& other) const
  {
    if (this->details.suite != other.details.suite)
      return *this->details.suite < *other.details.suite;
    if (this->details.lineNumber != other.details.lineNumber)
      return this->details.lineNumber < other.details.lineNumber;
    if (int testNameDiff = strcmp(this->details.name.c_str(), other.details.name.c_str()))
      return testNameDiff < 0;
    if (int fileNameDiff = strcmp(this->details.filename, other.details.filename))
      return fileNameDiff < 0;
    return false;
  }

  void Test::HandleExceptions(const char* context) const
  {
    try
    {
      try
      {
        // Rethrow the previously catched exception to catch it again.
        throw;
      }
      catch (AssertException const& e)
      {
        CurrentTest::Results()->onTestFailure(TestDetails(details.name.c_str(), details.suite, e.Filename(), e.LineNumber()), e.what());
      }
      catch (TerminateTestException const&)
      {
        return;
      }
      catch (std::exception const& e)
      {
        this->reportException("Unhandled exception while ", context, e.what());
      }
      catch (...)
      {
        this->reportException("Unhandled error while ", context);
      }
    }
    catch (TerminateTestException const&)
    {
        return;
    }
  }

  void Test::reportException(const char* explanation, const char* context, const char* exceptionMessage) const
  {
    MemoryOutStream stream;
    stream << explanation << context << (exceptionMessage ? ": " : "") << (exceptionMessage ? exceptionMessage : "");
    CurrentTest::Results()->onTestFailure(details, stream.GetText());
  }
}
