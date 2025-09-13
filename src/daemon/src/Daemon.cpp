#include "Daemon.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>   // <-- for std::remove_if
#include <string>
#include <vector>

using json = nlohmann::json;

Daemon::Daemon(){
  rpc_.registerMethod("listSensors",[this](const std::string& p){ return listSensors(p); });
  rpc_.registerMethod("listPwms",   [this](const std::string& p){ return listPwms(p); });
  rpc_.registerMethod("enumerate",  [this](const std::string& p){ return enumerate(p); });
  rpc_.registerMethod("detectCalibrate",[this](const std::string& p){ return detectCalibrate(p); });
  rpc_.registerMethod("listChannels",[this](const std::string& p){ return listChannels(p); });
  rpc_.registerMethod("createChannel",[this](const std::string& p){ return createChannel(p); });
  rpc_.registerMethod("deleteChannel",[this](const std::string& p){ return deleteChannel(p); });
  rpc_.registerMethod("setChannelMode",[this](const std::string& p){ return setChannelMode(p); });
  rpc_.registerMethod("setChannelManual",[this](const std::string& p){ return setChannelManual(p); });
  rpc_.registerMethod("setChannelCurve",[this](const std::string& p){ return setChannelCurve(p); });
  rpc_.registerMethod("setChannelHystTau",[this](const std::string& p){ return setChannelHystTau(p); });
  rpc_.registerMethod("engineStart",[this](const std::string& p){ return engineStart(p); });
  rpc_.registerMethod("engineStop",[this](const std::string& p){ return engineStop(p); });
  rpc_.registerMethod("deleteCoupling",[this](const std::string& p){ return deleteCoupling(p); });
  rpc_.registerMethod("noop",[this](const std::string& p){ return noop(p); });
}

void Daemon::run(){
  engine_.setSensors(Hwmon::discoverTemps());
  engine_.setOutputs(Hwmon::discoverPwms());
  rpc_.runStdio();
}

std::string Daemon::listSensors(const std::string&){
  json a=json::array();
  for(const auto& s: engine_.sensors())
    a.push_back({{"device",s.device},{"label",s.label},{"path",s.path},{"unit",s.unit},{"type",s.type}});
  return a.dump();
}

std::string Daemon::listPwms(const std::string&){
  json a=json::array();
  for(const auto& p: engine_.outputs())
    a.push_back({{"device",p.device},{"label",p.label},{"pwm_path",p.pwm_path},{"enable_path",p.enable_path},{"fan_input_path",p.tach_path}});
  return a.dump();
}

std::string Daemon::enumerate(const std::string&){
  json o;
  o["sensors"]=json::parse(listSensors("{}"));
  o["pwms"]=json::parse(listPwms("{}"));
  return o.dump();
}

std::string Daemon::detectCalibrate(const std::string&){
  json res;
  auto sensors = Hwmon::discoverTemps();
  auto pwms    = Hwmon::discoverPwms();
  engine_.setSensors(sensors); engine_.setOutputs(pwms);

  json cal=json::object();
  for(const auto& p: pwms){
    auto r = Hwmon::calibrate(p,0,100,5,0.8,20,100,nullptr);
    cal[p.label] = {{"ok",r.ok},{"min_pct",r.min_pct},{"spinup_pct",r.spinup_pct},{"rpm_at_min",r.rpm_at_min},{"error",r.error}};
  }

  // naive mapping by device id
  std::unordered_map<std::string,std::vector<TempSensor>> bydev;
  for(const auto& s: sensors){
    auto c=s.device.find(':'); std::string hw=c==std::string::npos? s.device: s.device.substr(0,c);
    bydev[hw].push_back(s);
  }
  json map=json::array();
  for(const auto& p: pwms){
    auto c=p.device.find(':'); std::string hw=c==std::string::npos? p.device: p.device.substr(0,c);
    if(bydev.count(hw) && !bydev[hw].empty()){
      map.push_back({{"pwm_label",p.label},{"sensor_label",bydev[hw][0].label},{"sensor_path",bydev[hw][0].path}});
      coupling_[p.label]=bydev[hw][0].path;
    }
  }

  res["sensors"]=json::parse(listSensors("{}"));
  res["pwms"]=json::parse(listPwms("{}"));
  res["mapping"]=map;
  res["calibration"]=cal;
  return res.dump();
}

std::string Daemon::listChannels(const std::string&){
  json a=json::array();
  for(const auto& c: engine_.channels()){
    json pts=json::array(); for(const auto& p : c.curve.points) pts.push_back({{"x",p.x},{"y",p.y}});
    a.push_back({
      {"name",c.name},{"sensor_path",c.sensor_path},{"output_label",c.output_label},{"mode",c.mode},
      {"manual",c.manual},{"hystC",c.ss.hystC},{"tauS",c.ss.tauS},{"points",pts}
    });
  }
  return a.dump();
}

std::string Daemon::createChannel(const std::string& params){
  auto p = json::parse(params);
  Channel c;
  c.name=p.value("name","Channel");
  c.sensor_path=p.value("sensor_path","");
  c.output_label=p.value("output_label","");
  c.mode="Auto";
  c.ss.hystC=p.value("hystC",0.5);
  c.ss.tauS=p.value("tauS",2.0);
  if(p.contains("points"))
    for(const auto& it: p["points"])
      c.curve.points.push_back({it.value("x",0.0),it.value("y",0.0)});
  engine_.channels().push_back(std::move(c));
  return "{\"ok\":true}";
}

std::string Daemon::deleteChannel(const std::string& params){
  auto p=json::parse(params); auto name=p.value("name","");
  auto& v=engine_.channels();
  v.erase(std::remove_if(v.begin(),v.end(),[&](const Channel& c){return c.name==name;}), v.end());
  return "{\"ok\":true}";
}

std::string Daemon::setChannelMode(const std::string& params){
  auto p=json::parse(params); auto n=p.value("name",""); auto m=p.value("mode","Auto");
  for(auto& c: engine_.channels()) if(c.name==n) c.mode=m;
  return "{\"ok\":true}";
}

std::string Daemon::setChannelManual(const std::string& params){
  auto p=json::parse(params); auto n=p.value("name",""); auto v=p.value("value",0.0);
  for(auto& c: engine_.channels()) if(c.name==n){ c.manual=v; c.mode="Manual"; }
  return "{\"ok\":true}";
}

std::string Daemon::setChannelCurve(const std::string& params){
  auto p=json::parse(params); auto n=p.value("name","");
  for(auto& c: engine_.channels()) if(c.name==n){
    c.curve.points.clear();
    for(const auto& it: p["points"]) c.curve.points.push_back({it.value("x",0.0),it.value("y",0.0)});
  }
  return "{\"ok\":true}";
}

std::string Daemon::setChannelHystTau(const std::string& params){
  auto p=json::parse(params); auto n=p.value("name",""); double h=p.value("hystC",0.5), t=p.value("tauS",2.0);
  for(auto& c: engine_.channels()) if(c.name==n){ c.ss.hystC=h; c.ss.tauS=t; }
  return "{\"ok\":true}";
}

std::string Daemon::engineStart(const std::string&){ engine_.start(); return "{\"ok\":true}"; }
std::string Daemon::engineStop(const std::string&){ engine_.stop();  return "{\"ok\":true}"; }
std::string Daemon::deleteCoupling(const std::string&){ coupling_.clear(); return "{\"ok\":true}"; }
std::string Daemon::noop(const std::string&){ return "{\"ok\":true}"; }
