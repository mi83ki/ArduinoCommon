#ifndef EEPROM_STORE_H
#define EEPROM_STORE_H

#include <Arduino.h>
#include <EEPROM.h>

/**
 * EEPROMStore - 任意の構造体をEEPROMに安全に読み書きするテンプレートクラス
 *
 * 特徴:
 *   - CRC16によるデータ整合性チェック
 *   - マジックナンバーによる初期化済み判定
 *   - EEPROM.put/get による簡潔な読み書き（AVRでは自動差分書き込み）
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
 */

namespace EEPROMStoreUtil {
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
}

template <typename T>
class EEPROMStore {
public:
    /// 読み書き対象のデータ（直接アクセス可）
    T data;

    /**
     * @param address   EEPROM上の開始アドレス
     * @param defaults  初回 or CRCエラー時に使うデフォルト値
     * @param magic     ストア識別用マジックナンバー（複数ストア使用時に変更）
     */
    EEPROMStore(uint16_t address, const T& defaults, uint16_t magic = 0xBEEF)
        : _address(address), _defaults(defaults), _magic(magic) {}

    /**
     * EEPROMから読み込み。CRC不一致またはマジックナンバー不一致の場合、
     * デフォルト値で初期化してEEPROMに書き込む。
     * @return true: 既存データの読み込み成功 / false: デフォルトで初期化した
     */
    bool begin() {
#if defined(ESP32) || defined(ESP8266)
        EEPROM.begin(_address + requiredSize() + 64);
#endif
        if (load()) {
            return true;
        }
        data = _defaults;
        forceSave();
        return false;
    }

    /**
     * EEPROMに書き込む。CRCが同一なら書き込みをスキップする。
     * AVRでは EEPROM.put() 内部で update() が使われるためバイト単位の差分書き込みとなる。
     * @return true: 変更があり書き込んだ / false: 変更なしでスキップ
     */
    bool save() {
        Header newHeader;
        newHeader.magic = _magic;
        newHeader.crc   = dataCRC();

        Header currentHeader;
        EEPROM.get(_address, currentHeader);
        if (currentHeader.magic == newHeader.magic &&
            currentHeader.crc   == newHeader.crc) {
            return false;
        }

        EEPROM.put(_address, newHeader);
        EEPROM.put(_address + sizeof(Header), data);
        commit();
        return true;
    }

    /**
     * 差分チェックなしで強制書き込み
     */
    void forceSave() {
        Header header;
        header.magic = _magic;
        header.crc   = dataCRC();

        EEPROM.put(_address, header);
        EEPROM.put(_address + sizeof(Header), data);
        commit();
    }

    /**
     * デフォルト値にリセットしてEEPROMに書き込む
     */
    void reset() {
        data = _defaults;
        forceSave();
    }

    /// このストアが使用するEEPROMのバイト数
    static constexpr uint16_t requiredSize() {
        return sizeof(Header) + sizeof(T);
    }

    /// 次のストアが使えるアドレスを返す
    uint16_t nextAddress() const {
        return _address + requiredSize();
    }

private:
    struct Header {
        uint16_t magic;
        uint16_t crc;
    };

    uint16_t _address;
    T        _defaults;
    uint16_t _magic;

    uint16_t dataCRC() const {
        return EEPROMStoreUtil::calcCRC(
            reinterpret_cast<const uint8_t*>(&data), sizeof(T));
    }

    bool load() {
        Header header;
        EEPROM.get(_address, header);
        if (header.magic != _magic) return false;

        T temp;
        EEPROM.get(_address + sizeof(Header), temp);
        if (EEPROMStoreUtil::calcCRC(
                reinterpret_cast<const uint8_t*>(&temp), sizeof(T)) != header.crc) {
            return false;
        }

        data = temp;
        return true;
    }

    static void commit() {
#if defined(ESP32) || defined(ESP8266)
        EEPROM.commit();
#endif
    }
};

#endif // EEPROM_STORE_H
