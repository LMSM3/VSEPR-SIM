#pragma once
/**
 * ViewportWidget.h — QOpenGLWidget hosting the molecular renderer
 *
 * Renders from scene::FrameData exclusively.
 * Does NOT know about atomistic::State or any engine type.
 *
 * Handles camera orbit/pan/zoom through mouse events.
 */

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QString>
#include <QTimer>

#include "scene/SceneDocument.h"

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>

class ViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    // --- Scene data (canonical model) ---
    void setFrame(const scene::FrameData& frame);
    void setDocument(std::shared_ptr<scene::SceneDocument> doc);
    const scene::FrameData* currentFrame() const;

    // --- Camera ---
    void resetCamera();
    void fitCamera();   // auto-center + auto-distance to fit molecule
    void setWireframe(bool on);

signals:
    void frameChanged();  // emitted when the displayed frame changes

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // Mouse interaction
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    // ---- Scene data (canonical) ----
    std::shared_ptr<scene::SceneDocument> doc_;
    int current_frame_idx_ = 0;

    // ---- Camera state (orbit model) ----
    float cam_theta_   = 0.4f;   // azimuthal (radians)
    float cam_phi_     = 0.3f;   // polar
    float cam_dist_    = 15.0f;  // distance from target
    float cam_fov_     = 45.0f;  // degrees
    scene::Vec3d cam_target_{0, 0, 0};

    bool   dragging_    = false;
    QPoint last_mouse_;
    Qt::MouseButton drag_button_ = Qt::NoButton;

    bool wireframe_ = false;

    // ---- GL resources ----
    GLuint sphere_vao_ = 0;
    GLuint sphere_vbo_ = 0;
    GLuint sphere_ebo_ = 0;
    int    sphere_idx_count_ = 0;

    GLuint cyl_vao_ = 0;
    GLuint cyl_vbo_ = 0;
    GLuint cyl_ebo_ = 0;
    int    cyl_idx_count_ = 0;

    GLuint shader_prog_ = 0;

    // ---- Helpers ----
    void buildSphereMesh(int subdivisions);
    void buildCylinderMesh(int segments);
    GLuint compileShader(const char* vert, const char* frag);
    void drawSphere(const scene::Vec3d& center, float radius,
                    float r, float g, float b);
    void drawCylinder(const scene::Vec3d& a, const scene::Vec3d& b,
                      float radius, float r, float g, float bl);

    // Color / radius lookup
    static void atomColor(int Z, float& r, float& g, float& b);
    static float atomRadius(int Z);

    void computeViewMatrix(float* m) const;
    void computeProjMatrix(float* m, float aspect) const;
};
