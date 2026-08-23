#!/usr/bin/env bash
cd /c/R/VSPER-SIM/build
for t in test_print_console test_chemplus_declarative test_chemplus_cli test_bio_object test_bridge_parse; do
  printf '%-32s ' "$t"
  if ./tests/"$t".exe >/tmp/t.log 2>&1; then
	grep -oE '[0-9]+ PASS( / [0-9]+ FAIL)?' /tmp/t.log | tail -1
  else
	echo "FAILED (exit $?)"
	tail -3 /tmp/t.log
  fi
done
