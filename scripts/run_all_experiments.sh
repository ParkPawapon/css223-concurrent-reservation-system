#!/usr/bin/env bash
# ==============================================================================
# CSS223 Cinema Seat Reservation System
# Automated Runner for All 3 Concurrency Experiments
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
SERVER_BIN="${ROOT_DIR}/build/debug/src/reservation_server"
CLIENT_BIN="${ROOT_DIR}/build/debug/src/reservation_client"

if [[ ! -x "${SERVER_BIN}" || ! -x "${CLIENT_BIN}" ]]; then
    echo "[ERROR] Binaries not found. Please build the project first."
    exit 1
fi

cleanup_queues() {
    rm -f /dev/mqueue/css223_* 2>/dev/null || true
}

stop_server() {
    local pid="$1"
    if kill -0 "${pid}" 2>/dev/null; then
        echo "  [Runner] Stopping server (PID ${pid}) with SIGINT..."
        kill -INT "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
    fi
}

wait_for_queue() {
    local queue_file="/dev/mqueue/css223_reservation_requests"
    local retries=30
    while [[ ! -e "${queue_file}" && ${retries} -gt 0 ]]; do
        sleep 0.1
        ((retries--)) || true
    done
    if [[ ! -e "${queue_file}" ]]; then
        echo "[ERROR] Server queue failed to initialize."
        return 1
    fi
}

echo "======================================================================"
echo "CSS223 Automated Concurrency Experiments Runner"
echo "======================================================================"

# ==============================================================================
# EXPERIMENT 1: Sequential Baseline (1 Worker)
# ==============================================================================
echo ""
echo ">>> [1/3] Running Experiment 1: Sequential Baseline (1 Worker)"
EXP1_DIR="${ROOT_DIR}/docs/experiments/experiment-01-sequential"
EXP1_LOG="${EXP1_DIR}/logs"
mkdir -p "${EXP1_LOG}"

cleanup_queues

"${SERVER_BIN}" --workers 1 > "${EXP1_LOG}/server_exp1.log" 2>&1 &
SERVER_PID=$!
wait_for_queue

echo "  [Runner] Server started (PID ${SERVER_PID}) with 1 Worker."
echo "  [Runner] Launching 5 concurrent clients targeting Seat A1..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 "${CLIENT_BIN}" | tee "${EXP1_LOG}/clients_exp1.log"

# Query seat map
printf "LIST\nQUIT\n" | "${CLIENT_BIN}" 99 > "${EXP1_LOG}/seat_map_final_exp1.log" 2>&1 || true

stop_server "${SERVER_PID}"
cleanup_queues
echo "  [Runner] Experiment 1 completed."

# ==============================================================================
# EXPERIMENT 2: Concurrent without Synchronization (Race Condition)
# ==============================================================================
echo ""
echo ">>> [2/3] Running Experiment 2: Unsynchronized (Race Condition / Double Booking)"
EXP2_DIR="${ROOT_DIR}/docs/experiments/experiment-02-without-synchronization"
EXP2_LOG="${EXP2_DIR}/logs"
mkdir -p "${EXP2_LOG}"

cleanup_queues

"${SERVER_BIN}" --workers 5 --no-sync --delay > "${EXP2_LOG}/server_exp2.log" 2>&1 &
SERVER_PID=$!
wait_for_queue

echo "  [Runner] Server started (PID ${SERVER_PID}) with 5 Workers, Mutex DISABLED, Delay ENABLED."
echo "  [Runner] Launching 5 concurrent clients targeting Seat A1 simultaneously..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 "${CLIENT_BIN}" | tee "${EXP2_LOG}/clients_exp2.log"

# Query seat map
printf "LIST\nQUIT\n" | "${CLIENT_BIN}" 99 > "${EXP2_LOG}/seat_map_final_exp2.log" 2>&1 || true

stop_server "${SERVER_PID}"
cleanup_queues
echo "  [Runner] Experiment 2 completed."

# ==============================================================================
# EXPERIMENT 3: Concurrent with Synchronization (Mutual Exclusion)
# ==============================================================================
echo ""
echo ">>> [3/3] Running Experiment 3: Synchronized (Mutual Exclusion via Mutex)"
EXP3_DIR="${ROOT_DIR}/docs/experiments/experiment-03-with-synchronization"
EXP3_LOG="${EXP3_DIR}/logs"
mkdir -p "${EXP3_LOG}"

cleanup_queues

"${SERVER_BIN}" --workers 5 --delay > "${EXP3_LOG}/server_exp3.log" 2>&1 &
SERVER_PID=$!
wait_for_queue

echo "  [Runner] Server started (PID ${SERVER_PID}) with 5 Workers, Mutex ENABLED, Delay ENABLED."
echo "  [Runner] Launching 5 concurrent clients targeting Seat A1 simultaneously..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 "${CLIENT_BIN}" | tee "${EXP3_LOG}/clients_exp3.log"

# Query seat map
printf "LIST\nQUIT\n" | "${CLIENT_BIN}" 99 > "${EXP3_LOG}/seat_map_final_exp3.log" 2>&1 || true

stop_server "${SERVER_PID}"
cleanup_queues
echo "  [Runner] Experiment 3 completed."

echo ""
echo "======================================================================"
echo "[SUCCESS] All 3 experiments have completed successfully!"
echo "Log files are stored in:"
echo "  - ${EXP1_LOG}"
echo "  - ${EXP2_LOG}"
echo "  - ${EXP3_LOG}"
echo "======================================================================"
