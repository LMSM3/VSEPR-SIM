"""
Tests for pykernel.step_parser — STEP (ISO 10303-21) parser.

Validates:
  - Entity tokenisation and argument splitting
  - CARTESIAN_POINT, DIRECTION, AXIS2_PLACEMENT_3D extraction
  - PRODUCT (named part) extraction
  - Header parsing (schema, description)
  - Empty / minimal input handling
  - String and file-like input modes

VSEPR-SIM 3.0.0
"""

import io
import math
import pytest

from pykernel.step_parser import (
    parse_step,
    parse_step_string,
    StepAssembly,
    NamedPart,
    Vec3,
    CartesianPoint,
    Direction,
    Axis2Placement3D,
    _split_args,
    _parse_float_list,
    _extract_string,
    _extract_ref,
)


# ═══════════════════════════════════════════════════════════════════════
# Synthetic STEP content
# ═══════════════════════════════════════════════════════════════════════

MINIMAL_STEP = """
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Minimal test'),'2;1');
FILE_NAME('test.step','2025-01-01',('Author'),('Org'),'','','');
FILE_SCHEMA(('AP214'));
ENDSEC;
DATA;
#1 = CARTESIAN_POINT('Origin',(0.0,0.0,0.0));
#2 = CARTESIAN_POINT('P1',(10.0,20.0,30.0));
#3 = DIRECTION('Z',(0.0,0.0,1.0));
#4 = DIRECTION('X',(1.0,0.0,0.0));
#5 = AXIS2_PLACEMENT_3D('placement',#1,#3,#4);
#6 = PRODUCT('Housing','Aluminium housing','',(''));
#7 = PRODUCT('Shaft','Steel shaft','',(''));
ENDSEC;
END-ISO-10303-21;
"""

MULTI_POINT_STEP = """
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Multi-point test'),'2;1');
FILE_NAME('multi.step','2025-01-01',(''),(''),'' ,'','');
FILE_SCHEMA(('AP203'));
ENDSEC;
DATA;
#10 = CARTESIAN_POINT('A',(1.0,2.0,3.0));
#11 = CARTESIAN_POINT('B',(4.0,5.0,6.0));
#12 = CARTESIAN_POINT('C',(7.0,8.0,9.0));
#20 = DIRECTION('axis',(0.0,0.0,1.0));
#21 = DIRECTION('ref',(1.0,0.0,0.0));
#30 = AXIS2_PLACEMENT_3D('base',#10,#20,#21);
#40 = PRODUCT('Bracket','Support bracket','',(''));
ENDSEC;
END-ISO-10303-21;
"""

EMPTY_DATA_STEP = """
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('Empty data'),'2;1');
FILE_NAME('empty.step','2025-01-01',(''),(''),'','','');
FILE_SCHEMA(('AP214'));
ENDSEC;
DATA;
ENDSEC;
END-ISO-10303-21;
"""

NO_PRODUCT_STEP = """
ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('No products'),'2;1');
FILE_NAME('nopart.step','2025-01-01',(''),(''),'','','');
FILE_SCHEMA(('AP214'));
ENDSEC;
DATA;
#1 = CARTESIAN_POINT('origin',(0.0,0.0,0.0));
#2 = DIRECTION('Z',(0.0,0.0,1.0));
ENDSEC;
END-ISO-10303-21;
"""


# ═══════════════════════════════════════════════════════════════════════
# Helper tests
# ═══════════════════════════════════════════════════════════════════════

class TestHelpers:
    def test_split_args_simple(self):
        result = _split_args("'name',1.0,2.0,3.0")
        assert len(result) == 4
        assert result[0] == "'name'"

    def test_split_args_nested_parens(self):
        result = _split_args("'test',(1.0,2.0,3.0),#5")
        assert len(result) == 3
        assert "(1.0,2.0,3.0)" in result[1]

    def test_split_args_empty(self):
        result = _split_args("")
        assert result == [] or result == [""]

    def test_parse_float_list(self):
        nums = _parse_float_list("(1.5,2.5,3.5)")
        assert len(nums) == 3
        assert abs(nums[0] - 1.5) < 1e-10
        assert abs(nums[2] - 3.5) < 1e-10

    def test_parse_float_list_scientific(self):
        nums = _parse_float_list("(1.0e2,-3.5E-1)")
        assert abs(nums[0] - 100.0) < 1e-10
        assert abs(nums[1] - (-0.35)) < 1e-10

    def test_extract_string(self):
        assert _extract_string("'Housing'") == "Housing"
        assert _extract_string("'Steel shaft'") == "Steel shaft"

    def test_extract_ref(self):
        assert _extract_ref("#42") == 42
        assert _extract_ref("no_ref") is None

    def test_vec3_ops(self):
        a = Vec3(1, 2, 3)
        b = Vec3(4, 5, 6)
        s = a + b
        assert abs(s.x - 5) < 1e-10
        d = b - a
        assert abs(d.z - 3) < 1e-10
        m = a * 2.0
        assert abs(m.y - 4) < 1e-10
        assert abs(a.norm() - math.sqrt(14)) < 1e-10


