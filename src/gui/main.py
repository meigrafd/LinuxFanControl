#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys, os, json
from PyQt6 import QtWidgets, QtCore, QtGui
try:
    import pyqtgraph as pg
except Exception:
    pg=None

from ipc import LfcdClient
from importer import load_fancontrol_json

APP_TITLE = "Linux Fan Control (GUI)"
CONFIG_PATH = os.path.expanduser("~/.config/linux_fan_control/gui_config.json")

def clamp(v,lo,hi): return max(lo,min(hi,v))

class MiniCurve(pg.PlotWidget if pg else QtWidgets.QWidget):
    def __init__(self, points, parent=None):
        if pg:
            super().__init__(parent=parent); self.setBackground(None); self.setFixedHeight(80)
            self.getPlotItem().hideAxis('left'); self.getPlotItem().hideAxis('bottom')
            self.getPlotItem().setMenuEnabled(False); self.getPlotItem().setMouseEnabled(x=False, y=False)
            self._line = self.plot([],[], pen=pg.mkPen(width=2))
            self.set_points(points)
        else:
            super().__init__(parent); self._pts=points; self.setFixedHeight(80)
    def set_points(self, pts):
        if pg:
            xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
            self._line.setData(xs, ys)
        else:
            self._pts=pts; self.update()
    def paintEvent(self, ev):
        if pg: return super().paintEvent(ev)
        qp=QtGui.QPainter(self); qp.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)
        r=self.rect().adjusted(8,8,-8,-8); qp.setPen(QtGui.QPen(QtGui.QColor("#6aa3ff"),2))
        if len(self._pts)>=2:
            x0=self._pts[0][0]; x1=self._pts[-1][0]
            def mx(x): return r.left()+(x-x0)/(x1-x0+1e-9)*r.width()
            def my(y): return r.bottom()-(y/100.0)*r.height()
            for i in range(1,len(self._pts)):
                xa,ya=self._pts[i-1]; xb,yb=self._pts[i]
                qp.drawLine(int(mx(xa)),int(my(ya)),int(mx(xb)),int(my(yb)))

class ChannelCard(QtWidgets.QFrame):
    editRequested = QtCore.pyqtSignal(object)
    def __init__(self, ch, parent=None):
        super().__init__(parent); self.ch=ch
        self.setFrameShape(QtWidgets.QFrame.Shape.StyledPanel); self.setStyleSheet("QFrame{border-radius:10px;}")
        v=QtWidgets.QVBoxLayout(self); v.setContentsMargins(12,10,12,10); v.setSpacing(6)
        self.title=QtWidgets.QLabel(ch["name"]); self.title.setWordWrap(True); v.addWidget(self.title)
        sub=QtWidgets.QHBoxLayout()
        sub.addWidget(QtWidgets.QLabel(ch.get("sensor") or "—"))
        sub.addStretch(1)
        self.out=QtWidgets.QLabel("-- %"); sub.addWidget(self.out); v.addLayout(sub)
        self.mini=MiniCurve(ch["curve"]); v.addWidget(self.mini)
        btn=QtWidgets.QPushButton("Edit"); btn.clicked.connect(lambda: self.editRequested.emit(self.ch)); v.addWidget(btn)
    def set_output(self, duty): self.out.setText(f"{duty:.0f} %")
    def set_name(self, name):   self.title.setText(name)

class MappingDialog(QtWidgets.QDialog):
    """Wizard to map imported channels to real sensors/PWMs."""
    def __init__(self, channels, temps, pwms, parent=None):
        super().__init__(parent); self.setWindowTitle("Map Controls"); self.resize(900,500)
        self._channels=channels; self._temps=temps; self._pwms=pwms
        v=QtWidgets.QVBoxLayout(self)
        self.tbl=QtWidgets.QTableWidget(0,6)
        self.tbl.setHorizontalHeaderLabels(["Use","Name","Sensor","PWM","Enable","Writable"])
        self.tbl.horizontalHeader().setStretchLastSection(True)
        v.addWidget(self.tbl)
        for ch in channels:
            r=self.tbl.rowCount(); self.tbl.insertRow(r)
            chk=QtWidgets.QCheckBox(); chk.setChecked(True); self.tbl.setCellWidget(r,0,chk)
            self.tbl.setItem(r,1,QtWidgets.QTableWidgetItem(ch["name"]))
            cbS=QtWidgets.QComboBox(); [cbS.addItem(t["label"],t) for t in temps]; self.tbl.setCellWidget(r,2,cbS)
            cbP=QtWidgets.QComboBox(); [cbP.addItem(p["label"],p) for p in pwms]; self.tbl.setCellWidget(r,3,cbP)
            self.tbl.setItem(r,4,QtWidgets.QTableWidgetItem(""))
            self.tbl.setItem(r,5,QtWidgets.QTableWidgetItem("unknown"))
        hb=QtWidgets.QHBoxLayout(); hb.addStretch(1)
        self.btn_probe=QtWidgets.QPushButton("Probe writability"); hb.addWidget(self.btn_probe)
        self.btn_ok=QtWidgets.QPushButton("Create Channels"); hb.addWidget(self.btn_ok)
        v.addLayout(hb)
        self.btn_ok.clicked.connect(self.accept)
        self.btn_probe.clicked.connect(self._probe)

    def _probe(self):
        cli=LfcdClient()
        for r in range(self.tbl.rowCount()):
            pwmd=self.tbl.cellWidget(r,3).currentData()
            res=cli.probe_pwm(pwmd["label"])
            self.tbl.item(r,5).setText("yes" if res["writable"] else f"no ({res.get('reason','')})")
            self.tbl.item(r,4).setText(pwmd.get("enable_path") or "")
    def selected(self):
        out=[]
        for r in range(self.tbl.rowCount()):
            if not self.tbl.cellWidget(r,0).isChecked(): continue
            ch={"name": self.tbl.item(r,1).text()}
            ch["sensor"]= self.tbl.cellWidget(r,2).currentData()["label"]
            ch["output"]= self.tbl.cellWidget(r,3).currentData()
            out.append(ch)
        return out

