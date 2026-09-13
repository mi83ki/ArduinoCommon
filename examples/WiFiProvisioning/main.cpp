#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <mutex>
#include "settings/AtomicRecordStore.h"
#include "settings/PreferencesBackend.h"
#include "provisioning/ProvisioningPortalESP32.h"
#include "ProvisioningWeb.h"

using namespace ArduinoCommon;
namespace {
struct ExampleConfig {std::string displayName="ESP32 device";WiFiProfile network;};
PreferencesBackend settingsBackend("ac_demo"),identityBackend("ac_demo_id");
const RecordSlots configSlots{{{{'A','C','D','M'}},1,1,2048},{{"cfg0","cfg1"}}};
const RecordSlots rootSlots{{{{'A','C','D','M'}},1,2,512},{{"root0","root1"}}};
AtomicRecordStore store(settingsBackend,{configSlots},rootSlots,0);
WiFiProvisioningProbe probe;
ProvisioningPortalESP32 portal(probe);
ApCredentialStore* identity=nullptr;
ApCredentials credentials;
std::mutex stateMutex;
ExampleConfig confirmed,candidate;
uint32_t revision=0,expectedRevision=0,completedAt=0;
uint64_t jobId=0;
bool readOnly=false,pending=false,active=false,cancelRequested=false,rebootPending=false;
std::string phase="idle",message="表示名とWi-Fiを入力してください。";

/** @brief Wi-Fiの乱数源が有効な状態で呼ぶ。 */
bool fillRandom(uint8_t* bytes,size_t length) {esp_fill_random(bytes,length);return true;}
/** @brief 組込みJSONの結果を所有文字列へ変換する。 */
std::string json(const JsonDocument& doc) {std::string result;serializeJson(doc,result);return result;}
PortalResponse failure(int status,const char* text) {
  DynamicJsonDocument doc(512);doc["error"]["message"]=text;return {status,json(doc)};
}
/** @brief IPv4の十進4区切りだけを受理する。 */
bool parseAddress(JsonVariantConst value,IPv4& result,bool optional=false) {
  if(optional && value.isNull()) {result={};return true;}
  if(!value.is<const char*>())return false;
  IPAddress ip;const char* text=value.as<const char*>();
  if(!ip.fromString(text) || ip.toString()!=text)return false;
  result={{ip[0],ip[1],ip[2],ip[3]}};return true;
}
/** @brief 表示名と単一Wi-Fiを読み、秘密操作・IPの意味検証は共通Validatorへ委譲する。 */
bool decode(JsonObjectConst object,ExampleConfig& result,bool submitted) {
  if(!object["displayName"].is<const char*>())return false;
  ExampleConfig next;next.displayName=object["displayName"].as<const char*>();
  if(next.displayName.empty() || next.displayName.size()>48 || !WiFiProfileValidator::validUtf8(next.displayName))return false;
  const auto profiles=object["profiles"].as<JsonArrayConst>();
  if(profiles.size()!=1 || !object["primaryProfileId"].is<uint8_t>() || object["primaryProfileId"].as<uint8_t>()!=0)return false;
  const auto p=profiles[0].as<JsonObjectConst>();
  if(!p["id"].is<uint8_t>() || p["id"].as<uint8_t>()!=0 || !p["ssid"].is<const char*>())return false;
  next.network.ssid=p["ssid"].as<const char*>();
  const char* mode=p["ip"]["mode"] | "";
  if(std::string(mode)!="dhcp" && std::string(mode)!="static")return false;
  next.network.staticIp=std::string(mode)=="static";
  if(next.network.staticIp && (!parseAddress(p["ip"]["address"],next.network.ip) ||
      !parseAddress(p["ip"]["gateway"],next.network.gateway) || !parseAddress(p["ip"]["subnet"],next.network.mask) ||
      !parseAddress(p["ip"]["dns1"],next.network.dns1,true) || !parseAddress(p["ip"]["dns2"],next.network.dns2,true)))return false;
  if(submitted) {
    const std::string action=p["passwordAction"] | "",security=p["security"] | "";
    if(action!="keep" && action!="replace" && action!="clear")return false;
    if(security!="open" && security!="wpa-personal")return false;
    const auto operation=action=="keep"?WiFiPasswordAction::Keep:action=="replace"?WiFiPasswordAction::Replace:WiFiPasswordAction::Clear;
    if(!WiFiProfileValidator::applyPassword(next.network,operation,p["password"] | "",security=="open",{confirmed.network}))return false;
  } else {
    if(!p["password"].is<const char*>())return false;
    next.network.password=p["password"].as<const char*>();
  }
  if(!WiFiProfileValidator::validate({next.network},0,true).empty())return false;
  result=std::move(next);return true;
}
/** @brief 保存時だけ秘密を含め、HTTP取得にはpasswordSetだけを公開する。 */
void encode(JsonObject object,const ExampleConfig& config,bool includeSecret) {
  object["displayName"]=config.displayName;object["primaryProfileId"]=0;
  auto profiles=object.createNestedArray("profiles");if(config.network.ssid.empty())return;
  auto profile=profiles.createNestedObject();profile["id"]=0;profile["ssid"]=config.network.ssid;
  profile["security"]=config.network.password.empty()?"open":"wpa-personal";
  profile["passwordSet"]=!config.network.password.empty();
  if(includeSecret)profile["password"]=config.network.password;
  auto ip=profile.createNestedObject("ip");ip["mode"]=config.network.staticIp?"static":"dhcp";
  if(config.network.staticIp) {
    auto put=[&](const char* key,const IPv4& value){ip[key]=IPAddress(value[0],value[1],value[2],value[3]).toString();};
    put("address",config.network.ip);put("gateway",config.network.gateway);put("subnet",config.network.mask);
    put("dns1",config.network.dns1);put("dns2",config.network.dns2);
  }
}
/** @brief HTTPハンドラーではRAMを更新するだけで、NVSやWi-Fiを操作しない。 */
void registerApi() {
  portal.addHandler(PortalMethod::Get,"/api/config",[](const PortalRequest&) {
    std::lock_guard<std::mutex> lock(stateMutex);DynamicJsonDocument doc(4096);
    doc["revision"]=revision;doc["configured"]=revision!=0;doc["readOnly"]=readOnly;
    encode(doc.createNestedObject("config"),confirmed,false);return PortalResponse{200,json(doc)};
  });
  portal.addHandler(PortalMethod::Get,"/api/status",[](const PortalRequest&) {
    std::lock_guard<std::mutex> lock(stateMutex);DynamicJsonDocument doc(1024);
    doc["phase"]=phase;doc["message"]=message;doc["revision"]=revision;doc["jobId"]=jobId;return PortalResponse{200,json(doc)};
  });
  portal.addHandler(PortalMethod::Post,"/api/config",[](const PortalRequest& request) {
    DynamicJsonDocument doc(8192);if(deserializeJson(doc,request.body))return failure(400,"JSONを読み取れませんでした。");
    std::lock_guard<std::mutex> lock(stateMutex);
    if(readOnly)return failure(503,"保存領域を確認できないため、読み取り専用です。");
    if(active || pending || rebootPending)return failure(409,"接続確認が進行中です。");
    if(!doc["expectedRevision"].is<uint32_t>() || doc["expectedRevision"].as<uint32_t>()!=revision)return failure(409,"設定が更新されています。画面を開き直してください。");
    if(!doc["testProfileId"].is<uint8_t>() || doc["testProfileId"].as<uint8_t>()!=0 ||
        !decode(doc["config"].as<JsonObjectConst>(),candidate,true))return failure(422,"表示名・Wi-Fi・IP設定を確認してください。");
    ++jobId;pending=true;cancelRequested=false;expectedRevision=revision;phase="queued";message="接続確認を受け付けました。まだ保存されていません。";
    return PortalResponse{202,"{\"jobId\":"+std::to_string(jobId)+"}"};
  });
  portal.addHandler(PortalMethod::Post,"/api/cancel",[](const PortalRequest& request) {
    DynamicJsonDocument doc(256);if(deserializeJson(doc,request.body))return failure(400,"要求を読み取れませんでした。");
    std::lock_guard<std::mutex> lock(stateMutex);
    if(!doc["jobId"].is<uint64_t>() || doc["jobId"].as<uint64_t>()!=jobId || phase=="committing" || rebootPending || (!active && !pending))return failure(409,"この処理は取り消せません。");
    cancelRequested=true;return PortalResponse{202,"{}"};
  });
}
/** @brief 接続確認に失敗した場合は確定設定を維持する。 */
void failJob(const char* text) {
  std::lock_guard<std::mutex> lock(stateMutex);active=false;pending=false;phase="failed";message=text;
}
/** @brief 所有タスクで接続確認後だけ一括保存し、読戻し後に成功を公開する。 */
void processJob() {
  bool start=false,cancel=false,hasJob=false;ExampleConfig proposed;uint32_t expected=0;uint64_t id=0;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if(pending) {pending=false;active=true;start=true;phase="testing_wifi";message="Wi-Fiへの接続を確認しています…";}
    hasJob=active;cancel=cancelRequested;proposed=candidate;expected=expectedRevision;id=jobId;
  }
  if(!hasJob)return;
  if(cancel) {
    probe.cancel(id);std::lock_guard<std::mutex> lock(stateMutex);active=false;phase="cancelled";message="接続確認を取り消しました。保存済みの設定は変わりません。";return;
  }
  if(start && !probe.start(proposed.network,id,millis()+20000)) {failJob("接続確認を開始できませんでした。検索終了後にやり直してください。");return;}
  const auto result=probe.result();
  if(result.state==WiFiProbeState::Failed) {failJob("Wi-Fiにつながりませんでした。設定は保存されていません。");return;}
  if(result.state!=WiFiProbeState::Succeeded)return;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    if(cancelRequested)return;
    phase="committing";message="設定を保存しています…";
  }
  DynamicJsonDocument doc(4096);encode(doc.to<JsonObject>(),proposed,true);const auto text=json(doc);
  SettingsBytes payload(text.begin(),text.end());const auto saved=store.commit(expected,{{0,payload}},{});
  SettingsSnapshot snapshot;const auto loaded=store.reconcile(snapshot);
  const bool committed=(saved==SettingsStatus::Ok || saved==SettingsStatus::Indeterminate) && loaded==SettingsStatus::Ok &&
      snapshot.generation==expected+1 && snapshot.records.size()==1 && snapshot.records[0].payload==payload;
  probe.finish(id);
  if(!committed) {failJob("保存を確認できませんでした。電源と保存領域を確認してください。");return;}
  std::lock_guard<std::mutex> lock(stateMutex);confirmed=std::move(proposed);revision=snapshot.generation;
  phase="complete";message="設定を保存しました。15秒後に再起動します。同じWi-Fiへ接続し直すと保存値を確認できます。";
  active=false;rebootPending=true;completedAt=millis();
}
}

