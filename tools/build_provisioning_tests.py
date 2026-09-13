"""共通プロビジョニングのSDKモックを各テスト実行ファイルへ追加する。"""
Import("env")
env.BuildSources(
    "$BUILD_DIR/provisioning_mock",
    "$PROJECT_DIR/test/test_wifi_esp32",
    src_filter=["+<FakeWiFi.cpp>"],
)
