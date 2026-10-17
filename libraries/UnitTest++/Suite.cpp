#include <Suite.hpp>
#include <SuiteList.hpp>

namespace UnitTest
{
  void Suite::addDependency(Suite* suite)
  {
    suite->dependentOnMe.push_back(this);
    this->myDependencies.push_back(suite);
  }

  void Suite::setup()
  {
    for (const LowercaseString& dependency : this->dependencies)
      this->addDependency(SuiteList::getInstance().getSuite(dependency));
  }

  void Suite::assignTopologyRecursive(uint32_t* currentValue)
  {
   if (this->topologyOrder != uint32_t(-1))
      return;
    for (Suite* suite : this->myDependencies)
      suite->assignTopologyRecursiveUp(currentValue);
    this->topologyOrder = (*currentValue)++;
    for (Suite* suite : this->dependentOnMe)
      suite->assignTopologyRecursive(currentValue);
  }

  void Suite::assignTopologyRecursiveUp(uint32_t* currentValue)
  {
    if (this->topologyOrder != uint32_t(-1))
      return;
    for (Suite* suite : this->myDependencies)
      suite->assignTopologyRecursiveUp(currentValue);
    this->topologyOrder = (*currentValue)++;
  }

  bool Suite::operator<(const Suite& other) const
  {
    if (this->topologyOrder != other.topologyOrder)
      return this->topologyOrder < other.topologyOrder;
    return this->name < other.name;
  }

  SuiteDependencyAdder::SuiteDependencyAdder(Suite* suite, const std::string& dependency)
  {
    suite->addDependency(LowercaseString(dependency));
  }
}
