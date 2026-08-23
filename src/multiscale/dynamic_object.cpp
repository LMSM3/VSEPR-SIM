#include "multiscale/dynamic_object.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace vsepr::multiscale {
namespace {

constexpr std::size_t kMaxCsgDepth = 64;
constexpr std::size_t kMaxAcknowledgements = 256;

bool is_finite(const Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Bounds3 invalid_bounds() noexcept {
    const double infinity = std::numeric_limits<double>::infinity();
    return {{infinity, infinity, infinity}, {-infinity, -infinity, -infinity}};
}

GeometryDiagnostic diagnostic(
    GeometryDiagnosticCode code,
    DiagnosticSeverity severity,
    std::string summary,
    std::string detail = {}) {
    GeometryDiagnostic result;
    result.code = code;
    result.severity = severity;
    result.summary = std::move(summary);
    result.detail = std::move(detail);
    return result;
}

bool is_valid(const SphereGeometry& sphere) noexcept {
    return is_finite(sphere.center) && std::isfinite(sphere.radius) && sphere.radius > 0.0;
}

bool is_valid(const BoxGeometry& box) noexcept {
    return is_finite(box.center) && is_finite(box.half_extent) &&
           box.half_extent.x > 0.0 && box.half_extent.y > 0.0 &&
           box.half_extent.z > 0.0;
}

double primitive_signed_distance(const SphereGeometry& sphere, const Vec3& point) noexcept {
    return (point - sphere.center).norm() - sphere.radius;
}

double primitive_signed_distance(const BoxGeometry& box, const Vec3& point) noexcept {
    const Vec3 offset = point - box.center;
    const Vec3 q{
        std::abs(offset.x) - box.half_extent.x,
        std::abs(offset.y) - box.half_extent.y,
        std::abs(offset.z) - box.half_extent.z
    };
    const Vec3 outside{
        std::max(q.x, 0.0),
        std::max(q.y, 0.0),
        std::max(q.z, 0.0)
    };
    return outside.norm() + std::min(std::max({q.x, q.y, q.z}), 0.0);
}

Bounds3 primitive_bounds(const SphereGeometry& sphere) noexcept {
    const Vec3 extent{sphere.radius, sphere.radius, sphere.radius};
    return {sphere.center - extent, sphere.center + extent};
}

Bounds3 primitive_bounds(const BoxGeometry& box) noexcept {
    return {box.center - box.half_extent, box.center + box.half_extent};
}

Bounds3 union_bounds(const Bounds3& left, const Bounds3& right) noexcept {
    if (!left.is_valid()) return right;
    if (!right.is_valid()) return left;
    return {
        {std::min(left.min.x, right.min.x), std::min(left.min.y, right.min.y),
         std::min(left.min.z, right.min.z)},
        {std::max(left.max.x, right.max.x), std::max(left.max.y, right.max.y),
         std::max(left.max.z, right.max.z)}
    };
}

Bounds3 intersection_bounds(const Bounds3& left, const Bounds3& right) noexcept {
    if (!left.is_valid() || !right.is_valid()) return invalid_bounds();
    Bounds3 result{
        {std::max(left.min.x, right.min.x), std::max(left.min.y, right.min.y),
         std::max(left.min.z, right.min.z)},
        {std::min(left.max.x, right.max.x), std::min(left.max.y, right.max.y),
         std::min(left.max.z, right.max.z)}
    };
    return result.is_valid() ? result : invalid_bounds();
}

std::optional<GeometryDiagnostic> validate_operand(
    const CsgOperand& operand,
    std::size_t depth) noexcept;

std::optional<GeometryDiagnostic> validate_composition(
    const PrimitiveComposition& composition,
    std::size_t depth) noexcept {
    if (depth > kMaxCsgDepth) {
        return diagnostic(
            GeometryDiagnosticCode::InvalidPrimitive,
            DiagnosticSeverity::Fatal,
            "CSG nesting exceeds the supported depth",
            "maximum depth is 64");
    }
    if (auto issue = validate_operand(composition.seed, depth + 1)) return issue;
    for (const CsgStep& step : composition.operations) {
        if (auto issue = validate_operand(step.primitive, depth + 1)) return issue;
    }
    return std::nullopt;
}

std::optional<GeometryDiagnostic> validate_operand(
    const CsgOperand& operand,
    std::size_t depth) noexcept {
    return std::visit([depth](const auto& value) -> std::optional<GeometryDiagnostic> {
        using Operand = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Operand, SphereGeometry>) {
            if (!is_valid(value)) {
                return diagnostic(
                    GeometryDiagnosticCode::InvalidPrimitive,
                    DiagnosticSeverity::Fatal,
                    "Invalid sphere primitive",
                    "center must be finite and radius must be finite and positive");
            }
        } else if constexpr (std::is_same_v<Operand, BoxGeometry>) {
            if (!is_valid(value)) {
                return diagnostic(
                    GeometryDiagnosticCode::InvalidPrimitive,
                    DiagnosticSeverity::Fatal,
                    "Invalid box primitive",
                    "center and half extents must be finite; every half extent must be positive");
            }
        } else {
            if (!value) {
                return diagnostic(
                    GeometryDiagnosticCode::InvalidPrimitive,
                    DiagnosticSeverity::Fatal,
                    "Invalid nested CSG operand",
                    "nested composition reference is null");
            }
            return validate_composition(*value, depth);
        }
        return std::nullopt;
    }, operand);
}

double operand_signed_distance(const CsgOperand& operand, const Vec3& point) noexcept;
Bounds3 operand_bounds(const CsgOperand& operand) noexcept;

double composition_signed_distance(
    const PrimitiveComposition& composition,
    const Vec3& point) noexcept {
    double result = operand_signed_distance(composition.seed, point);
    for (const CsgStep& step : composition.operations) {
        const double rhs = operand_signed_distance(step.primitive, point);
        switch (step.operation) {
            case CsgOperation::Union:
                result = std::min(result, rhs);
                break;
            case CsgOperation::Intersection:
                result = std::max(result, rhs);
                break;
            case CsgOperation::Subtraction:
                result = std::max(result, -rhs);
                break;
        }
    }
    return result;
}

Bounds3 composition_bounds(const PrimitiveComposition& composition) noexcept {
    Bounds3 result = operand_bounds(composition.seed);
    for (const CsgStep& step : composition.operations) {
        const Bounds3 rhs = operand_bounds(step.primitive);
        switch (step.operation) {
            case CsgOperation::Union:
                result = union_bounds(result, rhs);
                break;
            case CsgOperation::Intersection:
                result = intersection_bounds(result, rhs);
                break;
            case CsgOperation::Subtraction:
                break;
        }
    }
    return result;
}

bool bounds_contains(const Bounds3& outer, const Bounds3& inner) noexcept {
    return outer.is_valid() && inner.is_valid() &&
           outer.min.x <= inner.min.x && outer.min.y <= inner.min.y &&
           outer.min.z <= inner.min.z && outer.max.x >= inner.max.x &&
           outer.max.y >= inner.max.y && outer.max.z >= inner.max.z;
}

bool operand_possibly_empty(const CsgOperand& operand) noexcept;

bool composition_possibly_empty(const PrimitiveComposition& composition) noexcept {
    bool possible_empty = operand_possibly_empty(composition.seed);
    Bounds3 accumulated = operand_bounds(composition.seed);
    for (const CsgStep& step : composition.operations) {
        const bool rhs_possibly_empty =
            operand_possibly_empty(step.primitive);
        const Bounds3 rhs = operand_bounds(step.primitive);
        switch (step.operation) {
            case CsgOperation::Union:
                accumulated = union_bounds(accumulated, rhs);
                possible_empty = possible_empty && rhs_possibly_empty;
                break;
            case CsgOperation::Intersection:
                // Overlapping conservative bounds do not prove that two
                // arbitrary implicit operands share occupied volume.
                accumulated = intersection_bounds(accumulated, rhs);
                possible_empty = true;
                break;
            case CsgOperation::Subtraction:
                if (bounds_contains(rhs, accumulated)) {
                    possible_empty = true;
                }
                break;
        }
    }
    return possible_empty;
}

bool operand_possibly_empty(const CsgOperand& operand) noexcept {
    return std::visit([](const auto& value) {
        using Operand = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Operand, NestedComposition>) {
            return composition_possibly_empty(*value);
        }
        return false;
    }, operand);
}

