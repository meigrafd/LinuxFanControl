#!/usr/bin/env bash
#set -euo pipefail

HOST=127.0.0.1
PORT=8777
VALIDATE=false
RPM_MIN=0
TIMEOUT=10000

ARG_PATH=""
ARG_NAME=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --host) HOST="$2"; shift 2;;
        --port) PORT="$2"; shift 2;;
        --validate) VALIDATE=true; shift;;
        --rpmMin) RPM_MIN="$2"; shift 2;;
        --timeout) TIMEOUT="$2"; shift 2;;
        -h|--help)
            echo "Usage: $0 <path> <name> [--validate --rpmMin N --timeout MS] [--host H --port P]"
            echo "Eg.: $0 -p /tmp/userConfig.json -n Default"
            exit 0;;
        *) if [[ -z "$ARG_PATH" ]]; then ARG_PATH="$1"; else ARG_NAME="$1"; fi; shift;;
    esac
done

if [[ -z "$ARG_PATH" || -z "$ARG_NAME" ]]; then
    echo "need <path> and <name>"; exit 1
fi

rpc() {
    local method="$1"; shift
    local params="$1"; shift || true
    curl -s -X POST "http://${HOST}:${PORT}/rpc" -H 'Content-Type: application/json' \
        -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"${method}\",\"params\":${params:-{}}}"
}

REQ="{\"path\":\"${ARG_PATH}\",\"name\":\"${ARG_NAME}\",\"validateDetect\":${VALIDATE},\"rpmMin\":${RPM_MIN},\"timeoutMs\":${TIMEOUT}}"
JOB_ID=$(rpc "profile.importAs" "$REQ" | jq -r '.result.data.jobId')
[[ -z "$JOB_ID" || "$JOB_ID" == "null" ]] && { echo "importAs failed"; exit 2; }

while true; do
    sleep 0.5
    ST=$(rpc "profile.importStatus" "{\"jobId\":\"${JOB_ID}\"}")
    DONE=$(echo "$ST" | jq -r '.result.data.done')
    OK=$(echo "$ST" | jq -r '.result.data.ok')
    PROG=$(echo "$ST" | jq -r '.result.data.progress')
    STAGE=$(echo "$ST" | jq -r '.result.data.stage')
    CUR=$(echo "$ST" | jq -r '.result.data.currentIdentifier // ""')
    MAP=$(echo "$ST" | jq -r '.result.data.mappedPath // ""')
    echo "[${PROG}] ${STAGE}  ${CUR} -> ${MAP}"
    [[ "$DONE" == "true" ]] && break
done

[[ "$OK" != "true" ]] && { echo "$ST" | jq; exit 3; }

rpc "profile.importCommit" "{\"jobId\":\"${JOB_ID}\"}" | jq
