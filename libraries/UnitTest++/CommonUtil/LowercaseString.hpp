#pragma once
#include <algorithm>
#include <cctype>
#include <string>

/** Utility to provide original and lower cased version of a string without the need to
 * copy and convert it for every request of the lower cased variant. */
class LowercaseString
{
public:
  explicit LowercaseString() = default;
  explicit LowercaseString(const char* value) : LowercaseString(std::string(value)) {}
  explicit LowercaseString(const std::string& value) : value(value), lowercaseValue(lower(value)) {}
  operator const std::string&() const { return this->value; }
  const std::string& getValueExplicit() const { return this->value; }
  const std::string& getLowercase() const { return this->lowercaseValue; }
  const char* c_str() const { return this->value.c_str(); }
  bool operator<(const LowercaseString& other) const { return this->lowercaseValue < other.lowercaseValue; }
  bool operator==(const LowercaseString& other) const { return this->lowercaseValue == other.lowercaseValue; }
  bool operator!=(const LowercaseString& other) const { return this->lowercaseValue != other.lowercaseValue; }
  bool empty() const { return this->value.empty(); }

private:
  static std::string lower(std::string text)
  {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
  }

  std::string value, lowercaseValue;
};
