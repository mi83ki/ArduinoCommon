/**
 * @file WiFiProfileValidator.cpp
 * @brief Wi-Fiプロファイルの形式・ネットワーク設定を検証する実装。
 */

#include "WiFiProfileValidator.h"
#include <algorithm>
#include <set>

namespace ArduinoCommon {
/**
 * @brief NUL・不正な継続列・過長UTF-8を除外する。
 * @param text 検証するUTF-8文字列
 * @return true 正しいUTF-8文字列の場合
 * @return false NUL、不正な符号列、または範囲外の文字を含む場合
 */
bool WiFiProfileValidator::validUtf8(const std::string& text) {
  for(size_t i=0;i<text.size();) {
    uint32_t ch=static_cast<uint8_t>(text[i++]);
    if(!ch) return false;
    if(ch<128) continue;
    unsigned n; uint32_t minimum;
    if(ch>=0xc2 && ch<=0xdf) {n=1; minimum=0x80; ch&=31;}
    else if(ch>=0xe0 && ch<=0xef) {n=2; minimum=0x800; ch&=15;}
    else if(ch>=0xf0 && ch<=0xf4) {n=3; minimum=0x10000; ch&=7;}
    else return false;
    if(n>text.size()-i) return false;
    while(n--) { uint8_t next=text[i++]; if((next&0xc0)!=0x80)return false; ch=(ch<<6)|(next&63); }
    if(ch<minimum || ch>0x10ffff || (ch>=0xd800 && ch<=0xdfff)) return false;
  }
  return true;
}
/**
 * @brief パスフレーズまたは64桁の生PSKを検証する。
 * @param password 検証するWi-Fiパスワード
 * @return true 空文字列、正しいパスフレーズ、または64桁の16進PSKの場合
 * @return false 長さまたは文字形式が不正な場合
 */
bool WiFiProfileValidator::validPassword(const std::string& password) {
  if(password.empty()) return true;
  if(password.size()==64) return std::all_of(password.begin(),password.end(),[](unsigned char c) {
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
  });
  return password.size()>=8 && password.size()<=63 && validUtf8(password);
}
/**
 * @brief IPv4をネットワーク順の整数へ変換する。
 * @param ip 変換するIPv4アドレス
 * @return uint32_t ネットワーク順に並べたIPv4値
 */
uint32_t address(const IPv4& ip) {
  return uint32_t(ip[0])<<24 | uint32_t(ip[1])<<16 | uint32_t(ip[2])<<8 | ip[3];
}
/**
 * @brief 通常のユニキャストIPv4だけを受け付ける。
 * @param ip 検証するIPv4アドレス
 * @return true 通常のユニキャストアドレスの場合
 * @return false 未指定、ループバック、マルチキャストなどの場合
 */
bool unicast(const IPv4& ip) { return ip[0]!=0 && ip[0]!=127 && ip[0]<224; }
/**
 * @brief 固定IPv4のホスト・ネットワーク・DNS条件を確認する。
 * @param p 検証するWi-Fiプロファイル
 * @param requiresDns DNS1を必須にする場合はtrue
 * @return true 固定IP設定が有効な場合
 * @return false IP、ゲートウェイ、マスク、DNSのいずれかが不正な場合
 */
bool WiFiProfileValidator::validStaticIp(const WiFiProfile& p,bool requiresDns) {
  const uint32_t mask=address(p.mask), inv=~mask, ip=address(p.ip), gw=address(p.gateway);
  if(!mask || inv<3 || (inv&(inv+1)) || !unicast(p.ip) || !unicast(p.gateway))return false;
  if(!(ip&inv) || (ip&inv)==inv || !(gw&inv) || (gw&inv)==inv || ip==gw || (ip&mask)!=(gw&mask))return false;
  if(address(p.dns1) && !unicast(p.dns1))return false;
  if(address(p.dns2) && !unicast(p.dns2))return false;
  return !requiresDns || address(p.dns1)!=0;
}
/**
 * @brief Wi-Fi固有の制約だけを検証し、製品のサーバーや機種は扱わない。
 * @param profiles 検証するWi-Fiプロファイル一覧
 * @param primaryId 主プロファイルとして指定されたID
 * @param ready 保存・接続に使用できる状態まで検証する場合はtrue
 * @return std::vector<WiFiValidationError> 検出した入力エラー一覧。空なら有効
 */
std::vector<WiFiValidationError> WiFiProfileValidator::validate(const std::vector<WiFiProfile>& profiles,uint8_t primaryId,bool ready) {
  std::vector<WiFiValidationError> errors;
  auto add=[&](const std::string& field,const char* code){errors.push_back({field,code});};
  if(profiles.size()>4 || (ready && profiles.empty()))add("profiles","count");
  if(primaryId>3)add("primaryProfileId","invalid");
  std::set<uint8_t> ids;std::set<std::string> ssids;
  for(const auto& p:profiles) {
    const std::string key="profiles."+std::to_string(p.id);
    if(p.id>3 || !ids.insert(p.id).second)add(key+".id","duplicate_or_invalid");
    if(p.ssid.empty() || p.ssid.size()>31 || !validUtf8(p.ssid) || !ssids.insert(p.ssid).second)add(key+".ssid","invalid");
    if(!validPassword(p.password))add(key+".password","invalid");
    if(ready && p.staticIp && !validStaticIp(p,false))add(key+".staticIp","invalid");
  }
  if(!profiles.empty() && !ids.count(primaryId))add("primaryProfileId","missing");
  return errors;
}
/**
 * @brief 秘密値の保持・交換・明示消去を、変更先IDとSSIDの検証後だけ適用する。
 * @param profile パスワードを反映するプロファイル
 * @param action パスワードの保持・置換・消去操作
 * @param value 置換時の新しいパスワード。保持・消去時は空文字列
 * @param open 対象ネットワークをオープンにする場合はtrue
 * @param existing 現在保存されているプロファイル一覧
 * @return true 操作を適用できた場合
 * @return false 操作、秘密値、既存プロファイルの組み合わせが不正な場合
 */
bool WiFiProfileValidator::applyPassword(WiFiProfile& profile,WiFiPasswordAction action,const std::string& value,
                   bool open,const std::vector<WiFiProfile>& existing) {
  std::string password;
  switch(action) {
    case WiFiPasswordAction::Keep: {
      if(!value.empty())return false;
      auto it=std::find_if(existing.begin(),existing.end(),[&](const WiFiProfile& p){
        return p.id==profile.id && p.ssid==profile.ssid;
      });
      if(it==existing.end() || open!=it->password.empty())return false;
      password=it->password; break;
    }
    case WiFiPasswordAction::Replace:
      if(open || value.empty() || !validPassword(value))return false;
      password=value; break;
    case WiFiPasswordAction::Clear:
      if(!open || !value.empty())return false;
      break;
    default:return false;
  }
  profile.password=std::move(password); return true;
}


}
