#pragma once
#include <CommonUtil/LowercaseString.hpp>
#include <stdint.h>
#include <string>
#include <vector>
namespace UnitTest{ class Test; }

namespace UnitTest
{
  class Suite
  {
  public:
    Suite(const std::string& name) : name(name) {}
    void addDependency(const LowercaseString& name) { this->dependencies.push_back(name); }
    void addDependency(Suite* suite);
    void setup();
    void assignTopologyRecursive(uint32_t* currentValue);
    void assignTopologyRecursiveUp(uint32_t* currentValue);
    bool operator<(const Suite& other) const;

    LowercaseString name;
    std::vector<LowercaseString> dependencies;
    std::vector<Suite*> myDependencies;
    std::vector<Suite*> dependentOnMe;
    std::vector<Test*> tests;
    uint32_t topologyOrder = uint32_t(-1);
  };

  class SuiteDependencyAdder
  {
  public:
    SuiteDependencyAdder(Suite* suite, const std::string& dependency);
  };

}
