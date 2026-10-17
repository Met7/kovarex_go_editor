#pragma once
#include <Config.h>
#include <CurrentTest.h>
#include <TestResults.h>
#include <TestMemoryOutStream.h>
#include <algorithm>
#include <cstring>

namespace UnitTest
{
  template<typename Actual, typename Expected, typename Tolerance>
  bool AreClose(Actual const& actual, Expected const& expected, Tolerance const& tolerance)
  {
    return (actual >= (expected - tolerance)) && (actual <= (expected + tolerance));
  }

  template<typename Actual, typename Expected>
  void OnCheckEqualFailure(Actual const& actual, Expected const& expected, MemoryOutStream& stream)
  {
    stream << "Expected " << expected << " but was " << actual;
  }

  template<typename Actual, typename Expected>
  bool DoCheckEqual(Actual const& actual, Expected const& expected, MemoryOutStream& stream)
  {
    if (actual == expected)
      return true;
    OnCheckEqualFailure(actual, expected, stream);
    return false;
  }

  template<typename Actual, typename Expected>
  bool CheckEqualInternal(Actual const& actual, Expected const& expected, MemoryOutStream& stream)
  {
    if constexpr ((!std::is_integral_v<Actual> || !std::is_integral_v<Expected>) ||
                  (std::is_unsigned_v<Actual> == std::is_unsigned_v<Expected>))
      return DoCheckEqual(actual, expected, stream);
    else if constexpr (std::is_unsigned_v<Actual> && std::is_signed_v<Expected>)
    {
      if (expected < 0)
      { // negative values are never equal to unsigned values
        OnCheckEqualFailure(actual, expected, stream);
        return false;
      }
      return DoCheckEqual(actual, std::make_unsigned_t<Expected>(expected), stream);
    }
    else if constexpr (std::is_signed_v<Actual> && std::is_unsigned_v<Expected>)
    {
      if (actual < 0)
      { // negative values are never equal to unsigned values
        OnCheckEqualFailure(actual, expected, stream);
        return false;
      }
      return DoCheckEqual(std::make_unsigned_t<Actual>(actual), expected, stream);
    }
  }

  template<typename Type>
  bool CheckEqualInternal(Type const& actual, Type const& expected, MemoryOutStream& stream)
  {
    return DoCheckEqual(actual, expected, stream);
  }

  template<typename Actual, typename Expected>
  bool CheckEqual(Actual const& actual, Expected const& expected, MemoryOutStream& stream)
  {
    return CheckEqualInternal<std::remove_const_t<std::remove_reference_t<Actual>>,
                              std::remove_const_t<std::remove_reference_t<Expected>>>(actual, expected, stream);
  }

  template<typename Actual, typename Unexpected>
  void OnCheckUnequalFailure(Actual const& actual, Unexpected const& unexpected, MemoryOutStream& stream)
  {
    stream << "Did not expect " << unexpected << " but was " << actual;
  }

  template<typename Actual, typename Unexpected>
  bool DoCheckUnequal(Actual const& actual, Unexpected const& unexpected, MemoryOutStream& stream)
  {
    if (!(actual == unexpected))
      return true;
    OnCheckUnequalFailure(actual, unexpected, stream);
    return false;
  }

  template<typename Actual, typename Unexpected>
  bool CheckUnequalInternal(Actual const& actual, Unexpected const& unexpected, MemoryOutStream& stream)
  {
    if constexpr ((!std::is_integral_v<Actual> || !std::is_integral_v<Unexpected>) ||
                  (std::is_unsigned_v<Actual> == std::is_unsigned_v<Unexpected>))
      return DoCheckUnequal(actual, unexpected, stream);
    else if constexpr (std::is_unsigned_v<Actual> && std::is_signed_v<Unexpected>)
    {
      if (unexpected < 0)
        // negative values are always unequal to unsigned values
        return true;
      return DoCheckUnequal(actual, std::make_unsigned_t<Unexpected>(unexpected), stream);
    }
    else if constexpr (std::is_signed_v<Actual> && std::is_unsigned_v<Unexpected>)
    {
      if (actual < 0)
        // negative values are always unequal to unsigned values
        return true;
      return DoCheckUnequal(std::make_unsigned_t<Actual>(actual), unexpected, stream);
    }
  }

  template<typename Type>
  bool CheckUnequalInternal(Type const& actual, Type const& unexpected, MemoryOutStream& stream)
  {
    return DoCheckUnequal(actual, unexpected, stream);
  }

  template<typename Actual, typename Unexpected>
  bool CheckUnequal(Actual const& actual, Unexpected const& unexpected, MemoryOutStream& stream)
  {
    return CheckUnequalInternal<std::remove_const_t<std::remove_reference_t<Actual>>,
                                std::remove_const_t<std::remove_reference_t<Unexpected>>>(actual, unexpected, stream);
  }

