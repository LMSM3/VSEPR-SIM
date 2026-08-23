#pragma once
/*
 * Dynamic object foundation for the geometry-to-continuum pipeline.
 *
 * Geometry is authoritative here. Downstream products stay absent until their
 * producing stages can attach object/revision identity and provenance.
 */

#include "core/math_vec3.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vsepr::multiscale {

using ObjectId = std::uint64_t;
inline constexpr ObjectId kInvalidObjectId = 0;

struct GeometryRevision {
    std::uint64_t value = 0;
    auto operator<=>(const GeometryRevision&) const = default;
};

inline constexpr GeometryRevision kInvalidGeometryRevision{};

struct Bounds3 {
    Vec3 min{};
    Vec3 max{};

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] Vec3 center() const noexcept;
    [[nodiscard]] Vec3 extent() const noexcept;
};

enum class DiagnosticSeverity {
    Warning,
    Fatal
};

enum class GeometryDiagnosticCode {
    None,
    InvalidPrimitive,
    InvalidTransform,
    EmptyCsgResult,
    UndefinedNormal,
    InvalidBounds,
    StaleSnapshot,
    StaleObjectId,
    UnsupportedDownstreamSlot,
    PossiblyEmptyCsg,
    ViewerLaunchFailure,
    ObjectIdExhausted,
    RevisionExhausted
};

enum class AcknowledgementStatus {
    Accepted,
    AcceptedWithWarning,
    Rejected,
    Stale,
    Unsupported
};

[[nodiscard]] std::string_view diagnostic_code_name(GeometryDiagnosticCode code) noexcept;
[[nodiscard]] std::string_view diagnostic_severity_name(DiagnosticSeverity severity) noexcept;
[[nodiscard]] std::string_view acknowledgement_status_name(AcknowledgementStatus status) noexcept;

struct GeometryDiagnostic {
    GeometryDiagnosticCode code = GeometryDiagnosticCode::None;
    DiagnosticSeverity severity = DiagnosticSeverity::Fatal;
    std::string summary;
    std::string detail;
    ObjectId object_id = kInvalidObjectId;
    GeometryRevision geometry_revision{};
    std::string operation;
    bool state_changed = false;
};

struct GeometryAcknowledgement {
    std::uint64_t sequence = 0;
    AcknowledgementStatus status = AcknowledgementStatus::Accepted;
    std::string operation;
    ObjectId object_id = kInvalidObjectId;
    GeometryRevision geometry_revision{};
    bool state_changed = false;
    std::optional<GeometryDiagnostic> diagnostic;
    std::string comparison_key;
};

[[nodiscard]] GeometryAcknowledgement make_geometry_acknowledgement(
    AcknowledgementStatus status,
    std::string operation,
    ObjectId object_id = kInvalidObjectId,
    GeometryRevision geometry_revision = {},
    bool state_changed = false,
    std::optional<GeometryDiagnostic> diagnostic = std::nullopt);

[[nodiscard]] std::string format_diagnostic(const GeometryDiagnostic& diagnostic);
[[nodiscard]] std::string format_acknowledgement(const GeometryAcknowledgement& acknowledgement);

enum class ObjectStage {
    GeometryOnly,
    Sampled,
    LowerScaleResolved,
    Meshed,
    Assembled,
    Solving,
    Converged,
    Failed
};

[[nodiscard]] std::string_view object_stage_name(ObjectStage stage) noexcept;

struct SphereGeometry {
    Vec3 center{};
    double radius = 0.0;
};

struct BoxGeometry {
    Vec3 center{};
    Vec3 half_extent{};
};

struct PrimitiveComposition;
using NestedComposition = std::shared_ptr<const PrimitiveComposition>;
using CsgOperand = std::variant<SphereGeometry, BoxGeometry, NestedComposition>;

enum class CsgOperation {
    Union,
    Intersection,
    Subtraction
};

struct CsgStep {
    CsgOperation operation = CsgOperation::Union;
    CsgOperand primitive{};
};

struct PrimitiveComposition {
    CsgOperand seed{};
    std::vector<CsgStep> operations;
};

using GeometryDefinition = std::variant<SphereGeometry, BoxGeometry, PrimitiveComposition>;

