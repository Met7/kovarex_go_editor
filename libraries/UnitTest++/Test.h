#pragma once
#include "Fixtures.h"
#include "TestDetails.h"

#ifdef UNITTEST_POSIX
  #include "Posix/SignalTranslator.h"
#else
  #define UNITTEST_THROW_SIGNALS (void)0;
#endif

namespace UnitTest
{
  class TestResults;
  class TestList;

  class Test
  {
  public:
    explicit Test(char const* testName, Suite* suite, char const* filename = "", int lineNumber = 0);
    virtual ~Test() = default;
    virtual void Run() const;
    bool operator<(const Test& other) const;
    void HandleExceptions(const char* context) const;
    void reportException(const char* explanation, const char* context, const char* exceptionMessage = nullptr) const;
    const std::string& str() const { return this->details.name.getValueExplicit(); }
  protected:
    virtual FixtureHelperBase* CreateFixtureHelper() const { return nullptr; };

    Test(Test const&) = delete;
    Test& operator =(Test const&) = delete;
  public:

    const TestDetails details;
  };
}