double operand_signed_distance(const CsgOperand& operand, const Vec3& point) noexcept {
    return std::visit([&point](const auto& value) {
        using Operand = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Operand, NestedComposition>) {
            return composition_signed_distance(*value, point);
        } else {
            return primitive_signed_distance(value, point);
        }
    }, operand);
}

Bounds3 operand_bounds(const CsgOperand& operand) noexcept {
    return std::visit([](const auto& value) {
        using Operand = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Operand, NestedComposition>) {
            return composition_bounds(*value);
        } else {
            return primitive_bounds(value);
        }
    }, operand);
}

double geometry_signed_distance(const GeometryDefinition& geometry, const Vec3& point) noexcept {
    return std::visit([&point](const auto& value) {
        using Geometry = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Geometry, PrimitiveComposition>) {
            return composition_signed_distance(value, point);
        } else {
            return primitive_signed_distance(value, point);
        }
    }, geometry);
}

Bounds3 geometry_bounds(const GeometryDefinition& geometry) noexcept {
    return std::visit([](const auto& value) {
        using Geometry = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Geometry, PrimitiveComposition>) {
            return composition_bounds(value);
        } else {
            return primitive_bounds(value);
        }
    }, geometry);
}

CsgOperand clone_operand(const CsgOperand& operand);

PrimitiveComposition clone_composition(const PrimitiveComposition& source) {
    PrimitiveComposition result;
    result.seed = clone_operand(source.seed);
    result.operations.reserve(source.operations.size());
    for (const CsgStep& step : source.operations) {
        result.operations.push_back({step.operation, clone_operand(step.primitive)});
    }
    return result;
}

CsgOperand clone_operand(const CsgOperand& operand) {
    return std::visit([](const auto& value) -> CsgOperand {
        using Operand = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Operand, NestedComposition>) {
            return std::make_shared<const PrimitiveComposition>(clone_composition(*value));
        } else {
            return value;
        }
    }, operand);
}

