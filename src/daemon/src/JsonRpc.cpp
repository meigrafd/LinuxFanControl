#include "JsonRpc.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using json = nlohmann::json;

JsonRpcServer::JsonRpcServer(){}

void JsonRpcServer::registerMethod(const std::string& name, Handler h){
  std::lock_guard<std::mutex> lk(mtx_); handlers_[name]=std::move(h);
}

static std::string make_result(const std::string& id, const std::string& payload){
  json out; out["jsonrpc"]="2.0"; out["id"]=id;
  try{ out["result"] = json::parse(payload); }catch(...){ out["result"]=json::object(); }
  return out.dump();
}
static std::string make_error(const std::string& id, int code, const std::string& msg){
  json out; out["jsonrpc"]="2.0"; out["id"]=id; out["error"]={{"code",code},{"message",msg}}; return out.dump();
}

void JsonRpcServer::runStdio(){
  running_.store(true);
  std::string line;
  while(running_.load() && std::getline(std::cin, line)){
    if(line.empty()) continue;
    try{
      auto j = json::parse(line);
      auto handle_one = [&](const json& req)->std::string{
        if(!req.contains("id")) return "";
        std::string id = req["id"].is_string()? req["id"].get<std::string>() : std::to_string(req["id"].get<int>());
        std::string method = req.value("method","");
        json params = req.value("params", json::object());
        Handler h; { std::lock_guard<std::mutex> lk(mtx_); auto it=handlers_.find(method); if(it!=handlers_.end()) h=it->second; }
        if(!h) return make_error(id,-32601,"method not found");
        try{ return make_result(id, h(params.dump())); }
        catch(const std::exception& e){ return make_error(id,-32000,e.what()); }
      };
      if(j.is_array()){
        std::cout << "["; bool first=true;
        for(const auto& it : j){ auto resp=handle_one(it); if(resp.empty()) continue; if(!first) std::cout<<","; first=false; std::cout<<resp; }
        std::cout << "]
";
      } else {
        std::cout << handle_one(j) << std::endl;
      }
    }catch(...){
      // ignore
    }
  }
}
void JsonRpcServer::stop(){ running_.store(false); }
