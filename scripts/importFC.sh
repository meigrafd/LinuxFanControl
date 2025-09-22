#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Linux Fan Control — FanControl.Release Import & Live Progress (raw JSON-RPC)
# (c) 2025 LinuxFanControl contributors
# -----------------------------------------------------------------------------
# Purpose:
#   • Start import of a FanControl.Release profile via lfcd's JSON-RPC (raw TCP)
#   • Poll and display live progress until finished (done/error)
#   • Optionally commit the imported profile
#
# Expected RPC methods (must exist on server):
#   - profile.importAs     -> params: { path, name, validateDetect, rpmMin, timeoutMs } -> { jobId }
#   - profile.importStatus -> params: { jobId } -> { state, progress, message, error, profileName, isFanControlRelease, ... }
#   - profile.importCommit -> params: { jobId } -> persist profile
#
# Transport:
#   - Raw TCP JSON-RPC (newline-terminated, one JSON per line). No HTTP.
#   - Uses /dev/tcp (preferred). Falls back to socat or nc if /dev/tcp is not available.
# -----------------------------------------------------------------------------

set -euo pipefail

# ----------------------------- Defaults --------------------------------------
HOST="127.0.0.1"
PORT="8777"                # set to your lfcd TCP port

FCR_PATH=""                # -f /path/to/FanControl.json
PROFILE_NAME="Imported"    # -n "ProfileName"
VALIDATE_DETECT=0          # -V (bool)
RPM_MIN=0                  # -r 800
TIMEOUT_MS=0               # -T 4000

DO_COMMIT=0                # -C
POLL_MS=500                # -I 500
ID_START=1                 # -i 1

QUIET=0                    # -q
COLOR=1                    # --no-color -> 0
JQ_PRETTY=0                # -J (pretty-print raw responses with jq)

# ----------------------------- Usage (FULL) ----------------------------------
usage() {
  cat <<'USAGE'
importFC.sh - FanControl.Release import & progress for Linux Fan Control (lfcd)

Usage:
  importFC.sh -f /path/to/userConfig.json [-n Name] [-V] [-r RPM] [-T ms] [-C]
               [-H host] [-p port] [-I ms] [-i startId] [-q] [--no-color] [-J]

Options:
  -f FILE     Path to FanControl.Release JSON (required)
  -n NAME     Profile name (default: "Imported")
  -V          Enable deterministic validation step
  -r RPM      Minimum RPM threshold check (default: 0 = disabled)
  -T ms       Timeout for RPM threshold check (default: 0 = disabled)
  -C          Commit the imported profile after success
  -H HOST     Target host (default: 127.0.0.1)
  -p PORT     Target port (default: 8777)
  -I ms       Poll interval (default: 500)
  -i N        Starting JSON-RPC id (default: 1)
  -q          Quiet mode (less logs)
  --no-color  Disable colored output
  -J          Pretty-print raw JSON responses with jq (if available)

Example:
  importFC.sh -f ~/Downloads/FanControl.json -n "MyProfile" -V -r 900 -T 4000 -C
USAGE
}

# ----------------------------- Args ------------------------------------------
while (( "$#" )); do
  case "$1" in
    -f) FCR_PATH="${2:-}"; shift 2 ;;
    -n) PROFILE_NAME="${2:-}"; shift 2 ;;
    -V) VALIDATE_DETECT=1; shift ;;
    -r) RPM_MIN="${2:-0}"; shift 2 ;;
    -T) TIMEOUT_MS="${2:-0}"; shift 2 ;;
    -C) DO_COMMIT=1; shift ;;
    -H) HOST="${2:-}"; shift 2 ;;
    -p) PORT="${2:-}"; shift 2 ;;
    -I) POLL_MS="${2:-500}"; shift 2 ;;
    -i) ID_START="${2:-1}"; shift 2 ;;
    -q) QUIET=1; shift ;;
    --no-color) COLOR=0; shift ;;
    -J) JQ_PRETTY=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ -z "$FCR_PATH" ]]; then
  echo "Error: -f /path/to/FCR.json is required." >&2
  exit 2
