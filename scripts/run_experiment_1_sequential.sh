#!/usr/bin/env bash
# ==============================================================================
# CSS223 Cinema Seat Reservation System
# Experiment 1: Sequential Baseline (1 Worker)
#
# Server configuration expected:
#   ./build/debug/src/reservation_server --workers 1
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/.."
LOG_DIR="${ROOT_DIR}/docs/experiments/experiment-01-sequential/logs"
mkdir -p "${LOG_DIR}"

echo "======================================================================"
echo "CSS223 Experiment 1: Sequential Baseline"
echo "Configuration: 1 Worker Thread"
echo "Expected Behavior: Requests are processed one by one in order."
echo "                   No race conditions or double bookings occur."
echo "======================================================================"

echo "[Info] Launching 5 concurrent clients targeting different seats..."
bash "${SCRIPT_DIR}/run_concurrent_clients.sh" A1 5 | tee "${LOG_DIR}/client_run_$(date +%Y%m%d_%H%M%S).log"

echo "[Done] Experiment 1 run completed. Logs recorded in ${LOG_DIR}."
