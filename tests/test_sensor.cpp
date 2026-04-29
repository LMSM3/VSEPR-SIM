/**
 * test_sensor.cpp
 * ---------------
 * Verification suite for:
 *   - Sensor system (sensor.hpp): wind, material, energy sensors + friction
 *   - XYZL format (xyzl_format.hpp): bead dynamics + sensor serialisation
 *   - Bead dynamics basics: KE, temperature, segment geometry
 *
 * Tests:
 *   T01-T05: Sensor geometry (segment distance, capture, friction)
 *   T06-T09: Wind sensor measurement
 *   T10-T13: Material sensor measurement
 *   T14-T17: Energy sensor measurement
 *   T18-T20: Sensor friction model (μ ≈ 0.0014)
 *   T21-T24: XYZL bead data (line-segment, KE, midpoint)
 *   T25-T28: XYZL write/read round-trip
 *   T29-T31: XYZL sensor serialisation
 *   T32-T34: XYZL trajectory (multi-frame)
 *   T35-T37: Sensor placement helpers (grid, axis)
 *   T38-T40: Integration: bead frame → particle snapshot → sensor readings
 */

#include "sensor/sensor.hpp"
#include "io/xyzl_format.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <cassert>

static int pass_count = 0;
static int fail_count = 0;

#define TEST(name, cond) do { \
    if (cond) { std::cout << "  " << name << " [PASS]\n"; ++pass_count; } \
    else      { std::cout << "  " << name << " [FAIL]\n"; ++fail_count; } \
} while(0)

#define NEAR(a, b, tol) (std::abs((a) - (b)) < (tol))

using namespace vsepr::sensor;
using namespace vsepr::io;

// ============================================================================
// Helper: build a test particle snapshot (8 Ar atoms in a small box)
// ============================================================================
static ParticleSnapshot make_test_snapshot() {
    ParticleSnapshot snap;
    // 8 particles at corners of a 4Å cube centred at origin
    // All moving in +x direction at different speeds
    double coords[][3] = {
        {-2, -2, -2}, {-2, -2, 2}, {-2, 2, -2}, {-2, 2, 2},
        { 2, -2, -2}, { 2, -2, 2}, { 2, 2, -2}, { 2, 2, 2}
    };
    for (int i = 0; i < 8; ++i) {
        snap.positions.push_back({coords[i][0], coords[i][1], coords[i][2]});
        double vx = 0.001 * (i + 1); // 0.001 to 0.008 Å/fs
        snap.velocities.push_back({vx, 0.0, 0.0});
        snap.elements.push_back("Ar");
        snap.masses.push_back(39.948);
        snap.charges.push_back(0.0);
        snap.pe_per_atom.push_back(-0.01 * (i + 1));
    }
    return snap;
}

