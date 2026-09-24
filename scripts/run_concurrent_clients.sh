#!/usr/bin/env bash
# ==============================================================================
# CSS223 Cinema Seat Reservation System
# Concurrent Multi-Client Simulator
#
# Spawns N client processes in background aiming at the same target seat,
# waits for all of them to terminate, and summarizes the outcomes.
# ==============================================================================

set -euo pipefail

TARGET_SEAT="${1:-A1}"
NUM_CLIENTS="${2:-5}"
BINARY="${3:-./build/debug/src/reservation_client}"

if [[ ! -x "${BINARY}" ]]; then
    # Check fallback path
    if [[ -x "./build/release/src/reservation_client" ]]; then
        BINARY="./build/release/src/reservation_client"
    else
        echo "[ERROR] Client executable not found at: ${BINARY}"
        echo "Please build the project first: cmake --workflow --preset workflow-debug"
        exit 1
    fi
fi

LOG_DIR="/tmp/css223_clients_$$"
mkdir -p "${LOG_DIR}"

cleanup() {
    rm -rf "${LOG_DIR}"
}
trap cleanup EXIT

echo "======================================================================"
echo "CSS223 Concurrent Clients Simulator"
echo "Target Seat : ${TARGET_SEAT}"
echo "Client Count: ${NUM_CLIENTS} (IDs: 1 to ${NUM_CLIENTS})"
echo "Executable  : ${BINARY}"
echo "======================================================================"

PIDS=()

for i in $(seq 1 "${NUM_CLIENTS}"); do
    CLIENT_ID="${i}"
    LOG_FILE="${LOG_DIR}/client_${CLIENT_ID}.log"

    # Feed RESERVE <seat> and QUIT into client's stdin
    (
        printf "RESERVE %s\nQUIT\n" "${TARGET_SEAT}" | "${BINARY}" "${CLIENT_ID}" > "${LOG_FILE}" 2>&1
    ) &
    PID=$!
    PIDS+=("${PID}")
    echo "[Launcher] Started Client ${CLIENT_ID} (PID ${PID}) in background"
done

echo "----------------------------------------------------------------------"
echo "[Launcher] Waiting for all ${NUM_CLIENTS} clients to complete..."
for pid in "${PIDS[@]}"; do
    wait "${pid}" 2>/dev/null || true
done
echo "[Launcher] All client processes finished."
echo "======================================================================"
echo "Results Summary for Seat: ${TARGET_SEAT}"
echo "======================================================================"

SUCCESS_COUNT=0
FAILED_COUNT=0

for i in $(seq 1 "${NUM_CLIENTS}"); do
    CLIENT_ID="${i}"
    LOG_FILE="${LOG_DIR}/client_${CLIENT_ID}.log"

    if grep -q "SUCCESS" "${LOG_FILE}"; then
        STATUS_MSG="[SUCCESS] Reservation GRANTED"
        ((SUCCESS_COUNT++)) || true
    elif grep -q "FAILED" "${LOG_FILE}"; then
        STATUS_MSG="[FAILED]  Reservation REJECTED"
        ((FAILED_COUNT++)) || true
    else
        STATUS_MSG="[UNKNOWN] Could not determine outcome (check server queue)"
    fi

    echo "  Client ${CLIENT_ID}: ${STATUS_MSG}"
done

echo "----------------------------------------------------------------------"
echo "Total Clients: ${NUM_CLIENTS} | Successes: ${SUCCESS_COUNT} | Failures: ${FAILED_COUNT}"

if [[ ${SUCCESS_COUNT} -gt 1 ]]; then
    echo ">> [OBSERVATION] MULTIPLE RESERVATIONS SUCCEEDED! (Race condition / Double booking detected)"
elif [[ ${SUCCESS_COUNT} -eq 1 ]]; then
    echo ">> [OBSERVATION] Exactly ONE reservation succeeded. (Mutual exclusion preserved)"
else
    echo ">> [OBSERVATION] No client succeeded (check server status)."
fi
echo "======================================================================"
