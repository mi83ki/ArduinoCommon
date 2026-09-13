"""共通保存のESP32サンプルをプロジェクトのビルドへ追加する。"""

Import("env")

env.Append(CPPPATH=["$PROJECT_DIR/src"])
env.BuildSources(
    "$BUILD_DIR/preferences_example",
    "$PROJECT_DIR/examples/PreferencesSettings",
)
