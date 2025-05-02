#pragma once

/**
 * ESP32 の eFuse からデフォルト MAC アドレスを取得する
 *
 * @param sepChar 区切り文字（":" や "-" など）。省略時はコロン。
 * @return String 形式の MAC アドレス
 */
String getDefaultMacAddress(const String& sepChar = ":");

/** 区切り文字 "-" でフォーマットしたデフォルト MAC アドレス */
extern const String DEFAULT_MAC_ADDRESS;
