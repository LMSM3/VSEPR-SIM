#!/usr/bin/env bash
set -euo pipefail

FLOWER_ROOT="${1:-$HOME/FlowerOS}"
TARGETS=(
  "$FLOWER_ROOT/.flowerrc"
  "$FLOWER_ROOT/.flower_aliases"
  "$FLOWER_ROOT/.flower_env"
)

fix_file() {
  local file="$1"
  [[ -f "$file" ]] || return 0

  if grep -q $'\r' "$file"; then
    printf '[*] fixing CRLF -> LF: %s\n' "$file"
    sed -i 's/\r$//' "$file"
  else
    printf '[v] already clean: %s\n' "$file"
  fi
}

main() {
  printf '[*] FlowerOS newline repair starting in: %s\n' "$FLOWER_ROOT"

  for f in "${TARGETS[@]}"; do
    fix_file "$f"
  done

  printf '[v] done\n'
}

main "$@"
