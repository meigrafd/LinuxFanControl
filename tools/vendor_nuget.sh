#!/usr/bin/env bash
set -euo pipefail
# (c) 2025 LinuxFanControl contributors. MIT License.
# Download required .nupkg into third_party/nuget for offline restore.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/tools/nuget"
mkdir -p "$DEST"

# Paket + Versionen zentral definieren (GUI benötigt diese Stände)
pkgs=(
  "Avalonia:11.2.0"
  "Avalonia.Desktop:11.2.0"
  "Avalonia.Themes.Fluent:11.2.0"
  "Avalonia.Controls.DataGrid:11.2.0"
  "CommunityToolkit.Mvvm:8.2.2"
  "LiveChartsCore:2.0.0-rc3"
  "LiveChartsCore.SkiaSharpView:2.0.0-rc3"
  "LiveChartsCore.SkiaSharpView.Avalonia:2.0.0-rc3"
  "SkiaSharp:2.88.8"
)

echo "[i] Downloading .nupkg into ${DEST}"
for spec in "${pkgs[@]}"; do
  name="${spec%%:*}"
  ver="${spec##*:}"
  echo "  - ${name} ${ver}"
  dotnet nuget download "${name}" --version "${ver}" --source https://api.nuget.org/v3/index.json --output "${DEST}"
done

echo "[i] Done. You can now restore offline with this NuGet.config."
