#!/bin/sh
# blxshell plugin protocol, api 1: JSON lines on stdin (from the shell) and
# stdout (to it). Diagnostics go to stderr. Exit when stdin closes.

interval=2
warn=85
mode=usage

setting() { # setting <key> <json line>: a top-level number from "settings"
  printf '%s' "$2" | sed -n "s/.*\"$1\":\([0-9.]*\).*/\1/p" | head -n 1
}

sample() { # prints "busy total" from /proc/stat
  read -r _ user nice system idle iowait irq softirq steal _ < /proc/stat
  echo "$((user + nice + system + irq + softirq + steal)) $((user + nice + system + idle + iowait + irq + softirq + steal))"
}

report() {
  set -- $(sample)
  busy=$1 total=$2
  if [ -n "$last_total" ] && [ "$total" -gt "$last_total" ]; then
    pct=$(( (busy - last_busy) * 100 / (total - last_total) ))
  else
    pct=0
  fi
  last_busy=$busy last_total=$total
  tone=""
  [ "$pct" -ge "${warn%.*}" ] && tone=error
  if [ "$mode" = load ]; then
    read -r one _ < /proc/loadavg
    printf '{"type":"module","id":"usage","icon":"speed","text":"%s","tone":"%s","progress":null}\n' "$one" "$tone"
  else
    printf '{"type":"module","id":"usage","icon":"memory","text":"%s%%","tone":"%s","progress":%s}\n' \
      "$pct" "$tone" "$(awk "BEGIN{print $pct/100}")"
  fi
}

ticker() {
  while :; do
    report
    sleep "$interval"
  done
}

start_ticker() {
  [ -n "$ticker_pid" ] && kill "$ticker_pid" 2>/dev/null
  ticker &
  ticker_pid=$!
}

trap '[ -n "$ticker_pid" ] && kill "$ticker_pid" 2>/dev/null' EXIT

has() { case $line in *"$1"*) return 0 ;; esac; return 1; }

# Keys may come in any order: match each one on its own.
while IFS= read -r line; do
  if has '"type":"hello"' || has '"type":"settings"'; then
    value=$(setting interval "$line"); [ -n "$value" ] && interval=$value
    value=$(setting warn "$line"); [ -n "$value" ] && warn=$value
    start_ticker
  elif has '"type":"event"' && has '"event":"click"'; then
    if has '"button":"right"'; then
      printf '{"type":"shell","command":["sidebar","both"]}\n'
    else
      if [ "$mode" = usage ]; then mode=load; else mode=usage; fi
      start_ticker
    fi
  elif has '"type":"command"' && has '"name":"refresh"'; then
    start_ticker
  fi
done
