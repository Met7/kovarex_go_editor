#pragma once

namespace UnitTest
{
  class TestResults;
  class TestDetails;
  namespace CurrentTest
  {
    TestResults*& Results();
    const TestDetails*& Details();
    TestDetails Details(char const* filename, int lineNumber, char const* checkId);
    void onTestFailure(char const* filename, int lineNumber, char const* checkId, char const* failure);
  }
}
