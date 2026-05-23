#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if __has_include(<Arduino.h>)
#include <Arduino.h>
#define EEPROM_STORE_HAS_ARDUINO 1
#else
#define EEPROM_STORE_HAS_ARDUINO 0
#endif

#if EEPROM_STORE_HAS_ARDUINO
#include <EEPROM.h>
#define EEPROM_STORE_HAS_BACKEND 1
#else
#define EEPROM_STORE_HAS_BACKEND 0
#endif

namespace EEPROMStoreUtil {

/**
 * @brief バイト列から CRC16-CCITT を計算する。
 *
 * @param data 計算対象の先頭アドレス
 * @param len 計算対象のバイト数
 * @return uint16_t 計算結果
 */
inline uint16_t calcCRC(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
  }
  return crc;
}

/**
 * @brief EEPROM 上のセクション情報を表す。
 */
struct SectionInfo {
  uint16_t size;
  uint16_t address;
};

/**
 * @brief セクションのサイズと開始アドレスが一致するか判定する。
 *
 * @param expected 期待するセクション情報
 * @param actual EEPROM から読み出したセクション情報
 * @return true サイズと開始アドレスが一致する場合
 * @return false サイズまたは開始アドレスが異なる場合
 */
inline bool isSectionCompatible(const SectionInfo& expected,
                                const SectionInfo& actual) {
  return expected.size == actual.size && expected.address == actual.address;
}

/**
 * @brief レイアウト先頭アドレスとフィールドオフセットから保存先アドレスを計算する。
 *
 * @param baseAddress レイアウトの先頭アドレス
 * @param fieldOffset フィールドのオフセット
 * @return uint16_t フィールドの保存先アドレス
 */
constexpr uint16_t layoutFieldAddress(uint16_t baseAddress, size_t fieldOffset) {
  return static_cast<uint16_t>(baseAddress + fieldOffset);
}

/**
 * @brief ダンプ表示用の単純な 8bit チェックサムを計算する。
 *
 * @param data 計算対象の先頭アドレス
 * @param len 計算対象のバイト数
 * @return uint8_t 計算結果
 */
inline uint8_t calcChecksum(const uint8_t* data, size_t len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum = static_cast<uint8_t>(checksum + data[i]);
  }
  return checksum;
}

/**
 * @brief EEPROM ダンプ 1 行分の 16 進文字列を生成する。
 *
 * @param buffer 出力先バッファ
 * @param bufferSize 出力先バッファサイズ
 * @param address 行先頭のアドレス
 * @param data 表示対象データ
 * @param len 表示対象バイト数
 * @return size_t 出力した文字数。失敗時は 0
 */
inline size_t formatHexRow(char* buffer, size_t bufferSize, uint16_t address,
                           const uint8_t* data, size_t len) {
  if (buffer == nullptr || bufferSize == 0 || data == nullptr || len == 0) {
    return 0;
  }

  int written = snprintf(buffer, bufferSize, "%04X ", address);
  if (written < 0 || static_cast<size_t>(written) >= bufferSize) {
    if (bufferSize > 0) {
      buffer[0] = '\0';
    }
    return 0;
  }

  size_t offset = static_cast<size_t>(written);
  for (size_t i = 0; i < len; i++) {
    written = snprintf(buffer + offset, bufferSize - offset, "%02X ", data[i]);
    if (written < 0 || static_cast<size_t>(written) >= (bufferSize - offset)) {
      buffer[0] = '\0';
      return 0;
    }
    offset += static_cast<size_t>(written);
  }

  written = snprintf(buffer + offset, bufferSize - offset, ":%02X",
                     calcChecksum(data, len));
  if (written < 0 || static_cast<size_t>(written) >= (bufferSize - offset)) {
    buffer[0] = '\0';
    return 0;
  }
  offset += static_cast<size_t>(written);
  return offset;
}

}  // namespace EEPROMStoreUtil

#if EEPROM_STORE_HAS_BACKEND

