#pragma once

#include "config.h"
#include <fmt/format.h>
#include <string>
#include <string_view>
#include <vector>

enum class Level { Debug, Info, Warning, Error };

constexpr Level parseLevel(const char* s) {
  if (s[0] == 'D' || s[0] == 'd') return Level::Debug;
  if (s[0] == 'I' || s[0] == 'i') return Level::Info;
  if (s[0] == 'W' || s[0] == 'w') return Level::Warning;
  if (s[0] == 'E' || s[0] == 'e') return Level::Error;
  return Level::Debug;
}

inline constexpr Level ACTIVE_LEVEL = parseLevel(LOG_LEVEL_STR);

class Logger {
public:
  using Sink = void (*)(std::string_view);

  void init(std::string prefix = "");
  void addSink(Sink sink);
  void dispatch(Level level, std::string_view message);

  template <Level L, typename... Args> void emit(fmt::format_string<Args...> fmt, Args&&... args) {
    if constexpr (L >= ACTIVE_LEVEL) {
      auto message = fmt::format(fmt, std::forward<Args>(args)...);
      dispatch(L, message);
    }
  }

  template <typename... Args> void debug(fmt::format_string<Args...> fmt, Args&&... args) {
    emit<Level::Debug>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void info(fmt::format_string<Args...> fmt, Args&&... args) {
    emit<Level::Info>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void warn(fmt::format_string<Args...> fmt, Args&&... args) {
    emit<Level::Warning>(fmt, std::forward<Args>(args)...);
  }

  template <typename... Args> void err(fmt::format_string<Args...> fmt, Args&&... args) {
    emit<Level::Error>(fmt, std::forward<Args>(args)...);
  }

private:
  std::string m_prefix;
  std::vector<Sink> m_sinks;
};

inline Logger xplog;
