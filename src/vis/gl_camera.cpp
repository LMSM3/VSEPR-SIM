/**
 * gl_camera.cpp
 * Implementation of Camera and CameraController.
 */

#include "gl_camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace vsepr {
namespace vis {

// ============================================================================
// Camera
// ============================================================================

Camera::Camera()
    : position_(0.0f, 0.0f, 5.0f)
    , target_(0.0f, 0.0f, 0.0f)
    , up_(0.0f, 1.0f, 0.0f)
    , fov_(45.0f)
    , aspect_(1.0f)
    , near_(0.1f)
    , far_(1000.0f)
    , ortho_width_(10.0f)
    , ortho_height_(10.0f)
    , mode_(CameraMode::PERSPECTIVE)
{}

void Camera::set_perspective(float fov, float aspect, float near, float far) {
    fov_    = fov;
    aspect_ = aspect;
    near_   = near;
    far_    = far;
    mode_   = CameraMode::PERSPECTIVE;
}

void Camera::set_orthographic(float width, float height, float near, float far) {
    ortho_width_  = width;
    ortho_height_ = height;
    near_ = near;
    far_  = far;
    mode_ = CameraMode::ORTHOGRAPHIC;
}

void Camera::set_isometric(float size, float near, float far) {
    ortho_width_  = size * aspect_;
    ortho_height_ = size;
    near_ = near;
    far_  = far;
    mode_ = CameraMode::ISOMETRIC;
}

void Camera::set_position(const glm::vec3& pos) {
    position_ = pos;
}

void Camera::set_target(const glm::vec3& target) {
    target_ = target;
}

void Camera::set_up(const glm::vec3& up) {
    up_ = up;
}

void Camera::orbit(float delta_x, float delta_y, float /*distance*/) {
    glm::vec3 dir = position_ - target_;
    float r = glm::length(dir);
    if (r < 1e-6f) r = 5.0f;

    float theta = std::atan2(dir.x, dir.z) + delta_x;
    float phi   = std::acos(std::clamp(dir.y / r, -1.0f, 1.0f)) + delta_y;
    phi = std::clamp(phi, 0.01f, 3.14f);

    position_ = target_ + r * glm::vec3(
        std::sin(phi) * std::sin(theta),
        std::cos(phi),
        std::sin(phi) * std::cos(theta)
    );
}

void Camera::pan(float delta_x, float delta_y) {
    glm::vec3 right  = get_right();
    glm::vec3 up_dir = glm::normalize(glm::cross(right, get_forward()));
    glm::vec3 offset = right * (-delta_x) + up_dir * delta_y;
    position_ += offset;
    target_   += offset;
}

void Camera::zoom(float delta) {
    glm::vec3 dir = position_ - target_;
    float d = glm::length(dir) - delta;
    if (d < 0.5f) d = 0.5f;
    position_ = target_ + glm::normalize(dir) * d;
}

void Camera::reset() {
    position_ = glm::vec3(0.0f, 0.0f, 5.0f);
    target_   = glm::vec3(0.0f, 0.0f, 0.0f);
    up_       = glm::vec3(0.0f, 1.0f, 0.0f);
    fov_      = 45.0f;
    mode_     = CameraMode::PERSPECTIVE;
}

glm::mat4 Camera::get_view_matrix() const {
    return glm::lookAt(position_, target_, up_);
}

glm::mat4 Camera::get_projection_matrix() const {
    if (mode_ == CameraMode::PERSPECTIVE) {
        return glm::perspective(glm::radians(fov_), aspect_, near_, far_);
    }
    float hw = ortho_width_  * 0.5f;
    float hh = ortho_height_ * 0.5f;
    return glm::ortho(-hw, hw, -hh, hh, near_, far_);
}

glm::mat4 Camera::get_view_projection() const {
    return get_projection_matrix() * get_view_matrix();
}

glm::vec3 Camera::get_forward() const {
    return glm::normalize(target_ - position_);
}

glm::vec3 Camera::get_right() const {
    return glm::normalize(glm::cross(get_forward(), up_));
}

float Camera::get_distance() const {
    return glm::length(position_ - target_);
}

glm::vec3 Camera::get_ray_from_screen(float screen_x, float screen_y,
                                      float width, float height) {
    float ndc_x = (2.0f * screen_x) / width  - 1.0f;
    float ndc_y =  1.0f - (2.0f * screen_y) / height;
    glm::vec4 ray_clip(ndc_x, ndc_y, -1.0f, 1.0f);
    glm::vec4 ray_eye = glm::inverse(get_projection_matrix()) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(glm::inverse(get_view_matrix()) * ray_eye));
}

void Camera::set_aspect_ratio(float aspect) {
    aspect_ = aspect;
    if (mode_ == CameraMode::ISOMETRIC) {
        ortho_width_ = ortho_height_ * aspect;
    }
}

void Camera::update_matrices() {
    // Matrices are computed on demand; stub reserved for future caching.
}

// ============================================================================
// CameraController
// ============================================================================

CameraController::CameraController(Camera& camera)
    : camera_(camera)
{}

void CameraController::on_mouse_move(float x, float y) {
    float dx = x - last_mouse_x_;
    float dy = y - last_mouse_y_;
    last_mouse_x_ = x;
    last_mouse_y_ = y;

    if (left_button_down_) {
        camera_.orbit(dx * orbit_speed_, dy * orbit_speed_, 5.0f);
    }
    if (right_button_down_) {
        camera_.pan(dx * pan_speed_, dy * pan_speed_);
    }
}

void CameraController::on_mouse_button(int button, bool pressed) {
    if      (button == 0) left_button_down_   = pressed;
    else if (button == 1) right_button_down_  = pressed;
    else if (button == 2) middle_button_down_ = pressed;
}

void CameraController::on_mouse_wheel(float delta) {
    camera_.zoom(delta * zoom_speed_ * camera_.get_distance());
}

void CameraController::on_key(int /*key*/, bool /*pressed*/) {
    // Reserved for future keyboard navigation.
}

void CameraController::set_sensitivity(float orbit_speed, float pan_speed, float zoom_speed) {
    orbit_speed_ = orbit_speed;
    pan_speed_   = pan_speed;
    zoom_speed_  = zoom_speed;
}

} // namespace vis
} // namespace vsepr
