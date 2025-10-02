#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Linux Fan Control — FanControl.Release Import & Live Progress (persistent JSON-RPC)
# (c) 2025 LinuxFanControl contributors
# -----------------------------------------------------------------------------
# Purpose:
#   • Start import of a FanControl.Release profile via lfcd's JSON-RPC (raw TCP)
#   • Keep ONE TCP connection open for the whole import (status polling + commit)
#   • Optionally commit and enable the engine
#
# Expected RPC methods on server:
#   - profile.importAs     -> params: { path, name, validateDetect, rpmMin, timeoutMs } -> { jobId }
#   - profile.importStatus -> params: { jobId } -> { state, progress, message, error, ... }
#   - profile.importCommit -> params: { jobId } -> persist profile
#   - engine.enable        -> no params
#
# Transport:
#   - Raw TCP JSON-RPC (newline-delimited). One JSON per line.
#   - Preferred: /dev/tcp with a single long-lived FD.
#   - Fallback: socat/nc (non-persistent, only if /dev/tcp not available).
# -----------------------------------------------------------------------------

set -euo pipefail

# ----------------------------- Defaults --------------------------------------
HOST="127.0.0.1"
PORT="8777"

FCR_PATH=""                # -f /path/to/FanControl.json
PROFILE_NAME="Imported"    # -n "ProfileName"
VALIDATE_DETECT=0          # -V (bool)
RPM_MIN=0                  # -r 800
TIMEOUT_MS=0               # -T 4000

DO_COMMIT=0                # -C
DO_ENABLE=0                # -E
POLL_MS=100                # -I 500
ID_START=1                 # -i 1

QUIET=0                    # -q
COLOR=1                    # --no-color -> 0
JQ_PRETTY=0                # -J

# ----------------------------- Usage -----------------------------------------
usage() {
  cat <<'USAGE'
importFC.sh - FanControl.Release import & progress (persistent TCP)

Usage:
  importFC.sh -f /path/to/userConfig.json [-n Name] [-V] [-r RPM] [-T ms] [-C] [-E]
               [-H host] [-p port] [-I ms] [-i startId] [-q] [--no-color] [-J]

Options:
  -f FILE     Path to FanControl.Release JSON (required)
  -n NAME     Profile name (default: "Imported")
  -V          Enable deterministic validation step
  -r RPM      Minimum RPM threshold check (default: 0 = disabled)
  -T ms       Timeout for RPM threshold check (default: 0 = disabled)
  -C          Commit imported profile after success
  -E          Enable engine after commit
  -H HOST     Target host (default: 127.0.0.1)
  -p PORT     Target port (default: 8777)
  -I ms       Poll interval (default: 100)
  -i N        Starting JSON-RPC id (default: 1)
  -q          Quiet mode
  --no-color  Disable colored output
  -J          Pretty-print raw JSON replies with jq
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
    -E) DO_ENABLE=1; shift ;;
    -H) HOST="${2:-}"; shift 2 ;;
    -p) PORT="${2:-}"; shift 2 ;;
    -I) POLL_MS="${2:-100}"; shift 2 ;;
    -i) ID_START="${2:-1}"; shift 2 ;;
    -q) QUIET=1; shift ;;
    --no-color) COLOR=0; shift ;;
    -J) JQ_PRETTY=1; shift ;;
    --debug) DEBUG_RPC=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
  esac
done

[[ -n "$FCR_PATH" && -r "$FCR_PATH" ]] || { echo "Error: readable -f FILE required." >&2; exit 2; }

log() { [[ "$QUIET" -eq 0 ]] && echo -e "$*" >&2 || true; }
has_jq() { command -v jq >/dev/null 2>&1; }
ts() { date +"%H:%M:%S"; }

if [[ "$COLOR" -eq 1 ]]; then
  C_B="\033[1m"; C_R="\033[31m"; C_G="\033[32m"; C_Y="\033[33m"; C_C="\033[36m"; C_N="\033[0m"
else
  C_B=""; C_R=""; C_G=""; C_Y=""; C_C=""; C_N=""
fi

# --------------------- Persistent /dev/tcp Session ---------------------------
RPC_FD=3
SESSION_OPEN=0

open_session() {
  if [[ "$SESSION_OPEN" -eq 1 ]]; then return 0; fi
  if exec {RPC_FD}<>"/dev/tcp/${HOST}/${PORT}" 2>/dev/null; then
    SESSION_OPEN=1
    return 0
  fi
  SESSION_OPEN=0
  return 1
}