/** @brief センサー・MQTT・M5を使用せず、共通部品だけで設定画面を起動する。 */
void setup() {
  Serial.begin(115200);WiFi.persistent(false);WiFi.mode(WIFI_STA);
  std::array<uint8_t,6> mac{};WiFi.macAddress(mac.data());
  identity=new ApCredentialStore(identityBackend,"Example-",mac,fillRandom);
  if(identity->loadOrCreate(credentials)!=SettingsStatus::Ok) {Serial.println("AP identity unavailable. No automatic regeneration.");return;}
  SettingsSnapshot snapshot;const auto loaded=store.load(snapshot);
  if(loaded==SettingsStatus::Ok) {
    DynamicJsonDocument doc(4096);
    const auto& payload=snapshot.records[0].payload;
    if(deserializeJson(doc,payload.data(),payload.size()) || !decode(doc.as<JsonObjectConst>(),confirmed,false))readOnly=true;
    else revision=snapshot.generation;
  } else if(loaded!=SettingsStatus::NotFound)readOnly=true;
  registerApi();
  if(!portal.begin(credentials,fillRandom,kProvisioningHtml,kProvisioningHtmlSize))Serial.println("Portal start failed.");
  Serial.println("Send i over USB to display setup credentials. Restart after saving to verify persisted values.");
}
/** @brief USBで明示要求された時だけ資格情報を表示し、HTTP処理と保存を分離する。 */
void loop() {
  portal.tick();processJob();
  if(Serial.available() && Serial.read()=='i' && portal.running()) {
    Serial.printf("SSID: %s\nPassword: %s\nQR: %s\n",credentials.ssid.c_str(),credentials.password.c_str(),ApCredentialStore::wifiQr(credentials).c_str());
  }
  if(rebootPending && uint32_t(millis()-completedAt)>=15000) {portal.stop();ESP.restart();}
  delay(10);
}
