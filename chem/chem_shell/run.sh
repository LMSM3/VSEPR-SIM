#!/usr/bin/env bash
# chem_shell runner -- persistent controller, thin shell front end
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

CTRL_FIFO="/tmp/chem_ctrl_fifo"
RESP_FIFO="/tmp/chem_resp_fifo"

[[ -p "$CTRL_FIFO" ]] || mkfifo "$CTRL_FIFO"
[[ -p "$RESP_FIFO" ]] || mkfifo "$RESP_FIFO"

python3 "$SCRIPT_DIR/controller.py" "$CTRL_FIFO" "$RESP_FIFO" &
CTRL_PID=$!

trap 'kill $CTRL_PID 2>/dev/null; rm -f "$CTRL_FIFO" "$RESP_FIFO"' EXIT

echo ""
echo "  chem_shell v0.1  --  type help for commands, Ctrl-C to quit"
echo ""

while true; do
    read -rp "chem> " cmd
    [[ -z "$cmd" ]] && continue
    echo "$cmd" > "$CTRL_FIFO"
    if read -r response < "$RESP_FIFO"; then
        printf "%s\n" "$response"
    fi
done
