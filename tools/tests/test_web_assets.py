"""組込みWeb画面の生成結果を検証する。"""
import gzip
import re

import pytest

from web_assets import bundle_html, make_header


def test_utf8_bundle_is_reproducible_and_self_contained():
    """日本語と3つの部品を結合し、同一入力から同じgzipを生成する。"""
    template = '<meta charset="utf-8"><style>{{COMMON_CSS}}</style><p>設定</p><script>{{COMMON_JS}}</script><script>{{APPLICATION_JS}}</script>'
    compressed = bundle_html(template, "body{color:black}", "const common=1;", "const product=2;")
    assert compressed == bundle_html(template, "body{color:black}", "const common=1;", "const product=2;")
    html = gzip.decompress(compressed).decode("utf-8")
    assert "設定" in html and "const product=2;" in html and "const common=1;" in html
    assert "{{" not in html
    assert compressed[4:8] == b"\x00\x00\x00\x00"
    assert compressed[9] == 255


def test_embedded_header_contains_exact_compressed_bytes():
    """生成ヘッダーの配列を復元するとgzipの全バイトと一致する。"""
    data = b"\x1f\x8b\x00\xff"
    header = make_header(data)
    values = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", header))
    assert values == data
    assert "sizeof(kProvisioningHtml)" in header
    assert "PROGMEM" in header


@pytest.mark.parametrize("template", ["missing", "{{COMMON_CSS}}{{COMMON_CSS}}{{COMMON_JS}}{{APPLICATION_JS}}"])
def test_missing_or_duplicate_mount_points_are_rejected(template):
    """置換忘れや重複した埋込箇所のある画面を配布しない。"""
    with pytest.raises(ValueError):
        bundle_html(template, "", "", "")


def test_external_asset_reference_is_rejected():
    """端末のオフライン設定で読み込めない外部アセットを拒否する。"""
    template = '<script src="https://cdn.example.test/app.js"></script>{{COMMON_CSS}}{{COMMON_JS}}{{APPLICATION_JS}}'
    with pytest.raises(ValueError):
        bundle_html(template, "", "", "")