GeometryDefinition clone_geometry(const GeometryDefinition& geometry) {
    return std::visit([](const auto& value) -> GeometryDefinition {
        using Geometry = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Geometry, PrimitiveComposition>) {
            return clone_composition(value);
        } else {
            return value;
        }
    }, geometry);
}

double normal_sample_distance(const Bounds3& shape_bounds, double requested) noexcept {
    if (std::isfinite(requested) && requested > 0.0) return requested;
    if (!shape_bounds.is_valid()) return 1.0e-6;
    const double diagonal = shape_bounds.extent().norm();
    return std::max(1.0e-9, diagonal * 1.0e-6);
}

double matrix_value(const std::array<double, 16>& matrix, int row, int column) noexcept {
    return matrix[static_cast<std::size_t>(column * 4 + row)];
}

Vec3 matrix_column(const std::array<double, 16>& matrix, int column) noexcept {
    return {
        matrix_value(matrix, 0, column),
        matrix_value(matrix, 1, column),
        matrix_value(matrix, 2, column)
    };
}

std::array<double, 16> invert_similarity_transform(
    const AffineTransform& transform,
    double scale) noexcept {
    const auto& source = transform.world_from_local;
    const double inv_scale_squared = 1.0 / (scale * scale);
    std::array<double, 16> inverse{
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            inverse[static_cast<std::size_t>(column * 4 + row)] =
                matrix_value(source, column, row) * inv_scale_squared;
        }
    }

    const Vec3 translation{source[12], source[13], source[14]};
    for (int row = 0; row < 3; ++row) {
        inverse[12 + static_cast<std::size_t>(row)] =
            -(matrix_value(inverse, row, 0) * translation.x +
              matrix_value(inverse, row, 1) * translation.y +
              matrix_value(inverse, row, 2) * translation.z);
    }
    return inverse;
}

Vec3 apply_point(const std::array<double, 16>& matrix, const Vec3& point) noexcept {
    return {
        matrix_value(matrix, 0, 0) * point.x +
            matrix_value(matrix, 0, 1) * point.y +
            matrix_value(matrix, 0, 2) * point.z + matrix[12],
        matrix_value(matrix, 1, 0) * point.x +
            matrix_value(matrix, 1, 1) * point.y +
            matrix_value(matrix, 1, 2) * point.z + matrix[13],
        matrix_value(matrix, 2, 0) * point.x +
            matrix_value(matrix, 2, 1) * point.y +
            matrix_value(matrix, 2, 2) * point.z + matrix[14]
    };
}

Vec3 apply_direction(const std::array<double, 16>& matrix, const Vec3& value) noexcept {
    return {
        matrix_value(matrix, 0, 0) * value.x +
            matrix_value(matrix, 0, 1) * value.y +
            matrix_value(matrix, 0, 2) * value.z,
        matrix_value(matrix, 1, 0) * value.x +
            matrix_value(matrix, 1, 1) * value.y +
            matrix_value(matrix, 1, 2) * value.z,
        matrix_value(matrix, 2, 0) * value.x +
            matrix_value(matrix, 2, 1) * value.y +
            matrix_value(matrix, 2, 2) * value.z
    };
}

Bounds3 transformed_bounds(const Bounds3& local, const AffineTransform& transform) noexcept {
    if (!local.is_valid()) return invalid_bounds();
    Bounds3 result = invalid_bounds();
    for (int index = 0; index < 8; ++index) {
        const Vec3 corner{
            (index & 1) ? local.max.x : local.min.x,
            (index & 2) ? local.max.y : local.min.y,
            (index & 4) ? local.max.z : local.min.z
        };
        const Vec3 world = transform_point(transform, corner);
        if (!result.is_valid()) {
            result = {world, world};
        } else {
            result.min.x = std::min(result.min.x, world.x);
            result.min.y = std::min(result.min.y, world.y);
            result.min.z = std::min(result.min.z, world.z);
            result.max.x = std::max(result.max.x, world.x);
            result.max.y = std::max(result.max.y, world.y);
            result.max.z = std::max(result.max.z, world.z);
        }
    }
    return result;
}

std::string acknowledgement_key(const GeometryAcknowledgement& acknowledgement) {
    std::ostringstream stream;
    stream << acknowledgement.operation << '|'
           << acknowledgement_status_name(acknowledgement.status) << '|'
           << acknowledgement.object_id << '|'
           << acknowledgement.geometry_revision.value << '|'
           << (acknowledgement.state_changed ? "changed" : "unchanged") << '|';
    if (acknowledgement.diagnostic) {
        stream << diagnostic_code_name(acknowledgement.diagnostic->code);
    } else {
        stream << "NONE";
    }
    return stream.str();
}

bool slot_present(const DynamicObject& object, DownstreamSlot slot) noexcept {
    switch (slot) {
        case DownstreamSlot::SampleCloud: return static_cast<bool>(object.sample_cloud);
        case DownstreamSlot::LowerScalePacket: return static_cast<bool>(object.lower_scale_packet);
        case DownstreamSlot::MaterialField: return static_cast<bool>(object.material_field);
        case DownstreamSlot::VolumeMesh: return static_cast<bool>(object.volume_mesh);
        case DownstreamSlot::ContinuumState: return static_cast<bool>(object.continuum_state);
        case DownstreamSlot::SurfaceState: return static_cast<bool>(object.surface_state);
    }
    return false;
}

} // namespace