  inline bool CheckBoolean(const char* string, bool value, MemoryOutStream& stream)
  {
    if (value)
      return true;
    stream << string;
    return false;
  }

  template<typename Actual, typename Expected>
  bool CheckBigger(Actual const& value, Expected const& biggerThan, MemoryOutStream& stream)
  {
    if (value > biggerThan)
      return true;

    stream << "Value " << value << " wasn't bigger than " << biggerThan;
    return false;
  }

  template<typename Actual, typename Expected>
  bool CheckBiggerEqual(Actual const& value, Expected const& biggerThan, MemoryOutStream& stream)
  {
    if (value >= biggerThan)
      return true;

    stream << "Value " << value << " wasn't bigger or equal than " << biggerThan;
    return false;
  }

  template<typename Actual, typename Expected>
  bool CheckSmaller(Actual const& value, Expected const& smallerThan, MemoryOutStream& stream)
  {
    if (value < smallerThan)
      return true;

    stream << "Value " << value << " wasn't smaller than " << smallerThan;
    return false;
  }

  template<typename Actual, typename Expected>
  bool CheckSmallerEqual(Actual const& value, Expected const& smallerThan, MemoryOutStream& stream)
  {
    if (value <= smallerThan)
      return true;

    stream << "Value " << value << " wasn't smaller or equal than " << smallerThan;
    return false;
  }

  template<typename Actual, typename ExpectedLower, typename ExpectedUpper>
  bool CheckBetween(Actual const& value, ExpectedLower const& lowerBound, ExpectedUpper const& upperBound, MemoryOutStream& stream)
  {
    if (value > lowerBound && value < upperBound)
      return true;

    stream << "Value " << value << " wasn't between " << lowerBound << " and " << upperBound << " (exclusive)";
    return false;
  }

  template<typename Actual, typename ExpectedLower, typename ExpectedUpper>
  bool CheckBetweenEqual(Actual const& value, ExpectedLower const& lowerBound, ExpectedUpper const& upperBound, MemoryOutStream& stream)
  {
    if (value >= lowerBound && value <= upperBound)
      return true;

    stream << "Value " << value << " wasn't between " << lowerBound << " and " << upperBound << " (inclusive)";
    return false;
  }

  bool CheckEqual(char const* actual, char const* expected, MemoryOutStream& stream);

  template<typename Actual>
  bool CheckNull(Actual const& actual, MemoryOutStream& stream)
  {
    if (actual == nullptr)
      return true;
    stream << "Expected nullptr, but was non-null";
    return false;
  }

  template<typename Actual>
  bool CheckNotNull(Actual const& actual, MemoryOutStream& stream)
  {
    if (actual != nullptr)
      return true;
    stream << "Expected non-null, but was nullptr";
    return false;
  }

  template<typename Actual, typename Expected, typename Tolerance>
  bool CheckClose(Actual const& actual, Expected const& expected, Tolerance const& tolerance,
                  MemoryOutStream& stream)
  {
    if (AreClose(actual, expected, tolerance))
      return true;

    stream << "Expected " << expected << " +/- " << tolerance << " but was " << actual;
    return false;
  }

  template<typename Actual, typename Expected, typename Tolerance>
  bool CheckNotClose(Actual const& actual, Expected const& expected, Tolerance const& tolerance,
                     MemoryOutStream& stream)
  {
    if (!AreClose(actual, expected, tolerance))
      return true;

    stream << "Expected " << expected << " >+/<- " << tolerance << " but was " << actual;
    return false;
  }

  template<typename Actual, typename Expected, typename Tolerance>
  bool CheckClosePercentage(Actual const& actual, Expected const& expected, Tolerance const& tolerancePercentage,
                            MemoryOutStream& stream)
  {
    Expected toleranceAbsolute = tolerancePercentage * expected / 100;
    if (toleranceAbsolute < 0)
      toleranceAbsolute = -toleranceAbsolute;

    if (AreClose(actual, expected, toleranceAbsolute))
      return true;

    stream << "Expected " << expected << " +/- " << tolerancePercentage << "% (" << toleranceAbsolute << ") but was " << actual;
    return false;
  }

  bool CheckContains(const char* actual, const char* substring, MemoryOutStream& stream);
  bool CheckContains(const std::string& actual, const char* substring, MemoryOutStream& stream);
  bool CheckContains(const std::string& actual, const std::string& substring, MemoryOutStream& stream);

  bool CheckDoesNotContain(const char* actual, const char* substring, MemoryOutStream& stream);
  bool CheckDoesNotContain(const std::string& actual, const char* substring, MemoryOutStream& stream);
  bool CheckDoesNotContain(const std::string& actual, const std::string& substring, MemoryOutStream& stream);


  template<class T>
  concept has_cbegin = requires(T& value) { value.cbegin(); };
  template<class T>
  concept has_cend = requires(T& value) { value.cend(); };

