#pragma once
/**
 * ViewportWidget.h — QOpenGLWidget hosting the molecular renderer
 *
 * Render modes:  Ball-and-Stick | Space-Fill (CPK) | Sticks | Wireframe
 * Overlays:      Element labels | Unit-cell box | XYZ corner axes
 * Interaction:   Orbit (LMB-drag) | Pan (RMB/MMB-drag) | Zoom (wheel)
 *                Click (LMB, no drag) → pick nearest atom
 * Playback:      Frame stepping / auto-play for multi-frame documents
 */

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <QFont>
#include "scene/SceneDocument.h"
#include <memory>
#include <vector>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
enum class RenderStyle {
    BallAndStick,  ///< Scaled covalent radii + bond cylinders (default)
    SpaceFill,     ///< Van-der-Waals radii, no bonds
    Sticks,        ///< Thin spheres + slim bond cylinders
    Wireframe      ///< GL_LINE polygon mode
};

// ---------------------------------------------------------------------------
class ViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit ViewportWidget(QWidget* parent = nullptr);
    ~ViewportWidget() override;

    // --- Scene ---
    void setFrame(const scene::FrameData& frame);
    void setDocument(std::shared_ptr<scene::SceneDocument> doc);
    const scene::FrameData* currentFrame() const;

    // --- Camera ---
    void resetCamera();
    void fitCamera();
    void setWireframe(bool on);  // shortcut → setRenderStyle(Wireframe)

    // --- Render mode ---
    void setRenderStyle(RenderStyle s);
    RenderStyle renderStyle() const { return render_style_; }

    // --- Overlays ---
    void setShowLabels(bool on)  { show_labels_ = on; update(); }
    void setShowBox(bool on)     { show_box_    = on; update(); }
    void setShowAxes(bool on)    { show_axes_   = on; update(); }
    bool showLabels() const { return show_labels_; }
    bool showBox()    const { return show_box_;    }
    bool showAxes()   const { return show_axes_;   }

    // --- Trajectory ---
    void setFrameIndex(int idx);
    int  frameIndex() const  { return current_frame_idx_; }
    int  frameCount() const;
    void play();
    void pause();
    bool isPlaying() const;
    void stepForward();
    void stepBack();

    // --- Selection ---
    int  selectedAtom() const { return selected_atom_; }
    void clearSelection();

signals:
    void frameChanged();
    void frameIndexChanged(int idx, int total);
    void atomSelected(int atomIdx);  // -1 = deselected

protected:
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e)        override;
    void keyPressEvent(QKeyEvent* e)       override;

private slots:
    void onPlayTimer();

private:
    // ---- Scene ----
    std::shared_ptr<scene::SceneDocument> doc_;
    int current_frame_idx_ = 0;

    // ---- Camera (orbit model) ----
    float cam_theta_  = 0.4f;
    float cam_phi_    = 0.3f;
    float cam_dist_   = 15.0f;
    float cam_fov_    = 45.0f;
    scene::Vec3d cam_target_{0, 0, 0};

    bool   dragging_    = false;
    bool   mouse_moved_ = false;
    QPoint last_mouse_;
    QPoint press_pos_;
    Qt::MouseButton drag_button_ = Qt::NoButton;

    // ---- Render state ----
    RenderStyle render_style_ = RenderStyle::BallAndStick;
    bool show_labels_ = false;
    bool show_box_    = true;
    bool show_axes_   = true;

    // ---- Selection ----
    int selected_atom_ = -1;

    // ---- Playback ----
    QTimer* play_timer_;
    int     playback_fps_ = 10;

    // ---- GL resources — atoms/bonds (Blinn-Phong) ----
    GLuint sphere_vao_ = 0, sphere_vbo_ = 0, sphere_ebo_ = 0;
    int    sphere_idx_count_ = 0;
    GLuint cyl_vao_ = 0, cyl_vbo_ = 0, cyl_ebo_ = 0;
    int    cyl_idx_count_ = 0;
    GLuint shader_prog_ = 0;

    // ---- GL resources — lines (cell box, axes) ----
    GLuint line_vao_ = 0, line_vbo_ = 0;
    GLuint line_shader_ = 0;

    // ---- GL resources — gradient background ----
    GLuint bg_vao_ = 0, bg_vbo_ = 0;
    GLuint bg_shader_ = 0;

    // ---- Geometry builders ----
    void buildSphereMesh(int subdivisions);
    void buildCylinderMesh(int segments);
    void buildLineBuffer();
    void buildBackgroundQuad();

    // ---- Shader compile ----
    GLuint compileShader(const char* vert, const char* frag);

    // ---- Draw primitives ----
    void drawBackground();
    void drawAtoms(const scene::FrameData& f);
    void drawBonds(const scene::FrameData& f);
    void drawCellBox(const scene::BoxInfo& box);
    void drawAxesOverlay(QPainter& p);
    void drawLabels(QPainter& p, const scene::FrameData& f);
    void drawSelectionHighlight(const scene::Vec3d& pos, float radius);
    void drawSphere(const scene::Vec3d& center, float radius,
                    float r, float g, float b);
    void drawCylinder(const scene::Vec3d& a, const scene::Vec3d& b,
                      float radius, float r, float g, float bl);
    void drawLines(const std::vector<float>& xyz,
                   float r, float g, float b, float lineWidth = 1.5f);

    // ---- Camera math ----
    void computeViewMatrix(float* m) const;
    void computeProjMatrix(float* m, float aspect) const;
    QPointF worldToScreen(const scene::Vec3d& pos) const;
    float   camEyeX() const;
    float   camEyeY() const;
    float   camEyeZ() const;

    // ---- Atom pick ----
    void pickAtom(int screenX, int screenY);

    // ---- Element lookup ----
    float atomDisplayRadius(int Z) const;
    static void  atomColor(int Z, float& r, float& g, float& b);
    static float atomRadius(int Z);     // covalent
    static float atomVdwRadius(int Z);
};
