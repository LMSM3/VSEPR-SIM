/**
 * Regression for ChronoDay 89 live scheduling and read-only density proxies.
 * No OpenGL dependency: this validates the data and timing contracts only.
 */

#include "sim/live_simulation_clock.hpp"
#include "vis/electron_domain_clouds.hpp"
#include "vis/geometry_acknowledgement.hpp"
#include "vis/supported_viewer.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

#define CHECK(condition) \
    do { if (!(condition)) throw std::runtime_error("check failed: " #condition); } while (false)

#define TEST(name) \
    static void name(); \
    struct name##_registration { \
        name##_registration() { \
            try { name(); std::cout << "  PASS  " #name "\n"; ++g_passed; } \
            catch (const std::exception& error) { \
                std::cout << "  FAIL  " #name " - " << error.what() << "\n"; ++g_failed; \
            } \
        } \
    } name##_instance; \
    static void name()

vsepr::FrameSnapshot water_snapshot() {
    vsepr::FrameSnapshot snapshot;
    snapshot.positions = {
        {0.0, 0.0, 0.0},
        {0.96, 0.0, 0.0},
        {-0.24, 0.93, 0.0}
    };
    snapshot.atomic_numbers = {8, 1, 1};
    snapshot.bonds = {{0, 1}, {0, 2}};
    return snapshot;
}

TEST(fixed_tick_clock_is_independent_of_render_cadence) {
    vsepr::LiveSimulationClock clock;
    CHECK(clock.consume_elapsed(1.0 / 240.0) == 0);
    CHECK(clock.consume_elapsed(1.0 / 240.0) == 1);
    clock.complete_tick();
    CHECK(std::abs(clock.simulation_time_seconds() - vsepr::LiveSimulationClock::kFixedTickSeconds) < 1e-12);
    CHECK(clock.interpolation_alpha() < 1e-9);

    CHECK(clock.set_speed_multiplier(10.0));
    CHECK(clock.consume_elapsed(0.1) == 120);
    CHECK(!clock.set_speed_multiplier(3.0));
}

TEST(water_generates_bond_and_lone_pair_density_proxies) {
    const auto clouds = vsepr::vis::build_electron_domain_clouds(water_snapshot());
    int bonding = 0;
    int lone_pairs = 0;
    for (const auto& cloud : clouds) {
        CHECK(std::abs(cloud.major_axis.norm() - 1.0) < 1e-9);
        CHECK(cloud.sigma_major > 0.0);
        if (cloud.kind == vsepr::vis::ElectronDomainKind::Bonding) {
            ++bonding;
        } else if (cloud.kind == vsepr::vis::ElectronDomainKind::LonePair) {
            ++lone_pairs;
            CHECK(cloud.source_atom == 0);
            CHECK(cloud.amplitude > 0.5);
        }
    }
    CHECK(bonding == 2);
    CHECK(lone_pairs == 2);
}

TEST(clouds_follow_snapshot_geometry_without_mutating_it) {
    auto first = water_snapshot();
    auto second = first;
    second.positions[1].x = 1.20;

    const auto first_clouds = vsepr::vis::build_electron_domain_clouds(first);
    const auto second_clouds = vsepr::vis::build_electron_domain_clouds(second);
    CHECK(first.positions[1].x == 0.96);
    CHECK(second.positions[1].x == 1.20);
    CHECK(first_clouds[0].sigma_major != second_clouds[0].sigma_major);
}

TEST(geometry_acknowledgement_feed_is_derived_and_deduplicated) {
    using namespace vsepr::multiscale;

    vsepr::vis::GeometryAcknowledgementFeed feed;
    const GeometryAcknowledgement accepted = make_geometry_acknowledgement(
        AcknowledgementStatus::Accepted,
        "create",
        7,
        GeometryRevision{3},
        true);
    CHECK(feed.publish(accepted));
    CHECK(!feed.publish(accepted));
    CHECK(feed.size() == 1);
    CHECK(feed.entries().front().object_id == 7);
    CHECK(feed.entries().front().geometry_revision.value == 3);

    GeometryDiagnostic failure;
    failure.code = GeometryDiagnosticCode::ViewerLaunchFailure;
    failure.severity = DiagnosticSeverity::Fatal;
    failure.summary = "Viewer artifact is unavailable";
    failure.detail = "missing.vsim";
    const GeometryAcknowledgement rejected = make_geometry_acknowledgement(
        AcknowledgementStatus::Rejected,
        "viewer_bootstrap",
        kInvalidObjectId,
        kInvalidGeometryRevision,
        false,
        failure);
    CHECK(feed.publish(rejected));
    CHECK(feed.size() == 2);
    CHECK(format_acknowledgement(rejected).find("VIEW-E010") != std::string::npos);
}

TEST(object_acknowledgements_follow_lifecycle_without_mutating_snapshots) {
    using namespace vsepr::multiscale;

    ObjectWorld world;
    vsepr::vis::GeometryAcknowledgementFeed feed;
    const ObjectOperationResult created = world.create_checked(
        "visual-source", SphereGeometry{{0.0, 0.0, 0.0}, 1.0});
    CHECK(created.accepted());
    CHECK(feed.publish(created.acknowledgement));

    const ObjectSnapshotPtr retained = world.snapshot(created.object_id());
    CHECK(retained);
    CHECK(feed.publish_snapshot(retained));
    CHECK(feed.snapshots().at(created.object_id())->geometry_revision.value == 1);
    const ObjectOperationResult replaced = world.replace_design_geometry_checked(
        created.object_id(), SphereGeometry{{0.0, 0.0, 0.0}, 2.0});
    CHECK(replaced.accepted());
    CHECK(feed.publish(replaced.acknowledgement));
    CHECK(feed.publish_snapshot(world.snapshot(created.object_id())));
    CHECK(feed.snapshots().at(created.object_id())->geometry_revision.value == 2);
    CHECK(!feed.publish_snapshot(retained));
    CHECK(retained->geometry_revision.value == 1);
    CHECK(std::abs(retained->bounds.max.x - 1.0) < 1e-12);

    const ObjectOperationResult destroyed = world.destroy_checked(created.object_id());
    CHECK(destroyed.accepted());
    CHECK(feed.publish(destroyed.acknowledgement));
    CHECK(!world.snapshot(created.object_id()));
    CHECK(retained->geometry_revision.value == 1);
    CHECK(feed.snapshots().at(created.object_id())->geometry_revision.value == 2);
    CHECK(std::abs(
        feed.snapshots().at(created.object_id())->bounds.max.x - 2.0) < 1e-12);

    const ObjectOperationResult rejected = world.create_checked(
        "invalid", SphereGeometry{{0.0, 0.0, 0.0}, 0.0});
    CHECK(!rejected.accepted());
    CHECK(feed.publish(rejected.acknowledgement));
    CHECK(world.size() == 0);
    CHECK(feed.size() == 4);
    CHECK(feed.entries().back().diagnostic);
    CHECK(!feed.entries().back().state_changed);
}

TEST(only_the_supported_live_viewer_is_a_launch_candidate) {
    const std::string supported =
        std::string("C:\\build_vis\\") +
        std::string(vsepr::vis::kSupportedViewerExecutable);
    CHECK(vsepr::vis::is_supported_viewer_candidate(supported));
    CHECK(vsepr::vis::is_supported_viewer_candidate(
        vsepr::vis::kSupportedViewerExecutable));
    CHECK(!vsepr::vis::is_supported_viewer_candidate("vsepr-vtk.exe"));
    CHECK(!vsepr::vis::is_supported_viewer_candidate("vsepr-light.exe"));
    CHECK(!vsepr::vis::is_supported_viewer_candidate("vsepr-viewer.exe"));
    CHECK(!vsepr::vis::is_supported_viewer_candidate("legacy/vsepr-vtk"));
}

} // namespace

int main() {
    std::cout << "LiveSimulationVisuals regression\n";
    std::cout << "Passed: " << g_passed << ", Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
