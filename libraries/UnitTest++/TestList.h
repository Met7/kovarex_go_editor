#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace UnitTest
{
  class Test;

  class TestList
  {
  public:
    TestList() = default;
    void Add(Test* test);
    void sort();
    const Test* getTest(const std::string& name) const;
    size_t testCount(std::string name) const;
    struct SuiteCountResult
    {
      std::vector<std::string> suits;
      size_t testCount = 0;
    };
    SuiteCountResult suiteCount(std::string name) const;
    static TestList& getInstance();

    std::vector<Test*> tests;
    std::map<std::string, const Test*> index;
  };

  class ListAdder
  {
  public:
    ListAdder(TestList& list, Test* test);
  };
}
