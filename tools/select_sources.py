"""利用側の明示プロファイルからライブラリのソースを選択する。

未指定時は 0.6.0 の動作を維持し、製品設定を他の環境へ波及させない。
PlatformIO の library.json build.extraScript から実行する。
"""

Import("env")

LEGACY_SOURCES = (
    "Log.cpp", "MQTTClientESP32.cpp", "MacUtils.cpp", "Menu.cpp",
    "SleepHandler.cpp", "Timer.cpp", "WiFiESP32.cpp",
)
SCOUTER_SOURCES = (
    "Log.cpp", "MacUtils.cpp", "Timer.cpp", "WiFiESP32.cpp",
    "TCPClientESP32.cpp", "NeoPixelArrayBase.cpp", "Vibrator.cpp",
    "TimedPatternPlayer.cpp",
)
KUNAI_SOURCES = SCOUTER_SOURCES + (
    "Buzzer.cpp", "Speaker.cpp", "InfraredRemote.cpp", "Filter.cpp",
    "Menu.cpp",
)
PROFILES = {
    "legacy": LEGACY_SOURCES,
    "scouter": SCOUTER_SOURCES,
    "kunai": KUNAI_SOURCES,
}

profile = env.GetProjectOption("custom_arduinocommon_profile", "legacy")
if profile not in PROFILES:
    raise ValueError(
        "custom_arduinocommon_profile must be legacy, scouter or kunai; "
        f"got {profile!r}"
    )

# manifest に srcFilter を残すと、こちらの設定より優先されるため置かない。
env.Replace(SRC_FILTER=["-<*>"] + [f"+<{name}>" for name in PROFILES[profile]])
