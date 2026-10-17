#pragma once
#include <CommonUtil/LowercaseString.hpp>
#include <string>
namespace UnitTest{ class Suite; }

namespace UnitTest
{
  class TestDetails
  {
  public:
    TestDetails(char const* name, Suite* suite, char const* filename, int lineNumber, char const* checkId = 0);
    TestDetails(const TestDetails& other, char const* filename, int lineNumber, char const* checkId = 0);
    std::string fullName() const;
  private:
    TestDetails& operator=(TestDetails const&);

  public:
    LowercaseString name;
    Suite* suite = nullptr;
    char const* const filename;
    int const lineNumber;
    char const* const checkId;
  };
}

