"""Speaker の DAC 対応判定を実プリプロセッサーで検証する。"""

from pathlib import Path
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("supported", [None, 0, 1])
def test_speaker_header_requires_hardware_dac(supported):
    """DAC 対応時のみ公開クラスを提供し、非対応時は明示する。"""
    compiler = shutil.which("g++")
    assert compiler, "ビルド設定テストには g++ が必要"
    command = [
        compiler, "-E", "-P", "-x", "c++", "-",
        "-DARDUINO_ARCH_ESP32",
        "-I", str(ROOT / "tests" / "preprocess_mock"),
        "-I", str(ROOT / "src"),
    ]
    if supported is not None:
        command.append(f"-DSOC_DAC_SUPPORTED={supported}")
    result = subprocess.run(
        command, input='#include "Speaker.h"\n',
        text=True, capture_output=True, check=False,
    )
    if supported == 1:
        assert result.returncode == 0, result.stderr
        assert "class Speaker" in result.stdout
    else:
        assert result.returncode != 0
        assert "Speaker requires hardware DAC" in result.stderr
        assert "class Speaker" not in result.stdout
