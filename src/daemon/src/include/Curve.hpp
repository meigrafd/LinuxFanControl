#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

struct CurvePoint { double x{0}, y{0}; };

struct Curve {
  std::vector<CurvePoint> points {{20,20},{40,40},{60,80}};
  double eval(double x) const {
    if(points.empty()) return 0.0;
    if(x<=points.front().x) return points.front().y;
    if(x>=points.back().x)  return points.back().y;
    for(size_t i=1;i<points.size();++i){
      const auto& a=points[i-1]; const auto& b=points[i];
      if(x>=a.x && x<=b.x){ double t=(x-a.x)/(b.x-a.x); return a.y + t*(b.y-a.y); }
    }
    return points.back().y;
  }
};

struct SchmittSlew {
  double hystC{0.5};
  double tauS{2.0};
  double lastY{0.0};
  double lastX{NAN};
  bool dirUp{true};
  double lastT{0.0};
  double step(const Curve& c, double x, double now){
    if(!std::isnan(lastX)) dirUp = x>lastX;
    double xeff = x + (dirUp? +hystC/2.0 : -hystC/2.0);
    double tgt = std::clamp(c.eval(xeff), 0.0, 100.0);
    lastX = x;
    if(tauS<=0){ lastY=tgt; lastT=now; return lastY; }
    if(lastT==0){ lastT=now; lastY=tgt; return lastY; }
    double dt = std::max(0.0, now-lastT); lastT=now;
    if(dt<=0) return lastY;
    double alpha = 1.0 - std::exp(-dt/tauS);
    lastY = lastY + alpha*(tgt-lastY);
    return std::clamp(lastY,0.0,100.0);
  }
};
