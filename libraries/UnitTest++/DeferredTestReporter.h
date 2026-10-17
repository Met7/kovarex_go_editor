#ifndef UNITTEST_DEFERREDTESTREPORTER_H
#define UNITTEST_DEFERREDTESTREPORTER_H

#include "TestReporter.h"
#include "DeferredTestResult.h"
#include "Config.h"

#include <vector>

namespace UnitTest
{

class DeferredTestReporter : public TestReporter
{
public:
    virtual void reportTestStart(TestDetails const& details) override;
    virtual void reportFailure(TestDetails const& details, char const* failure) override;
    virtual void reportTestFinish(const Test& test, TestTime time) override;
    virtual void setTag(const std::string& tag) override;

    typedef std::vector< DeferredTestResult > DeferredTestResultList;
    DeferredTestResultList& GetResults();

private:
    DeferredTestResultList m_results;
    std::string tag;
};

}

#endif
