#pragma once
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

/** printf into a std::string. */
inline std::string ssprintf(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  const int length = std::vsnprintf(nullptr, 0, format, copy);
  va_end(copy);
  std::string result(length > 0 ? size_t(length) : 0, '\0');
  if (length > 0)
    std::vsnprintf(result.data(), result.size() + 1, format, args);
  va_end(args);
  return result;
}

namespace StringUtil
{
  inline void toLower(std::string& input)
  {
    std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  }

  inline bool replace_all(std::string& input, char search, char replace)
  {
    bool replaced = false;
    for (char& c : input)
      if (c == search)
      {
        c = replace;
        replaced = true;
      }
    return replaced;
  }

  template<class T>
  concept Streamable = requires(std::ostream& stream, const T& value) { stream << value; };

  /** Anything as text, for the failure messages of the checks. */
  template<class T>
  std::string str(const T& value)
  {
    if constexpr (std::is_same_v<T, bool>)
      return value ? "true" : "false";
    else if constexpr (std::is_enum_v<T>)
      return std::to_string(static_cast<std::underlying_type_t<T>>(value));
    else if constexpr (Streamable<T>)
    {
      std::ostringstream stream;
      stream << value;
      return stream.str();
    }
    else
      return "<value>";
  }
}