close_session() {
  if [[ "$SESSION_OPEN" -eq 1 ]]; then
    exec {RPC_FD}>&- {RPC_FD}<&- || true
    SESSION_OPEN=0
  fi
}

rpc_send_recv() {
  local json="$1"
  [[ "$SESSION_OPEN" -eq 1 ]] || return 99
  [[ "$json" == *$'\n' ]] || json="${json}"$'\n'
  [[ "$DEBUG_RPC" -eq 1 ]] && >&2 echo ">>> $json"
  printf '%s' "$json" >&$RPC_FD || return 98

  local line=""
  if ! IFS= read -r -t 10 line <&$RPC_FD; then
    return 97
  fi
  [[ "$DEBUG_RPC" -eq 1 ]] && >&2 echo "<<< $line"
  printf '%s\n' "$line"
  return 0
}

rpc_send_recv_fallback() {
  local json="$1"
  [[ "$json" == *$'\n' ]] || json="${json}"$'\n'
  [[ "$DEBUG_RPC" -eq 1 ]] && >&2 echo "(fb)>>> $json"
  if command -v socat >/dev/null 2>&1; then
    local resp; resp="$(printf '%s' "$json" | socat - TCP:"$HOST":"$PORT")" || return $?
    [[ "$DEBUG_RPC" -eq 1 ]] && >&2 echo "(fb)<<< $resp"
    printf '%s\n' "$resp"
    return 0
  fi
  if command -v nc >/dev/null 2>&1; then
    local help; help="$(nc -h 2>&1 || true)"
    local resp
    if   echo "$help" | grep -qi -- ' -N'; then resp="$(printf '%s' "$json" | nc -N "$HOST" "$PORT")"
    elif echo "$help" | grep -qi -- ' -q '; then resp="$(printf '%s' "$json" | nc -q 1 "$HOST" "$PORT")"
    else resp="$(printf '%s' "$json" | nc "$HOST" "$PORT")"; fi
    [[ "$DEBUG_RPC" -eq 1 ]] && >&2 echo "(fb)<<< $resp"
    printf '%s\n' "$resp"
    return 0
  fi
  echo "Error: neither /dev/tcp nor socat/nc available." >&2
  return 96
}

rpc_id="$ID_START"
rpc_call() {
  local method="$1"; local params="${2:-}"; local id="$rpc_id"
  rpc_id=$((rpc_id+1))

  local req
  if [[ -n "$params" ]]; then
    req=$(printf '{"jsonrpc":"2.0","id":%s,"method":"%s","params":%s}' "$id" "$method" "$params")
  else
    req=$(printf '{"jsonrpc":"2.0","id":%s,"method":"%s"}' "$id" "$method")
  fi

  local resp=""
  if [[ "$SESSION_OPEN" -eq 1 ]]; then
    resp="$(rpc_send_recv "$req" || true)"
    if [[ -z "$resp" ]]; then
      close_session || true
      if open_session; then
        resp="$(rpc_send_recv "$req" || true)"
      fi
    fi   # <- hier war fälschlich 'end'
  fi

  if [[ -z "$resp" ]]; then
    resp="$(rpc_send_recv_fallback "$req" || true)"
  fi

  if [[ -z "$resp" ]]; then
    echo ""
    return 1
  fi

  if [[ "$JQ_PRETTY" -eq 1 && $(has_jq; echo $?) -eq 0 ]]; then
    echo "$resp" | jq . || echo "$resp"
  else
    echo "$resp"
  fi
}

# ------------------------ Robust JSON field extraction -----------------------
json_try_paths() {
  local j="$1"; shift
  if command -v jq >/dev/null 2>&1; then
    for p in "$@"; do
      local out
      out="$(jq -er "$p" <<<"$j" 2>/dev/null || true)"
      [[ -n "$out" ]] && { echo "$out"; return 0; }
    done
    return 1
  else
    local out
    for p in "$@"; do
      case "$p" in
        .result.data.jobId|.result.jobId)
          out="$(sed -n 's/.*"jobId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)";;
        .result.data.state|.result.state)
          out="$(sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)";;
        .result.data.progress|.result.progress)
          out="$(sed -n 's/.*"progress"[[:space:]]*:[[:space:]]*\([0-9]\+\).*/\1/p' <<<"$j" | head -n1)";;
        .result.data.message|.result.message|.result.msg)
          out="$(sed -n 's/.*"message"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)"; [[ -z "$out" ]] && out="$(sed -n 's/.*"msg"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)";;
        .result.data.error|.result.error)
          out="$(sed -n 's/.*"error"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)";;
        .error.message)
          out="$(sed -n 's/.*"message"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"$j" | head -n1)";;
        *) out="";;
      esac
      [[ -n "$out" ]] && { echo "$out"; return 0; }
    done
    return 1
  fi
}

