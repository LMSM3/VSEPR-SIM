"""
pipe.py -- Typed data pipe connecting benchmark -> fitter -> eigen -> report.

A lightweight, synchronous, typed pipeline system for chaining data
processing stages. Each Pipe has a source type and a sink type.
Stages connect via push/pull semantics.

This is the plumbing that makes the test forest's output flow into
polynomial fitting, eigenvalue analysis, and reporting without
manual wiring.

Architecture:
    Source -> Pipe<A> -> Transform<A,B> -> Pipe<B> -> Sink

Anti-black-box: every datum that flows through a pipe is logged
with timestamp, source tag, and sequence number.

VSEPR-SIM 4.0.4
"""

import time
import json
import csv
import os
import logging
from dataclasses import dataclass, field
from typing import (
    TypeVar, Generic, Callable, List, Optional, Dict, Any, Tuple,
)
from datetime import datetime

T = TypeVar("T")
U = TypeVar("U")

_log = logging.getLogger(__name__)


@dataclass
class PipeRecord:
    """Single datum flowing through a pipe."""
    data: Any
    source: str = ""
    sequence: int = 0
    timestamp: float = 0.0

    def __post_init__(self):
        if self.timestamp == 0.0:
            self.timestamp = time.time()


class Pipe(Generic[T]):
    """
    Typed data pipe with fan-out to subscribers.

    Usage:
        pipe = Pipe[float]("energy_pipe")
        pipe.subscribe(lambda rec: print(rec.data))
        pipe.push(42.0, source="benchmark")
    """

    def __init__(self, name: str = "", max_buffer: Optional[int] = None):
        self.name = name
        self._subscribers: List[Callable[[PipeRecord], None]] = []
        self._buffer: List[PipeRecord] = []
        self._sequence = 0
        self._total_pushed = 0
        self._errors = 0
        self._max_buffer = max_buffer

    def subscribe(self, callback: Callable[[PipeRecord], None]):
        """Register a subscriber to receive data."""
        self._subscribers.append(callback)

    def unsubscribe(self, callback: Callable[[PipeRecord], None]) -> bool:
        """Remove a subscriber. Returns True if found."""
        try:
            self._subscribers.remove(callback)
            return True
        except ValueError:
            return False

    def push(self, data: T, source: str = ""):
        """Push data through the pipe to all subscribers."""
        self._sequence += 1
        self._total_pushed += 1
        record = PipeRecord(
            data=data,
            source=source or self.name,
            sequence=self._sequence,
        )
        self._buffer.append(record)
        if self._max_buffer is not None and len(self._buffer) > self._max_buffer:
            self._buffer = self._buffer[-self._max_buffer:]
        for sub in self._subscribers:
            try:
                sub(record)
            except Exception as exc:
                self._errors += 1
                _log.warning("Pipe[%s] subscriber error: %s", self.name, exc)

    def push_batch(self, items: List[T], source: str = ""):
        """Push multiple items."""
        for item in items:
            self.push(item, source)

    @property
    def buffer(self) -> List[PipeRecord]:
        """Access the full buffer of records (read-only for inspection)."""
        return self._buffer

    @property
    def total_pushed(self) -> int:
        return self._total_pushed

    def drain(self) -> List[T]:
        """Drain buffer and return raw data values."""
        data = [r.data for r in self._buffer]
        self._buffer.clear()
        return data

    def last(self) -> Optional[T]:
        """Get the most recent datum."""
        if self._buffer:
            return self._buffer[-1].data
        return None

    def stats(self) -> Dict:
        """Pipe statistics."""
        return {
            "name": self.name,
            "total_pushed": self._total_pushed,
            "buffer_size": len(self._buffer),
            "subscribers": len(self._subscribers),
            "errors": self._errors,
        }

    def __repr__(self) -> str:
        return (
            f"Pipe(name={self.name!r}, pushed={self._total_pushed}, "
            f"buf={len(self._buffer)}, subs={len(self._subscribers)})"
        )


