"""Quick integration test for ZMQ PUB/SUB molecular data pipeline."""
import zmq
import time
import threading
import json
import sys

sys.path.insert(0, ".")
from pykernel.zmq_producer import MolecularPublisher

PORT = 5556
ENDPOINT = f"tcp://127.0.0.1:{PORT}"

def test_pubsub_roundtrip():
    """Verify publisher sends and subscriber receives a frame."""
    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.connect(ENDPOINT)
    sub.subscribe(b"FRAME")
    sub.setsockopt(zmq.RCVTIMEO, 5000)

    pub = MolecularPublisher(endpoint=ENDPOINT, use_msgpack=False)

    # Publish in background
    def publish_frames():
        time.sleep(0.5)  # let subscriber connect
        for i in range(5):
            pub.publish_frame(
                positions=[[0, 0, 0], [1.1, 0, 0]],
                symbols=["C", "H"],
                bonds=[[0, 1]],
                title=f"test frame {i}",
            )
            time.sleep(0.1)

    t = threading.Thread(target=publish_frames, daemon=True)
    t.start()

    # Receive
    msg = sub.recv()
    payload = msg[5:]  # strip "FRAME" prefix
    frame = json.loads(payload.decode("utf-8"))

    assert frame["frame_id"] >= 1, f"Bad frame_id: {frame['frame_id']}"
    assert len(frame["positions"]) == 2, f"Expected 2 atoms, got {len(frame['positions'])}"
    assert frame["symbols"] == ["C", "H"], f"Bad symbols: {frame['symbols']}"
    assert frame["bonds"] == [[0, 1]], f"Bad bonds: {frame['bonds']}"
    assert len(frame["radii"]) == 2, f"Bad radii: {frame['radii']}"
    assert len(frame["colors"]) == 2, f"Bad colors count"

    print(f"  frame_id:  {frame['frame_id']}")
    print(f"  atoms:     {len(frame['positions'])}")
    print(f"  symbols:   {frame['symbols']}")
    print(f"  bonds:     {frame['bonds']}")
    print(f"  radii:     {frame['radii']}")
    print(f"  title:     {frame['title']}")
    print()

    sub.close()
    pub.close()
    ctx.term()
    print("  ZMQ PUB/SUB roundtrip: PASS")


def test_xyz_parsing():
    """Verify XYZ file parser."""
    from pykernel.zmq_producer import parse_xyz_file
    import tempfile, os

    xyz_content = """3
Water molecule
O   0.000   0.000   0.000
H   0.757   0.586   0.000
H  -0.757   0.586   0.000
"""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".xyz",
                                     delete=False) as f:
        f.write(xyz_content)
        tmp_path = f.name

    try:
        frame = parse_xyz_file(tmp_path)
        assert frame is not None, "Failed to parse XYZ"
        assert len(frame["positions"]) == 3, f"Expected 3 atoms, got {len(frame['positions'])}"
        assert frame["symbols"] == ["O", "H", "H"], f"Bad symbols: {frame['symbols']}"
        assert len(frame["bonds"]) == 2, f"Expected 2 bonds (O-H, O-H), got {len(frame['bonds'])}"
        assert frame["title"] == "Water molecule"

        print(f"  atoms:   {frame['symbols']}")
        print(f"  bonds:   {frame['bonds']}")
        print(f"  labels:  {frame['labels']}")
        print(f"  title:   {frame['title']}")
        print()
        print("  XYZ parsing: PASS")
    finally:
        os.unlink(tmp_path)


def test_bond_inference():
    """Verify covalent bond inference."""
    import numpy as np
    from pykernel.zmq_producer import infer_bonds

    # Methane: C at origin, 4 H at tetrahedral positions
    pos = np.array([
        [0.000, 0.000, 0.000],  # C
        [0.629, 0.629, 0.629],  # H
        [-0.629, -0.629, 0.629],  # H
        [-0.629, 0.629, -0.629],  # H
        [0.629, -0.629, -0.629],  # H
    ])
    symbols = ["C", "H", "H", "H", "H"]
    bonds = infer_bonds(pos, symbols)

    assert len(bonds) == 4, f"Expected 4 C-H bonds, got {len(bonds)}: {bonds}"
    for b in bonds:
        assert 0 in b, f"All bonds should include C(0): {b}"

    print(f"  CH4 bonds: {bonds}")
    print("  Bond inference: PASS")


if __name__ == "__main__":
    print("=" * 60)
    print("VSEPR-SIM Cartoon Renderer Integration Tests")
    print("=" * 60)

    print("\n[1] Bond inference test")
    test_bond_inference()

    print("\n[2] XYZ parsing test")
    test_xyz_parsing()

    print("\n[3] ZMQ PUB/SUB roundtrip test")
    test_pubsub_roundtrip()

    print("\n" + "=" * 60)
    print("ALL TESTS PASSED")
    print("=" * 60)