# ----------------------------- Build import params ---------------------------
params_import="$(jq -cn --arg p "$FCR_PATH" \
                       --arg n "$PROFILE_NAME" \
                       --argjson v "$VALIDATE_DETECT" \
                       --argjson r "$RPM_MIN" \
                       --argjson t "$TIMEOUT_MS" \
                       '{path:$p, name:$n, validateDetect:($v==1), rpmMin:$r, timeoutMs:$t}' 2>/dev/null || true)"
if [[ -z "$params_import" ]]; then
  params_import="{\"path\":\"$FCR_PATH\",\"name\":\"$PROFILE_NAME\",\"validateDetect\":$([[ $VALIDATE_DETECT -eq 1 ]] && echo true || echo false),\"rpmMin\":${RPM_MIN},\"timeoutMs\":${TIMEOUT_MS}}"
fi

# -------------------------------- Run ----------------------------------------
log "${C_C}[$(ts)] Connecting ${HOST}:${PORT}${C_N}"
open_session || log "${C_Y}No persistent /dev/tcp; using fallback transport.${C_N}"

log "${C_C}[$(ts)] Starting import${C_N}: path=${FCR_PATH}, name=${PROFILE_NAME}"
resp_start="$(rpc_call "profile.importAs" "$params_import" || true)"
[[ -n "${resp_start:-}" ]] || { echo "Error: no response (profile.importAs)" >&2; close_session || true; exit 20; }

jobId="$(json_try_paths "$resp_start" .result.data.jobId .result.jobId)"
[[ -n "$jobId" ]] || { echo "Error: jobId missing in response" >&2; close_session || true; exit 21; }
log "${C_G}[$(ts)] Job started${C_N}: jobId=${jobId}"

state="running"
progress="0"

while true; do
  resp_stat="$(rpc_call "profile.importStatus" "{\"jobId\":\"$jobId\"}" || true)"
  [[ -n "${resp_stat:-}" ]] || { echo "Error: no response (profile.importStatus)" >&2; close_session || true; exit 22; }

  state="$(json_try_paths "$resp_stat" .result.data.state .result.state)"
  [[ -z "$state" ]] && state="running"
  progress="$(json_try_paths "$resp_stat" .result.data.progress .result.progress)"
  [[ -z "$progress" ]] && progress="0"
  message="$(json_try_paths "$resp_stat" .result.data.message .result.message .result.msg || true)"
  errorMsg="$(json_try_paths "$resp_stat" .result.data.error .result.error .error.message || true)"

  case "$state" in
    running|pending)
      log "[$(ts)] ${C_B}${progress}%${C_N} ${message}"
      ;;
    done|finished|success)
      log "${C_G}[$(ts)] Import finished.${C_N}"
      break
      ;;
    error|failed)
      log "${C_R}[$(ts)] Import error:${C_N} ${errorMsg:-$message}"
      close_session || true
      exit 23
      ;;
    *)
      log "[$(ts)] ${state}: ${progress}% ${message}"
      ;;
  esac

  sleep "$(awk -v ms="$POLL_MS" 'BEGIN{ printf("%.3f", ms/1000.0) }')"
done

if [[ "$DO_COMMIT" -eq 1 ]]; then
  log "${C_C}[$(ts)] Committing profile...${C_N}"
  resp_commit="$(rpc_call "profile.importCommit" "{\"jobId\":\"$jobId\"}" || true)"
  [[ -n "${resp_commit:-}" ]] || { echo "Error: no response (profile.importCommit)" >&2; close_session || true; exit 24; }
  log "${C_G}[$(ts)] Commit done.${C_N}"

  if [[ "$DO_ENABLE" -eq 1 ]]; then
    resp_engine="$(rpc_call "engine.enable" || true)"
    [[ -n "${resp_engine:-}" ]] || { echo "Error: no response (engine.enable)" >&2; close_session || true; exit 25; }
    log "${C_G}[$(ts)] Engine enabled.${C_N}"
  fi
fi

close_session || true
exit 0
