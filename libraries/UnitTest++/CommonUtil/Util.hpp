#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

namespace Util
{
  enum class TextColor
  {
    NoColor = -1,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
  };

  /** The terminal escape code that switches the text to `color`. */
  inline std::string colorize(TextColor color)
  {
    if (color == TextColor::NoColor)
      return "\033[0m";
    return "\033[" + std::to_string(30 + int(color)) + "m";
  }

  template <typename ContainerType, typename Pred>
  void removeIf(ContainerType& container, Pred pred)
  {
    container.erase(std::remove_if(container.begin(), container.end(), pred), container.end());
  }

  template <typename ContainerType, typename Pred>
  void sort(ContainerType& container, const Pred& pred)
  {
    std::sort(container.begin(), container.end(), pred);
  }

  template <typename ContainerType, typename Pred>
  uint32_t countIf(const ContainerType& container, Pred pred)
  {
    return uint32_t(std::count_if(container.begin(), container.end(), pred));
  }

  template <typename T>
  struct PointerLess
  {
    bool operator()(const T* a , const T* b) const { return *a < *b; }
  };
}