/**
 * @brief Arduino EEPROM API へのアクセスをまとめる薄いラッパー。
 *
 * 特徴:
 *   - `begin()` / `end()` / `commit()` をまとめて扱える
 *   - `put()` / `get()` / `putBytes()` / `getBytes()` を統一した形で使える
 *   - EEPROM全体の16進ダンプを生成できる
 *
 * 主な用途:
 *   - プロジェクト固有の保存クラスから EEPROM 操作を直接分離する
 *   - ArduinoCommon 側の汎用レイヤとして EEPROM 入出力を共通化する
 */
class EEPROMSession {
 public:
  /**
   * @brief セッションを初期化する。
   *
   * @param size 使用する EEPROM サイズ
   */
  explicit EEPROMSession(uint16_t size) : _size(size) {}

  /**
   * @brief EEPROM を使用可能な状態に初期化する。
   *
   * @return true 初期化に成功した場合
   * @return false 初期化に失敗した場合
   */
  bool begin() const {
#if defined(ESP32) || defined(ESP8266)
    return EEPROM.begin(_size);
#else
    return true;
#endif
  }

  /**
   * @brief EEPROM セッションを終了する。
   */
  void end() const {
#if defined(ESP32) || defined(ESP8266)
    EEPROM.end();
#endif
  }

  template <typename T>
  /**
   * @brief 任意の型を EEPROM に書き込む。
   *
   * @tparam T 書き込む型
   * @param address 書き込み先アドレス
   * @param value 書き込む値
   */
  void put(uint16_t address, const T& value) const {
    EEPROM.put(address, value);
  }

  template <typename T>
  /**
   * @brief 任意の型を EEPROM から読み出す。
   *
   * @tparam T 読み出す型
   * @param address 読み出し元アドレス
   * @param value 読み出し結果の格納先
   */
  void get(uint16_t address, T& value) const {
    EEPROM.get(address, value);
  }

  /**
   * @brief 任意のバイト列を EEPROM に書き込む。
   *
   * @param address 書き込み先アドレス
   * @param data 書き込むデータ
   * @param length 書き込むバイト数
   */
  void putBytes(uint16_t address, const void* data, size_t length) const {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t index = 0; index < length; index++) {
      EEPROM.write(address + static_cast<uint16_t>(index), bytes[index]);
    }
  }

  /**
   * @brief 任意のバイト列を EEPROM から読み出す。
   *
   * @param address 読み出し元アドレス
   * @param data 読み出し結果の格納先
   * @param length 読み出すバイト数
   */
  void getBytes(uint16_t address, void* data, size_t length) const {
    uint8_t* bytes = static_cast<uint8_t*>(data);
    for (size_t index = 0; index < length; index++) {
      bytes[index] = EEPROM.read(address + static_cast<uint16_t>(index));
    }
  }

  /**
   * @brief 1 バイトだけ EEPROM から読み出す。
   *
   * @param address 読み出し元アドレス
   * @return uint8_t 読み出した値
   */
  uint8_t read(uint16_t address) const { return EEPROM.read(address); }

  /**
   * @brief EEPROM への書き込みを確定する。
   *
   * @return true 確定に成功した場合
   * @return false 確定に失敗した場合
   */
  bool commit() const {
#if defined(ESP32) || defined(ESP8266)
    return EEPROM.commit();
#else
    return true;
#endif
  }

  template <typename Writer>
  /**
   * @brief EEPROM の内容を 16 進ダンプとして出力する。
   *
   * @tparam Writer 出力関数の型
   * @param writer 1 行ずつ受け取る関数
   * @param startAddress ダンプ開始アドレス
   * @param length ダンプするバイト数。0 の場合は末尾まで
   */
  void dump(Writer writer, uint16_t startAddress = 0,
            uint16_t length = 0) const {
    const uint16_t dumpLength =
        length == 0 ? static_cast<uint16_t>(_size - startAddress) : length;
    uint16_t address = startAddress;
    char line[80];

    writer(String("Add  +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F Sum"));
    while (address < static_cast<uint16_t>(startAddress + dumpLength)) {
      uint8_t row[16] = {};
      uint8_t rowLength = 0;
      for (; rowLength < 16 &&
             address < static_cast<uint16_t>(startAddress + dumpLength);
           rowLength++, address++) {
        row[rowLength] = EEPROM.read(address);
      }

      if (EEPROMStoreUtil::formatHexRow(
              line, sizeof(line),
              static_cast<uint16_t>(address - rowLength), row, rowLength) > 0) {
        writer(String(line));
      }
    }
  }

 private:
  uint16_t _size;
};

