#!/usr/bin/env bash
# ==============================================================================
# CSS223 Cinema Seat Reservation System
# Experiment 2: Concurrent without Synchronization (Race Condition)
#
# Server configuration expected:
#   ./build/debug/src/reservation_server --workers 3 --no-sync --delay
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
LOG_DIR="${ROOT_DIR}/docs/experiments/experiment-02-without-synchronization/logs"
mkdir -p "${LOG_DIR}"

echo "======================================================================"
echo "CSS223 Experiment 2: Concurrent Unsynchronized"
echo "Configuration: 3+ Workers, Mutex DISABLED (--no-sync), Random Delay ENABLED (--delay)"
echo "Expected Behavior: Multiple worker threads access the Reservation Table concurrently."
echo "                   Race Condition occurs between CHECK and UPDATE."
echo "                   Double booking (multiple owners) can be observed."
echo "======================================================================"

echo "[Info] Launching 5 competing clients simultaneously targeting seat A1..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 | tee "${LOG_DIR}/client_run_$(date +%Y%m%d_%H%M%S).log"

echo "[Done] Experiment 2 run completed. Logs recorded in ${LOG_DIR}."
