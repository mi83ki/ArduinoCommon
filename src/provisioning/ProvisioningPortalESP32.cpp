#if !defined(ARDUINOCOMMON_DISABLE_PROVISIONING) && (defined(ARDUINO_ARCH_ESP32) || defined(ARDUINOCOMMON_TEST_ESP32))
#include "ProvisioningPortalESP32.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

namespace ArduinoCommon {
namespace {
/** @brief 制限付きのヘッダー取得で、過大入力を空値として扱わない。 */
bool header(httpd_req_t* request,const char* name,size_t maximum,std::string& output) {
  const size_t length=httpd_req_get_hdr_value_len(request,name);
  if(length>maximum)return false;
  if(!length) {output.clear();return true;}
  std::vector<char> value(length+1);
  if(httpd_req_get_hdr_value_str(request,name,value.data(),value.size())!=ESP_OK)return false;
  output.assign(value.data(),length);return output.find('\0')==std::string::npos;
}
/** @brief JSON文字列として予約文字と制御文字をエスケープする。 */
std::string quote(const std::string& value) {
  std::string output="\"";
  for(unsigned char c:value) {
    if(c=='"' || c=='\\') {output+='\\';output+=char(c);}
    else if(c<32) {char encoded[7];std::snprintf(encoded,sizeof(encoded),"\\u%04x",c);output+=encoded;}
    else output+=char(c);
  }
  return output+'"';
}
/** @brief 公開するHTTPステータスを固定文字列へ変換する。 */
const char* statusText(int status) {
  switch(status) {
    case 200:return "200 OK";case 202:return "202 Accepted";
    case 400:return "400 Bad Request";case 403:return "403 Forbidden";
    case 404:return "404 Not Found";case 405:return "405 Method Not Allowed";
    case 408:return "408 Request Timeout";case 409:return "409 Conflict";
    case 413:return "413 Payload Too Large";case 415:return "415 Unsupported Media Type";
    case 422:return "422 Unprocessable Entity";case 503:return "503 Service Unavailable";
    default:return "500 Internal Server Error";
  }
}
/** @brief キャッシュ・外部読込・埋込を禁止して応答する。 */
esp_err_t respond(httpd_req_t* request,int status,const char* body,size_t length,const char* type="application/json; charset=utf-8") {
  httpd_resp_set_status(request,statusText(status));httpd_resp_set_type(request,type);
  httpd_resp_set_hdr(request,"Cache-Control","no-store");
  httpd_resp_set_hdr(request,"X-Content-Type-Options","nosniff");
  httpd_resp_set_hdr(request,"Connection","close");
  httpd_resp_set_hdr(request,"Content-Security-Policy",
      "default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
  return httpd_resp_send(request,body,length);
}
/** @brief 未読bodyを持つエラーでは接続も閉じて、次の要求へ混入させない。 */
esp_err_t error(httpd_req_t* request,int status) {
  const std::string body="{\"error\":"+quote(statusText(status))+"}";
  respond(request,status,body.data(),body.size());return ESP_FAIL;
}
}

/** @brief Wi-Fi所有者と制限値を受け取り、製品の型や保存処理を保持しない。 */
ProvisioningPortalESP32::ProvisioningPortalESP32(WiFiProvisioningProbe& probe,PortalOptions options)
    :_probe(probe),_options(options) {
  const auto& ip=options.apAddress;_host=IPAddress(ip[0],ip[1],ip[2],ip[3]).toString().c_str();
}
/** @brief 利用側の所有タスクで、HTTP終了を待ってからWi-Fiを解放する。 */
ProvisioningPortalESP32::~ProvisioningPortalESP32() {stop();}
/** @brief 起動前だけ製品APIを登録し、標準APIと重複する経路を拒否する。 */
bool ProvisioningPortalESP32::setSessionHandler(std::function<PortalResponse(const std::string&)> handler) {
  if(_running)return false;
  _sessionHandler=std::move(handler);return true;
}
/** @brief 起動前に値所有型の製品APIを登録する。 */
bool ProvisioningPortalESP32::addHandler(PortalMethod method,const std::string& path,Handler handler) {
  if(_running || !handler || path.size()>63 || path.compare(0,5,"/api/")!=0 ||
      path=="/api/session" || path=="/api/scan" || path=="/api/activity" ||
      _routes.size()+10>=_options.maximumHandlers)return false;
  for(char c:path)if(!std::isalnum(static_cast<unsigned char>(c)) && c!='/' && c!='-' && c!='_')return false;
  for(const auto& route:_routes)if(route.method==method && route.path==path)return false;
  _routes.push_back({method,path,std::move(handler)});return true;
}
/** @brief AP開始後の乱数からセッションを生成し、DNSと制限付きHTTPDを起動する。 */
bool ProvisioningPortalESP32::begin(const ApCredentials& credentials,ApCredentialStore::RandomFill random,
                                   const uint8_t* gzipHtml,size_t size) {
  if(_running || !_options.maximumBody || _options.maximumBody>4096 || !_options.receiveMillis ||
      _options.receiveMillis>3000 || !_options.maximumSockets || _options.maximumSockets>2 ||
      _options.maximumHandlers<10 || _options.maximumHandlers>24 || (size && !gzipHtml))return false;
  if(!_probe.begin(credentials,_options.apAddress))return false;
  uint8_t bytes[16];
  if(!random || !random(bytes,sizeof(bytes))) {_probe.stop();return false;}
  _token.clear();const char hex[]="0123456789abcdef";
  for(uint8_t value:bytes) {_token+=hex[value>>4];_token+=hex[value&15];}
  _html=gzipHtml;_htmlSize=size;_stopRequested=false;_lastActivity=millis();
  _scanRequested=false;_scanRejected=false;_scanState=WiFiScanState::Idle;_scanResults.clear();
  const auto& ip=_options.apAddress;
  if(!_dns.start(53,"*",IPAddress(ip[0],ip[1],ip[2],ip[3]))) {_probe.stop();return false;}
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();
  config.stack_size=8192;config.max_open_sockets=_options.maximumSockets;config.max_uri_handlers=_options.maximumHandlers;
  config.recv_wait_timeout=3;config.send_wait_timeout=3;config.lru_purge_enable=true;
  config.global_user_ctx=this;config.global_user_ctx_free_fn=[](void*){};
  config.open_fn=accept;config.uri_match_fn=httpd_uri_match_wildcard;
  if(httpd_start(&_server,&config)!=ESP_OK) {_dns.stop();_probe.stop();return false;}
  _running=true;
  for(auto method:{HTTP_GET,HTTP_POST}) {
    httpd_uri_t route{};route.uri="/*";route.method=method;route.handler=dispatch;route.user_ctx=this;
    if(httpd_register_uri_handler(_server,&route)!=ESP_OK) {stop();return false;}
  }
  return true;
}
/** @brief accepted socketの宛先を検証し、dual-stackのIPv4-mapped AP宛ても扱う。 */
bool ProvisioningPortalESP32::apSocket(int socket) const {
  if(!_probe.apAvailable())return false;
  sockaddr_storage destination{};socklen_t size=sizeof(destination);
  if(getsockname(socket,reinterpret_cast<sockaddr*>(&destination),&size)!=0)return false;
  const auto expected=inet_addr(_host.c_str());
  if(destination.ss_family==AF_INET) {
    const auto* ipv4=reinterpret_cast<const sockaddr_in*>(&destination);
    return size>=sizeof(sockaddr_in) && ipv4->sin_addr.s_addr==expected;
  }
  if(destination.ss_family==AF_INET6 && size>=sizeof(sockaddr_in6)) {
    const auto* ipv6=reinterpret_cast<const sockaddr_in6*>(&destination);
    const auto* bytes=reinterpret_cast<const uint8_t*>(&ipv6->sin6_addr);
    for(unsigned i=0;i<10;++i)if(bytes[i]!=0)return false;
    return bytes[10]==0xff && bytes[11]==0xff && std::memcmp(bytes+12,&expected,4)==0;
  }
  return false;
}
/** @brief HTTPヘッダーを読む前に、設置先LAN宛てのソケットを拒否する。 */
esp_err_t ProvisioningPortalESP32::accept(httpd_handle_t handle,int socket) {
  auto* portal=static_cast<ProvisioningPortalESP32*>(httpd_get_global_user_ctx(handle));
  return portal && portal->apSocket(socket)?ESP_OK:ESP_FAIL;
}
/** @brief 全標準・製品APIを同じ検証経路へ通す。 */
esp_err_t ProvisioningPortalESP32::dispatch(httpd_req_t* request) {
  auto* portal=static_cast<ProvisioningPortalESP32*>(request->user_ctx);
  return portal?portal->handle(request):ESP_FAIL;
}
/** @brief HTTPから受けたbodyは所有文字列へコピーし、SDK操作を所有タスクへ渡す。 */
esp_err_t ProvisioningPortalESP32::handle(httpd_req_t* request) {
  if(!_running || !apSocket(httpd_req_to_sockfd(request)))return error(request,403);
  std::string path=request->uri;path=path.substr(0,path.find('?'));
  const bool api=path.compare(0,5,"/api/")==0;
  std::string host,origin,token,contentType,transfer;
  if(!header(request,"Host",64,host))return error(request,403);
  const bool canonical=host==_host || host==_host+":80";
  if(api) {
    if(!canonical || !header(request,"Origin",80,origin) || (!origin.empty() && origin!="http://"+_host))return error(request,403);
    if(!(request->method==HTTP_GET && path=="/api/session")) {
      if(!header(request,"X-Setup-Token",32,token) || token.size()!=32)return error(request,403);
      uint8_t difference=0;for(size_t i=0;i<32;++i)difference|=uint8_t(token[i]^_token[i]);
      if(difference)return error(request,403);
    }
  }
  if(!header(request,"Transfer-Encoding",32,transfer) || !transfer.empty())return error(request,400);
  if(request->content_len>_options.maximumBody)return error(request,413);
  std::string body;
  if(request->method==HTTP_POST) {
    if(!api)return error(request,404);
    if(!header(request,"Content-Type",80,contentType))return error(request,415);
    contentType=contentType.substr(0,contentType.find(';'));
    while(!contentType.empty() && contentType.back()==' ')contentType.pop_back();
    for(char& c:contentType)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(contentType!="application/json")return error(request,415);
    if(!request->content_len)return error(request,400);
    body.resize(request->content_len);size_t received=0;const uint32_t started=millis();
    while(received<body.size()) {
      const uint32_t elapsed=millis()-started;if(elapsed>=_options.receiveMillis)return error(request,408);
      const uint32_t remaining=_options.receiveMillis-elapsed;
      timeval timeout{};timeout.tv_sec=remaining/1000;timeout.tv_usec=(remaining%1000)*1000;
      if(setsockopt(httpd_req_to_sockfd(request),SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))!=0)return error(request,500);
      const int count=httpd_req_recv(request,&body[received],body.size()-received);
      if(uint32_t(millis()-started)>=_options.receiveMillis || count==HTTPD_SOCK_ERR_TIMEOUT)return error(request,408);
      if(count<=0 || size_t(count)>body.size()-received)return error(request,400);
      received+=count;
    }
    std::lock_guard<std::mutex> lock(_mutex);_lastActivity=millis();
  } else if(request->content_len)return error(request,400);
  if(!api) {
    {std::lock_guard<std::mutex> lock(_mutex);_lastActivity=millis();}
    if(path=="/" && canonical && _htmlSize) {
      httpd_resp_set_hdr(request,"Content-Encoding","gzip");
      return respond(request,200,reinterpret_cast<const char*>(_html),_htmlSize,"text/html; charset=utf-8");
    }
    const std::string guide="<!doctype html><html lang=\"ja\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>Wi-Fi設定</title><h1>Wi-Fi設定</h1><p>このWi-Fiへの接続を維持し、Safariまたは通常のブラウザーで次のURLを開いてください。</p><p>http://"+_host+"</p><p>「インターネットに接続せずに使用」を選べる場合は選択してください。ログイン画面のキャンセルでWi-Fiが切れる場合は、このWi-Fiへ接続し直してください。</p></html>";
    return respond(request,200,guide.data(),guide.size(),"text/html; charset=utf-8");
  }
  PortalResponse response;
  if(path=="/api/session" && request->method==HTTP_GET) {
    if(_sessionHandler)response=_sessionHandler(_token);
    else response.body="{\"token\":"+quote(_token)+"}";
  }
  else if(path=="/api/activity" && request->method==HTTP_POST) {}
  else if(path=="/api/scan") {
    std::lock_guard<std::mutex> lock(_mutex);
    if(request->method==HTTP_POST) {
      if(_scanRequested || _scanState==WiFiScanState::Scanning)return error(request,409);
      _scanRequested=true;_scanState=WiFiScanState::Scanning;response.status=202;
    } else {
      response.body="{\"state\":"+quote(_scanRequested || _scanState==WiFiScanState::Scanning?"scanning":
          _scanState==WiFiScanState::Ready?"ready":_scanState==WiFiScanState::Failed?"failed":"idle")+",\"networks\":[";
      for(size_t i=0;i<_scanResults.size();++i) {
        const auto& entry=_scanResults[i];if(i)response.body+=',';
        response.body+="{\"ssid\":"+quote(entry.ssid)+",\"rssi\":"+std::to_string(entry.rssi)+",\"open\":"+(entry.open?"true":"false")+"}";
      }
      response.body+="]}";
    }
  } else {
    const PortalMethod method=request->method==HTTP_GET?PortalMethod::Get:PortalMethod::Post;
    const auto found=std::find_if(_routes.begin(),_routes.end(),[&](const Route& route){return route.method==method && route.path==path;});
    if(found==_routes.end())return error(request,404);
    response=found->handler({method,path,std::move(body)});
  }
  return respond(request,response.status,response.body.data(),response.body.size());
}
/** @brief DNS・スキャン・Probeを所有タスクからだけ進め、停止要求も同じ場所で処理する。 */
void ProvisioningPortalESP32::tick() {
  if(_stopRequested.exchange(false)) {stop();return;}
  if(!_running)return;
  bool scan=false;
  {std::lock_guard<std::mutex> lock(_mutex);scan=_scanRequested;_scanRequested=false;}
  if(scan) {_scanRejected=!_probe.startScan();}
  _probe.poll();
  if(!_probe.result().apSuspended)_dns.processNextRequest();
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _scanState=_scanRejected?WiFiScanState::Failed:_probe.scanState();_scanResults=_probe.scanResults();
  }
}
/** @brief HTTPハンドラーからは停止を予約するだけにし、自己タスクの終了待ちを避ける。 */
void ProvisioningPortalESP32::requestStop() {_stopRequested=true;}
/** @brief HTTP要求の終了後にDNSと無線を停止する。所有タスクから呼ぶ。 */
void ProvisioningPortalESP32::stop() {
  if(!_running.exchange(false))return;
  if(_server) {httpd_stop(_server);_server=nullptr;}
  _dns.stop();_probe.stop();_token.clear();
}
bool ProvisioningPortalESP32::running() const {return _running;}
uint32_t ProvisioningPortalESP32::lastActivityMillis() const {std::lock_guard<std::mutex> lock(_mutex);return _lastActivity;}
}
#endif
