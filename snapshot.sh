#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="${1:-$(pwd)}"
OUT_DIR="${2:-/tmp/lfc_snapshot}"
mkdir -p "$OUT_DIR"

echo "[i] Root        : $ROOT"
echo "[i] Output dir  : $OUT_DIR"

# Filter: alles außer Build-/Tool-Ordnern und großen Binary-Artefakten
readarray -t FILES < <(
  find "$ROOT" -type f \
    -not -path "*/.git/*" \
    -not -path "*/build/*" \
    -not -path "*/bin/*" \
    -not -path "*/obj/*" \
    -not -path "*/.idea/*" \
    -not -path "*/.vs/*" \
    -not -path "*/.vscode/*" \
    -not -name "*.png" \
    -not -name "*.jpg" \
    -not -name "*.jpeg" \
    -not -name "*.gif" \
    -not -name "*.svg" \
    -not -name "*.ico" \
    -not -name "*.pdf" \
    -not -name "*.zip" \
    -not -name "*.tar" \
    -not -name "*.tar.gz" \
    -not -name "*.tgz" \
    -not -name "*.7z" \
    -not -name "*.dll" \
    -not -name "*.so" \
    -not -name "*.wasm" \
    -not -name "*.pdb" \
    -not -name "*.a" \
    -not -name "*.o" \
    -not -name "project.assets.json" \
    | LC_ALL=C sort
)

# 1) Manifest
MANIFEST="$OUT_DIR/manifest.txt"
{
  echo "# LinuxFanControl snapshot manifest"
  echo "# generated: $(date -Is)"
  echo "# root: $ROOT"
  echo
  for f in "${FILES[@]}"; do
    rel="${f#$ROOT/}"
    # kurze Größe + SHA256
    size=$(wc -c <"$f" | tr -d ' ')
    sha=$(sha256sum "$f" | awk '{print $1}')
    echo "$rel|$size|$sha"
  done
} > "$MANIFEST"
echo "[i] Wrote $MANIFEST"

# 2) Dump aller Dateien mit Inhalt
DUMP="$OUT_DIR/repo_dump.txt"
: > "$DUMP"
for f in "${FILES[@]}"; do
  rel="${f#$ROOT/}"
  size=$(wc -c <"$f" | tr -d ' ')
  sha=$(sha256sum "$f" | awk '{print $1}')
  {
    echo
    echo "===== FILE START ====="
    echo "PATH : $rel"
    echo "SIZE : $size bytes"
    echo "SHA256: $sha"
    echo "----- CONTENT BEGIN -----"
    # sicherstellen, dass alles lesbar UTF-8 ist
    if iconv -f UTF-8 -t UTF-8 "$f" >/dev/null 2>&1; then
      cat "$f"
    else
      iconv -f ISO-8859-1 -t UTF-8 "$f" 2>/dev/null || cat "$f"
    fi
    echo
    echo "----- CONTENT END -----"
    echo "=====  FILE END  ====="
  } >> "$DUMP"
done
echo "[i] Wrote $DUMP"

# Optional zusätzlich ZIP
ZIP="$OUT_DIR/lfc_snapshot.zip"
( cd "$ROOT" && \
  zip -q -r "$ZIP" . \
    -x "*.git/*" "build/*" "bin/*" "obj/*" ".idea/*" ".vs/*" ".vscode/*" \
    "*.png" "*.jpg" "*.jpeg" "*.gif" "*.svg" "*.ico" "*.pdf" "*.zip" "*.tar" "*.tar.gz" "*.tgz" "*.7z" \
    "*.dll" "*.so" "*.wasm" "*.pdb" "*.a" "*.o" "project.assets.json" \
)
echo "[i] Wrote $ZIP"

echo
echo "[i] Preview (erste 30 Dateien):"
head -n 30 "$MANIFEST"
echo "[i] Done."
