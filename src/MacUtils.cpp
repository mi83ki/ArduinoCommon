#include <Arduino.h>
#include "MacUtils.h"

String getDefaultMacAddress(const String& sepChar)
{
    unsigned char mac_base[6] = {0};
    if (esp_efuse_mac_get_default(mac_base) != ESP_OK) {
        return String();          // 失敗時は空文字列
    }

    char buf[18];                 // "xx:xx:xx:xx:xx:xx" + '\0'
    sprintf(buf,
            "%02x%s%02x%s%02x%s%02x%s%02x%s%02x",
            mac_base[0], sepChar.c_str(),
            mac_base[1], sepChar.c_str(),
            mac_base[2], sepChar.c_str(),
            mac_base[3], sepChar.c_str(),
            mac_base[4], sepChar.c_str(),
            mac_base[5]);

    return String(buf);
}

/*---------------- グローバル定数 ----------------*/
const String DEFAULT_MAC_ADDRESS = getDefaultMacAddress("-");
