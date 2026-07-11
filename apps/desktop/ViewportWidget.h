#pragma once
/**
 * ViewportWidget.h — QOpenGLWidget hosting the molecular renderer
 *
 * Renders from scene::FrameData exclusively.
 * Does NOT know about atomistic::State or any engine type.
 *
 * Camera:
 *   Left-drag    — orbit
 *   Right/Mid    — pan
 *   Scroll       — zoom (cursor-tracked)
 *   F            — fit camera to current frame
 *   R            — reset camera to default
 *   W            — toggle wireframe
 *
 * Screenshot:
 *   grabScreenshot(path) — saves the current GL framebuffer to file.
 *   Called by MainWindow::onFileExportImage and the --screenshot CLI flag.
 */

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
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
    void fitCamera();            // auto-center + auto-distance (accounts for atom radii)
    void setWireframe(bool on);
    bool wireframe() const { return wireframe_; }

    // --- Screenshot ---
    // Saves the current GL framebuffer to 'path'.  Returns true on success.
    // Format is inferred from the file extension (.png, .jpg, .bmp).
    // Safe to call from outside the paint loop; forces a render via repaint().
    bool grabScreenshot(const QString& path);

signals:
    void frameChanged();         // emitted when the displayed frame changes

protected:
    // QOpenGLWidget overrides
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // Input
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    // ---- Scene data (canonical) ----
    std::shared_ptr<scene::SceneDocument> doc_;
    int current_frame_idx_ = 0;

    // ---- Camera state (orbit model) ----
    float cam_theta_   = 0.4f;   // azimuthal angle (radians)
    float cam_phi_     = 0.3f;   // polar angle (radians)
    float cam_dist_    = 15.0f;  // distance from target (Å)
    float cam_fov_     = 45.0f;  // vertical field of view (degrees)
    scene::Vec3d cam_target_{0, 0, 0};

    bool   dragging_    = false;
    QPoint last_mouse_;
    Qt::MouseButton drag_button_ = Qt::NoButton;

    bool wireframe_ = false;

    // ---- Cached view/proj matrices (rebuilt once per resize or camera move) ----
    // Avoids recomputing per draw-call inside drawSphere/drawCylinder.
    float cached_view_[16]{};
    float cached_proj_[16]{};
    float cached_vp_[16]{};      // proj × view
    float cached_cam_pos_[3]{};  // eye position in world space
    bool  matrices_dirty_ = true;
    void  rebuildMatrices();

    // ---- GL resources ----
    GLuint sphere_vao_ = 0;
    GLuint sphere_vbo_ = 0;
    GLuint sphere_ebo_ = 0;
    int    sphere_idx_count_ = 0;

    GLuint cyl_vao_ = 0;
    GLuint cyl_vbo_ = 0;
    GLuint cyl_ebo_ = 0;
    int    cyl_idx_count_ = 0;

    // Cached uniform locations (set once after shader link)
    GLint uloc_mvp_       = -1;
    GLint uloc_model_     = -1;
    GLint uloc_normal_    = -1;
    GLint uloc_color_     = -1;
    GLint uloc_cam_pos_   = -1;

    GLuint shader_prog_ = 0;

    // ---- Helpers ----
    void buildSphereMesh(int subdivisions);
    void buildCylinderMesh(int segments);
    GLuint compileShader(const char* vert, const char* frag);
    void cacheUniformLocations();

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
