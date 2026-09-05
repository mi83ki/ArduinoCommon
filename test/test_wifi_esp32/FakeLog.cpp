#include "Log.h"

#include "FakeLogState.h"

namespace FakeLogState {

std::vector<std::string> messages;

void reset() { messages.clear(); }

}  // namespace FakeLogState

Log::Log() : m_level(ALL) {}
Log::~Log() {}
void Log::serialBegin(uint32_t) {}
void Log::serialEnd(void) {}
Log::logLevelEnum Log::getLevel(void) { return m_level; }
void Log::setLevel(logLevelEnum level) { m_level = level; }
void Log::log(logLevelEnum, String message) {
  FakeLogState::messages.push_back(message.c_str());
}
void Log::info(String message) { log(INFO, message); }
void Log::debug(String message) { log(DEBUG, message); }
void Log::warn(String message) { log(WARN, message); }
void Log::error(String message) { log(ERROR, message); }
void Log::changeBaudrate(uint32_t) {}

Log logger;
