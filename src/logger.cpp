#include "logger.h"
#include <fmt/format.h>

static constexpr const char* levelTag(Level level) {
  switch (level) {
  case Level::Debug:   return "Debug";
  case Level::Info:    return "Info";
  case Level::Warning: return "Warn";
  case Level::Error:   return "Error";
  }
  return "";
}

void Logger::init(std::string prefix) {
  m_prefix = std::move(prefix);
}

void Logger::addSink(Sink sink) {
  m_sinks.push_back(sink);
}

void Logger::dispatch(Level level, std::string_view message) {
  auto formatted = fmt::format("{}{}: {}\n", m_prefix, levelTag(level), message);
  std::string_view sv{formatted};
  for (auto sink : m_sinks) {
    sink(sv);
  }
}