/**
 * @brief 任意レイアウト構造体を EEPROM 上の固定位置に保存するクラス。
 *
 * 特徴:
 *   - レイアウト構造体全体を一括で読み書きできる
 *   - フィールド単位でオフセット指定の読み書きができる
 *   - 既存EEPROMレイアウトを保ったまま段階的な共通化に使える
 *
 * 使い方:
 *   struct Layout {
 *     uint16_t version;
 *     uint16_t stateSize;
 *     char itemName[4];
 *   };
 *
 *   EEPROMSession session(512);
 *   EEPROMLayoutStore<Layout> store(session, 10);
 *
 *   void setup() {
 *     session.begin();
 *     Layout layout = store.read();
 *     store.writeField(offsetof(Layout, version), static_cast<uint16_t>(2));
 *   }
 *
 * @tparam Layout レイアウトを表す構造体
 */
template <typename Layout>
class EEPROMLayoutStore {
 public:
  /**
   * @brief レイアウトストアを初期化する。
   *
   * @param session 使用する EEPROM セッション
   * @param baseAddress レイアウト先頭アドレス
   */
  EEPROMLayoutStore(const EEPROMSession& session, uint16_t baseAddress)
      : _session(session), _baseAddress(baseAddress) {}

  /**
   * @brief レイアウト全体を EEPROM に書き込む。
   *
   * @param layout 書き込むレイアウト
   */
  void write(const Layout& layout) const { _session.put(_baseAddress, layout); }

  /**
   * @brief レイアウト全体を EEPROM から読み出す。
   *
   * @return Layout 読み出したレイアウト
   */
  Layout read() const {
    Layout layout = {};
    _session.get(_baseAddress, layout);
    return layout;
  }

  template <typename Field>
  /**
   * @brief レイアウト内の指定フィールドへ値を書き込む。
   *
   * @tparam Field フィールドの型
   * @param offset レイアウト先頭からのオフセット
   * @param value 書き込む値
   */
  void writeField(size_t offset, const Field& value) const {
    _session.put(addressOf(offset), value);
  }

  /**
   * @brief レイアウト内の指定位置へバイト列を書き込む。
   *
   * @param offset レイアウト先頭からのオフセット
   * @param data 書き込むデータ
   * @param length 書き込むバイト数
   */
  void writeBytes(size_t offset, const void* data, size_t length) const {
    _session.putBytes(addressOf(offset), data, length);
  }

  template <typename Field>
  /**
   * @brief レイアウト内の指定フィールドを読み出す。
   *
   * @tparam Field フィールドの型
   * @param offset レイアウト先頭からのオフセット
   * @param value 読み出し結果の格納先
   */
  void readField(size_t offset, Field* value) const {
    _session.get(addressOf(offset), *value);
  }

  /**
   * @brief レイアウト内の指定位置からバイト列を読み出す。
   *
   * @param offset レイアウト先頭からのオフセット
   * @param data 読み出し結果の格納先
   * @param length 読み出すバイト数
   */
  void readBytes(size_t offset, void* data, size_t length) const {
    _session.getBytes(addressOf(offset), data, length);
  }

  /**
   * @brief レイアウト内の指定位置に対応する EEPROM アドレスを返す。
   *
   * @param offset レイアウト先頭からのオフセット
   * @return uint16_t EEPROM アドレス
   */
  uint16_t addressOf(size_t offset) const {
    return EEPROMStoreUtil::layoutFieldAddress(_baseAddress, offset);
  }

 private:
  const EEPROMSession& _session;
  uint16_t _baseAddress;
};