  template<typename Actual, typename Prefix>
  bool CheckStartsWith(const Actual& actual, const Prefix& prefix, MemoryOutStream& stream)
  {
    if constexpr ((std::is_same_v<Actual, std::string> || std::is_same_v<Actual, const char*> || std::is_array_v<Actual> && std::is_same_v<std::remove_extent_t<Actual>, char>) &&
                  (std::is_same_v<Prefix, std::string> || std::is_same_v<Prefix, const char*> || std::is_array_v<Prefix> && std::is_same_v<std::remove_extent_t<Prefix>, char>))
    {
      const char* actualCStr;
      if constexpr (std::is_same_v<Actual, std::string>)
        actualCStr = actual.c_str();
      else
        actualCStr = actual;
      const char* prefixCStr;
      if constexpr (std::is_same_v<Prefix, std::string>)
        prefixCStr = prefix.c_str();
      else
        prefixCStr = prefix;

      if (strlen(actualCStr) >= strlen(prefixCStr) && memcmp(prefixCStr, actualCStr, strlen(prefixCStr)) == 0)
        return true;

      stream << "Expected to start with " << prefix << ", but was " << actual;
      return false;
    }
    else if constexpr (has_cbegin<Actual> && has_cend<Actual> && has_cbegin<Prefix> && has_cend<Prefix>)
    {
      if (std::distance(actual.cbegin(), actual.cend()) >= std::distance(prefix.cbegin(), prefix.cend()) &&
          std::equal(prefix.cbegin(), prefix.cend(), actual.cbegin()))
        return true;

      stream << "Expected to start with " << prefix << ", but was " << actual;
      return true;
    }
    else
    {
      static_assert(sizeof(Actual) == 0, "Don't know how to compare prefixes of these types.");
    }
  }

  bool CheckDoesNotStartWith(const char* actual, const char* prefix, MemoryOutStream& stream);
  bool CheckDoesNotStartWith(const std::string& actual, const char* prefix, MemoryOutStream& stream);
  bool CheckDoesNotStartWith(const std::string& actual, const std::string& prefix, MemoryOutStream& stream);

  template<typename Actual, typename Expected>
  bool CheckArrayEqual(Actual const& actual, Expected const& expected, MemoryOutStream& stream)
  {
    bool equal = actual.size() == expected.size();
    if (equal)
      for (unsigned i = 0; i < actual.size(); ++i)
        equal &= (actual[i] == expected[i]);

    if (equal)
      return true;

    stream << "Expected [ ";

    for (unsigned expectedIndex = 0; expectedIndex < expected.size(); ++expectedIndex)
      stream << expected[expectedIndex] << " ";

    stream << "] but was [ ";

    for (unsigned actualIndex = 0; actualIndex < actual.size(); ++actualIndex)
      stream << actual[actualIndex] << " ";

    stream << "]";

    return false;
  }

  template<typename Actual, typename Expected, typename Tolerance>
  bool CheckArrayClose(Actual const& actual, Expected const& expected,
                       Tolerance const& tolerance, MemoryOutStream& stream)
  {
    bool equal = actual.size() == expected.size();
    if (equal)
      for (unsigned i = 0; i < actual.size(); ++i)
        equal &= AreClose(actual[i], expected[i], tolerance);

    if (equal)
      return true;

    stream << "Expected [ ";

    for (unsigned expectedIndex = 0; expectedIndex < expected.size(); ++expectedIndex)
      stream << expected[expectedIndex] << " ";

    stream << "] +/- " << tolerance << " but was [ ";

    for (unsigned actualIndex = 0; actualIndex < actual.size(); ++actualIndex)
      stream << actual[actualIndex] << " ";

    stream << "]";

    return false;
  }

  template<typename ExpectedException, typename ExpressionFunction>
  ExpectedException CheckThrow(const ExpressionFunction& expression,
                               const char* expectedMessage,
                               const char* filename,
                               int lineNumber)
  {
    try
    {
      try
      {
        expression();
      }
      catch (const ExpectedException& e)
      {
        return e;
      }
    }
    catch (const std::exception& e)
    {
      UnitTest::MemoryOutStream UNIT_TEST_MEMORY_OUT_stream;
      UNIT_TEST_MEMORY_OUT_stream << "Unhandled exception in check macro: " << e.what();
      UnitTest::CurrentTest::onTestFailure(filename, lineNumber, nullptr, UNIT_TEST_MEMORY_OUT_stream.GetText());
    }
    catch (...)
    {
      UnitTest::CurrentTest::onTestFailure(filename, lineNumber, nullptr, "Unhandled exception in check macro");
    }
    UnitTest::MemoryOutStream UNIT_TEST_MEMORY_OUT_stream;
    UNIT_TEST_MEMORY_OUT_stream << "Expected exception " << expectedMessage << " (not thrown)";
    UnitTest::CurrentTest::onTestFailure(filename, lineNumber, nullptr, UNIT_TEST_MEMORY_OUT_stream.GetText());
    throw std::exception(); // To make the compiler happy about the return type.
  }
}
