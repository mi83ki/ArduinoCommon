#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using esp_err_t = int;
using nvs_handle_t = uint32_t;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;
constexpr int ESP_ERR_NVS_NOT_FOUND = 1;
constexpr int ESP_ERR_NVS_TYPE_MISMATCH = 2;
constexpr int ESP_ERR_NVS_INVALID_LENGTH = 3;

namespace FakeNvs {
struct Value { bool blob = true; std::vector<uint8_t> bytes; };
using Values = std::map<std::string, Value>;
struct State {
  std::map<std::string, Values> spaces;
  std::map<nvs_handle_t, std::string> handles;
  nvs_handle_t nextHandle = 1;
  bool openError = false, readError = false, writeError = false, removeError = false;
  unsigned readCalls = 0, writeCalls = 0;
};
inline State& state() { static State value; return value; }
inline void reset() { state() = State{}; }
}

inline esp_err_t nvs_open(const char* name, nvs_open_mode_t mode, nvs_handle_t* handle) {
  auto& state = FakeNvs::state();
  if (state.openError) return ESP_FAIL;
  if (!state.spaces.count(name)) {
    if (mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    state.spaces[name] = {};
  }
  *handle = state.nextHandle++;
  state.handles[*handle] = name;
  return ESP_OK;
}
inline void nvs_close(nvs_handle_t handle) { FakeNvs::state().handles.erase(handle); }
inline esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* output, size_t* length) {
  auto& state = FakeNvs::state();
  if (state.readError || !state.handles.count(handle)) return ESP_FAIL;
  auto& values = state.spaces[state.handles[handle]];
  auto it = values.find(key);
  if (it == values.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (!it->second.blob) return ESP_ERR_NVS_TYPE_MISMATCH;
  if (output && *length < it->second.bytes.size()) return ESP_ERR_NVS_INVALID_LENGTH;
  if (output && !it->second.bytes.empty()) std::memcpy(output, it->second.bytes.data(), it->second.bytes.size());
  *length = it->second.bytes.size();
  return ESP_OK;
}
