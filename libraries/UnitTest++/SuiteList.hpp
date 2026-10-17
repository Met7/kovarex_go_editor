#pragma once
#include <Suite.hpp>
#include <map>
#include <memory>

namespace UnitTest
{
  class SuiteList
  {
  public:
    void setup();
    Suite* create(const LowercaseString& name);
    Suite* getSuite(const LowercaseString& name);
    static SuiteList& getInstance() { static SuiteList instance; return instance; }

    std::map<LowercaseString, std::unique_ptr<Suite>> suites;
  };
}