struct GeometryValidationResult {
    Bounds3 bounds;
    std::optional<GeometryDiagnostic> diagnostic;
    std::vector<GeometryDiagnostic> warnings;

    [[nodiscard]] bool accepted() const noexcept;
};

[[nodiscard]] GeometryValidationResult validate_geometry(
    const GeometryDefinition& geometry) noexcept;
[[nodiscard]] bool is_valid_geometry(const GeometryDefinition& geometry) noexcept;

struct AffineTransform {
    // Column-major world_from_local affine matrix.
    std::array<double, 16> world_from_local{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    std::string length_unit = "model_unit";
    std::string local_frame = "object_local";
    std::string world_frame = "world";

    [[nodiscard]] static AffineTransform identity();
};

struct TransformValidationResult {
    double uniform_scale = 1.0;
    std::optional<GeometryDiagnostic> diagnostic;

    [[nodiscard]] bool accepted() const noexcept;
};

[[nodiscard]] TransformValidationResult validate_transform(
    const AffineTransform& transform) noexcept;
[[nodiscard]] Vec3 transform_point(const AffineTransform& transform,
                                   const Vec3& point) noexcept;

enum class NormalValidity {
    Valid,
    UndefinedZeroGradient,
    AmbiguousCusp,
    OutsideTolerance,
    Unsupported
};

[[nodiscard]] std::string_view normal_validity_name(NormalValidity validity) noexcept;

struct NormalQueryResult {
    Vec3 normal{};
    NormalValidity validity = NormalValidity::Unsupported;

    [[nodiscard]] bool is_valid() const noexcept {
        return validity == NormalValidity::Valid;
    }
};

class SpatialQuery {
public:
    virtual ~SpatialQuery() = default;

    [[nodiscard]] virtual double signed_distance(const Vec3& point) const noexcept = 0;
    [[nodiscard]] virtual NormalQueryResult normal_query(
        const Vec3& point, double sample_distance = 0.0) const noexcept = 0;
    [[nodiscard]] virtual Bounds3 bounds() const noexcept = 0;

    [[nodiscard]] Vec3 surface_normal(const Vec3& point,
                                      double sample_distance = 0.0) const noexcept;
};

class ImplicitGeometry final : public SpatialQuery {
public:
    explicit ImplicitGeometry(GeometryDefinition definition);

    [[nodiscard]] const GeometryDefinition& definition() const noexcept;
    [[nodiscard]] double signed_distance(const Vec3& point) const noexcept override;
    [[nodiscard]] NormalQueryResult normal_query(
        const Vec3& point, double sample_distance = 0.0) const noexcept override;
    [[nodiscard]] Bounds3 bounds() const noexcept override;

private:
    GeometryDefinition definition_;
    Bounds3 bounds_;
};

class TransformedSpatialQuery final : public SpatialQuery {
public:
    TransformedSpatialQuery(GeometryDefinition definition, AffineTransform transform);

    [[nodiscard]] double signed_distance(const Vec3& point) const noexcept override;
    [[nodiscard]] NormalQueryResult normal_query(
        const Vec3& point, double sample_distance = 0.0) const noexcept override;
    [[nodiscard]] Bounds3 bounds() const noexcept override;
    [[nodiscard]] const AffineTransform& transform() const noexcept;

private:
    ImplicitGeometry local_geometry_;
    AffineTransform transform_;
    std::array<double, 16> local_from_world_{};
    double uniform_scale_ = 1.0;
    Bounds3 world_bounds_;
};

// Downstream types remain opaque. This prevents placeholder physical values
// from becoming object truth before their source stages exist.
class SampleCloud;
class LowerScalePacket;
class MaterialField;
class VolumeMesh;
class ContinuumState;
class SurfaceState;

enum class DownstreamSlot {
    SampleCloud,
    LowerScalePacket,
    MaterialField,
    VolumeMesh,
    ContinuumState,
    SurfaceState
};

[[nodiscard]] std::string_view downstream_slot_name(DownstreamSlot slot) noexcept;

struct GeometryRecord {
    GeometryRevision revision{};
    GeometryDefinition geometry;
    AffineTransform transform;
};

struct ObjectHistory {
    std::vector<GeometryRecord> design_revisions;
    std::optional<GeometryDefinition> formed_geometry;
    std::optional<GeometryDefinition> analysis_geometry;
};

struct DynamicObject {
    ObjectId id = kInvalidObjectId;
    std::string name;
    ObjectStage stage = ObjectStage::GeometryOnly;
    GeometryRevision geometry_revision{};
    GeometryDefinition geometry;
    AffineTransform transform;
    ObjectHistory history;

