#pragma once
#include "esp_netif.h"
#include "Arduino.h"
#include <map>
#include <vector>
#include <cstring>
#include <algorithm>
using httpd_handle_t=void*;
constexpr int ESP_FAIL=-1,HTTPD_SOCK_ERR_TIMEOUT=-3;
enum httpd_method_t {HTTP_GET,HTTP_POST};
struct httpd_req_t {
  httpd_handle_t handle=nullptr;
  httpd_method_t method=HTTP_GET;
  const char* uri="/";
  size_t content_len=0;
  void* user_ctx=nullptr;
  int socket=1,recvCalls=0;
  size_t cursor=0,chunk=4096;
  uint32_t recvDelay=0;
  bool disconnect=false;
  std::map<std::string,std::string> headers,responseHeaders;
  std::string body,response,status="200 OK",type;
};
struct httpd_uri_t {const char* uri;httpd_method_t method;esp_err_t(*handler)(httpd_req_t*);void* user_ctx;};
struct httpd_config_t {
  size_t stack_size=4096;
  uint16_t max_open_sockets=7,max_uri_handlers=8,max_resp_headers=8,recv_wait_timeout=5,send_wait_timeout=5;
  bool lru_purge_enable=false;
  void* global_user_ctx=nullptr;
  void(*global_user_ctx_free_fn)(void*)=nullptr;
  esp_err_t(*open_fn)(httpd_handle_t,int)=nullptr;
  bool(*uri_match_fn)(const char*,const char*,size_t)=nullptr;
};
#define HTTPD_DEFAULT_CONFIG() httpd_config_t{}
namespace FakeHttp {
struct State {httpd_config_t config;std::vector<httpd_uri_t> routes;bool running=false;int stops=0;};
inline State& state() {static State result;return result;}
inline esp_err_t request(httpd_req_t& req) {
  req.handle=&state();
  for(const auto& route:state().routes)if(route.method==req.method &&
      (std::strcmp(route.uri,req.uri)==0 || std::strcmp(route.uri,"/*")==0)) {
    req.user_ctx=route.user_ctx;return route.handler(&req);
  }
  return ESP_FAIL;
}
}
inline esp_err_t httpd_start(httpd_handle_t* handle,const httpd_config_t* config) {
  FakeHttp::state()={};FakeHttp::state().config=*config;FakeHttp::state().running=true;
  *handle=&FakeHttp::state();return ESP_OK;
}
inline esp_err_t httpd_stop(httpd_handle_t) {FakeHttp::state().running=false;++FakeHttp::state().stops;return ESP_OK;}
inline void* httpd_get_global_user_ctx(httpd_handle_t) {return FakeHttp::state().config.global_user_ctx;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t* route) {FakeHttp::state().routes.push_back(*route);return ESP_OK;}
inline bool httpd_uri_match_wildcard(const char*,const char*,size_t) {return true;}
inline int httpd_req_to_sockfd(httpd_req_t* req) {return req->socket;}
inline size_t httpd_req_get_hdr_value_len(httpd_req_t* req,const char* key) {return req->headers[key].size();}
inline esp_err_t httpd_req_get_hdr_value_str(httpd_req_t* req,const char* key,char* output,size_t length) {
  const auto& value=req->headers[key];if(value.size()+1>length)return ESP_FAIL;
  std::memcpy(output,value.c_str(),value.size()+1);return ESP_OK;
}
inline int httpd_req_recv(httpd_req_t* req,char* out,size_t length) {
  ++req->recvCalls;fakeMillis+=req->recvDelay;if(req->disconnect)return 0;
  const size_t count=std::min({length,req->chunk,req->body.size()-req->cursor});
  std::memcpy(out,req->body.data()+req->cursor,count);req->cursor+=count;return int(count);
}
inline esp_err_t httpd_resp_set_hdr(httpd_req_t* req,const char* key,const char* value) {req->responseHeaders[key]=value;return ESP_OK;}
inline esp_err_t httpd_resp_set_status(httpd_req_t* req,const char* status) {req->status=status;return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t* req,const char* type) {req->type=type;return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t* req,const char* data,int length) {
  req->response.assign(data,length<0?std::strlen(data):size_t(length));return ESP_OK;
}