bool Bounds3::is_valid() const noexcept {
    return is_finite(min) && is_finite(max) &&
           min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

Vec3 Bounds3::center() const noexcept {
    return (min + max) * 0.5;
}

Vec3 Bounds3::extent() const noexcept {
    return max - min;
}

std::string_view diagnostic_code_name(GeometryDiagnosticCode code) noexcept {
    switch (code) {
        case GeometryDiagnosticCode::None: return "NONE";
        case GeometryDiagnosticCode::InvalidPrimitive: return "GEO-E001";
        case GeometryDiagnosticCode::InvalidTransform: return "GEO-E002";
        case GeometryDiagnosticCode::EmptyCsgResult: return "GEO-E003";
        case GeometryDiagnosticCode::UndefinedNormal: return "GEO-W004";
        case GeometryDiagnosticCode::InvalidBounds: return "GEO-E005";
        case GeometryDiagnosticCode::StaleSnapshot: return "GEO-E006";
        case GeometryDiagnosticCode::StaleObjectId: return "GEO-E007";
        case GeometryDiagnosticCode::UnsupportedDownstreamSlot: return "GEO-E008";
        case GeometryDiagnosticCode::PossiblyEmptyCsg: return "GEO-W009";
        case GeometryDiagnosticCode::ViewerLaunchFailure: return "VIEW-E010";
        case GeometryDiagnosticCode::ObjectIdExhausted: return "GEO-E011";
        case GeometryDiagnosticCode::RevisionExhausted: return "GEO-E012";
    }
    return "GEO-E000";
}

std::string_view diagnostic_severity_name(DiagnosticSeverity severity) noexcept {
    return severity == DiagnosticSeverity::Fatal ? "fatal" : "warning";
}

std::string_view acknowledgement_status_name(AcknowledgementStatus status) noexcept {
    switch (status) {
        case AcknowledgementStatus::Accepted: return "accepted";
        case AcknowledgementStatus::AcceptedWithWarning: return "accepted_with_warning";
        case AcknowledgementStatus::Rejected: return "rejected";
        case AcknowledgementStatus::Stale: return "stale";
        case AcknowledgementStatus::Unsupported: return "unsupported";
    }
    return "unsupported";
}

GeometryAcknowledgement make_geometry_acknowledgement(
    AcknowledgementStatus status,
    std::string operation,
    ObjectId object_id,
    GeometryRevision geometry_revision,
    bool state_changed,
    std::optional<GeometryDiagnostic> source_diagnostic) {
    GeometryAcknowledgement result;
    result.status = status;
    result.operation = std::move(operation);
    result.object_id = object_id;
    result.geometry_revision = geometry_revision;
    result.state_changed = state_changed;
    result.diagnostic = std::move(source_diagnostic);
    if (result.diagnostic) {
        result.diagnostic->object_id = object_id;
        result.diagnostic->geometry_revision = geometry_revision;
        result.diagnostic->operation = result.operation;
        result.diagnostic->state_changed = state_changed;
    }
    result.comparison_key = acknowledgement_key(result);
    return result;
}

std::string format_diagnostic(const GeometryDiagnostic& value) {
    std::ostringstream stream;
    stream << '[' << diagnostic_code_name(value.code) << "] "
           << value.summary;
    if (value.object_id != kInvalidObjectId) {
        stream << " | object=" << value.object_id;
    }
    if (value.geometry_revision != kInvalidGeometryRevision) {
        stream << " revision=" << value.geometry_revision.value;
    }
    if (!value.operation.empty()) {
        stream << " operation=" << value.operation;
    }
    if (!value.detail.empty()) {
        stream << " | " << value.detail;
    }
    return stream.str();
}

std::string format_acknowledgement(const GeometryAcknowledgement& value) {
    if (value.diagnostic) return format_diagnostic(*value.diagnostic);
    std::ostringstream stream;
    stream << '[' << acknowledgement_status_name(value.status) << "] "
           << value.operation;
    if (value.object_id != kInvalidObjectId) {
        stream << " | object=" << value.object_id;
    }
    if (value.geometry_revision != kInvalidGeometryRevision) {
        stream << " revision=" << value.geometry_revision.value;
    }
    return stream.str();
}

std::string_view object_stage_name(ObjectStage stage) noexcept {
    switch (stage) {
        case ObjectStage::GeometryOnly: return "GeometryOnly";
        case ObjectStage::Sampled: return "Sampled";
        case ObjectStage::LowerScaleResolved: return "LowerScaleResolved";
        case ObjectStage::Meshed: return "Meshed";
        case ObjectStage::Assembled: return "Assembled";
        case ObjectStage::Solving: return "Solving";
        case ObjectStage::Converged: return "Converged";
        case ObjectStage::Failed: return "Failed";
    }
    return "Unknown";
}

bool GeometryValidationResult::accepted() const noexcept {
    return !diagnostic.has_value();
}

GeometryValidationResult validate_geometry(const GeometryDefinition& geometry) noexcept {
    GeometryValidationResult result;
    result.bounds = invalid_bounds();

    const auto issue = std::visit([](const auto& value) -> std::optional<GeometryDiagnostic> {
        using Geometry = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Geometry, SphereGeometry>) {
            return validate_operand(CsgOperand{value}, 0);
        } else if constexpr (std::is_same_v<Geometry, BoxGeometry>) {
            return validate_operand(CsgOperand{value}, 0);
        } else {
            return validate_composition(value, 0);
        }
    }, geometry);
    if (issue) {
        result.diagnostic = issue;
        return result;
    }

    result.bounds = geometry_bounds(geometry);
    if (!result.bounds.is_valid()) {
        if (std::holds_alternative<PrimitiveComposition>(geometry)) {
            result.diagnostic = diagnostic(
                GeometryDiagnosticCode::EmptyCsgResult,
                DiagnosticSeverity::Fatal,
                "CSG composition has an empty conservative bound",
                "the ordered intersection produced no overlapping bounds");
        } else {
            result.diagnostic = diagnostic(
                GeometryDiagnosticCode::InvalidBounds,
                DiagnosticSeverity::Fatal,
                "Geometry produced invalid bounds");
        }
    } else if (const auto* composition =
                   std::get_if<PrimitiveComposition>(&geometry);
               composition && composition_possibly_empty(*composition)) {
        result.warnings.push_back(diagnostic(
            GeometryDiagnosticCode::PossiblyEmptyCsg,
            DiagnosticSeverity::Warning,
            "CSG composition may be empty",
            "conservative bounds cannot prove occupied volume; retain the "
            "geometry and require sampling or a stronger proof"));
    }
    return result;
}

