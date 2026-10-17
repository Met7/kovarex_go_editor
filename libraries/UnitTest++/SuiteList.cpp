#include <SuiteList.hpp>
#include <stdexcept>

namespace UnitTest
{
  Suite* SuiteList::create(const LowercaseString& name)
  {
    auto position = this->suites.find(name);
    if (position != this->suites.end())
      throw std::runtime_error("Duplicate suite definition: " + name.getValueExplicit());
    Suite* result = new Suite(name);
    this->suites[name] = std::unique_ptr<Suite>(result);
    return result;
  }

  Suite* SuiteList::getSuite(const LowercaseString& name)
  {
    auto position = this->suites.find(name);
    if (position == this->suites.end())
      throw std::runtime_error("Suite couldn't be found: " + name.getValueExplicit());
    return &*position->second;
  }

  void SuiteList::setup()
  {
    for (const auto& it : this->suites)
      it.second->setup();
    std::vector<Suite*> suitesWithoutDependency;
    for (const auto& it : this->suites)
      if (it.second->dependencies.empty())
        suitesWithoutDependency.push_back(&*it.second);

    uint32_t topologyValue = 0;
    for (Suite* suite : suitesWithoutDependency)
      suite->assignTopologyRecursive(&topologyValue);
  }
}