fi
if [[ ! -r "$FCR_PATH" ]]; then
  echo "Error: file not readable: $FCR_PATH" >&2
  exit 2
fi

# ----------------------------- Helpers ---------------------------------------
log() { [[ "$QUIET" -eq 0 ]] && echo -e "$*" >&2 || true; }
has_jq() { command -v jq >/dev/null 2>&1; }
ts() { date +"%H:%M:%S"; }

# Colors
if [[ "$COLOR" -eq 1 ]]; then
  C_B="\033[1m"; C_R="\033[31m"; C_G="\033[32m"; C_Y="\033[33m"; C_C="\033[36m"; C_N="\033[0m"
else
  C_B=""; C_R=""; C_G=""; C_Y=""; C_C=""; C_N=""
fi

# Preferred: /dev/tcp (bidirectional; reliably reads server response)
send_raw_devtcp() {
  local data="$1" host="$2" port="$3"
  # Ensure newline termination (line-delimited JSON on the server)
  [[ "$data" == *$'\n' ]] || data="${data}"$'\n'

  # Open bidirectional socket on FD 3
  exec 3<>"/dev/tcp/${host}/${port}" || return 15

  # Send request
  printf '%s' "$data" >&3

  # Read EXACTLY ONE LINE (server keeps the socket open for further requests)
  # Add a safety timeout so we don't hang forever if something goes wrong.
  local line=""
  if ! IFS= read -r -t 10 line <&3; then
    # Timeout or read error; still close the FD and propagate failure
    exec 3>&- 3<&-
    return 16
  fi

  printf '%s\n' "$line"

  # Close socket
  exec 3>&- 3<&-
  return 0
}


# Fallbacks: socat / nc
send_raw_fallback() {
  local data="$1" host="$2" port="$3"
  [[ "$data" == *$'\n' ]] || data="${data}"$'\n'

  if command -v socat >/dev/null 2>&1; then
    printf '%s' "$data" | socat - TCP:"$host":"$port"
    return $?
  fi

  if command -v nc >/dev/null 2>&1; then
    local help; help="$(nc -h 2>&1 || true)"
    if echo "$help" | grep -qi -- ' -N'; then
      printf '%s' "$data" | nc -N "$host" "$port"
    elif echo "$help" | grep -qi -- ' -q '; then
      printf '%s' "$data" | nc -q 1 "$host" "$port"
    else
      printf '%s' "$data" | nc "$host" "$port"
    fi
    return $?
  fi

  return 14
}

send_raw() {
  local data="$1" host="$2" port="$3"
  if send_raw_devtcp "$data" "$host" "$port"; then
    return 0
  fi
  send_raw_fallback "$data" "$host" "$port"
}

rpc_id="$ID_START"
rpc_call() {
  local method="$1"; local params="$2"; local id="$rpc_id"
  rpc_id=$((rpc_id+1))
  local req
  req=$(printf '{"jsonrpc":"2.0","id":%s,"method":"%s","params":%s}' "$id" "$method" "$params")
  send_raw "$req" "$HOST" "$PORT"
}

# Extract helper with jq or minimal sed fallback
json_get() {
  local j="$1" filt="$2"
  if has_jq; then
    echo "$j" | jq -er "$filt" 2>/dev/null || return 1
  else
    case "$filt" in
      .result.data.jobId)             sed -n 's/.*"jobId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.state)             sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.progress)          sed -n 's/.*"progress"[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.message)           sed -n 's/.*"message"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.error)             sed -n 's/.*"error"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.profileName)       sed -n 's/.*"profileName"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1 ;;
      .result.data.isFanControlRelease) sed -n 's/.*"isFanControlRelease"[[:space:]]*:[[:space:]]*\(true\|false\).*/\1/p' <<<"$j" | head -n1 ;;
      *) return 1 ;;
    esac
  fi
}

print_resp_if_requested() {
  local resp="$1"
  if [[ "$JQ_PRETTY" -eq 1 && $(has_jq; echo $?) -eq 0 ]]; then
    echo "$resp" | jq .
  fi
}

