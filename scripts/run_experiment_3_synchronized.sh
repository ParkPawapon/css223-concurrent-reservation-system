#!/usr/bin/env bash
# ==============================================================================
# CSS223 Cinema Seat Reservation System
# Experiment 3: Concurrent with Synchronization (Mutual Exclusion)
#
# Server configuration expected:
#   ./build/debug/src/reservation_server --workers 3 --delay
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
LOG_DIR="${ROOT_DIR}/docs/experiments/experiment-03-with-synchronization/logs"
mkdir -p "${LOG_DIR}"

echo "======================================================================"
echo "CSS223 Experiment 3: Concurrent Synchronized"
echo "Configuration: 3+ Workers, Mutex ENABLED, Random Delay ENABLED (--delay)"
echo "Expected Behavior: Mutual exclusion ensures atomic CHECK -> DELAY -> UPDATE."
echo "                   Only ONE client successfully reserves seat A1."
echo "                   All other competing clients receive 'Reservation failed'."
echo "======================================================================"

echo "[Info] Launching 5 competing clients simultaneously targeting seat A1..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 | tee "${LOG_DIR}/client_run_$(date +%Y%m%d_%H%M%S).log"

echo "[Done] Experiment 3 run completed. Logs recorded in ${LOG_DIR}."
