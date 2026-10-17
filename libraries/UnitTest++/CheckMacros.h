#ifndef UNITTEST_CHECKMACROS_H
#define UNITTEST_CHECKMACROS_H

#include "Checks.h"
#include "TestMemoryOutStream.h"
#include "CurrentTest.h"
#include "TerminateTestException.h"

#ifdef CHECK
  #error UnitTest++ redefines CHECK
#endif

#ifdef CHECK_EQUAL
  #error UnitTest++ redefines CHECK_EQUAL
#endif

#ifdef CHECK_CLOSE
  #error UnitTest++ redefines CHECK_CLOSE
#endif

#ifdef CHECK_ARRAY_EQUAL
  #error UnitTest++ redefines CHECK_ARRAY_EQUAL
#endif

#ifdef CHECK_ARRAY_CLOSE
  #error UnitTest++ redefines CHECK_ARRAY_CLOSE
#endif

#ifdef CHECK_ARRAY2D_CLOSE
  #error UnitTest++ redefines CHECK_ARRAY2D_CLOSE
#endif

#ifdef _MSC_VER
#include <windows.h>
#define BREAK_UNDER_DEBUGGER (IsDebuggerPresent() && (DebugBreak(), true))
#else
#define BREAK_UNDER_DEBUGGER ((void)0)
#endif

#define CHECK_INTERNAL(condition, filename, line, id)                           \
  do                                                                            \
  {                                                                             \
      try                                                                       \
      {                                                                         \
          UnitTest::MemoryOutStream CHECK_INTERNAL_stream;                      \
          if (!(condition))                                                     \
          {                                                                     \
              BREAK_UNDER_DEBUGGER;                                             \
              UnitTest::MemoryOutStream UNIT_TEST_ID_OUT_stream;                \
              UNIT_TEST_ID_OUT_stream << (id);                                  \
              UnitTest::CurrentTest::onTestFailure((filename), (line),          \
                                                   UNIT_TEST_ID_OUT_stream.GetText(),     \
                                                   CHECK_INTERNAL_stream.GetText()); \
          }                                                                     \
      }                                                                         \
      catch (UnitTest::TerminateTestException const&)                           \
      {                                                                         \
          throw;                                                                \
      }                                                                         \
      catch (const std::exception& e)                                       \
      {                                                                         \
          BREAK_UNDER_DEBUGGER;                                                 \
          UnitTest::MemoryOutStream UNIT_TEST_MEMORY_OUT_stream;                                     \
          UNIT_TEST_MEMORY_OUT_stream << "Unhandled exception in check macro: " << e.what();         \
          UnitTest::MemoryOutStream UNIT_TEST_ID_OUT_stream;                    \
          UNIT_TEST_ID_OUT_stream << (id);                                      \
          UnitTest::CurrentTest::onTestFailure((filename), (line),              \
                                               UNIT_TEST_ID_OUT_stream.GetText(),         \
                                               UNIT_TEST_MEMORY_OUT_stream.GetText());               \
      }                                                                         \
      catch (...)                                                               \
      {                                                                         \
          BREAK_UNDER_DEBUGGER;                                                 \
          UnitTest::MemoryOutStream UNIT_TEST_ID_OUT_stream;                    \
          UNIT_TEST_ID_OUT_stream << (id);                                      \
          UnitTest::CurrentTest::onTestFailure((filename), (line),              \
                                               UNIT_TEST_ID_OUT_stream.GetText(),         \
                                               "Unhandled exception in check macro"); \
      }                                                                         \
  } while (0)

#define CHECK_FAILED(value)  \
  CHECK_INTERNAL((CHECK_INTERNAL_stream << value, false), __FILE__, __LINE__, nullptr)
