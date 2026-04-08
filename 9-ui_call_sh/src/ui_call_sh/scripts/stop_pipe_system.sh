#!/usr/bin/env bash
set -euo pipefail

SESSION=pipe_system

if tmux has-session -t "$SESSION" 2>/dev/null; then
  tmux kill-session -t "$SESSION"
  echo "tmux session '$SESSION' stopped."
else
  echo "tmux session '$SESSION' not running."
fi
