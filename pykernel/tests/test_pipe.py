"""
Tests for pykernel.pipe — typed data pipe infrastructure.
"""

import os
import json
import tempfile
import pytest

from pykernel.pipe import (
    Pipe, PipeRecord, Transform, FanOut, Accumulator, CSVSink, JSONSink,
)


# ============================================================================
# Pipe basics
# ============================================================================

class TestPipe:
    def test_create_empty(self):
        p = Pipe[float]("test")
        assert p.name == "test"
        assert p.total_pushed == 0
        assert p.buffer == []
        assert p.last() is None

    def test_push_single(self):
        p = Pipe[int]("ints")
        p.push(42, source="src")
        assert p.total_pushed == 1
        assert len(p.buffer) == 1
        assert p.buffer[0].data == 42
        assert p.buffer[0].source == "src"
        assert p.buffer[0].sequence == 1
        assert p.last() == 42

    def test_push_multiple(self):
        p = Pipe[str]("names")
        p.push("alpha")
        p.push("beta")
        p.push("gamma")
        assert p.total_pushed == 3
        assert p.last() == "gamma"
        assert p.buffer[0].sequence == 1
        assert p.buffer[2].sequence == 3

    def test_push_batch(self):
        p = Pipe[int]("batch")
        p.push_batch([10, 20, 30], source="batch_src")
        assert p.total_pushed == 3
        assert [r.data for r in p.buffer] == [10, 20, 30]

    def test_drain(self):
        p = Pipe[float]("drain")
        p.push(1.0)
        p.push(2.0)
        p.push(3.0)
        data = p.drain()
        assert data == [1.0, 2.0, 3.0]
        assert p.buffer == []
        assert p.total_pushed == 3  # total is not reset

    def test_subscriber_called(self):
        received = []
        p = Pipe[int]("sub")
        p.subscribe(lambda rec: received.append(rec.data))
        p.push(100)
        p.push(200)
        assert received == [100, 200]

    def test_multiple_subscribers(self):
        a_vals, b_vals = [], []
        p = Pipe[int]("multi_sub")
        p.subscribe(lambda rec: a_vals.append(rec.data))
        p.subscribe(lambda rec: b_vals.append(rec.data))
        p.push(7)
        assert a_vals == [7]
        assert b_vals == [7]

    def test_stats(self):
        p = Pipe[int]("stats_test")
        p.subscribe(lambda rec: None)
        p.push(1)
        p.push(2)
        s = p.stats()
        assert s["name"] == "stats_test"
        assert s["total_pushed"] == 2
        assert s["buffer_size"] == 2
        assert s["subscribers"] == 1

    def test_default_source(self):
        p = Pipe[int]("default_src")
        p.push(5)
        assert p.buffer[0].source == "default_src"

    def test_timestamp_auto(self):
        p = Pipe[int]("ts")
        p.push(1)
        assert p.buffer[0].timestamp > 0


# ============================================================================
# PipeRecord
# ============================================================================

class TestPipeRecord:
    def test_auto_timestamp(self):
        r = PipeRecord(data=42, source="test", sequence=1)
        assert r.timestamp > 0

    def test_explicit_timestamp(self):
        r = PipeRecord(data=42, source="test", sequence=1, timestamp=1000.0)
        assert r.timestamp == 1000.0


# ============================================================================
# Transform
# ============================================================================

class TestTransform:
    def test_basic_transform(self):
        src = Pipe[int]("src")
        dst = Pipe[float]("dst")
        Transform(src, dst, lambda x: x * 2.5)
        src.push(4)
        src.push(10)
        assert dst.last() == 25.0
        assert dst.total_pushed == 2

    def test_transform_count(self):
        src = Pipe[int]("src2")
        dst = Pipe[str]("dst2")
        xf = Transform(src, dst, lambda x: str(x))
        src.push(1)
        src.push(2)
        src.push(3)
        assert xf.count == 3

    def test_transform_error_handling(self):
        src = Pipe[int]("err_src")
        dst = Pipe[float]("err_dst")
        xf = Transform(src, dst, lambda x: 1.0 / x)
        src.push(2)
        src.push(0)  # ZeroDivisionError
        src.push(5)
        assert xf.count == 2
        assert xf.stats()["errors"] == 1

    def test_transform_chain(self):
        a = Pipe[int]("a")
        b = Pipe[int]("b")
        c = Pipe[int]("c")
        Transform(a, b, lambda x: x + 1)
        Transform(b, c, lambda x: x * 10)
        a.push(5)
        assert b.last() == 6
        assert c.last() == 60


# ============================================================================
# FanOut
# ============================================================================

class TestFanOut:
    def test_fanout_broadcast(self):
        src = Pipe[int]("fanout_src")
        s1 = Pipe[int]("s1")
        s2 = Pipe[int]("s2")
        s3 = Pipe[int]("s3")
        FanOut(src, [s1, s2, s3])
        src.push(42)
        assert s1.last() == 42
        assert s2.last() == 42
        assert s3.last() == 42

    def test_fanout_empty_sinks(self):
        src = Pipe[int]("fanout_empty")
        FanOut(src, [])
        src.push(1)  # should not raise


# ============================================================================
# Accumulator
# ============================================================================

