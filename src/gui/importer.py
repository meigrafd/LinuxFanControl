import json, re

def parse_points(arr):
    pts=[]
    for item in arr or []:
        if isinstance(item, str) and "," in item:
            x,y=item.split(",",1)
            pts.append((float(x), float(y)))
        elif isinstance(item, (list,tuple)) and len(item)==2:
            pts.append((float(item[0]), float(item[1])))
    if not pts: pts=[(20,20),(40,40),(60,60),(80,100)]
    pts.sort(key=lambda p:p[0])
    return pts

def load_fancontrol_json(path):
    with open(path,"r",encoding="utf-8") as f:
        data=json.load(f)
    main=data.get("Main",{})
    controls=main.get("Controls",[])
    curves=main.get("FanCurves",[])
    # map curve name -> points
    curve_map={}
    for c in curves:
        name=c.get("Name") or c.get("name")
        if not name: continue
        if "Points" in c:     # Linear
            curve_map[name]=parse_points(c.get("Points"))
        elif "IdleTemperature" in c: # Trigger-like
            idleT=c.get("IdleTemperature",50); loadT=c.get("LoadTemperature",65)
            idleF=c.get("IdleFanSpeed",30);   loadF=c.get("LoadFanSpeed",80)
            curve_map[name]=sorted([(idleT,idleF),(loadT,loadF)], key=lambda p:p[0])
        elif "SelectedFanCurves" in c: # Mix – flatten to average
            pts_lists=[curve_map.get(n,[]) for n in c.get("SelectedFanCurves",[])]
            merged={}
            for pts in pts_lists:
                for x,y in pts: merged.setdefault(x,[]).append(y)
            pts=[(x, sum(v)/len(v)) for x,v in merged.items()] if merged else []
            curve_map[name]=sorted(pts, key=lambda p:p[0]) or [(20,20),(60,60),(80,100)]
    # produce internal channels (sensor/output mapping später per Wizard)
    channels=[]
    for ctrl in controls:
        label = ctrl.get("Name") or ctrl.get("name") or ctrl.get("Identifier","Control")
        sel_curve = (ctrl.get("SelectedFanCurve") or {}).get("Name")
        points = curve_map.get(sel_curve, [(20,20),(60,60),(80,100)])
        mode = "Manual" if ctrl.get("ManualFanSpeedValue") else "Auto"
        channels.append({
            "name": label,
            "curve": points,
            "mode": mode,
            "hyst": float(ctrl.get("ResponseTimeUp", 0.5)),
            "tau":  float(ctrl.get("ResponseTimeDown", 2.0)),
            "sensor": None,     # to be mapped by wizard
            "output": None      # to be mapped by wizard
        })
    return {"channels": channels}
