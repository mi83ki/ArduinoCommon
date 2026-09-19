"""公開ビルド設定から選択されるソースを検証する。"""

import fnmatch
import json
from pathlib import Path
import runpy

import pytest


ROOT = Path(__file__).resolve().parents[1]
LEGACY = {
    "Log.cpp", "MQTTClientESP32.cpp", "MacUtils.cpp", "Menu.cpp",
    "SleepHandler.cpp", "Timer.cpp", "WiFiESP32.cpp",
}
SCOUTER = {
    "Log.cpp", "MacUtils.cpp", "Timer.cpp", "WiFiESP32.cpp",
    "TCPClientESP32.cpp", "NeoPixelArrayBase.cpp", "Vibrator.cpp",
    "TimedPatternPlayer.cpp",
}
KUNAI = SCOUTER | {
    "Buzzer.cpp", "Speaker.cpp", "InfraredRemote.cpp", "Filter.cpp",
    "Menu.cpp",
}


class BuildEnvironment(dict):
    """スクリプトが使用する SCons 公開操作を記録する。"""

    def __init__(self, profile):
        super().__init__(SRC_FILTER="")
        self._profile = profile

    def GetProjectOption(self, name, default=None):
        """製品側のプロファイル値を返す。"""
        assert name == "custom_arduinocommon_profile"
        return default if self._profile is None else self._profile

    def Replace(self, **values):
        """ライブラリ専用環境の変更を記録する。"""
        self.update(values)


def selected_source_paths(profile):
    """manifest と登録済みスクリプトを通して相対パスの選択結果を得る。

    Args:
        profile: 利用側オプション。None は未指定。

    Returns:
        選択されたソース相対パスの集合。
    """
    manifest = json.loads((ROOT / "library.json").read_text("utf-8"))
    build = manifest["build"]
    env = BuildEnvironment(profile)
    if "extraScript" in build:
        runpy.run_path(
            str(ROOT / build["extraScript"]),
            init_globals={"env": env, "Import": lambda name: None},
        )
    # PlatformIO は manifest の srcFilter を env より優先する。
    filters = build.get("srcFilter", env["SRC_FILTER"])
    assert filters, "ソース選択は明示する"
    selected = set()
    for rule in filters:
        matches = {
            path.relative_to(ROOT / "src").as_posix()
            for path in (ROOT / "src").rglob("*.cpp")
            if fnmatch.fnmatch(
                path.relative_to(ROOT / "src").as_posix(), rule[2:-1])
        }
        if rule.startswith("+<"):
            selected.update(matches)
        else:
            assert rule.startswith("-<")
            selected.difference_update(matches)
    return selected


def selected_sources(profile):
    """直下にある機能ソースの選択結果を得る。

    Args:
        profile: 利用側オプション。None は未指定。

    Returns:
        選択された直下ソースファイル名の集合。
    """
    return {path for path in selected_source_paths(profile) if "/" not in path}


@pytest.mark.parametrize("profile", [None, "legacy"])
def test_legacy_keeps_existing_sources(profile):
    """未指定と legacy は既存利用者の選択を変えない。"""
    assert selected_sources(profile) == LEGACY


def test_scouter_links_required_drivers_without_ir_audio_mqtt():
    """スカウターの必要ドライバーだけを選択する。"""
    assert selected_sources("scouter") == SCOUTER


def test_kunai_includes_ir_audio_and_filter():
    """クナイに必要な IR・音声・フィルターを選択する。"""
    assert selected_sources("kunai") == KUNAI


@pytest.mark.parametrize("profile", ["", "unknown", "Scouter", "../src"])
def test_invalid_profile_is_rejected(profile):
    """不正な値を暗黙の全ソース有効化として扱わない。"""
    with pytest.raises(ValueError, match="custom_arduinocommon_profile"):
        selected_sources(profile)


def test_profile_evaluations_do_not_leak_between_environments():
    """複数環境のプロファイルが互いに影響しない。"""
    assert selected_sources("kunai") == KUNAI
    assert selected_sources("scouter") == SCOUTER
    assert selected_sources(None) == LEGACY


@pytest.mark.parametrize("profile", [None, "legacy", "scouter", "kunai"])
def test_every_profile_preserves_settings_and_provisioning(profile):
    """製品別ドライバー選択後も設定保存とプロビジョニングをリンクする。"""
    selected = selected_source_paths(profile)
    expected = {
        path.relative_to(ROOT / "src").as_posix()
        for directory in ("settings", "provisioning")
        for path in (ROOT / "src" / directory).glob("*.cpp")
    }
    assert expected <= selected


@pytest.mark.parametrize("profile", ["scouter", "kunai"])
def test_products_select_bounded_transport(profile):
    """両製品は新しいフレーム処理と非待機socketを選択する。"""
    assert {"transport/FramedChannel.cpp", "transport/TcpSocketESP32.cpp"} <= selected_source_paths(profile)


def test_legacy_does_not_add_transport():
    """旧利用者には追加transportを暗黙リンクしない。"""
    assert not any(path.startswith("transport/") for path in selected_source_paths(None))
