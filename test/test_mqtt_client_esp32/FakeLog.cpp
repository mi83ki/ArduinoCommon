#include "Log.h"

Log::Log() : m_level(ALL) {}
Log::~Log() {}
void Log::serialBegin(uint32_t) {}
void Log::serialEnd(void) {}
Log::logLevelEnum Log::getLevel(void) { return m_level; }
void Log::setLevel(logLevelEnum level) { m_level = level; }
void Log::log(logLevelEnum, String) {}
void Log::info(String message) { log(INFO, message); }
void Log::debug(String message) { log(DEBUG, message); }
void Log::warn(String message) { log(WARN, message); }
void Log::error(String message) { log(ERROR, message); }
void Log::changeBaudrate(uint32_t) {}

Log logger;