/**
 * @brief EEPROMStore - 任意の構造体をEEPROMに安全に読み書きするテンプレートクラス
 *
 * 特徴:
 *   - CRC16によるデータ整合性チェック
 *   - マジックナンバーによる初期化済み判定
 *   - EEPROM.put/get による簡潔な読み書き
 *   - デフォルト値による自動初期化
 *
 * 使い方:
 *   struct Config {
 *     int   sensorInterval;
 *     float threshold;
 *     char  name[16];
 *   };
 *
 *   Config defaultCfg = { 1000, 25.5f, "sensor01" };
 *   EEPROMStore<Config> store(0, defaultCfg);
 *
 *   void setup() {
 *     store.begin();
 *     store.data.threshold = 30.0f;
 *     store.save();
 *   }
 *
 * @tparam T 保存対象の構造体
 */
template <typename T>
class EEPROMStore {
 public:
  /// 読み書き対象のデータ本体
  T data;

  /**
   * @brief 保存先アドレスとデフォルト値を指定して初期化する。
   *
   * @param address 保存先アドレス
   * @param defaults 初期化時に使用するデフォルト値
   * @param magic 保存領域を識別するマジック値
   */
  EEPROMStore(uint16_t address, const T& defaults, uint16_t magic = 0xBEEF)
      : _address(address),
        _defaults(defaults),
        _magic(magic),
        _session(storageSizeFor(address)) {}

  /**
   * @brief EEPROM からデータを読み込み、無効ならデフォルト値で初期化する。
   *
   * @return true EEPROM から有効なデータを読み込めた場合
   * @return false デフォルト値で初期化した場合
   */
  bool begin() {
    if (!_session.begin()) {
      return false;
    }

    if (load()) {
      return true;
    }

    data = _defaults;
    forceSave();
    return false;
  }

  /**
   * @brief 現在のデータを EEPROM に保存する。
   *
   * @return true データが変更され、書き込みを行った場合
   * @return false 変更がなく、書き込みを省略した場合
   */
  bool save() {
    Header newHeader;
    newHeader.magic = _magic;
    newHeader.crc = dataCRC();

    Header currentHeader;
    _session.get(_address, currentHeader);
    if (currentHeader.magic == newHeader.magic &&
        currentHeader.crc == newHeader.crc) {
      return false;
    }

    _session.put(_address, newHeader);
    _session.put(static_cast<uint16_t>(_address + sizeof(Header)), data);
    _session.commit();
    return true;
  }

  /**
   * @brief 差分判定を行わずに現在のデータを書き込む。
   */
  void forceSave() {
    Header header;
    header.magic = _magic;
    header.crc = dataCRC();

    _session.put(_address, header);
    _session.put(static_cast<uint16_t>(_address + sizeof(Header)), data);
    _session.commit();
  }

  /**
   * @brief データをデフォルト値へ戻して保存する。
   */
  void reset() {
    data = _defaults;
    forceSave();
  }

  /**
   * @brief このストアが必要とする EEPROM サイズを返す。
   *
   * @return uint16_t ヘッダと本体を含む必要サイズ
   */
  static constexpr uint16_t requiredSize() {
    return sizeof(Header) + sizeof(T);
  }

  /**
   * @brief 次のストアを配置できる先頭アドレスを返す。
   *
   * @return uint16_t 次の先頭アドレス
   */
  uint16_t nextAddress() const { return _address + requiredSize(); }

 private:
  struct Header {
    uint16_t magic;
    uint16_t crc;
  };

  static constexpr uint16_t storageSizeFor(uint16_t address) {
    return static_cast<uint16_t>(address + requiredSize() + 64);
  }

  uint16_t dataCRC() const {
    return EEPROMStoreUtil::calcCRC(
        reinterpret_cast<const uint8_t*>(&data), sizeof(T));
  }

  bool load() {
    Header header;
    _session.get(_address, header);
    if (header.magic != _magic) {
      return false;
    }

    T temp;
    _session.get(static_cast<uint16_t>(_address + sizeof(Header)), temp);
    if (EEPROMStoreUtil::calcCRC(reinterpret_cast<const uint8_t*>(&temp),
                                 sizeof(T)) != header.crc) {
      return false;
    }

    data = temp;
    return true;
  }

  uint16_t _address;
  T _defaults;
  uint16_t _magic;
  EEPROMSession _session;
};

#endif  // EEPROM_STORE_HAS_BACKEND

#endif  // EEPROM_STORE_H
