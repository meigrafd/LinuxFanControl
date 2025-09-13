# LinuxFanControl – Hybrid (C++ Daemon + Avalonia GUI)

- Daemon (C++17/CMake, optional **libsensors**; fallback `/sys/class/hwmon`)
- GUI (.NET 9, **Avalonia**) im Stil FanControl.Release: Tiles (Drag&Drop), Setup (Detect/Calibrate), Profile, Curve-Editor (Punkte), Hysterese/Tau, i18n (en/de), Theme Switch.
- IPC: JSON-RPC 2.0 (Batch) via stdio.

## Build

### Daemon
```bash
./build.sh
./run.sh daemon
```
Falls libsensors nicht automatisch gefunden wird:
```bash
cd build
cmake -DSENSORS_INCLUDE_DIRS=/usr/include -DSENSORS_LIBRARIES=/usr/lib64/libsensors.so ..
cmake --build . -j
```

### GUI
```bash
cd src/gui-avalonia/LinuxFanControl.Gui
dotnet restore
export LFC_DAEMON=../../../build/lfcd
export LFC_DAEMON_ARGS="--debug"
dotnet run -c Debug
```

## Logs
- /tmp/daemon_lfc.log
- /tmp/gui_lfc.log
