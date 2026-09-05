#pragma once

#include <string>
#include <vector>

namespace FakeLogState {

extern std::vector<std::string> messages;

void reset();

}  // namespace FakeLogState
