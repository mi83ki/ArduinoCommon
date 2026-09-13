#include "PreferencesBackend.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINOCOMMON_TEST_ESP32)
#include <Preferences.h>
#include <nvs.h>
#include <utility>

namespace ArduinoCommon {
namespace {

/** @brief NVSで利用するASCII名を、アクセス前に検証する。 */
bool validName(const char* name) {
  if (!name || !*name) return false;
  size_t length = 0;
  while (name[length]) {
    const auto ch = static_cast<unsigned char>(name[length]);
    if (ch < 32 || ch > 126 || ++length > 15) return false;
  }
  return true;
}

/** @brief namespace/キー不存在とIO障害を読取専用のNVS照会で区別する。 */
SettingsStatus inspect(const char* nameSpace, const char* key, size_t& length) {
  nvs_handle_t handle;
  esp_err_t result = nvs_open(nameSpace, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) return SettingsStatus::NotFound;
  if (result != ESP_OK) return SettingsStatus::IoError;
  result = nvs_get_blob(handle, key, nullptr, &length);
  nvs_close(handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) return SettingsStatus::NotFound;
  if (result == ESP_ERR_NVS_TYPE_MISMATCH) return SettingsStatus::Corrupt;
  return result == ESP_OK ? SettingsStatus::Ok : SettingsStatus::IoError;
}

}  // namespace

/** @brief namespaceとアクセス方針を保持し、生成時にはNVSを変更しない。 */
PreferencesBackend::PreferencesBackend(const char* nameSpace, bool readOnly)
    : _namespace(nameSpace ? nameSpace : ""), _readOnly(readOnly) {}

/**
 * @brief 不存在・型・上限を確認してからバイト列を読み出す。
 * @param key 対象キー
 * @param output 成功時にだけ置き換える出力
 * @param limit データ確保前に検証する最大長
 * @return SettingsStatus 読込結果。不存在でも初期化しない
 */
SettingsStatus PreferencesBackend::read(const char* key, SettingsBytes& output,
                                       size_t limit) {
  if (!validName(_namespace.c_str()) || !validName(key)) return SettingsStatus::InvalidArgument;
  size_t length = 0;
  const auto status = inspect(_namespace.c_str(), key, length);
  if (status != SettingsStatus::Ok) return status;
  if (length > limit) return SettingsStatus::TooLarge;
  Preferences preferences;
  if (!preferences.begin(_namespace.c_str(), true)) return SettingsStatus::IoError;
  if (preferences.getBytesLength(key) != length) return SettingsStatus::IoError;
  SettingsBytes bytes(length);
  if (length && preferences.getBytes(key, bytes.data(), length) != length)
    return SettingsStatus::IoError;
  output = std::move(bytes);
  return SettingsStatus::Ok;
}

/**
 * @brief 指定キーのblobだけを保存し、保存件数の不一致を失敗として返す。
 * @param key 対象キー
 * @param bytes 空ではない保存データ
 * @return SettingsStatus 空blobは削除の代用とせず引数エラーとする
 */
SettingsStatus PreferencesBackend::write(const char* key, const SettingsBytes& bytes) {
  if (!validName(_namespace.c_str()) || !validName(key)) return SettingsStatus::InvalidArgument;
  if (_readOnly) return SettingsStatus::ReadOnly;
  if (bytes.empty()) return SettingsStatus::InvalidArgument;
  Preferences preferences;
  if (!preferences.begin(_namespace.c_str(), false)) return SettingsStatus::IoError;
  return preferences.putBytes(key, bytes.data(), bytes.size()) == bytes.size()
      ? SettingsStatus::Ok : SettingsStatus::IoError;
}

/** @brief 指定blobだけを削除し、namespace全体や他キーを消去しない。 */
SettingsStatus PreferencesBackend::remove(const char* key) {
  if (!validName(_namespace.c_str()) || !validName(key)) return SettingsStatus::InvalidArgument;
  if (_readOnly) return SettingsStatus::ReadOnly;
  size_t length = 0;
  const auto status = inspect(_namespace.c_str(), key, length);
  if (status != SettingsStatus::Ok) return status;
  Preferences preferences;
  if (!preferences.begin(_namespace.c_str(), false)) return SettingsStatus::IoError;
  return preferences.remove(key) ? SettingsStatus::Ok : SettingsStatus::IoError;
}

}  // namespace ArduinoCommon
#endif