class Main(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle(APP_TITLE); self.resize(1200,800)
        self.cli=LfcdClient()
        self.channels=[]   # [{name, sensor(label), output{paths}, curve, ...}]
        self.temps=[]; self.pwms=[]
        self.toolbar=self.addToolBar("tb")
        act_detect=QtGui.QAction("Detect", self); act_import=QtGui.QAction("Import FanControl JSON", self)
        self.toolbar.addAction(act_detect); self.toolbar.addAction(act_import)
        act_detect.triggered.connect(self.detect)
        act_import.triggered.connect(self.import_fc)

        self.scroll=QtWidgets.QScrollArea(); self.scroll.setWidgetResizable(True); self.setCentralWidget(self.scroll)
        self.wrap=QtWidgets.QWidget(); self.scroll.setWidget(self.wrap)
        self.grid=QtWidgets.QGridLayout(self.wrap); self.grid.setContentsMargins(12,12,12,12); self.grid.setSpacing(10)
        self.cards=[]

        self.timer=QtCore.QTimer(self); self.timer.setInterval(1000); self.timer.timeout.connect(self.tick); self.timer.start()
        self.detect()

    def rebuild(self):
        for i in reversed(range(self.grid.count())):
            w=self.grid.itemAt(i).widget()
            if w: w.setParent(None)
        self.cards=[]
        cols=4
        for i,ch in enumerate(self.channels):
            card=ChannelCard(ch); self.cards.append(card)
            r,c=divmod(i, cols); self.grid.addWidget(card, r,c)

    def detect(self):
        res=self.cli.enumerate()
        self.temps=res["temps"]; self.pwms=res["pwms"]
        self.rebuild()

    def import_fc(self):
        fn,_=QtWidgets.QFileDialog.getOpenFileName(self,"Open FanControl JSON","","JSON (*.json)")
        if not fn: return
        imported=load_fancontrol_json(fn)
        # mapping wizard
        dlg=MappingDialog(imported["channels"], self.temps, self.pwms, self)
        if dlg.exec()!=QtWidgets.QDialog.DialogCode.Accepted: return
        mapping=dlg.selected()
        # build channels
        self.channels=[]
        for chImp, mapSel in zip(imported["channels"], mapping):
            ch={
                "name": mapSel["name"],
                "sensor": mapSel["sensor"],
                "output": mapSel["output"],
                "curve": chImp["curve"],
                "mode": chImp["mode"],
                "hyst": chImp["hyst"],
                "tau":  chImp["tau"],
                "manual": 0.0
            }
            self.channels.append(ch)
        self.rebuild()
        self.save_config()

    def save_config(self):
        os.makedirs(os.path.dirname(CONFIG_PATH), exist_ok=True)
        with open(CONFIG_PATH,"w") as f:
            json.dump({"version":"0.1","channels":self.channels}, f, indent=2)

    def tick(self):
        # apply AUTO channels (very basic demo: just write midpoint of curve)
        for ch, card in zip(self.channels, self.cards):
            out=ch["output"]
            if not out.get("writable", False): 
                card.set_output(0); continue
            # read temp
            s_lbl=ch["sensor"]
            s = next((t for t in self.temps if t["label"]==s_lbl), None)
            if not s: continue
            try: val=self.cli.read_temp(s["path"])
            except Exception: continue
            # eval curve (piecewise linear)
            pts=sorted(ch["curve"], key=lambda p:p[0])
            y=pts[0][1] if val<=pts[0][0] else pts[-1][1] if val>=pts[-1][0] else None
            if y is None:
                for i in range(1,len(pts)):
                    x0,y0=pts[i-1]; x1,y1=pts[i]
                    if x0<=val<=x1:
                        t=(val-x0)/(x1-x0+1e-9); y=y0+t*(y1-y0); break
            y=clamp(y, 0, 100)
            try: self.cli.write_pwm(out["pwm_path"], y)
            except Exception: pass
            card.set_output(y)

def main():
    app=QtWidgets.QApplication(sys.argv)
    w=Main(); w.show()
    sys.exit(app.exec())

if __name__=="__main__":
    main()