bool is_valid_geometry(const GeometryDefinition& geometry) noexcept {
    return validate_geometry(geometry).accepted();
}

AffineTransform AffineTransform::identity() {
    return {};
}

bool TransformValidationResult::accepted() const noexcept {
    return !diagnostic.has_value();
}

TransformValidationResult validate_transform(const AffineTransform& transform) noexcept {
    TransformValidationResult result;
    const auto& matrix = transform.world_from_local;

    if (transform.length_unit.empty() || transform.local_frame.empty() ||
        transform.world_frame.empty()) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform metadata is incomplete",
            "length unit, local frame, and world frame are required");
        return result;
    }
    if (!std::all_of(matrix.begin(), matrix.end(),
                     [](double value) { return std::isfinite(value); })) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform contains a non-finite value");
        return result;
    }

    constexpr double affine_tolerance = 1.0e-12;
    if (std::abs(matrix[3]) > affine_tolerance ||
        std::abs(matrix[7]) > affine_tolerance ||
        std::abs(matrix[11]) > affine_tolerance ||
        std::abs(matrix[15] - 1.0) > affine_tolerance) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform is not affine",
            "the final row must be [0, 0, 0, 1]");
        return result;
    }

    const Vec3 x = matrix_column(matrix, 0);
    const Vec3 y = matrix_column(matrix, 1);
    const Vec3 z = matrix_column(matrix, 2);
    const double sx = x.norm();
    const double sy = y.norm();
    const double sz = z.norm();
    if (sx <= affine_tolerance || sy <= affine_tolerance || sz <= affine_tolerance) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform is non-invertible",
            "one or more basis vectors have zero length");
        return result;
    }

    const double scale = (sx + sy + sz) / 3.0;
    const double metric_tolerance = std::max(1.0e-10, scale * 1.0e-9);
    if (std::abs(sx - scale) > metric_tolerance ||
        std::abs(sy - scale) > metric_tolerance ||
        std::abs(sz - scale) > metric_tolerance ||
        std::abs(x.dot(y)) > metric_tolerance * scale ||
        std::abs(x.dot(z)) > metric_tolerance * scale ||
        std::abs(y.dot(z)) > metric_tolerance * scale) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform is unsupported for signed-distance queries",
            "only rigid or uniform-scale affine transforms are accepted");
        return result;
    }

    const double determinant = x.dot(y.cross(z));
    if (std::abs(determinant) <= affine_tolerance) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Transform is non-invertible");
        return result;
    }
    if (determinant < 0.0) {
        result.diagnostic = diagnostic(
            GeometryDiagnosticCode::InvalidTransform,
            DiagnosticSeverity::Fatal,
            "Reflected transform lacks orientation metadata",
            "reflections remain unsupported until orientation is explicit");
        return result;
    }
    result.uniform_scale = scale;
    return result;
}

Vec3 transform_point(const AffineTransform& transform, const Vec3& point) noexcept {
    return apply_point(transform.world_from_local, point);
}

std::string_view normal_validity_name(NormalValidity validity) noexcept {
    switch (validity) {
        case NormalValidity::Valid: return "valid";
        case NormalValidity::UndefinedZeroGradient: return "undefined_zero_gradient";
        case NormalValidity::AmbiguousCusp: return "ambiguous_cusp";
        case NormalValidity::OutsideTolerance: return "outside_tolerance";
        case NormalValidity::Unsupported: return "unsupported";
    }
    return "unsupported";
}

Vec3 SpatialQuery::surface_normal(const Vec3& point, double sample_distance) const noexcept {
    return normal_query(point, sample_distance).normal;
}

ImplicitGeometry::ImplicitGeometry(GeometryDefinition definition) {
    const GeometryValidationResult validation = validate_geometry(definition);
    if (!validation.accepted()) {
        throw std::invalid_argument(format_diagnostic(*validation.diagnostic));
    }
    definition_ = clone_geometry(definition);
    bounds_ = validation.bounds;
}

