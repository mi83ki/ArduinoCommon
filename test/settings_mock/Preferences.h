#pragma once
#include "nvs.h"

class Preferences {
 public:
  ~Preferences() { end(); }
  bool begin(const char* name, bool readOnly = false, const char* = nullptr) {
    _readonly = readOnly;
    _open = nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &_handle) == ESP_OK;
    return _open;
  }
  void end() { if (_open) nvs_close(_handle); _open = false; }
  size_t getBytesLength(const char* key) {
    size_t length = 0;
    return _open && nvs_get_blob(_handle, key, nullptr, &length) == ESP_OK ? length : 0;
  }
  size_t getBytes(const char* key, void* output, size_t length) {
    ++FakeNvs::state().readCalls;
    return _open && nvs_get_blob(_handle, key, output, &length) == ESP_OK ? length : 0;
  }
  size_t putBytes(const char* key, const void* input, size_t length) {
    auto& state = FakeNvs::state();
    ++state.writeCalls;
    if (!_open || _readonly || state.writeError) return 0;
    const auto* data = static_cast<const uint8_t*>(input);
    state.spaces[state.handles[_handle]][key] = {true, {data, data + length}};
    return length;
  }
  bool remove(const char* key) {
    auto& state = FakeNvs::state();
    if (!_open || _readonly || state.removeError) return false;
    return state.spaces[state.handles[_handle]].erase(key) == 1;
  }
 private:
  nvs_handle_t _handle = 0;
  bool _open = false, _readonly = true;
};
