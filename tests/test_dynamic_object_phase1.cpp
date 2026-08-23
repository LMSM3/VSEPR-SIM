/**
 * WO-91 regression: canonical geometry, object identity, and acknowledgements.
 */

#include "multiscale/dynamic_object.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using vsepr::Vec3;
using namespace vsepr::multiscale;

#define CHECK(condition) \
    do { if (!(condition)) throw std::runtime_error("check failed: " #condition); } while (false)

bool close(double left, double right, double tolerance = 1.0e-8) {
    return std::abs(left - right) <= tolerance;
}

GeometryDefinition sphere(const Vec3& center, double radius) {
    return SphereGeometry{center, radius};
}

AffineTransform translated_scaled(
    const Vec3& translation,
    double uniform_scale) {
    AffineTransform transform = AffineTransform::identity();
    transform.world_from_local[0] = uniform_scale;
    transform.world_from_local[5] = uniform_scale;
    transform.world_from_local[10] = uniform_scale;
    transform.world_from_local[12] = translation.x;
    transform.world_from_local[13] = translation.y;
    transform.world_from_local[14] = translation.z;
    transform.length_unit = "angstrom";
    return transform;
}

void primitive_queries_report_sign_normals_and_undefined_points() {
    const ImplicitGeometry shape(sphere({0.0, 0.0, 0.0}, 2.0));
    CHECK(shape.signed_distance({0.0, 0.0, 0.0}) < 0.0);
    CHECK(close(shape.signed_distance({2.0, 0.0, 0.0}), 0.0));
    CHECK(close(shape.signed_distance({3.0, 0.0, 0.0}), 1.0));

    const NormalQueryResult normal = shape.normal_query({2.0, 0.0, 0.0});
    CHECK(normal.is_valid());
    CHECK(close(normal.normal.x, 1.0, 1.0e-5));
    CHECK(close(normal.normal.y, 0.0, 1.0e-5));
    CHECK(close(normal.normal.z, 0.0, 1.0e-5));

    const NormalQueryResult undefined = shape.normal_query({0.0, 0.0, 0.0});
    CHECK(!undefined.is_valid());
    CHECK(undefined.validity == NormalValidity::UndefinedZeroGradient);

    const ImplicitGeometry box(BoxGeometry{{1.0, 2.0, 3.0}, {2.0, 3.0, 4.0}});
    CHECK(box.signed_distance({1.0, 2.0, 3.0}) < 0.0);
    CHECK(close(box.signed_distance({3.0, 2.0, 3.0}), 0.0));
    CHECK(close(box.signed_distance({4.0, 2.0, 3.0}), 1.0));
    CHECK(box.normal_query({3.0, 2.0, 3.0}).is_valid());

    PrimitiveComposition cusp;
    cusp.seed = SphereGeometry{{-1.0, 0.0, 0.0}, 1.0};
    cusp.operations.push_back(
        {CsgOperation::Union, SphereGeometry{{1.0, 0.0, 0.0}, 1.0}});
    const NormalQueryResult ambiguous =
        ImplicitGeometry(cusp).normal_query({0.0, 0.0, 0.0});
    CHECK(!ambiguous.is_valid());
    CHECK(ambiguous.validity == NormalValidity::AmbiguousCusp);
}

GeometryDefinition three_sphere_composition() {
    PrimitiveComposition composition;
    composition.seed = SphereGeometry{{-2.0, 0.0, 0.0}, 1.0};
    composition.operations.push_back(
        {CsgOperation::Union, SphereGeometry{{0.0, 0.0, 0.0}, 1.0}});
    composition.operations.push_back(
        {CsgOperation::Union, SphereGeometry{{2.0, 0.0, 0.0}, 1.0}});
    composition.operations.push_back(
        {CsgOperation::Subtraction, SphereGeometry{{0.0, 0.0, 0.0}, 0.25}});
    return composition;
}

void nested_csg_is_ordered_and_canonicalized() {
    auto child = std::make_shared<PrimitiveComposition>();
    child->seed = SphereGeometry{{-1.0, 0.0, 0.0}, 1.25};
    child->operations.push_back(
        {CsgOperation::Union, SphereGeometry{{1.0, 0.0, 0.0}, 1.25}});

    PrimitiveComposition parent;
    parent.seed = NestedComposition{child};
    parent.operations.push_back(
        {CsgOperation::Subtraction, BoxGeometry{{0.0, 0.0, 0.0}, {0.2, 2.0, 2.0}}});
    const GeometryDefinition definition = parent;

    const ImplicitGeometry shape(definition);
    CHECK(shape.signed_distance({-1.0, 0.0, 0.0}) < 0.0);
    CHECK(shape.signed_distance({1.0, 0.0, 0.0}) < 0.0);
    CHECK(shape.signed_distance({0.0, 0.0, 0.0}) > 0.0);

    PrimitiveComposition lens;
    lens.seed = SphereGeometry{{0.0, 0.0, 0.0}, 2.0};
    lens.operations.push_back(
        {CsgOperation::Intersection, SphereGeometry{{2.0, 0.0, 0.0}, 2.0}});
    const ImplicitGeometry intersection(lens);
    CHECK(intersection.signed_distance({1.0, 0.0, 0.0}) < 0.0);
    CHECK(intersection.signed_distance({-0.5, 0.0, 0.0}) > 0.0);

    ObjectWorld world;
    const ObjectOperationResult created = world.create_checked("nested", definition);
    CHECK(created.accepted());
    const ObjectSnapshotPtr before_external_mutation = world.snapshot(created.object_id());
    CHECK(before_external_mutation);
    CHECK(close(before_external_mutation->bounds.min.x, -2.25));
    CHECK(close(before_external_mutation->bounds.max.x, 2.25));

    child->operations.push_back(
        {CsgOperation::Union, SphereGeometry{{50.0, 0.0, 0.0}, 1.0}});
    const ObjectSnapshotPtr after_external_mutation = world.snapshot(created.object_id());
    CHECK(after_external_mutation);
    CHECK(close(after_external_mutation->bounds.min.x, -2.25));
    CHECK(close(after_external_mutation->bounds.max.x, 2.25));
}

void empty_csg_is_rejected_without_creating_state() {
    PrimitiveComposition disjoint;
    disjoint.seed = SphereGeometry{{-5.0, 0.0, 0.0}, 1.0};
    disjoint.operations.push_back(
        {CsgOperation::Intersection, SphereGeometry{{5.0, 0.0, 0.0}, 1.0}});

    const GeometryValidationResult validation = validate_geometry(disjoint);
    CHECK(!validation.accepted());
    CHECK(validation.diagnostic);
    CHECK(validation.diagnostic->code == GeometryDiagnosticCode::EmptyCsgResult);

    ObjectWorld world;
    const ObjectOperationResult rejected = world.create_checked("empty", disjoint);
    CHECK(!rejected.accepted());
    CHECK(rejected.object_id() == kInvalidObjectId);
    CHECK(rejected.acknowledgement.status == AcknowledgementStatus::Rejected);
    CHECK(rejected.acknowledgement.diagnostic);
    CHECK(rejected.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::EmptyCsgResult);
    CHECK(!rejected.acknowledgement.state_changed);
    CHECK(world.size() == 0);

    PrimitiveComposition possibly_empty;
    possibly_empty.seed = SphereGeometry{{0.0, 0.0, 0.0}, 1.0};
    possibly_empty.operations.push_back(
        {CsgOperation::Subtraction, SphereGeometry{{0.0, 0.0, 0.0}, 2.0}});
    const GeometryValidationResult warning_validation =
        validate_geometry(possibly_empty);
    CHECK(warning_validation.accepted());
    CHECK(warning_validation.warnings.size() == 1);
    CHECK(warning_validation.warnings.front().code ==
          GeometryDiagnosticCode::PossiblyEmptyCsg);

    const ObjectOperationResult accepted_with_warning =
        world.create_checked("possibly-empty", possibly_empty);
    CHECK(accepted_with_warning.accepted());
    CHECK(accepted_with_warning.acknowledgement.status ==
          AcknowledgementStatus::AcceptedWithWarning);
    CHECK(accepted_with_warning.acknowledgement.diagnostic);
    CHECK(accepted_with_warning.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::PossiblyEmptyCsg);
    CHECK(accepted_with_warning.acknowledgement.state_changed);
    CHECK(world.size() == 1);
}

void transforms_preserve_world_distance_bounds_and_normal_contracts() {
    const AffineTransform transform = translated_scaled({10.0, -2.0, 3.0}, 2.0);
    const TransformValidationResult validation = validate_transform(transform);
    CHECK(validation.accepted());
    CHECK(close(validation.uniform_scale, 2.0));

    const TransformedSpatialQuery query(
        sphere({0.0, 0.0, 0.0}, 1.0), transform);
    CHECK(close(query.signed_distance({10.0, -2.0, 3.0}), -2.0));
    CHECK(close(query.signed_distance({12.0, -2.0, 3.0}), 0.0));
    CHECK(close(query.signed_distance({13.0, -2.0, 3.0}), 1.0));
    CHECK(close(query.bounds().min.x, 8.0));
    CHECK(close(query.bounds().max.x, 12.0));
    CHECK(query.normal_query({12.0, -2.0, 3.0}).is_valid());
    CHECK(close(query.normal_query({12.0, -2.0, 3.0}).normal.x, 1.0, 1.0e-5));

    AffineTransform non_invertible = AffineTransform::identity();
    non_invertible.world_from_local[0] = 0.0;
    CHECK(!validate_transform(non_invertible).accepted());
    CHECK(validate_transform(non_invertible).diagnostic->code ==
          GeometryDiagnosticCode::InvalidTransform);

    AffineTransform non_uniform = AffineTransform::identity();
    non_uniform.world_from_local[0] = 2.0;
    CHECK(!validate_transform(non_uniform).accepted());
    CHECK(validate_transform(non_uniform).diagnostic->code ==
          GeometryDiagnosticCode::InvalidTransform);

    AffineTransform reflected = AffineTransform::identity();
    reflected.world_from_local[0] = -1.0;
    CHECK(!validate_transform(reflected).accepted());
    CHECK(validate_transform(reflected).diagnostic->code ==
          GeometryDiagnosticCode::InvalidTransform);
}

void replacement_is_atomic_and_revision_checked() {
    ObjectWorld world;
    const ObjectOperationResult created =
        world.create_checked("atomic", three_sphere_composition());
    CHECK(created.accepted());
    CHECK(created.geometry_revision().value == 1);

    const ObjectSnapshotPtr original = world.snapshot(created.object_id());
    CHECK(original);
    CHECK(close(original->bounds.min.x, -3.0));

    const ObjectOperationResult rejected = world.replace_design_geometry_checked(
        created.object_id(), sphere({0.0, 0.0, 0.0}, 0.0));
    CHECK(!rejected.accepted());
    CHECK(rejected.acknowledgement.diagnostic);
    CHECK(rejected.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::InvalidPrimitive);
    CHECK(!rejected.acknowledgement.state_changed);

    const ObjectSnapshotPtr after_rejection = world.snapshot(created.object_id());
    CHECK(after_rejection);
    CHECK(after_rejection->geometry_revision.value == 1);
    CHECK(close(after_rejection->bounds.min.x, -3.0));

    const ObjectOperationResult replaced = world.replace_design_geometry_checked(
        created.object_id(),
        sphere({0.0, 0.0, 0.0}, 2.0),
        translated_scaled({5.0, 0.0, 0.0}, 1.0));
    CHECK(replaced.accepted());
    CHECK(replaced.geometry_revision().value == 2);
    CHECK(replaced.acknowledgement.state_changed);

    const SnapshotLookupResult stale =
        world.snapshot_checked(created.object_id(), GeometryRevision{1});
    CHECK(!stale.accepted());
    CHECK(!stale.snapshot);
    CHECK(stale.acknowledgement.status == AcknowledgementStatus::Stale);
    CHECK(stale.acknowledgement.diagnostic);
    CHECK(stale.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::StaleSnapshot);

    const SnapshotLookupResult current =
        world.snapshot_checked(created.object_id(), GeometryRevision{2});
    CHECK(current.accepted());
    CHECK(current.snapshot);
    CHECK(close(current.snapshot->bounds.min.x, 3.0));
    CHECK(close(current.snapshot->bounds.max.x, 7.0));
}

void retained_snapshots_survive_destroy_and_ids_are_never_reused() {
    ObjectWorld world;
    const ObjectOperationResult first =
        world.create_checked("first", sphere({0.0, 0.0, 0.0}, 1.0));
    CHECK(first.accepted());
    const ObjectSnapshotPtr retained = world.snapshot(first.object_id());
    CHECK(retained);

    const ObjectOperationResult destroyed = world.destroy_checked(first.object_id());
    CHECK(destroyed.accepted());
    CHECK(destroyed.geometry_revision().value == 1);
    CHECK(!world.snapshot(first.object_id()));
    CHECK(close(retained->bounds.max.x, 1.0));

    const SnapshotLookupResult stale = world.snapshot_checked(first.object_id());
    CHECK(!stale.accepted());
    CHECK(stale.acknowledgement.diagnostic);
    CHECK(stale.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::StaleObjectId);

    const ObjectOperationResult second =
        world.create_checked("second", sphere({4.0, 0.0, 0.0}, 1.0));
    const ObjectOperationResult third =
        world.create_checked(
            "third", BoxGeometry{{8.0, 0.0, 0.0}, {1.0, 2.0, 3.0}});
    const ObjectOperationResult fourth =
        world.create_checked("fourth", sphere({12.0, 0.0, 0.0}, 1.0));
    CHECK(second.object_id() > first.object_id());
    CHECK(third.object_id() > second.object_id());
    CHECK(fourth.object_id() > third.object_id());

    const std::vector<ObjectSnapshotPtr> all = world.snapshots();
    CHECK(all.size() == 3);
    CHECK(all[0]->id == second.object_id());
    CHECK(all[1]->id == third.object_id());
    CHECK(all[2]->id == fourth.object_id());
}

void unsupported_downstream_access_is_explicit_and_immutable() {
    ObjectWorld world;
    const ObjectOperationResult created =
        world.create_checked("geometry-only", sphere({0.0, 0.0, 0.0}, 1.0));
    CHECK(created.accepted());

    const ObjectOperationResult unsupported = world.inspect_downstream_slot(
        created.object_id(), DownstreamSlot::MaterialField, GeometryRevision{1});
    CHECK(!unsupported.accepted());
    CHECK(unsupported.acknowledgement.status == AcknowledgementStatus::Unsupported);
    CHECK(unsupported.acknowledgement.diagnostic);
    CHECK(unsupported.acknowledgement.diagnostic->code ==
          GeometryDiagnosticCode::UnsupportedDownstreamSlot);
    CHECK(!unsupported.acknowledgement.state_changed);

    const ObjectSnapshotPtr snapshot = world.snapshot(created.object_id());
    CHECK(snapshot);
    CHECK(snapshot->stage == ObjectStage::GeometryOnly);
    CHECK(!snapshot->has_material_field);
    CHECK(snapshot->geometry_revision.value == 1);
}

void acknowledgement_keys_are_stable_and_sequences_are_monotonic() {
    ObjectWorld direct;
    const ObjectOperationResult direct_create =
        direct.create_checked("same", sphere({0.0, 0.0, 0.0}, 1.0));

    ObjectWorld prefixed;
    const ObjectOperationResult invalid =
        prefixed.create_checked("invalid", sphere({0.0, 0.0, 0.0}, 0.0));
    const ObjectOperationResult prefixed_create =
        prefixed.create_checked("same", sphere({0.0, 0.0, 0.0}, 1.0));

    CHECK(invalid.acknowledgement.sequence == 1);
    CHECK(prefixed_create.acknowledgement.sequence == 2);
    CHECK(direct_create.acknowledgement.comparison_key ==
          prefixed_create.acknowledgement.comparison_key);
    CHECK(format_acknowledgement(invalid.acknowledgement).find("GEO-E001") !=
          std::string::npos);

    const std::vector<GeometryAcknowledgement> log = prefixed.acknowledgements();
    CHECK(log.size() == 2);
    CHECK(log[0].sequence < log[1].sequence);
    CHECK(log[1].object_id == prefixed_create.object_id());
    CHECK(log[1].geometry_revision.value == 1);
}

using TestFunction = void (*)();

struct TestCase {
    const char* name;
    TestFunction function;
};

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"primitive_queries_report_sign_normals_and_undefined_points",
         primitive_queries_report_sign_normals_and_undefined_points},
        {"nested_csg_is_ordered_and_canonicalized",
         nested_csg_is_ordered_and_canonicalized},
        {"empty_csg_is_rejected_without_creating_state",
         empty_csg_is_rejected_without_creating_state},
        {"transforms_preserve_world_distance_bounds_and_normal_contracts",
         transforms_preserve_world_distance_bounds_and_normal_contracts},
        {"replacement_is_atomic_and_revision_checked",
         replacement_is_atomic_and_revision_checked},
        {"retained_snapshots_survive_destroy_and_ids_are_never_reused",
         retained_snapshots_survive_destroy_and_ids_are_never_reused},
        {"unsupported_downstream_access_is_explicit_and_immutable",
         unsupported_downstream_access_is_explicit_and_immutable},
        {"acknowledgement_keys_are_stable_and_sequences_are_monotonic",
         acknowledgement_keys_are_stable_and_sequences_are_monotonic},
    };

    try {
        for (const TestCase& test : tests) {
            test.function();
            std::cout << "PASS " << test.name << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL DynamicObjectPhase1 - " << error.what() << '\n';
        return 1;
    }
}
