#include "ApCredentialStore.h"
#include "WiFiProfileValidator.h"
#include <algorithm>
#include <cstdio>
#include <utility>

namespace ArduinoCommon {
namespace {
const RecordFormat format{{{'A','P','I','D'}},1,1,77};
constexpr const char* key="ap_auth";
constexpr const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
/** @brief ラベル情報の最大長と文字列終端を検証する。 */
bool valid(const ApCredentials& value) {
  return !value.ssid.empty() && value.ssid.size()<=31 && WiFiProfileValidator::validUtf8(value.ssid) &&
      value.password.size()==20 && std::all_of(value.password.begin(),value.password.end(),[](char c) {
        return (c>='A' && c<='Z') || (c>='2' && c<='7');
      });
}
}
/** @brief namespaceを選択済みのbackendと、利用側の安全な乱数源を受け取る。 */
ApCredentialStore::ApCredentialStore(ISettingsBackend& backend,std::string prefix,
    std::array<uint8_t,6> mac,RandomFill random)
    :_backend(backend),_prefix(std::move(prefix)),_mac(mac),_random(std::move(random)) {}

/** @brief CRC・schemaと内容が正常なレコードだけを呼出元へ公開する。 */
SettingsStatus ApCredentialStore::load(ApCredentials& output) {
  SettingsBytes bytes;auto status=_backend.read(key,bytes,format.maximumSize);
  if(status!=SettingsStatus::Ok)return status;
  DecodedRecord record;status=RecordEnvelopeCodec::decode(format,bytes,record);
  if(status!=SettingsStatus::Ok)return status;
  const auto& p=record.payload;
  if(p.size()<2 || p[0]>31 || p[1]!=20 || p.size()!=size_t(2+p[0]+p[1]))return SettingsStatus::Corrupt;
  ApCredentials candidate{std::string(p.begin()+2,p.begin()+2+p[0]),std::string(p.begin()+2+p[0],p.end())};
  if(!valid(candidate))return SettingsStatus::Corrupt;
  output=std::move(candidate);return SettingsStatus::Ok;
}
/** @brief 不存在の場合だけ初回生成し、破損・読取障害では既存ラベルを置き換えない。 */
SettingsStatus ApCredentialStore::loadOrCreate(ApCredentials& output) {
  const auto status=load(output);
  return status==SettingsStatus::NotFound?regenerate(output):status;
}
/** @brief 明示要求で資格情報を再生成し、保存と読戻しが一致した場合だけ利用可能にする。 */
SettingsStatus ApCredentialStore::regenerate(ApCredentials& output) {
  if(_prefix.size()>25 || !WiFiProfileValidator::validUtf8(_prefix))return SettingsStatus::InvalidArgument;
  uint8_t random[20];
  if(!_random || !_random(random,sizeof(random)))return SettingsStatus::IoError;
  char suffix[7];std::snprintf(suffix,sizeof(suffix),"%02X%02X%02X",_mac[3],_mac[4],_mac[5]);
  ApCredentials candidate{_prefix+suffix,{}};
  for(uint8_t value:random)candidate.password.push_back(alphabet[value&31]);
  SettingsBytes payload{uint8_t(candidate.ssid.size()),uint8_t(candidate.password.size())};
  payload.insert(payload.end(),candidate.ssid.begin(),candidate.ssid.end());
  payload.insert(payload.end(),candidate.password.begin(),candidate.password.end());
  SettingsBytes bytes;auto status=RecordEnvelopeCodec::encode(format,1,payload,bytes);
  if(status!=SettingsStatus::Ok)return status;
  status=_backend.write(key,bytes);if(status!=SettingsStatus::Ok)return status;
  ApCredentials verified;status=load(verified);
  if(status!=SettingsStatus::Ok || verified.ssid!=candidate.ssid || verified.password!=candidate.password)
    return SettingsStatus::Indeterminate;
  output=std::move(verified);return SettingsStatus::Ok;
}
/** @brief Wi-Fi QR形式の予約文字をエスケープする。印刷・表示の許可は利用側が管理する。 */
std::string ApCredentialStore::wifiQr(const ApCredentials& credentials) {
  auto escape=[](const std::string& text) {
    std::string result;
    for(char c:text) {if(c=='\\' || c==';' || c==',' || c==':' || c=='"')result+='\\';result+=c;}
    return result;
  };
  return "WIFI:T:WPA;S:"+escape(credentials.ssid)+";P:"+escape(credentials.password)+";;";
}
}
