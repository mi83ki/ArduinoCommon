"""共通Wi-Fi設定サンプルと圧縮Web画面をビルドへ追加する。"""
from pathlib import Path
import sys

Import("env")

root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(root / "tools"))
from web_assets import bundle_html, make_header

example = root / "examples" / "WiFiProvisioning"
web = root / "src" / "provisioning" / "web"
data = bundle_html(
    (example / "index.html").read_text(encoding="utf-8"),
    (web / "common.css").read_text(encoding="utf-8"),
    (web / "common.js").read_text(encoding="utf-8"),
    (example / "application.js").read_text(encoding="utf-8"),
)
generated = Path(env.subst("$BUILD_DIR")) / "generated"
generated.mkdir(parents=True, exist_ok=True)
header = generated / "ProvisioningWeb.h"
content = make_header(data)
if not header.exists() or header.read_text(encoding="utf-8") != content:
    header.write_text(content, encoding="utf-8", newline="\n")
(generated / "provisioning.html.gz").write_bytes(data)
framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
# BuildSourcesの追加ソースには、後段LDFが作るプロジェクト用includeを明示する。
env.Append(CPPPATH=[
    str(root / "src"), str(generated),
    str(Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "ArduinoJson" / "src"),
    str(framework / "libraries" / "WiFi" / "src"),
    str(framework / "libraries" / "DNSServer" / "src"),
])
env.BuildSources("$BUILD_DIR/provisioning_example", str(example))
