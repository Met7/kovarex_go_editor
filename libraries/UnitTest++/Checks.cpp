#include <Checks.h>

namespace UnitTest
{
  bool CheckEqual(char const* actual, char const* expected, MemoryOutStream& stream)
  {
    if (!std::strcmp(expected, actual))
      return true;

    stream << "Expected " << expected << " but was " << actual;
    return false;
  }

  static bool Contains(const char* actual,
                       const char* substring,
                       MemoryOutStream& stream)
  {
    if (std::strstr(actual, substring) != nullptr)
      return true;

    stream << "Expected that " << actual << " contains " << substring;
    return false;
  }

  static bool DoesNotContain(const char* actual,
                             const char* substring,
                             MemoryOutStream& stream)
  {
    if (std::strstr(actual, substring) == nullptr)
      return true;

    stream << "Expected that " << actual << " does not contain " << substring;
    return false;
  }

  bool CheckContains(const char* actual, const char* substring, MemoryOutStream& stream)
  {
    return Contains(actual, substring, stream);
  }

  bool CheckContains(const std::string& actual, const char* substring, MemoryOutStream& stream)
  {
    return Contains(actual.c_str(), substring, stream);
  }

  bool CheckContains(const std::string& actual, const std::string& substring, MemoryOutStream& stream)
  {
    return Contains(actual.c_str(), substring.c_str(), stream);
  }

  bool CheckDoesNotContain(const char* actual, const char* substring, MemoryOutStream& stream)
  {
    return DoesNotContain(actual, substring, stream);
  }

  bool CheckDoesNotContain(const std::string& actual, const char* substring, MemoryOutStream& stream)
  {
    return DoesNotContain(actual.c_str(), substring, stream);
  }

  bool CheckDoesNotContain(const std::string& actual, const std::string& substring, MemoryOutStream& stream)
  {
    return DoesNotContain(actual.c_str(), substring.c_str(), stream);
  }

  static bool DoesNotStartsWith(char const* actual, char const* prefix, MemoryOutStream& stream)
  {
    if (strlen(actual) < strlen(prefix) ||
        memcmp(prefix, actual, strlen(prefix)) != 0)
      return true;

    stream << "Expected that " << actual << " does not start with " << prefix;
    return false;
  }

  bool CheckDoesNotStartWith(char const* actual, char const* prefix, MemoryOutStream& stream)
  {
    return DoesNotStartsWith(actual, prefix, stream);
  }

  bool CheckDoesNotStartWith(const std::string& actual, const char* prefix, MemoryOutStream& stream)
  {
    return DoesNotStartsWith(actual.c_str(), prefix, stream);
  }

  bool CheckDoesNotStartWith(const std::string& actual, const std::string& prefix, MemoryOutStream& stream)
  {
    return DoesNotStartsWith(actual.c_str(), prefix.c_str(), stream);
  }
}
