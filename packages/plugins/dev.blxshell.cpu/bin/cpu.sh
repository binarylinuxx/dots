#!/bin/sh
# The backend of an example blxshell plugin, in plain POSIX sh: it measures
# CPU usage and sends it as {"type": "state", ...} for plugin.facet to draw.
# blxshell plugin protocol, api 2: JSON lines on stdin and stdout; exit when
# stdin closes.

sample() { # prints "busy total" from /proc/stat
  read -r _ user nice system idle iowait irq softirq steal _ < /proc/stat
  echo "$((user + nice + system + irq + softirq + steal)) $((user + nice + system + idle + iowait + irq + softirq + steal))"
}

ticker() {
  set -- $(sample)
  last_busy=$1 last_total=$2
  while sleep 2; do
    set -- $(sample)
    pct=0
    [ "$2" -gt "$last_total" ] && pct=$(( ($1 - last_busy) * 100 / ($2 - last_total) ))
    last_busy=$1 last_total=$2
    read -r load _ < /proc/loadavg
    printf '{"type":"state","state":{"percent":%s,"load":"%s"}}\n' "$pct" "$load"
  done
}

ticker &
trap 'kill $! 2>/dev/null' EXIT
# Nothing to do with what the shell sends; read until it closes stdin.
while IFS= read -r _; do :; done