# ----------------------------- Start Import ----------------------------------
# Build params — IMPORTANT: server expects keys "name" and "validateDetect"
params_import="$(jq -cn --arg p "$FCR_PATH" \
                       --arg n "$PROFILE_NAME" \
                       --argjson v "$VALIDATE_DETECT" \
                       --argjson r "$RPM_MIN" \
                       --argjson t "$TIMEOUT_MS" \
                       '{path:$p, name:$n, validateDetect:($v==1), rpmMin:$r, timeoutMs:$t}' 2>/dev/null || true)"
if [[ -z "$params_import" ]]; then
  params_import="{\"path\":\"$FCR_PATH\",\"name\":\"$PROFILE_NAME\",\"validateDetect\":$([[ $VALIDATE_DETECT -eq 1 ]] && echo true || echo false),\"rpmMin\":$RPM_MIN,\"timeoutMs\":$TIMEOUT_MS}"
fi

log "${C_C}[$(ts)] Starting import:${C_N} path=${FCR_PATH}, name=${PROFILE_NAME}, validateDetect=${VALIDATE_DETECT}, rpmMin=${RPM_MIN}, timeoutMs=${TIMEOUT_MS}"

resp_start="$(rpc_call "profile.importAs" "$params_import" || true)"
if [[ -z "${resp_start:-}" ]]; then
  echo "Error: no response from server (profile.importAs)." >&2
  exit 20
fi
print_resp_if_requested "$resp_start"

jobId="$(json_get "$resp_start" '.result.data.jobId' || true)"
if [[ -z "$jobId" ]]; then
  echo "Error: jobId not found in response from profile.importAs." >&2
  exit 21
fi
log "${C_G}[$(ts)] Import job started:${C_N} jobId=${jobId}"

# ----------------------------- Poll Status -----------------------------------
state="pending"
progress="0"
message=""
errorMsg=""

while true; do
  params_status="{\"jobId\":\"$jobId\"}"
  resp_stat="$(rpc_call "profile.importStatus" "$params_status" || true)"
  if [[ -z "${resp_stat:-}" ]]; then
    echo "Error: no response from server (profile.importStatus)." >&2
    exit 22
  fi

  print_resp_if_requested "$resp_stat"

  # Primary: fields under .result.data
  state="$(json_get "$resp_stat" '.result.data.state' || true)"
  progress="$(json_get "$resp_stat" '.result.data.progress' || true)"
  message="$(json_get "$resp_stat" '.result.data.message' || true)"
  errorMsg="$(json_get "$resp_stat" '.result.data.error' || true)"

  # Fallback: nested .result.data.status.*
  if [[ -z "$state" ]]; then
    state="$(json_get "$resp_stat" '.result.data.status.state' || true)"
    progress="$(json_get "$resp_stat" '.result.data.status.progress' || true)"
    message="$(json_get "$resp_stat" '.result.data.status.message' || true)"
    errorMsg="$(json_get "$resp_stat" '.result.data.status.error' || true)"
  fi

  [[ -z "$state" ]] && state="running"
  [[ -z "$progress" ]] && progress="0"

  case "$state" in
    running|pending)
      log "[$(ts)] ${C_B}${progress}%${C_N} ${message}"
      ;;
    done)
      log "${C_G}[$(ts)] Import finished.${C_N}"
      break
      ;;
    error)
      log "${C_R}[$(ts)] Import error:${C_N} ${errorMsg:-$message}"
      exit 23
      ;;
    *)
      log "[$(ts)] ${state}: ${progress}% ${message}"
      ;;
  esac

  sleep "$(awk -v ms="$POLL_MS" 'BEGIN{ printf("%.3f", ms/1000.0) }')"
done

# ----------------------------- Commit (optional) -----------------------------
if [[ "$DO_COMMIT" -eq 1 ]]; then
  log "${C_C}[$(ts)] Committing profile...${C_N}"
  resp_commit="$(rpc_call "profile.importCommit" "{\"jobId\":\"$jobId\"}" || true)"
  if [[ -z "${resp_commit:-}" ]]; then
    echo "Error: no response from server (profile.importCommit)." >&2
    exit 24
  fi
  print_resp_if_requested "$resp_commit"
  log "${C_G}[$(ts)] Commit done.${C_N}"
fi
