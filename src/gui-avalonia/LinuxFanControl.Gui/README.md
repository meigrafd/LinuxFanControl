# LinuxFanControl.Gui (Avalonia / .NET 9)

Minimal but working GUI shell for Linux Fan Control with runtime Themes+Locales, Setup flow, and tile-based dashboard.

## Build
```bash
cd LinuxFanControl.Gui
dotnet build -c Debug
dotnet run -c Debug
```

## What's inside
- Themes from `Themes/*.json` (no hardcoding).
- Locales from `Locales/*.json` (EN+DE provided).
- Setup dialog to pick theme+language and optional mock detection.
- Dashboard with simple fan tiles, updated by a mock telemetry loop.

## Next
- Wire JSON-RPC (batch) to daemon.
- Real detection progress + charts.
- Draggable tile reordering + profiles.