class TestAccumulator:
    def test_accumulator_batch(self):
        batches = []
        src = Pipe[int]("acc_src")
        Accumulator(src, batch_size=3, on_flush=lambda b: batches.append(list(b)))
        src.push(1)
        src.push(2)
        assert len(batches) == 0
        src.push(3)
        assert len(batches) == 1
        assert batches[0] == [1, 2, 3]

    def test_accumulator_multiple_batches(self):
        batches = []
        src = Pipe[int]("acc_multi")
        Accumulator(src, batch_size=2, on_flush=lambda b: batches.append(list(b)))
        src.push_batch([10, 20, 30, 40, 50])
        assert len(batches) == 2
        assert batches[0] == [10, 20]
        assert batches[1] == [30, 40]

    def test_accumulator_pending(self):
        src = Pipe[int]("acc_pending")
        acc = Accumulator(src, batch_size=5)
        src.push(1)
        src.push(2)
        assert acc.pending == 2

    def test_accumulator_manual_flush(self):
        flushed = []
        src = Pipe[int]("acc_flush")
        acc = Accumulator(src, batch_size=100, on_flush=lambda b: flushed.extend(b))
        src.push(1)
        src.push(2)
        acc.flush()
        assert flushed == [1, 2]
        assert acc.pending == 0


# ============================================================================
# CSVSink
# ============================================================================

class TestCSVSink:
    def test_csv_write(self, tmp_path):
        path = str(tmp_path / "test.csv")
        src = Pipe[dict]("csv_src")
        CSVSink(src, path, ["name", "value"])
        src.push({"name": "alpha", "value": 1.0})
        src.push({"name": "beta", "value": 2.0})

        with open(path) as f:
            lines = f.readlines()
        assert len(lines) == 3  # header + 2 rows
        assert "name,value" in lines[0]
        assert "alpha" in lines[1]
        assert "beta" in lines[2]

    def test_csv_append(self, tmp_path):
        path = str(tmp_path / "append.csv")
        src1 = Pipe[dict]("csv1")
        CSVSink(src1, path, ["x"])
        src1.push({"x": 10})

        src2 = Pipe[dict]("csv2")
        CSVSink(src2, path, ["x"])
        src2.push({"x": 20})

        with open(path) as f:
            lines = f.readlines()
        # Header written only once (first sink), second appends
        assert len(lines) == 3

    def test_csv_list_data(self, tmp_path):
        path = str(tmp_path / "list.csv")
        src = Pipe[list]("csv_list")
        CSVSink(src, path, ["a", "b"])
        src.push([1, 2])
        src.push([3, 4])

        with open(path) as f:
            lines = f.readlines()
        assert len(lines) == 3
        assert "1,2" in lines[1]

    def test_csv_count(self, tmp_path):
        path = str(tmp_path / "count.csv")
        src = Pipe[dict]("csv_cnt")
        sink = CSVSink(src, path, ["v"])
        src.push({"v": 1})
        src.push({"v": 2})
        assert sink.count == 2


# ============================================================================
# JSONSink
# ============================================================================

class TestJSONSink:
    def test_json_write(self, tmp_path):
        path = str(tmp_path / "test.jsonl")
        src = Pipe[dict]("json_src")
        JSONSink(src, path)
        src.push({"key": "val1"})
        src.push({"key": "val2"})

        with open(path) as f:
            lines = f.readlines()
        assert len(lines) == 2
        obj = json.loads(lines[0])
        assert obj["data"]["key"] == "val1"
        assert "seq" in obj
        assert "ts" in obj

    def test_json_count(self, tmp_path):
        path = str(tmp_path / "count.jsonl")
        src = Pipe[int]("json_cnt")
        sink = JSONSink(src, path)
        src.push(1)
        src.push(2)
        src.push(3)
        assert sink.count == 3


# ============================================================================
# Integration: pipe chains
# ============================================================================

class TestPipeIntegration:
    def test_full_pipeline(self, tmp_path):
        """Full pipeline: source → transform → fanout → sinks."""
        csv_path = str(tmp_path / "pipeline.csv")
        json_path = str(tmp_path / "pipeline.jsonl")

        raw = Pipe[dict]("raw")
        values = Pipe[float]("values")
        doubled = Pipe[float]("doubled")

        Transform(raw, values, lambda d: d["v"])
        Transform(values, doubled, lambda x: x * 2)

        CSVSink(doubled, csv_path, ["value"])
        JSONSink(doubled, json_path)

        raw.push({"v": 5.0})
        raw.push({"v": 10.0})
        raw.push({"v": 15.0})

        assert doubled.last() == 30.0

        with open(csv_path) as f:
            lines = f.readlines()
        assert len(lines) == 4  # header + 3

    def test_fanout_to_transforms(self):
        """FanOut → multiple transforms → separate sinks."""
        src = Pipe[dict]("src")
        cpu = Pipe[float]("cpu")
        gpu = Pipe[float]("gpu")

        Transform(src, cpu, lambda d: d.get("cpu", 0))
        Transform(src, gpu, lambda d: d.get("gpu", 0))

        src.push({"cpu": 10.0, "gpu": 5.0})
        src.push({"cpu": 20.0, "gpu": 8.0})

        assert cpu.drain() == [10.0, 20.0]
        assert gpu.drain() == [5.0, 8.0]