#define CHECK_FAILED_SOURCELOCATION(value, sourceLocation)  \
  CHECK_INTERNAL((CHECK_INTERNAL_stream << value, false), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_FAILED_SIGNED(value, id)  \
  CHECK_INTERNAL((CHECK_INTERNAL_stream << value, false), __FILE__, __LINE__, (id))

#define CHECK(value) \
  CHECK_INTERNAL(UnitTest::CheckBoolean(#value, !!(value), CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)
#define CHECK_SOURCELOCATION(value, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckBoolean(#value, !!(value), CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_SIGNED(value, id) \
  CHECK_INTERNAL(UnitTest::CheckBoolean(#value, !!(value), CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))
#define CHECK_SIGNED_SOURCELOCATION(value, id, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckBoolean(#value, !!(value), CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), (id))

#define CHECK_EQUAL(actual, expected) \
  CHECK_INTERNAL(UnitTest::CheckEqual(actual, expected, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)
#define CHECK_EQUAL_SOURCELOCATION(actual, expected, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckEqual(actual, expected, CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_UNEQUAL(actual, unexpected) \
  CHECK_INTERNAL(UnitTest::CheckUnequal(actual, unexpected, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_UNEQUAL_SIGNED(actual, unexpected, id) \
  CHECK_INTERNAL(UnitTest::CheckUnequal(actual, unexpected, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_NULL(actual) \
  CHECK_INTERNAL(UnitTest::CheckNull(actual, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_NULL_SIGNED(actual, id) \
  CHECK_INTERNAL(UnitTest::CheckNull(actual, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_NOT_NULL(actual) \
  CHECK_INTERNAL(UnitTest::CheckNotNull(actual, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_NOT_NULL_SIGNED(actual, id) \
  CHECK_INTERNAL(UnitTest::CheckNotNull(actual, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_BIGGER(value, biggerThan) \
  CHECK_INTERNAL(UnitTest::CheckBigger(value, biggerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_BIGGER_SIGNED(value, biggerThan, id) \
  CHECK_INTERNAL(UnitTest::CheckBigger(value, biggerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_BIGGEREQUAL(value, biggerThan) \
  CHECK_INTERNAL(UnitTest::CheckBiggerEqual(value, biggerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_BIGGEREQUAL_SIGNED(value, biggerThan, id) \
  CHECK_INTERNAL(UnitTest::CheckBiggerEqual(value, biggerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_SMALLER(value, smallerThan) \
  CHECK_INTERNAL(UnitTest::CheckSmaller(value, smallerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_SMALLER_SIGNED(value, smallerThan, id) \
  CHECK_INTERNAL(UnitTest::CheckSmaller(value, smallerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_SMALLEREQUAL(value, smallerThan) \
  CHECK_INTERNAL(UnitTest::CheckSmallerEqual(value, smallerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_SMALLEREQUAL_SIGNED(value, smallerThan, id) \
  CHECK_INTERNAL(UnitTest::CheckSmallerEqual(value, smallerThan, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_BETWEEN(value, lowerBound, upperBound) \
  CHECK_INTERNAL(UnitTest::CheckBetween(value, lowerBound, upperBound, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_BETWEENEQUAL(value, lowerBound, upperBound) \
  CHECK_INTERNAL(UnitTest::CheckBetweenEqual(value, lowerBound, upperBound, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_EQUAL_SIGNED(actual, expected, id) \
  CHECK_INTERNAL(UnitTest::CheckEqual(actual, expected, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))
#define CHECK_EQUAL_SIGNED_SOURCELOCATION(actual, expected, id, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckEqual(actual, expected, CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), (id))

#define CHECK_CLOSE(actual, expected, tolerance) \
  CHECK_INTERNAL(UnitTest::CheckClose(actual, expected, tolerance, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_NOT_CLOSE(actual, expected, tolerance) \
  CHECK_INTERNAL(UnitTest::CheckNotClose(actual, expected, tolerance, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_CLOSE_SIGNED(actual, expected, tolerance, id) \
  CHECK_INTERNAL(UnitTest::CheckClose(actual, expected, tolerance, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_CLOSE_PERCENTAGE(actual, expected, tolerancePercentage) \
  CHECK_INTERNAL(UnitTest::CheckClosePercentage(actual, expected, tolerancePercentage, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_CLOSE_PERCENTAGE_SIGNED(actual, expected, tolerancePercentage, id) \
  CHECK_INTERNAL(UnitTest::CheckClosePercentage(actual, expected, tolerancePercentage, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_CONTAINS(actual, substring) \
  CHECK_INTERNAL(UnitTest::CheckContains(actual, substring, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)
#define CHECK_CONTAINS_SOURCELOCATION(actual, substring, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckContains(actual, substring, CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_CONTAINS_SIGNED(actual, substring, id) \
  CHECK_INTERNAL(UnitTest::CheckContains(actual, substring, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_DOES_NOT_CONTAIN(actual, substring) \
  CHECK_INTERNAL(UnitTest::CheckDoesNotContain(actual, substring, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_DOES_NOT_CONTAIN_SIGNED(actual, substring, id) \
  CHECK_INTERNAL(UnitTest::CheckDoesNotContain(actual, substring, CHECK_INTERNAL_stream), __FILE__, __LINE__, (id))

#define CHECK_STARTS_WITH(actual, prefix) \
  CHECK_INTERNAL(UnitTest::CheckStartsWith(actual, prefix, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)
#define CHECK_STARTS_WITH_SOURCELOCATION(actual, prefix, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckStartsWith(actual, prefix, CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_DOES_NOT_START_WITH(actual, prefix) \
  CHECK_INTERNAL(UnitTest::CheckDoesNotStartWith(actual, prefix, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)
#define CHECK_DOES_NOT_START_WITH_SOURCELOCATION(actual, prefix, sourceLocation) \
  CHECK_INTERNAL(UnitTest::CheckDoesNotStartWith(actual, prefix, CHECK_INTERNAL_stream), sourceLocation.file_name(), sourceLocation.line(), nullptr)

#define CHECK_ARRAY_EQUAL(actual, expected) \
  CHECK_INTERNAL(UnitTest::CheckArrayEqual(actual, expected, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_ARRAY_CLOSE(actual, expected, tolerance) \
  CHECK_INTERNAL(UnitTest::CheckArrayClose(actual, expected, tolerance, CHECK_INTERNAL_stream), __FILE__, __LINE__, nullptr)

#define CHECK_THROW(expression, ExpectedExceptionType) \
  (void)UnitTest::CheckThrow<ExpectedExceptionType>([&]{ expression; }, #ExpectedExceptionType, __FILE__, __LINE__)

#define CHECK_THROW_EQUAL(expression, ExpectedExceptionType, expectedMessage) \
  CHECK_EQUAL(UnitTest::CheckThrow<ExpectedExceptionType>([&]{ expression; }, expectedMessage, __FILE__, __LINE__).what(), expectedMessage)

#define CHECK_THROW_CONTAINS(expression, ExpectedExceptionType, expectedMessage) \
  CHECK_CONTAINS(UnitTest::CheckThrow<ExpectedExceptionType>([&]{ expression; }, "containing " expectedMessage, __FILE__, __LINE__).what(), expectedMessage)

#endif
