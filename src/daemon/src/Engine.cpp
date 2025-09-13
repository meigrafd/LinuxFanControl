#include "Engine.hpp"
#include <chrono>
#include <cmath>

void Engine::start(){
  if(running_.load()) return; running_.store(true);
  th_ = std::thread([this](){
    using namespace std::chrono;
    while(running_.load()){
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      double now = duration<double>(steady_clock::now().time_since_epoch()).count();
      std::lock_guard<std::mutex> lk(mx_);
      std::map<std::string,double> temps;
      for(const auto& s : sensors_){
        auto v = Hwmon::readTempC(s.path); if(v) temps[s.path]=*v;
      }
      for(auto& ch : channels_){
        double duty = ch.manual;
        if(ch.mode=="Auto"){
          double sv = temps.count(ch.sensor_path)? temps[ch.sensor_path] : Hwmon::readTempC(ch.sensor_path).value_or(NAN);
          if(!std::isnan(sv)) duty = ch.ss.step(ch.curve, sv, now);
        }
        for(const auto& o : outputs_){
          if(o.label==ch.output_label){ Hwmon::writePwmPct(o.pwm_path, (int)llround(duty)); break; }
        }
      }
    }
  });
}
void Engine::stop(){ if(!running_.load()) return; running_.store(false); if(th_.joinable()) th_.join(); }
