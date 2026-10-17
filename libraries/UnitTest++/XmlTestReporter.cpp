#include "XmlTestReporter.h"
#include "Config.h"

#include <iostream>
#include <string>

using std::string;
using std::ostringstream;
using std::ostream;

namespace {

void ReplaceChar(string& str, char c, string const& replacement)
{
    for (size_t pos = str.find(c); pos != string::npos; pos = str.find(c, pos + 1))
        str.replace(pos, 1, replacement);
}

string XmlEscape(string const& value)
{
    string escaped = value;

    ReplaceChar(escaped, '&', "&amp;");
    ReplaceChar(escaped, '<', "&lt;");
    ReplaceChar(escaped, '>', "&gt;");
    ReplaceChar(escaped, '\'', "&apos;");
    ReplaceChar(escaped, '\"', "&quot;");

    return escaped;
}

string BuildFailureMessage(string const& file, int line, string const& message)
{
  return file + '(' + std::to_string(line) + ')' + message;
}

}

namespace UnitTest {

XmlTestReporter::XmlTestReporter(ostream& ostream)
    : m_ostream(ostream)
{
}

void XmlTestReporter::reportSummary(int totalTestCount, int failedTestCount,
                                    int failureCount, TestTime time)
{
    AddXmlElement(m_ostream, NULL);

    BeginResults(m_ostream, totalTestCount, failedTestCount, failureCount, time);

    DeferredTestResultList const& results = GetResults();
    for (DeferredTestResultList::const_iterator i = results.begin(); i != results.end(); ++i)
    {
        BeginTest(m_ostream, *i);

        if (i->failed)
            AddFailure(m_ostream, *i);

        EndTest(m_ostream, *i);
    }

    EndResults(m_ostream);
}

void XmlTestReporter::AddXmlElement(ostream& os, char const* encoding)
{
    os << "<?xml version=\"1.0\"";

    if (encoding != NULL)
        os << " encoding=\"" << encoding << "\"";

    os << "?>";
}

void XmlTestReporter::BeginResults(std::ostream& os, int totalTestCount, int failedTestCount,
                                   int failureCount, TestTime time)
{
   os << "<unittest-results"
       << " tests=\"" << totalTestCount << "\""
       << " failedtests=\"" << failedTestCount << "\""
       << " failures=\"" << failureCount << "\""
       << " time=\"" << time.secondsElapsed << "\""
       << ">";
}

void XmlTestReporter::EndResults(std::ostream& os)
{
    os << "</unittest-results>";
}

void XmlTestReporter::BeginTest(std::ostream& os, DeferredTestResult const& result)
{
    os << "<test"
        << " suite=\"" << result.suiteName << "\""
        << " name=\"" << result.testName << "\""
        << " time=\"" << result.time.secondsElapsed << "\"";
}

void XmlTestReporter::EndTest(std::ostream& os, DeferredTestResult const& result)
{
    if (result.failed)
        os << "</test>";
    else
        os << "/>";
}

void XmlTestReporter::AddFailure(std::ostream& os, DeferredTestResult const& result)
{
    os << ">"; // close <test> element

    for (DeferredTestResult::FailureVec::const_iterator it = result.failures.begin();
         it != result.failures.end();
         ++it)
    {
        string const escapedMessage = XmlEscape(it->second);
        string const message = BuildFailureMessage(result.failureFile, it->first, escapedMessage);

        os << "<failure" << " message=\"" << message << "\"" << "/>";
    }
}

}