int main() {
    std::cout << "\n=== Sensor & XYZL Format Tests ===\n\n";

    // =====================================================================
    // Sensor geometry
    // =====================================================================
    {
        Sensor s(0, "test", SensorType::WIND,
                 Vec3{0,0,0}, Vec3{10,0,0}, 2.0);

        // T01: Segment length
        TEST("T01 Segment length = 10 Å",
             NEAR(s.length, 10.0, 1e-10));

        // T02: Midpoint
        TEST("T02 Midpoint = (5, 0, 0)",
             NEAR(s.midpoint.x, 5.0, 1e-10) && NEAR(s.midpoint.y, 0.0, 1e-10));

        // T03: Distance from point on segment = 0
        TEST("T03 Distance from (5,0,0) to segment = 0",
             NEAR(s.distance_to_segment(Vec3{5,0,0}), 0.0, 1e-10));

        // T04: Distance from point above segment = 3
        TEST("T04 Distance from (5,3,0) to segment = 3",
             NEAR(s.distance_to_segment(Vec3{5,3,0}), 3.0, 1e-10));

        // T05: Capture test
        TEST("T05 (5,1.5,0) within r=2 → captured",
             s.captures(Vec3{5, 1.5, 0}));
        TEST("T05b (5,3,0) outside r=2 → not captured",
             !s.captures(Vec3{5, 3, 0}));
    }

    // =====================================================================
    // Wind sensor measurement
    // =====================================================================
    {
        // Sensor at origin, along X, radius 5 Å (captures everything)
        Sensor ws(0, "wind_all", SensorType::WIND,
                  Vec3{-5,0,0}, Vec3{5,0,0}, 5.0);
        auto snap = make_test_snapshot();

        WindReading wr = measure_wind(ws, snap, 343.0);

        TEST("T06 Wind: captures all 8 particles",
             wr.particle_count == 8);
        TEST("T07 Wind: speed > 0",
             wr.speed > 0.0);
        TEST("T08 Wind: direction is +x",
             wr.direction.x > 0.9);
        TEST("T09 Wind: mach_number computed (c_sound=343)",
             wr.mach_number > 0.0);
    }

    // =====================================================================
    // Material sensor measurement
    // =====================================================================
    {
        // Sensor captures a subset (only particles near origin)
        Sensor ms(1, "mat_near", SensorType::MATERIAL,
                  Vec3{0,0,-3}, Vec3{0,0,3}, 3.0);
        auto snap = make_test_snapshot();

        MaterialReading mr = measure_material(ms, snap);

        TEST("T10 Material: captures some particles",
             mr.total_atoms > 0);
        TEST("T11 Material: dominant element is Ar",
             mr.dominant_element == "Ar");
        TEST("T12 Material: total mass > 0",
             mr.total_mass > 0.0);
        TEST("T13 Material: composition has 1 element (Ar only)",
             mr.composition.size() == 1 && mr.composition[0].element == "Ar");
    }

    // =====================================================================
    // Energy sensor measurement
    // =====================================================================
    {
        Sensor es(2, "energy_all", SensorType::ENERGY,
                  Vec3{0,0,-5}, Vec3{0,0,5}, 10.0);
        auto snap = make_test_snapshot();

        EnergyReading er = measure_energy(es, snap, 1.0);

        TEST("T14 Energy: captures all 8 particles",
             er.particle_count == 8);
        TEST("T15 Energy: thermal > 0 (particles moving)",
             er.thermal_energy > 0.0);
        TEST("T16 Energy: chemical_energy < 0 (negative PE)",
             er.chemical_energy < 0.0);
        TEST("T17 Energy: total flux computed",
             er.total_flux != 0.0);
    }

    // =====================================================================
    // Friction model
    // =====================================================================
    {
        Sensor fs(3, "fric", SensorType::WIND,
                  Vec3{0,0,0}, Vec3{10,0,0}, 2.0);

        TEST("T18 Default μ = 0.0014",
             NEAR(fs.mu_dynamic, 0.0014, 1e-10));

        // Velocity perpendicular to sensor axis (along Y)
        Vec3 v_perp{0.0, 1.0, 0.0};
        Vec3 drag = fs.friction_force(v_perp);
        // F = -μ * |v_perp| * v̂_perp = -0.0014 * 1.0 * ĵ
        TEST("T19 Friction drag magnitude = μ * v_perp = 0.0014",
             NEAR(drag.norm(), 0.0014, 1e-10));
        TEST("T19b Drag direction is -Y",
             drag.y < 0.0 && NEAR(drag.x, 0.0, 1e-15));

        // Velocity parallel to sensor axis → no drag
        Vec3 v_par{1.0, 0.0, 0.0};
        Vec3 drag_par = fs.friction_force(v_par);
        TEST("T20 Parallel velocity → zero drag",
             drag_par.norm() < 1e-15);
    }

    // =====================================================================
    // XYZL bead data
    // =====================================================================
    {
        XYZLBead b;
        b.label = "BB";
        b.p0 = {0, 0, 0};
        b.p1 = {3, 4, 0};
        b.mass = 72.0;
        b.charge = -0.1;
        b.velocity = {0.005, 0.0, 0.0};
        b.mu = 0.0014;

        TEST("T21 Bead length = 5 Å (3-4-5 triangle)",
             NEAR(b.length(), 5.0, 1e-10));

        auto mid = b.midpoint();
        TEST("T22 Bead midpoint = (1.5, 2, 0)",
             NEAR(mid[0], 1.5, 1e-10) && NEAR(mid[1], 2.0, 1e-10));

        TEST("T23 Bead speed = 0.005",
             NEAR(b.speed(), 0.005, 1e-15));

        TEST("T24 Bead KE = 0.5 * 72 * 0.005² = 0.0009",
             NEAR(b.kinetic_energy(), 0.5 * 72.0 * 0.005 * 0.005, 1e-15));
    }

    // =====================================================================
    // XYZL write/read round-trip
    // =====================================================================
    {
        XYZLFrame frame;
        frame.frame_number = 42;
        frame.time_fs = 100.5;
        frame.dt_fs = 0.5;
        frame.total_energy = -12.345;
        frame.temperature = 298.15;

        // Add beads
        XYZLBead b1;
        b1.label = "BB"; b1.p0 = {0,0,0}; b1.p1 = {1,0,0};
        b1.mass = 14.0; b1.charge = 0.0; b1.velocity = {0.001, 0.0, 0.0}; b1.mu = 0.0014;
        frame.beads.push_back(b1);

        XYZLBead b2;
        b2.label = "SC1"; b2.p0 = {2,0,0}; b2.p1 = {3,1,0};
        b2.mass = 28.0; b2.charge = -0.5; b2.velocity = {0.0, 0.002, 0.0}; b2.mu = 0.0014;
        frame.beads.push_back(b2);

        // Add sensor
        XYZLSensor s;
        s.name = "probe1"; s.type = SensorType::WIND;
        s.p0 = {0,0,-1}; s.p1 = {0,0,1}; s.radius = 2.0; s.mu = 0.0014;
        frame.sensors.push_back(s);

        // Add link
        XYZLLink lnk;
        lnk.bead_i = 0; lnk.bead_j = 1; lnk.order = 1.0;
        frame.links.push_back(lnk);

        // Write to string
        XYZLWriter writer;
        std::string output = writer.to_string(frame);

        TEST("T25 XYZL write: output not empty",
             !output.empty());

        // Read back
        std::istringstream iss(output);
        XYZLReader reader;
        XYZLFrame loaded;
        bool ok = reader.read_stream(iss, loaded);

        TEST("T26 XYZL read: success",
             ok);
        TEST("T27 XYZL round-trip: 2 beads + 1 sensor + 1 link",
             loaded.beads.size() == 2 && loaded.sensors.size() == 1 && loaded.links.size() == 1);
        TEST("T28 XYZL round-trip: bead label preserved",
             loaded.beads[0].label == "BB" && loaded.beads[1].label == "SC1");
    }

    // =====================================================================
    // XYZL sensor serialisation
    // =====================================================================
    {
        XYZLSensor ls;
        ls.name = "flow_probe";
        ls.type = SensorType::ENERGY;
        ls.p0 = {1,2,3}; ls.p1 = {4,5,6};
        ls.radius = 3.5; ls.mu = 0.002;

        // Convert to runtime sensor and back
        auto rt = ls.to_sensor(99);
        TEST("T29 Sensor round-trip: id=99",
             rt.id == 99);
        TEST("T30 Sensor round-trip: type=ENERGY",
             rt.type == SensorType::ENERGY);
        TEST("T31 Sensor round-trip: μ preserved",
             NEAR(rt.mu_dynamic, 0.002, 1e-10));
    }

    // =====================================================================
    // XYZL trajectory (multi-frame)
    // =====================================================================
    {
        XYZLTrajectory traj;
        for (int f = 0; f < 5; ++f) {
            XYZLFrame frame;
            frame.frame_number = f;
            frame.time_fs = f * 10.0;
            XYZLBead b;
            b.label = "W";
            b.p0 = {0, 0, 0}; b.p1 = {1, 0, 0};
            b.mass = 72.0; b.velocity = {0.001 * (f+1), 0, 0};
            b.mu = 0.0014;
            frame.beads.push_back(b);
            traj.add_frame(frame);
        }

        // Write and read back
        XYZLWriter writer;
        std::ostringstream oss;
        for (const auto& f : traj.frames)
            writer.write_stream(oss, f);

        std::istringstream iss(oss.str());
        XYZLTrajectory loaded;
        XYZLReader reader;
        reader.read_trajectory("", loaded); // won't work (needs file)

        // Direct stream read
        XYZLTrajectory loaded2;
        std::istringstream iss2(oss.str());
        while (iss2.good() && iss2.peek() != EOF) {
            XYZLFrame f;
            if (!reader.read_stream(iss2, f)) break;
            loaded2.frames.push_back(std::move(f));
        }

        TEST("T32 Trajectory: wrote 5 frames",
             traj.num_frames() == 5);
        TEST("T33 Trajectory: read back 5 frames",
             loaded2.num_frames() == 5);
        TEST("T34 Trajectory: frame 3 time = 30 fs",
             NEAR(loaded2.frames[3].time_fs, 30.0, 1.0));
    }

    // =====================================================================
    // Sensor placement helpers
    // =====================================================================
    {
        auto ws = make_wind_sensor(0, "ws", 0, 10, 5, 5, 3.0);
        TEST("T35 make_wind_sensor: length = 10",
             NEAR(ws.length, 10.0, 1e-10));
        TEST("T35b make_wind_sensor: type = WIND",
             ws.type == SensorType::WIND);

        auto grid = make_wind_grid(0, 10, -5, 5, 3, -5, 5, 3, 2.0);
        TEST("T36 Wind grid 3×3 = 9 sensors",
             grid.size() == 9);
        TEST("T37 Grid sensors all WIND type",
             grid[0].type == SensorType::WIND && grid[8].type == SensorType::WIND);
    }

    // =====================================================================
    // Integration: bead frame → snapshot → sensor
    // =====================================================================
    {
        // Build a frame with beads + a wind sensor
        XYZLFrame frame;
        for (int i = 0; i < 4; ++i) {
            XYZLBead b;
            b.label = "Ar";
            b.p0 = {static_cast<double>(i), 0, 0};
            b.p1 = {static_cast<double>(i) + 0.5, 0, 0};
            b.mass = 39.948;
            b.velocity = {0.003, 0.001, 0.0};
            b.mu = 0.0014;
            frame.beads.push_back(b);
        }

        XYZLSensor ws;
        ws.name = "flow"; ws.type = SensorType::WIND;
        ws.p0 = {0,-3,0}; ws.p1 = {4,-3,0}; ws.radius = 5.0; ws.mu = 0.0014;
        frame.sensors.push_back(ws);

        // Convert to snapshot + sensor array
        auto snap = frame.to_particle_snapshot();
        auto sarr = frame.to_sensor_array();

        TEST("T38 Snapshot has 4 particles",
             snap.size() == 4);
        TEST("T39 SensorArray has 1 sensor",
             sarr.size() == 1);

        // Measure
        auto readings = sarr.measure_all_wind(snap, 0.0);
        TEST("T40 Wind reading: captures particles",
             !readings.empty() && readings[0].particle_count > 0);
    }

    // =====================================================================
    // Constant checks
    // =====================================================================
    {
        TEST("T41 MU_DYNAMIC_SENSOR = 0.0014",
             NEAR(MU_DYNAMIC_SENSOR, 0.0014, 1e-10));
        TEST("T42 SensorType names",
             std::string(sensor_type_name(SensorType::WIND))     == "wind" &&
             std::string(sensor_type_name(SensorType::MATERIAL)) == "material" &&
             std::string(sensor_type_name(SensorType::ENERGY))   == "energy");
    }

    // =====================================================================
    // SensorArray
    // =====================================================================
    {
        SensorArray arr;
        arr.add(make_wind_sensor(0, "w1", 0, 5, 0, 0));
        arr.add(make_material_sensor(1, "m1", 0, 5, 0, 0));
        arr.add(make_energy_sensor(2, "e1", 0, 5, 0, 0));

        TEST("T43 SensorArray: 3 sensors",
             arr.size() == 3);
        TEST("T44 SensorArray: find by name",
             arr.find("m1") != nullptr && arr.find("m1")->type == SensorType::MATERIAL);
        TEST("T45 SensorArray: summary not empty",
             !arr.summary().empty());
    }

    // =====================================================================
    // XYZL format detection
    // =====================================================================
    {
        TEST("T46 is_xyzl_file('test.xyzL') = true",
             is_xyzl_file("test.xyzL"));
        TEST("T47 is_xyzl_file('test.xyzl') = true",
             is_xyzl_file("test.xyzl"));
        TEST("T48 is_xyzl_file('test.xyz') = false",
             !is_xyzl_file("test.xyz"));
    }

    // =====================================================================
    // Friction with custom μ
    // =====================================================================
    {
        Sensor cs(0, "custom_mu", SensorType::WIND,
                  Vec3{0,0,0}, Vec3{10,0,0}, 2.0);
        cs.mu_dynamic = 0.05; // 5% friction

        Vec3 drag = cs.friction_force(Vec3{0, 2.0, 0});
        TEST("T49 Custom μ=0.05: drag = 0.05 * 2.0 = 0.1",
             NEAR(drag.norm(), 0.1, 1e-10));

        // Zero friction
        cs.mu_dynamic = 0.0;
        Vec3 drag0 = cs.friction_force(Vec3{0, 2.0, 0});
        TEST("T50 Zero μ: no drag",
             drag0.norm() < 1e-15);
    }

    // ── Results ──────────────────────────────────────────────────────────
    std::cout << "\n--- Results: " << pass_count << " passed, "
              << fail_count << " failed out of " << (pass_count + fail_count) << " ---\n\n";

    return fail_count > 0 ? 1 : 0;
}
