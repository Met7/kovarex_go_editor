#pragma once
#include <CommonUtil/StringUtil.hpp>

class MemoryOutStream
{
public:
  MemoryOutStream() = default;

  template<class T>
  MemoryOutStream& operator<<(const T& value)
  {
    (void)value;
    if constexpr (std::is_same_v<std::remove_const_t<std::remove_reference_t<T>>, std::string_view> ||
                  std::is_same_v<std::remove_const_t<std::remove_reference_t<T>>, std::string>)
      this->data += value;
    else if constexpr (!std::is_null_pointer_v<T>)
      this->data += StringUtil::str(value);
    return *this;
  }

  MemoryOutStream& operator<<(const char* value)
  {
    if (value)
      this->data += value;
    return *this;
  }

  const std::string& getData() const { return this->data; }
  const char* GetText() const { return this->data.c_str(); }

private:
  std::string data;
};