const GeometryDefinition& ImplicitGeometry::definition() const noexcept {
    return definition_;
}

double ImplicitGeometry::signed_distance(const Vec3& point) const noexcept {
    return geometry_signed_distance(definition_, point);
}

NormalQueryResult ImplicitGeometry::normal_query(
    const Vec3& point,
    double sample_distance) const noexcept {
    const double h = normal_sample_distance(bounds_, sample_distance);
    const Vec3 dx{h, 0.0, 0.0};
    const Vec3 dy{0.0, h, 0.0};
    const Vec3 dz{0.0, 0.0, h};
    const Vec3 gradient{
        signed_distance(point + dx) - signed_distance(point - dx),
        signed_distance(point + dy) - signed_distance(point - dy),
        signed_distance(point + dz) - signed_distance(point - dz)
    };
    const double magnitude = gradient.norm();
    if (!std::isfinite(magnitude) || magnitude <= 1.0e-12) {
        const bool on_surface = std::abs(signed_distance(point)) <= h;
        return {
            {},
            on_surface ? NormalValidity::AmbiguousCusp
                       : NormalValidity::UndefinedZeroGradient
        };
    }
    return {gradient / magnitude, NormalValidity::Valid};
}

Bounds3 ImplicitGeometry::bounds() const noexcept {
    return bounds_;
}

TransformedSpatialQuery::TransformedSpatialQuery(
    GeometryDefinition definition,
    AffineTransform transform)
    : local_geometry_(std::move(definition)),
      transform_(std::move(transform)) {
    const TransformValidationResult validation = validate_transform(transform_);
    if (!validation.accepted()) {
        throw std::invalid_argument(format_diagnostic(*validation.diagnostic));
    }
    uniform_scale_ = validation.uniform_scale;
    local_from_world_ = invert_similarity_transform(transform_, uniform_scale_);
    world_bounds_ = transformed_bounds(local_geometry_.bounds(), transform_);
}

double TransformedSpatialQuery::signed_distance(const Vec3& point) const noexcept {
    return local_geometry_.signed_distance(apply_point(local_from_world_, point)) *
           uniform_scale_;
}

NormalQueryResult TransformedSpatialQuery::normal_query(
    const Vec3& point,
    double sample_distance) const noexcept {
    const Vec3 local_point = apply_point(local_from_world_, point);
    const double local_sample = sample_distance > 0.0
        ? sample_distance / uniform_scale_ : 0.0;
    NormalQueryResult result = local_geometry_.normal_query(local_point, local_sample);
    if (result.is_valid()) {
        result.normal = (apply_direction(transform_.world_from_local, result.normal) /
                         uniform_scale_).normalized();
    }
    return result;
}

Bounds3 TransformedSpatialQuery::bounds() const noexcept {
    return world_bounds_;
}

const AffineTransform& TransformedSpatialQuery::transform() const noexcept {
    return transform_;
}

std::string_view downstream_slot_name(DownstreamSlot slot) noexcept {
    switch (slot) {
        case DownstreamSlot::SampleCloud: return "SampleCloud";
        case DownstreamSlot::LowerScalePacket: return "LowerScalePacket";
        case DownstreamSlot::MaterialField: return "MaterialField";
        case DownstreamSlot::VolumeMesh: return "VolumeMesh";
        case DownstreamSlot::ContinuumState: return "ContinuumState";
        case DownstreamSlot::SurfaceState: return "SurfaceState";
    }
    return "Unknown";
}

bool ObjectOperationResult::accepted() const noexcept {
    return acknowledgement.status == AcknowledgementStatus::Accepted ||
           acknowledgement.status == AcknowledgementStatus::AcceptedWithWarning;
}

ObjectId ObjectOperationResult::object_id() const noexcept {
    return acknowledgement.object_id;
}

GeometryRevision ObjectOperationResult::geometry_revision() const noexcept {
    return acknowledgement.geometry_revision;
}

bool SnapshotLookupResult::accepted() const noexcept {
    return static_cast<bool>(snapshot) &&
           acknowledgement.status == AcknowledgementStatus::Accepted;
}

ObjectId ObjectWorld::create(std::string name, GeometryDefinition geometry) {
    const ObjectOperationResult result = create_checked(std::move(name), std::move(geometry));
    return result.accepted() ? result.object_id() : kInvalidObjectId;
}