    std::shared_ptr<const SampleCloud> sample_cloud;
    std::shared_ptr<const LowerScalePacket> lower_scale_packet;
    std::shared_ptr<const MaterialField> material_field;
    std::shared_ptr<const VolumeMesh> volume_mesh;
    std::shared_ptr<const ContinuumState> continuum_state;
    std::shared_ptr<const SurfaceState> surface_state;
};

struct ObjectSnapshot {
    ObjectId id = kInvalidObjectId;
    std::string name;
    ObjectStage stage = ObjectStage::GeometryOnly;
    GeometryRevision geometry_revision{};
    GeometryDefinition geometry;
    AffineTransform transform;
    Bounds3 bounds;

    bool has_formed_geometry = false;
    bool has_analysis_geometry = false;
    bool has_sample_cloud = false;
    bool has_lower_scale_packet = false;
    bool has_material_field = false;
    bool has_volume_mesh = false;
    bool has_continuum_state = false;
    bool has_surface_state = false;
};

using ObjectSnapshotPtr = std::shared_ptr<const ObjectSnapshot>;

struct ObjectOperationResult {
    GeometryAcknowledgement acknowledgement;

    [[nodiscard]] bool accepted() const noexcept;
    [[nodiscard]] ObjectId object_id() const noexcept;
    [[nodiscard]] GeometryRevision geometry_revision() const noexcept;
};

struct SnapshotLookupResult {
    ObjectSnapshotPtr snapshot;
    GeometryAcknowledgement acknowledgement;

    [[nodiscard]] bool accepted() const noexcept;
};

class ObjectWorld {
public:
    // Compatibility API. Checked variants provide structured acknowledgements.
    [[nodiscard]] ObjectId create(std::string name, GeometryDefinition geometry);
    [[nodiscard]] bool replace_design_geometry(ObjectId id, GeometryDefinition geometry);
    [[nodiscard]] bool mark_failed(ObjectId id);
    [[nodiscard]] bool destroy(ObjectId id);
    [[nodiscard]] ObjectSnapshotPtr snapshot(ObjectId id) const;

    [[nodiscard]] ObjectOperationResult create_checked(
        std::string name,
        GeometryDefinition geometry,
        AffineTransform transform = AffineTransform::identity());
    [[nodiscard]] ObjectOperationResult replace_design_geometry_checked(
        ObjectId id,
        GeometryDefinition geometry,
        AffineTransform transform = AffineTransform::identity());
    [[nodiscard]] ObjectOperationResult destroy_checked(ObjectId id);
    [[nodiscard]] SnapshotLookupResult snapshot_checked(
        ObjectId id,
        std::optional<GeometryRevision> required_revision = std::nullopt);
    [[nodiscard]] ObjectOperationResult inspect_downstream_slot(
        ObjectId id,
        DownstreamSlot slot,
        std::optional<GeometryRevision> required_revision = std::nullopt);

    [[nodiscard]] std::vector<ObjectSnapshotPtr> snapshots() const;
    [[nodiscard]] std::vector<GeometryAcknowledgement> acknowledgements() const;
    [[nodiscard]] std::size_t size() const;

private:
    [[nodiscard]] static ObjectSnapshotPtr make_snapshot(const DynamicObject& object);
    [[nodiscard]] GeometryAcknowledgement record_acknowledgement_locked(
        GeometryAcknowledgement acknowledgement);

    mutable std::shared_mutex mutex_;
    ObjectId next_id_ = 1;
    std::uint64_t next_acknowledgement_sequence_ = 1;
    std::map<ObjectId, std::unique_ptr<DynamicObject>> objects_;
    std::vector<GeometryAcknowledgement> acknowledgement_log_;
};

} // namespace vsepr::multiscale
