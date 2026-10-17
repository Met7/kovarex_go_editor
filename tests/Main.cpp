// Runs the tests: all of them, or the ones asked for.
//
//   kovarex_go_editor_tests [--test NAME]... [--suite NAME]... [--contains TEXT] [--list] [--no-colors]
//
// The exit code is the number of failed checks. The editor's own window opens
// for every scenario, so on a machine without a screen run it under xvfb-run.

#include <Suite.hpp>
#include <SuiteList.hpp>
#include <TestList.h>
#include <TestReporterStdout.h>
#include <TestRunner.h>

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
  struct Selection
  {
    std::vector<std::string> tests, suites;
    std::string contains;

    static std::string lower(std::string text)
    {
      std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
      return text;
    }

    bool wants(const UnitTest::Test& test) const
    {
      const std::string name  = lower(test.str());
      const std::string suite = test.details.suite ? lower(test.details.suite->name.getValueExplicit()) : std::string();
      if (this->tests.empty() && this->suites.empty() && this->contains.empty())
        return true;
      for (const std::string& wanted : this->tests)
        if (lower(wanted) == name)
          return true;
      for (const std::string& wanted : this->suites)
        if (lower(wanted) == suite)
          return true;
      return !this->contains.empty() &&
             (name.find(lower(this->contains)) != std::string::npos || suite.find(lower(this->contains)) != std::string::npos);
    }
  };

  class Runner : public UnitTest::TestRunner
  {
  public:
    using UnitTest::TestRunner::TestRunner;

    int run(const std::vector<UnitTest::Test*>& tests)
    {
      for (UnitTest::Test* test : tests)
        this->RunTest(test);
      return this->Finish();
    }
  };

  // The tests' settings and files go somewhere of their own, never over the
  // player's: Settings::path() looks at these, APPDATA first, on every system.
  void isolate()
  {
    const std::filesystem::path home = std::filesystem::temp_directory_path() / "kovarex_go_editor_tests";
    std::filesystem::create_directories(home);
#ifdef _WIN32
    _putenv_s("APPDATA", home.string().c_str());
#else
    setenv("APPDATA", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", home.c_str(), 1);
#endif
  }
}

int main(int argc, char** argv)
{
  Selection selection;
  bool list   = false;
  bool colors = true;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    const bool hasValue   = i + 1 < argc;
    if ((arg == "--test" || arg == "-t") && hasValue)
      selection.tests.push_back(argv[++i]);
    else if ((arg == "--suite" || arg == "-s") && hasValue)
      selection.suites.push_back(argv[++i]);
    else if (arg == "--contains" && hasValue)
      selection.contains = argv[++i];
    else if (arg == "--list")
      list = true;
    else if (arg == "--no-colors")
      colors = false;
    else
    {
      std::printf("Usage: %s [--test NAME]... [--suite NAME]... [--contains TEXT] [--list] [--no-colors]\n", argv[0]);
      return 1;
    }
  }

  isolate();
  SetTraceLogLevel(LOG_WARNING);

  UnitTest::SuiteList::getInstance().setup();
  UnitTest::TestList& all = UnitTest::TestList::getInstance();
  all.sort();

  std::vector<UnitTest::Test*> chosen;
  for (UnitTest::Test* test : all.tests)
    if (selection.wants(*test))
      chosen.push_back(test);

  if (list)
  {
    for (const UnitTest::Test* test : chosen)
      std::printf("%s\n", test->details.fullName().c_str());
    return 0;
  }
  if (chosen.empty())
  {
    std::printf("No test matches.\n");
    return 1;
  }

  UnitTest::TestReporterStdout reporter(colors);
  Runner runner(reporter);
  return runner.run(chosen);
}
