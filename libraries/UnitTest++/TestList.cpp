#include <TestList.h>
#include <Test.h>
#include <CommonUtil/StringUtil.hpp>
#include <CommonUtil/Util.hpp>
#include <Suite.hpp>
#include <SuiteList.hpp>
#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <string>

namespace UnitTest
{
  TestList& TestList::getInstance()
  {
    static TestList staticList;
    return staticList;
  }

  void TestList::Add(Test* test)
  {
    this->tests.push_back(test);
    auto it = this->index.try_emplace(test->details.name, test);
    if (!it.second)
      throw std::runtime_error(ssprintf("Test name %s already exists owned by the suite: %s - all test names must be unique.",
                                        test->details.fullName().c_str(),
                                        it.first->second->details.name.c_str()));
  }

  void TestList::sort()
  {
    Util::sort(this->tests, Util::PointerLess<Test>());
  }

  const UnitTest::Test* TestList::getTest(const std::string& name) const
  {
    auto position = this->index.find(name);
    if (position == this->index.end())
      return nullptr;
    return position->second;
  }

  size_t TestList::testCount(std::string name) const
  {
    StringUtil::toLower(name);
    return Util::countIf(tests, [&name](Test* test){ return test->details.name.getLowercase().find(name) != std::string::npos; });
  }

  TestList::SuiteCountResult TestList::suiteCount(std::string suiteName) const
  {
    SuiteCountResult result;
    StringUtil::toLower(suiteName);
    for (const auto& it : SuiteList::getInstance().suites)
      if (it.first.getLowercase().find(suiteName) != std::string::npos)
      {
        result.suits.emplace_back(it.second->name);
        result.testCount += it.second->tests.size();
      }
    return result;
  }

  ListAdder::ListAdder(TestList& list, Test* test)
  {
    list.Add(test);
  }
}
