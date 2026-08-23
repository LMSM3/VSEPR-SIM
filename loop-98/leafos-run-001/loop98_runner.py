#!/usr/bin/env python3
"""Durable VSEPR LOOP-98 lifecycle runner with append-only version directories."""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent


def utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def read(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def event(path: Path, run_id: str, version: str, kind: str, data: dict[str, Any]) -> None:
    with path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps({"schema": "vsepr.loop-98-event.v1", "time": utc(), "run_id": run_id, "version": version, "event": kind, "data": data}, separators=(",", ":")) + "\n")


def version_directory(version: str) -> Path:
    path = ROOT / version
    if not path.is_dir():
        raise ValueError(f"version directory not found: {path}")
    return path


def run(version: str) -> int:
    directory = version_directory(version)
    manifest = read(directory / "loop-manifest.json")
    if manifest.get("schema") != "vsepr.loop-98.v1":
        raise ValueError("unsupported LOOP-98 manifest")
    events, artifacts = directory / "events.jsonl", directory / "artifacts"
    event(events, manifest["run_id"], version, "loop.started", {"manifest": str(directory / 'loop-manifest.json')})
    stages = {
        "plan.json": {"stage": "plan", "goal": "Run explicit LOOP-98 lifecycle with durable artifacts.", "created_at": utc()},
        "hypothesis.json": {"stage": "hypothesize", "hypothesis": "The bounded kernel command exits zero and supports a verifiable checkpoint.", "created_at": utc()},
        "code.json": {"stage": "code", "change": "Run #1 tracks the manifest and lifecycle runner; mutation is proposal-only.", "created_at": utc()},
    }
    for filename, artifact in stages.items():
        write(artifacts / filename, artifact)
        event(events, manifest["run_id"], version, "stage.completed", {"stage": artifact["stage"]})
    command = [str(item) for item in manifest["kernel"]["command"]]
    completed = subprocess.run(command, cwd=directory, capture_output=True, text=True, check=False)
    kernel = {"stage": "kernel_run", "command": command, "returncode": completed.returncode, "stdout": completed.stdout, "stderr": completed.stderr, "created_at": utc()}
    write(artifacts / "kernel-run.json", kernel)
    event(events, manifest["run_id"], version, "stage.completed", {"stage": "kernel_run", "returncode": completed.returncode})
    passed = completed.returncode == manifest["kernel"].get("expected_exit_code", 0)
    write(artifacts / "verify.json", {"stage": "verify", "passed": passed, "expected_exit_code": manifest["kernel"].get("expected_exit_code", 0), "actual_exit_code": completed.returncode, "created_at": utc()})
    event(events, manifest["run_id"], version, "stage.completed", {"stage": "verify", "passed": passed})
    report = {"schema": "vsepr.loop-98-report.v1", "run_id": manifest["run_id"], "version": version, "status": "passed" if passed else "failed", "artifacts": {name: str(artifacts / name) for name in ("plan.json", "hypothesis.json", "code.json", "kernel-run.json", "verify.json")}, "created_at": utc()}
    write(directory / "report.json", report)
    event(events, manifest["run_id"], version, "stage.completed", {"stage": "report", "status": report["status"]})
    checkpoint = {"schema": "vsepr.loop-98-checkpoint.v1", "run_id": manifest["run_id"], "version": version, "status": "checkpointed" if passed else "blocked", "resume_command": f"python {Path(__file__).name} run --version {version}", "clone_command": f"python {Path(__file__).name} clone --from-version {version} --mutation 'describe variant'", "report": str(directory / "report.json"), "created_at": utc()}
    write(directory / "checkpoint.json", checkpoint)
    event(events, manifest["run_id"], version, "stage.completed", {"stage": "checkpoint", "status": checkpoint["status"]})
    write(directory / "status.json", {"run_id": manifest["run_id"], "version": version, "status": report["status"], "updated_at": utc()})
    print(json.dumps(report, indent=2))
    return 0 if passed else 1


def clone(source_version: str, mutation: str) -> int:
    source = version_directory(source_version)
    versions = sorted(path.name for path in ROOT.glob("v[0-9][0-9][0-9]") if path.is_dir())
    next_version = f"v{max(int(item[1:]) for item in versions) + 1:03d}"
    target = ROOT / next_version
    shutil.copytree(source, target, ignore=shutil.ignore_patterns("events.jsonl", "artifacts", "report.json", "checkpoint.json", "status.json", "__pycache__"))
    manifest = read(target / "loop-manifest.json")
    manifest.update({"version": next_version, "parent_version": source_version, "status": "variant_planned"})
    write(target / "loop-manifest.json", manifest)
    write(target / "mutation-proposal.json", {"schema": "vsepr.loop-98-mutation-proposal.v1", "parent_version": source_version, "version": next_version, "mutation": mutation, "auto_apply": False, "created_at": utc()})
    event(target / "events.jsonl", manifest["run_id"], next_version, "variant.cloned", {"parent_version": source_version, "mutation": mutation})
    print(json.dumps({"version": next_version, "directory": str(target), "mutation_proposal": str(target / "mutation-proposal.json")}, indent=2))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    run_parser = commands.add_parser("run")
    run_parser.add_argument("--version", default="v001")
    clone_parser = commands.add_parser("clone")
    clone_parser.add_argument("--from-version", default="v001")
    clone_parser.add_argument("--mutation", required=True)
    args = parser.parse_args()
    return run(args.version) if args.action == "run" else clone(args.from_version, args.mutation)


if __name__ == "__main__":
    raise SystemExit(main())
