#pragma once
#include "Config.h"
#include <Fixtures.h>
#include <Suite.hpp>
#include <SuiteList.hpp>

#ifdef TEST
    #error UnitTest++ redefines TEST
#endif

#ifdef TEST_FIXTURE_EX
    #error UnitTest++ redefines TEST_FIXTURE_EX
#endif

#define SUITE(Name)\
namespace Suite##Name\
{\
  namespace UnitTestSuite\
  {\
      inline char const* GetSuiteName()\
      {\
          return #Name;\
      }\
  }\
  UnitTest::Suite* currentSuite = UnitTest::SuiteList::getInstance().create(LowercaseString(#Name)); \
}\
namespace Suite##Name

#define DEPENDENCY(Name) UnitTest::SuiteDependencyAdder Name##Dependency(currentSuite, #Name)

#define TEST_FIXTURE_EX(FixtureNamespace, Fixture, Parameter, Name, List)                              \
    class Fixture##Name##Helper : public FixtureNamespace Fixture, public UnitTest::FixtureHelperBase \
    {                                                                           \
    public:                                                                     \
        Fixture##Name##Helper() : Fixture(Parameter) {}                         \
        void Run() override;                                                    \
    private:                                                                    \
        Fixture##Name##Helper(Fixture##Name##Helper const&) = delete;           \
        Fixture##Name##Helper& operator=(Fixture##Name##Helper const&) = delete; \
    };                                                                          \
                                                                                \
    class Test##Fixture##Name : public UnitTest::Test                           \
    {                                                                           \
    public:                                                                     \
        Test##Fixture##Name() : Test(#Name, currentSuite, __FILE__, __LINE__) {} \
    private:                                                                    \
        UnitTest::FixtureHelperBase* CreateFixtureHelper() const override;      \
    } test##Fixture##Name##Instance;                                            \
                                                                                \
    UnitTest::ListAdder adder##Fixture##Name (List, &test##Fixture##Name##Instance); \
                                                                                \
    UnitTest::FixtureHelperBase* Test##Fixture##Name::CreateFixtureHelper() const \
    {                                                                           \
        return new Fixture##Name##Helper();                                     \
    }                                                                           \
                                                                                \
    void Fixture##Name##Helper::Run()

#define TEST(Name) TEST_FIXTURE_EX(UnitTest::, NullFixture, , Name, UnitTest::TestList::getInstance())

#define TEST_FIXTURE(Fixture, Name) TEST_FIXTURE_EX(, Fixture, , Name, UnitTest::TestList::getInstance())
#define TEST_PARAMETRISED_FIXTURE(Fixture, Parameter, Name) TEST_FIXTURE_EX(, Fixture, Parameter, Name, UnitTest::TestList::getInstance())
