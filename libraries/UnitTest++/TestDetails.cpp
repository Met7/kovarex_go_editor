#include <CommonUtil/StringUtil.hpp>
#include <Suite.hpp>
#include <TestDetails.h>

namespace UnitTest
{
  TestDetails::TestDetails(char const* name, Suite* suite, char const* filename_, int lineNumber_, char const* checkId_)
    : name(name)
    , suite(suite)
    , filename(filename_)
    , lineNumber(lineNumber_)
    , checkId((checkId_ && *checkId_) ? checkId_ : 0)
  {}

  std::string TestDetails::fullName() const
  {
    return ssprintf("%s/%s",  this->suite->name.c_str(), this->name.c_str());
  }

  TestDetails::TestDetails(const TestDetails& details, char const* filename_, int lineNumber_, char const* checkId_)
    : name(details.name)
    , suite(details.suite)
    , filename(filename_)
    , lineNumber(lineNumber_)
    , checkId((checkId_ && *checkId_) ? checkId_ : 0)
  {}
}