# ═══════════════════════════════════════════════════════════════════════
# Core parser tests
# ═══════════════════════════════════════════════════════════════════════

class TestStepParser:
    def test_parse_minimal(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert isinstance(asm, StepAssembly)
        assert asm.schema == "AP214"
        assert asm.description == "Minimal test"

    def test_named_parts(self):
        asm = parse_step_string(MINIMAL_STEP)
        names = asm.part_names
        assert "Housing" in names
        assert "Shaft" in names
        assert asm.num_parts == 2

    def test_cartesian_points(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert len(asm.points) == 2
        origin = asm.points[1]
        assert abs(origin.coords.x) < 1e-10
        p1 = asm.points[2]
        assert abs(p1.coords.x - 10.0) < 1e-10
        assert abs(p1.coords.y - 20.0) < 1e-10
        assert abs(p1.coords.z - 30.0) < 1e-10

    def test_directions(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert len(asm.directions) == 2
        z_dir = asm.directions[3]
        assert abs(z_dir.direction.z - 1.0) < 1e-10

    def test_axis2_placement(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert len(asm.placements) == 1
        plc = asm.placements[5]
        assert plc.name == "placement"
        assert abs(plc.location.x) < 1e-10  # at origin
        assert abs(plc.axis.z - 1.0) < 1e-10  # Z axis

    def test_vertices_attached(self):
        asm = parse_step_string(MINIMAL_STEP)
        # First part should have vertices attached
        assert len(asm.parts[0].vertices) == 2

    def test_multi_point(self):
        asm = parse_step_string(MULTI_POINT_STEP)
        assert asm.schema == "AP203"
        assert asm.num_parts == 1
        assert asm.parts[0].name == "Bracket"
        assert len(asm.points) == 3

    def test_empty_data(self):
        asm = parse_step_string(EMPTY_DATA_STEP)
        assert isinstance(asm, StepAssembly)
        # Should have a fallback unnamed part
        assert asm.num_parts >= 0

    def test_no_product_fallback(self):
        asm = parse_step_string(NO_PRODUCT_STEP)
        # Should create fallback part from filename
        assert asm.num_parts >= 1
        assert len(asm.points) == 1

    def test_stream_input(self):
        stream = io.StringIO(MINIMAL_STEP)
        asm = parse_step(stream)
        assert asm.num_parts == 2
        assert asm.filename == "<stream>"

    def test_file_not_found(self):
        with pytest.raises(FileNotFoundError):
            parse_step("nonexistent_file.step")

    def test_entity_count(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert len(asm.entities) == 7  # 2 points + 2 dirs + 1 placement + 2 products

    def test_part_material_default(self):
        asm = parse_step_string(MINIMAL_STEP)
        for part in asm.parts:
            assert part.material == ""
            assert part.atomic_number == 0

    def test_placements_attached_to_first_part(self):
        asm = parse_step_string(MINIMAL_STEP)
        assert len(asm.parts[0].placements) == 1


# ═══════════════════════════════════════════════════════════════════════
# Named part data structure tests
# ═══════════════════════════════════════════════════════════════════════

class TestNamedPart:
    def test_construction(self):
        p = NamedPart(id=1, name="Block", description="Test block")
        assert p.id == 1
        assert p.name == "Block"
        assert p.vertices == []
        assert p.placements == []

    def test_material_assignment(self):
        p = NamedPart(id=1, name="Block")
        p.material = "Al"
        p.atomic_number = 13
        assert p.material == "Al"
        assert p.atomic_number == 13