ObjectOperationResult ObjectWorld::create_checked(
    std::string name,
    GeometryDefinition geometry,
    AffineTransform transform) {
    const GeometryValidationResult geometry_validation = validate_geometry(geometry);
    const TransformValidationResult transform_validation = validate_transform(transform);

    std::unique_lock lock(mutex_);
    if (!geometry_validation.accepted()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "create",
            kInvalidObjectId,
            kInvalidGeometryRevision,
            false,
            geometry_validation.diagnostic);
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    if (!transform_validation.accepted()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "create",
            kInvalidObjectId,
            kInvalidGeometryRevision,
            false,
            transform_validation.diagnostic);
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    if (next_id_ == kInvalidObjectId) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "create",
            kInvalidObjectId,
            kInvalidGeometryRevision,
            false,
            diagnostic(
                GeometryDiagnosticCode::ObjectIdExhausted,
                DiagnosticSeverity::Fatal,
                "Object ID space is exhausted"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }

    const ObjectId id = next_id_;
    next_id_ = next_id_ == std::numeric_limits<ObjectId>::max()
        ? kInvalidObjectId : next_id_ + 1;
    const GeometryRevision revision{1};
    GeometryDefinition canonical = clone_geometry(geometry);

    auto object = std::make_unique<DynamicObject>();
    object->id = id;
    object->name = std::move(name);
    object->geometry_revision = revision;
    object->geometry = canonical;
    object->transform = transform;
    object->history.design_revisions.push_back({revision, canonical, transform});
    objects_.emplace(id, std::move(object));

    const bool has_warning = !geometry_validation.warnings.empty();
    auto acknowledgement = make_geometry_acknowledgement(
        has_warning ? AcknowledgementStatus::AcceptedWithWarning
                    : AcknowledgementStatus::Accepted,
        "create",
        id,
        revision,
        true,
        has_warning
            ? std::optional<GeometryDiagnostic>{geometry_validation.warnings.front()}
            : std::nullopt);
    return {record_acknowledgement_locked(std::move(acknowledgement))};
}

bool ObjectWorld::replace_design_geometry(ObjectId id, GeometryDefinition geometry) {
    return replace_design_geometry_checked(id, std::move(geometry)).accepted();
}

ObjectOperationResult ObjectWorld::replace_design_geometry_checked(
    ObjectId id,
    GeometryDefinition geometry,
    AffineTransform transform) {
    const GeometryValidationResult geometry_validation = validate_geometry(geometry);
    const TransformValidationResult transform_validation = validate_transform(transform);

    std::unique_lock lock(mutex_);
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "replace_design_geometry",
            id,
            kInvalidGeometryRevision,
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleObjectId,
                DiagnosticSeverity::Fatal,
                "Object ID is stale or destroyed"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }

    DynamicObject& object = *found->second;
    if (!geometry_validation.accepted()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "replace_design_geometry",
            id,
            object.geometry_revision,
            false,
            geometry_validation.diagnostic);
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    if (!transform_validation.accepted()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "replace_design_geometry",
            id,
            object.geometry_revision,
            false,
            transform_validation.diagnostic);
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    if (object.geometry_revision.value == std::numeric_limits<std::uint64_t>::max()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Rejected,
            "replace_design_geometry",
            id,
            object.geometry_revision,
            false,
            diagnostic(
                GeometryDiagnosticCode::RevisionExhausted,
                DiagnosticSeverity::Fatal,
                "Geometry revision space is exhausted"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }

    const GeometryRevision next_revision{object.geometry_revision.value + 1};
    GeometryDefinition canonical = clone_geometry(geometry);
    object.history.design_revisions.push_back({next_revision, canonical, transform});
    object.geometry = std::move(canonical);
    object.transform = std::move(transform);
    object.geometry_revision = next_revision;
    object.history.formed_geometry.reset();
    object.history.analysis_geometry.reset();
    object.sample_cloud.reset();
    object.lower_scale_packet.reset();
    object.material_field.reset();
    object.volume_mesh.reset();
    object.continuum_state.reset();
    object.surface_state.reset();
    object.stage = ObjectStage::GeometryOnly;

    const bool has_warning = !geometry_validation.warnings.empty();
    auto acknowledgement = make_geometry_acknowledgement(
        has_warning ? AcknowledgementStatus::AcceptedWithWarning
                    : AcknowledgementStatus::Accepted,
        "replace_design_geometry",
        id,
        next_revision,
        true,
        has_warning
            ? std::optional<GeometryDiagnostic>{geometry_validation.warnings.front()}
            : std::nullopt);
    return {record_acknowledgement_locked(std::move(acknowledgement))};
}

bool ObjectWorld::mark_failed(ObjectId id) {
    std::unique_lock lock(mutex_);
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "mark_failed",
            id,
            kInvalidGeometryRevision,
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleObjectId,
                DiagnosticSeverity::Fatal,
                "Object ID is stale or destroyed"));
        const auto recorded = record_acknowledgement_locked(std::move(acknowledgement));
        (void)recorded;
        return false;
    }
    found->second->stage = ObjectStage::Failed;
    auto acknowledgement = make_geometry_acknowledgement(
        AcknowledgementStatus::Accepted,
        "mark_failed",
        id,
        found->second->geometry_revision,
        true);
    const auto recorded = record_acknowledgement_locked(std::move(acknowledgement));
    (void)recorded;
    return true;
}

bool ObjectWorld::destroy(ObjectId id) {
    return destroy_checked(id).accepted();
}

ObjectOperationResult ObjectWorld::destroy_checked(ObjectId id) {
    std::unique_lock lock(mutex_);
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "destroy",
            id,
            kInvalidGeometryRevision,
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleObjectId,
                DiagnosticSeverity::Fatal,
                "Object ID is stale or destroyed"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    const GeometryRevision revision = found->second->geometry_revision;
    objects_.erase(found);
    auto acknowledgement = make_geometry_acknowledgement(
        AcknowledgementStatus::Accepted, "destroy", id, revision, true);
    return {record_acknowledgement_locked(std::move(acknowledgement))};
}

