#!/usr/bin/env bash
set -euo pipefail

need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

missing=()

for cmd in cmake git gcc g++ make curl; do
  if ! need_cmd "$cmd"; then
    missing+=("$cmd")
  fi
done

printf 'FlowerAI dependency check\n'
printf '-------------------------\n'

if ((${#missing[@]} == 0)); then
  printf '[v] all core build tools present\n'
else
  printf '[x] missing: %s\n' "${missing[*]}"
  printf '\nSuggested install for RHEL/Alma:\n'
  printf '  sudo dnf install -y cmake git gcc gcc-c++ make curl libcurl-devel\n'
fi