class Transform(Generic[T, U]):
    """
    Transform stage: maps Pipe[T] -> Pipe[U] via a function.

    Usage:
        raw = Pipe[dict]("raw_data")
        energies = Pipe[float]("energies")
        t = Transform(raw, energies, lambda d: d["energy_eV"])
    """

    def __init__(self, source: Pipe[T], sink: Pipe[U],
                 fn: Callable[[T], U], name: str = ""):
        self.name = name or f"xform_{source.name}->{sink.name}"
        self._fn = fn
        self._sink = sink
        self._count = 0
        self._errors = 0
        source.subscribe(self._transform)

    def _transform(self, record: PipeRecord):
        try:
            result = self._fn(record.data)
            self._sink.push(result, source=self.name)
            self._count += 1
        except Exception as exc:
            self._errors += 1
            _log.warning("Transform[%s] error: %s", self.name, exc)

    @property
    def count(self) -> int:
        return self._count

    def stats(self) -> Dict:
        return {
            "name": self.name,
            "transformed": self._count,
            "errors": self._errors,
        }


class FanOut(Generic[T]):
    """
    Fan-out: one source pipe -> multiple sink pipes (broadcast).

    Usage:
        source = Pipe[dict]("results")
        sink_a = Pipe[dict]("to_fitter")
        sink_b = Pipe[dict]("to_eigen")
        fan = FanOut(source, [sink_a, sink_b])
    """

    def __init__(self, source: Pipe[T], sinks: List[Pipe[T]], name: str = ""):
        self.name = name or f"fanout_{source.name}"
        self._source = source
        self._sinks = sinks
        source.subscribe(self._broadcast)

    def _broadcast(self, record: PipeRecord):
        for sink in self._sinks:
            sink.push(record.data, source=self.name)


class Accumulator(Generic[T]):
    """
    Accumulates pipe records into batches, then flushes to a callback.

    Usage:
        pipe = Pipe[float]("timings")
        acc = Accumulator(pipe, batch_size=10, on_flush=process_batch)
    """

    def __init__(self, source: Pipe[T], batch_size: int = 10,
                 on_flush: Optional[Callable[[List[T]], None]] = None):
        self._batch: List[T] = []
        self._batch_size = batch_size
        self._on_flush = on_flush
        self._flush_count = 0
        source.subscribe(self._collect)

    def _collect(self, record: PipeRecord):
        self._batch.append(record.data)
        if len(self._batch) >= self._batch_size:
            self.flush()

    def flush(self):
        if self._batch and self._on_flush:
            self._on_flush(list(self._batch))
            self._flush_count += 1
        self._batch.clear()

    def finalize(self):
        """Flush any remaining partial batch."""
        self.flush()

    @property
    def pending(self) -> int:
        return len(self._batch)

    @property
    def flush_count(self) -> int:
        return self._flush_count


class CSVSink:
    """
    Writes pipe records to a CSV file (append-mode, walk-away safe).

    Usage:
        pipe = Pipe[dict]("benchmark_results")
        sink = CSVSink(pipe, "out/benchmark.csv",
                       columns=["n_atoms", "cpu_ms", "gpu_ms", "speedup"])
    """

    def __init__(self, source: Pipe, path: str, columns: List[str]):
        self._path = path
        self._columns = columns
        self._count = 0

        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)

        if not os.path.exists(path):
            with open(path, "w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(columns)

        source.subscribe(self._write_row)

    def _write_row(self, record: PipeRecord):
        data = record.data
        if isinstance(data, dict):
            row = [data.get(col, "") for col in self._columns]
        elif isinstance(data, (list, tuple)):
            row = list(data)
        else:
            row = [data]

        with open(self._path, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(row)
        self._count += 1

    @property
    def count(self) -> int:
        return self._count


class JSONSink:
    """
    Appends pipe records to a JSONL file (one JSON object per line).
    """

    def __init__(self, source: Pipe, path: str):
        self._path = path
        self._count = 0
        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else ".", exist_ok=True)
        source.subscribe(self._write_record)

    def _write_record(self, record: PipeRecord):
        entry = {
            "seq": record.sequence,
            "source": record.source,
            "ts": record.timestamp,
            "data": record.data,
        }
        with open(self._path, "a") as f:
            f.write(json.dumps(entry, default=str) + "\n")
        self._count += 1

    @property
    def count(self) -> int:
        return self._count