ObjectSnapshotPtr ObjectWorld::snapshot(ObjectId id) const {
    std::shared_lock lock(mutex_);
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : make_snapshot(*found->second);
}

SnapshotLookupResult ObjectWorld::snapshot_checked(
    ObjectId id,
    std::optional<GeometryRevision> required_revision) {
    std::unique_lock lock(mutex_);
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "snapshot",
            id,
            required_revision.value_or(kInvalidGeometryRevision),
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleObjectId,
                DiagnosticSeverity::Fatal,
                "Object ID is stale or destroyed"));
        return {nullptr, record_acknowledgement_locked(std::move(acknowledgement))};
    }
    const DynamicObject& object = *found->second;
    if (required_revision && *required_revision != object.geometry_revision) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "snapshot",
            id,
            *required_revision,
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleSnapshot,
                DiagnosticSeverity::Fatal,
                "Requested geometry revision is stale",
                "current revision is " + std::to_string(object.geometry_revision.value)));
        return {nullptr, record_acknowledgement_locked(std::move(acknowledgement))};
    }
    return {
        make_snapshot(object),
        make_geometry_acknowledgement(
            AcknowledgementStatus::Accepted,
            "snapshot",
            id,
            object.geometry_revision,
            false)
    };
}

ObjectOperationResult ObjectWorld::inspect_downstream_slot(
    ObjectId id,
    DownstreamSlot slot,
    std::optional<GeometryRevision> required_revision) {
    std::unique_lock lock(mutex_);
    const auto found = objects_.find(id);
    if (found == objects_.end()) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "inspect_downstream_slot",
            id,
            required_revision.value_or(kInvalidGeometryRevision),
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleObjectId,
                DiagnosticSeverity::Fatal,
                "Object ID is stale or destroyed"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    const DynamicObject& object = *found->second;
    if (required_revision && *required_revision != object.geometry_revision) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Stale,
            "inspect_downstream_slot",
            id,
            *required_revision,
            false,
            diagnostic(
                GeometryDiagnosticCode::StaleSnapshot,
                DiagnosticSeverity::Fatal,
                "Downstream slot request targets a stale geometry revision"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    if (!slot_present(object, slot)) {
        auto acknowledgement = make_geometry_acknowledgement(
            AcknowledgementStatus::Unsupported,
            "inspect_downstream_slot",
            id,
            object.geometry_revision,
            false,
            diagnostic(
                GeometryDiagnosticCode::UnsupportedDownstreamSlot,
                DiagnosticSeverity::Fatal,
                "Downstream slot is absent or unsupported",
                std::string(downstream_slot_name(slot)) + " has no accepted payload"));
        return {record_acknowledgement_locked(std::move(acknowledgement))};
    }
    return {
        make_geometry_acknowledgement(
            AcknowledgementStatus::Accepted,
            "inspect_downstream_slot",
            id,
            object.geometry_revision,
            false)
    };
}

std::vector<ObjectSnapshotPtr> ObjectWorld::snapshots() const {
    std::shared_lock lock(mutex_);
    std::vector<ObjectSnapshotPtr> result;
    result.reserve(objects_.size());
    for (const auto& [id, object] : objects_) {
        (void)id;
        result.push_back(make_snapshot(*object));
    }
    return result;
}

std::vector<GeometryAcknowledgement> ObjectWorld::acknowledgements() const {
    std::shared_lock lock(mutex_);
    return acknowledgement_log_;
}

std::size_t ObjectWorld::size() const {
    std::shared_lock lock(mutex_);
    return objects_.size();
}

ObjectSnapshotPtr ObjectWorld::make_snapshot(const DynamicObject& object) {
    auto result = std::make_shared<ObjectSnapshot>();
    result->id = object.id;
    result->name = object.name;
    result->stage = object.stage;
    result->geometry_revision = object.geometry_revision;
    result->geometry = object.geometry;
    result->transform = object.transform;
    result->bounds = TransformedSpatialQuery(result->geometry, result->transform).bounds();
    result->has_formed_geometry = object.history.formed_geometry.has_value();
    result->has_analysis_geometry = object.history.analysis_geometry.has_value();
    result->has_sample_cloud = static_cast<bool>(object.sample_cloud);
    result->has_lower_scale_packet = static_cast<bool>(object.lower_scale_packet);
    result->has_material_field = static_cast<bool>(object.material_field);
    result->has_volume_mesh = static_cast<bool>(object.volume_mesh);
    result->has_continuum_state = static_cast<bool>(object.continuum_state);
    result->has_surface_state = static_cast<bool>(object.surface_state);
    return result;
}

GeometryAcknowledgement ObjectWorld::record_acknowledgement_locked(
    GeometryAcknowledgement acknowledgement) {
    acknowledgement.sequence = next_acknowledgement_sequence_++;
    acknowledgement.comparison_key = acknowledgement_key(acknowledgement);
    acknowledgement_log_.push_back(acknowledgement);
    if (acknowledgement_log_.size() > kMaxAcknowledgements) {
        acknowledgement_log_.erase(acknowledgement_log_.begin());
    }
    return acknowledgement;
}

} // namespace vsepr::multiscale
