"""共通部品と製品画面を、再現可能なgzipへ結合する。"""
import gzip
import re


def bundle_html(template: str, common_css: str, common_js: str, application_js: str) -> bytes:
    """外部依存のないUTF-8画面を圧縮する。

    Args:
        template: 3つの埋込マーカーを各1個持つHTML。
        common_css: 共通の表示スタイル。
        common_js: 共通Wi-Fiフォームと通信処理。
        application_js: 利用側のフォーム拡張と画面制御。

    Returns:
        タイムスタンプとOS識別子を固定したgzipバイト列。

    Raises:
        ValueError: マーカー不整合または外部アセット参照がある場合。
    """
    parts = {"COMMON_CSS": common_css, "COMMON_JS": common_js, "APPLICATION_JS": application_js}
    for name in parts:
        if template.count("{{" + name + "}}") != 1:
            raise ValueError(f"Expected exactly one {name} marker")
    for name, value in parts.items():
        template = template.replace("{{" + name + "}}", value)
    if re.search(r'''(?:src|href)\s*=\s*["']\s*(?:https?:)?//|@import\s|url\(\s*["']?(?:https?:)?//''', template, re.I):
        raise ValueError("External assets cannot be used in offline provisioning")
    data = bytearray(gzip.compress(template.encode("utf-8"), compresslevel=9, mtime=0))
    data[9] = 255
    return bytes(data)


def make_header(data: bytes) -> str:
    """圧縮画面と正確なバイト数を持つArduino用ヘッダーを生成する。

    Args:
        data: 圧縮済みの画面バイト列。

    Returns:
        UTF-8で保存するヘッダー文字列。
    """
    rows = ["  " + ",".join(f"0x{value:02x}" for value in data[index:index + 24]) for index in range(0, len(data), 24)]
    return (
        "#pragma once\n#include <Arduino.h>\n"
        "static const uint8_t kProvisioningHtml[] PROGMEM = {\n"
        + ",\n".join(rows)
        + "\n};\nstatic constexpr size_t kProvisioningHtmlSize = sizeof(kProvisioningHtml);\n"
    )
