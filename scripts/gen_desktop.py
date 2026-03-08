#!/usr/bin/env python3
"""Generate expanded desktop files for v2.8 3D visualization milestone."""
import os, sys
ROOT = "/mnt/c/Users/Liam/Desktop/vsepr-sim"
D    = ROOT + "/apps/desktop"

files = {}

# ===========================================================================
# ViewportWidget.h
# ===========================================================================
files[D + "/ViewportWidget.h"] = r'''#pragma once
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
'''

# ===========================================================================
# ViewportWidget.cpp  (full rewrite — keeps all existing geometry/math,
#                      adds new features on top)
# ===========================================================================
files[D + "/ViewportWidget.cpp"] = r'''#include "ViewportWidget.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <array>
#include <map>
#include <limits>

// ============================================================================
// Element data — CPK colors + covalent radii + VDW radii
// ============================================================================
namespace {
struct ElemInfo { float r,g,b; float cov; float vdw; };
constexpr int NUM_ELEM = 54;
const ElemInfo ELEM[NUM_ELEM] = {
    {0.90f,0.90f,0.90f,0.31f,1.20f}, // 1  H
    {0.85f,1.00f,1.00f,0.28f,1.40f}, // 2  He
    {0.80f,0.50f,1.00f,1.28f,1.82f}, // 3  Li
    {0.76f,1.00f,0.00f,0.96f,1.53f}, // 4  Be
    {1.00f,0.71f,0.71f,0.84f,1.92f}, // 5  B
    {0.20f,0.20f,0.20f,0.76f,1.70f}, // 6  C
    {0.19f,0.31f,0.97f,0.71f,1.55f}, // 7  N
    {0.90f,0.05f,0.05f,0.66f,1.52f}, // 8  O
    {0.56f,0.88f,0.31f,0.57f,1.47f}, // 9  F
    {0.70f,0.89f,0.96f,0.58f,1.54f}, // 10 Ne
    {0.67f,0.36f,0.95f,1.66f,2.27f}, // 11 Na
    {0.54f,1.00f,0.00f,1.41f,1.73f}, // 12 Mg
    {0.75f,0.65f,0.65f,1.21f,1.84f}, // 13 Al
    {0.94f,0.78f,0.63f,1.11f,2.10f}, // 14 Si
    {1.00f,0.50f,0.00f,1.07f,1.80f}, // 15 P
    {1.00f,1.00f,0.19f,1.05f,1.80f}, // 16 S
    {0.12f,0.94f,0.12f,1.02f,1.75f}, // 17 Cl
    {0.50f,0.82f,0.89f,1.06f,1.88f}, // 18 Ar
    {0.56f,0.25f,0.83f,2.03f,2.75f}, // 19 K
    {0.24f,1.00f,0.00f,1.76f,2.31f}, // 20 Ca
    {0.90f,0.90f,0.90f,1.70f,2.11f}, // 21 Sc
    {0.75f,0.76f,0.78f,1.60f,2.00f}, // 22 Ti
    {0.65f,0.65f,0.67f,1.53f,2.00f}, // 23 V
    {0.54f,0.60f,0.78f,1.39f,2.00f}, // 24 Cr
    {0.61f,0.48f,0.78f,1.39f,2.00f}, // 25 Mn
    {0.88f,0.40f,0.20f,1.32f,2.00f}, // 26 Fe
    {0.94f,0.56f,0.63f,1.26f,2.00f}, // 27 Co
    {0.31f,0.82f,0.31f,1.24f,1.63f}, // 28 Ni
    {0.78f,0.50f,0.20f,1.32f,1.40f}, // 29 Cu
    {0.49f,0.50f,0.69f,1.22f,1.39f}, // 30 Zn
    {0.76f,0.56f,0.56f,1.22f,1.87f}, // 31 Ga
    {0.40f,0.56f,0.56f,1.20f,2.11f}, // 32 Ge
    {0.74f,0.50f,0.89f,1.19f,1.85f}, // 33 As
    {1.00f,0.63f,0.00f,1.20f,1.90f}, // 34 Se
    {0.65f,0.16f,0.16f,1.20f,1.85f}, // 35 Br
    {0.36f,0.72f,0.82f,1.16f,2.02f}, // 36 Kr
    {0.44f,0.18f,0.69f,2.20f,3.03f}, // 37 Rb
    {0.00f,1.00f,0.00f,1.95f,2.49f}, // 38 Sr
    {0.58f,1.00f,1.00f,1.90f,2.00f}, // 39 Y
    {0.58f,0.88f,0.88f,1.75f,2.00f}, // 40 Zr
    {0.45f,0.76f,0.79f,1.64f,2.00f}, // 41 Nb
    {0.33f,0.71f,0.71f,1.54f,2.00f}, // 42 Mo
    {0.23f,0.62f,0.62f,1.47f,2.00f}, // 43 Tc
    {0.14f,0.56f,0.56f,1.46f,2.00f}, // 44 Ru
    {0.04f,0.49f,0.55f,1.42f,2.00f}, // 45 Rh
    {0.00f,0.41f,0.52f,1.39f,1.63f}, // 46 Pd
    {0.75f,0.75f,0.75f,1.45f,1.72f}, // 47 Ag
    {1.00f,0.85f,0.56f,1.44f,1.58f}, // 48 Cd
    {0.65f,0.46f,0.45f,1.42f,1.93f}, // 49 In
    {0.40f,0.50f,0.50f,1.39f,2.17f}, // 50 Sn
    {0.62f,0.39f,0.71f,1.39f,2.06f}, // 51 Sb
    {0.83f,0.48f,0.00f,1.38f,2.06f}, // 52 Te
    {0.58f,0.00f,0.58f,1.39f,1.98f}, // 53 I
    {0.26f,0.62f,0.69f,1.40f,2.16f}, // 54 Xe
};

// Element symbol table (Z=1..54)
const char* ELEM_SYM[NUM_ELEM] = {
    "H","He","Li","Be","B","C","N","O","F","Ne",
    "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
    "Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
    "Ga","Ge","As","Se","Br","Kr","Rb","Sr","Y","Zr",
    "Nb","Mo","Tc","Ru","Rh","Pd","Ag","Cd","In","Sn",
    "Sb","Te","I","Xe"
};
} // namespace

void ViewportWidget::atomColor(int Z, float& r, float& g, float& b)
{
    if (Z>=1 && Z<=NUM_ELEM) { r=ELEM[Z-1].r; g=ELEM[Z-1].g; b=ELEM[Z-1].b; }
    else                     { r=0.6f; g=0.6f; b=0.6f; }
}
float ViewportWidget::atomRadius(int Z)
{ return (Z>=1 && Z<=NUM_ELEM) ? ELEM[Z-1].cov : 1.5f; }

float ViewportWidget::atomVdwRadius(int Z)
{ return (Z>=1 && Z<=NUM_ELEM) ? ELEM[Z-1].vdw : 1.7f; }

float ViewportWidget::atomDisplayRadius(int Z) const
{
    switch (render_style_) {
        case RenderStyle::SpaceFill:  return atomVdwRadius(Z);
        case RenderStyle::Sticks:     return 0.10f;
        case RenderStyle::BallAndStick:
        default:                      return atomRadius(Z) * 0.4f;
    }
}

// ============================================================================
// Construction / destruction
// ============================================================================
ViewportWidget::ViewportWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    play_timer_ = new QTimer(this);
    connect(play_timer_, &QTimer::timeout, this, &ViewportWidget::onPlayTimer);
}

ViewportWidget::~ViewportWidget()
{
    makeCurrent();
    if (sphere_vao_)  glDeleteVertexArrays(1, &sphere_vao_);
    if (sphere_vbo_)  glDeleteBuffers(1, &sphere_vbo_);
    if (sphere_ebo_)  glDeleteBuffers(1, &sphere_ebo_);
    if (cyl_vao_)     glDeleteVertexArrays(1, &cyl_vao_);
    if (cyl_vbo_)     glDeleteBuffers(1, &cyl_vbo_);
    if (cyl_ebo_)     glDeleteBuffers(1, &cyl_ebo_);
    if (line_vao_)    glDeleteVertexArrays(1, &line_vao_);
    if (line_vbo_)    glDeleteBuffers(1, &line_vbo_);
    if (bg_vao_)      glDeleteVertexArrays(1, &bg_vao_);
    if (bg_vbo_)      glDeleteBuffers(1, &bg_vbo_);
    if (shader_prog_) glDeleteProgram(shader_prog_);
    if (line_shader_) glDeleteProgram(line_shader_);
    if (bg_shader_)   glDeleteProgram(bg_shader_);
    doneCurrent();
}

// ============================================================================
// Shaders
// ============================================================================
// --- Blinn-Phong (atoms + bonds) ---
static const char* VERT_SRC = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;
out vec3 vWorldPos;
out vec3 vNormal;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos  = world.xyz;
    vNormal    = normalize(uNormalMat * aNorm);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FRAG_SRC = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
uniform vec3 uColor;
uniform vec3 uCamPos;
out vec4 FragColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.6, 0.8, 0.5));
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);
    float ambient  = 0.15;
    float diffuse  = max(dot(N,L), 0.0) * 0.65;
    float spec     = pow(max(dot(N,H), 0.0), 64.0) * 0.35;
    vec3 L2 = normalize(vec3(-0.4, 0.3, -0.6));
    float fill = max(dot(N,L2), 0.0) * 0.20;
    float rim  = pow(1.0 - max(dot(N,V), 0.0), 3.0) * 0.10;
    vec3 color = uColor*(ambient+diffuse+fill) + vec3(spec+rim);
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, 1.0);
}
)";

// --- Flat color (lines: cell box, axes) ---
static const char* VERT_LINE = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";
static const char* FRAG_LINE = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor, 1.0); }
)";

// --- Per-vertex color (gradient background) ---
static const char* VERT_BG = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aColor;
out vec3 vColor;
void main() { vColor = aColor; gl_Position = vec4(aPos, 0.9999, 1.0); }
)";
static const char* FRAG_BG = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)";

GLuint ViewportWidget::compileShader(const char* vert, const char* frag)
{
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { char l[512]; glGetShaderInfoLog(s, 512, nullptr, l); qWarning("Shader: %s", l); }
        return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vert);
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char l[512]; glGetProgramInfoLog(p, 512, nullptr, l); qWarning("Link: %s", l); }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

// ============================================================================
// Geometry — icosphere
// ============================================================================
void ViewportWidget::buildSphereMesh(int subdivisions)
{
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<float> verts;
    std::vector<unsigned int> idxs;

    auto addV = [&](float x, float y, float z) {
        float len = std::sqrt(x*x+y*y+z*z);
        verts.push_back(x/len); verts.push_back(y/len); verts.push_back(z/len);
        verts.push_back(x/len); verts.push_back(y/len); verts.push_back(z/len);
        return (unsigned int)(verts.size()/6 - 1);
    };
    addV(-1, t,0); addV(1, t,0); addV(-1,-t,0); addV(1,-t,0);
    addV(0,-1, t); addV(0, 1, t); addV(0,-1,-t); addV(0, 1,-t);
    addV( t,0,-1); addV( t,0, 1); addV(-t,0,-1); addV(-t,0, 1);

    unsigned int faces[] = {
        0,11,5,0,5,1,0,1,7,0,7,10,0,10,11,
        1,5,9,5,11,4,11,10,2,10,7,6,7,1,8,
        3,9,4,3,4,2,3,2,6,3,6,8,3,8,9,
        4,9,5,2,4,11,6,2,10,8,6,7,9,8,1
    };
    for (auto f : faces) idxs.push_back(f);

    std::map<uint64_t, unsigned int> cache;
    auto midpoint = [&](unsigned int a, unsigned int b) -> unsigned int {
        uint64_t key = (uint64_t)std::min(a,b)<<32 | std::max(a,b);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        float mx=(verts[a*6]+verts[b*6])/2, my=(verts[a*6+1]+verts[b*6+1])/2, mz=(verts[a*6+2]+verts[b*6+2])/2;
        unsigned int idx = addV(mx,my,mz);
        return cache[key] = idx;
    };
    for (int s=0; s<subdivisions; ++s) {
        std::vector<unsigned int> ni;
        cache.clear();
        for (size_t i=0; i<idxs.size(); i+=3) {
            unsigned int a=idxs[i],b=idxs[i+1],c=idxs[i+2];
            unsigned int ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);
            ni.insert(ni.end(),{a,ab,ca,b,bc,ab,c,ca,bc,ab,bc,ca});
        }
        idxs = std::move(ni);
    }
    sphere_idx_count_ = (int)idxs.size();
    glGenVertexArrays(1,&sphere_vao_); glGenBuffers(1,&sphere_vbo_); glGenBuffers(1,&sphere_ebo_);
    glBindVertexArray(sphere_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.size()*sizeof(unsigned int), idxs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ============================================================================
// Geometry — cylinder
// ============================================================================
void ViewportWidget::buildCylinderMesh(int segments)
{
    std::vector<float> verts;
    std::vector<unsigned int> idxs;
    for (int i=0; i<=segments; ++i) {
        float a=2.0f*(float)M_PI*i/segments, cx=std::cos(a), cz=std::sin(a);
        verts.insert(verts.end(),{cx,0.0f,cz, cx,0.0f,cz});
        verts.insert(verts.end(),{cx,1.0f,cz, cx,0.0f,cz});
    }
    for (int i=0; i<segments; ++i) {
        unsigned int b=i*2,t=b+1,bn=(i+1)*2,tn=bn+1;
        idxs.insert(idxs.end(),{b,bn,t,t,bn,tn});
    }
    cyl_idx_count_ = (int)idxs.size();
    glGenVertexArrays(1,&cyl_vao_); glGenBuffers(1,&cyl_vbo_); glGenBuffers(1,&cyl_ebo_);
    glBindVertexArray(cyl_vao_);
    glBindBuffer(GL_ARRAY_BUFFER,cyl_vbo_);
    glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(float),verts.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,cyl_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,idxs.size()*sizeof(unsigned int),idxs.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ============================================================================
// Geometry — reusable line buffer (dynamic upload)
// ============================================================================
void ViewportWidget::buildLineBuffer()
{
    glGenVertexArrays(1,&line_vao_);
    glGenBuffers(1,&line_vbo_);
    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER,line_vbo_);
    glBufferData(GL_ARRAY_BUFFER, 512*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// ============================================================================
// Geometry — fullscreen gradient background quad
// ============================================================================
void ViewportWidget::buildBackgroundQuad()
{
    // Bottom: dark blue-grey  Top: slightly lighter
    float bg[] = {
        -1.0f,-1.0f,  0.13f,0.14f,0.16f,
         1.0f,-1.0f,  0.13f,0.14f,0.16f,
         1.0f, 1.0f,  0.20f,0.21f,0.24f,
        -1.0f,-1.0f,  0.13f,0.14f,0.16f,
         1.0f, 1.0f,  0.20f,0.21f,0.24f,
        -1.0f, 1.0f,  0.20f,0.21f,0.24f,
    };
    glGenVertexArrays(1,&bg_vao_);
    glGenBuffers(1,&bg_vbo_);
    glBindVertexArray(bg_vao_);
    glBindBuffer(GL_ARRAY_BUFFER,bg_vbo_);
    glBufferData(GL_ARRAY_BUFFER,sizeof(bg),bg,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ============================================================================
// initializeGL
// ============================================================================
void ViewportWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.16f,0.17f,0.19f,1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glLineWidth(1.5f);

    shader_prog_ = compileShader(VERT_SRC, FRAG_SRC);
    line_shader_ = compileShader(VERT_LINE, FRAG_LINE);
    bg_shader_   = compileShader(VERT_BG, FRAG_BG);

    buildSphereMesh(3);
    buildCylinderMesh(16);
    buildLineBuffer();
    buildBackgroundQuad();
}

void ViewportWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

// ============================================================================
// Matrix helpers (no GLM)
// ============================================================================
static void mat4_identity(float* m) {
    for (int i=0;i<16;++i) m[i]=0; m[0]=m[5]=m[10]=m[15]=1;
}
static void mat4_mul(float* out, const float* a, const float* b) {
    float t[16];
    for (int c=0;c<4;++c) for (int r=0;r<4;++r) {
        t[c*4+r]=0; for (int k=0;k<4;++k) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    for (int i=0;i<16;++i) out[i]=t[i];
}
static void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m); m[12]=x; m[13]=y; m[14]=z;
}
static void mat4_scale(float* m, float sx, float sy, float sz) {
    mat4_identity(m); m[0]=sx; m[5]=sy; m[10]=sz;
}

void ViewportWidget::computeViewMatrix(float* m) const
{
    float sp=std::sin(cam_phi_), cp=std::cos(cam_phi_);
    float st=std::sin(cam_theta_), ct=std::cos(cam_theta_);
    float ex=(float)cam_target_.x+cam_dist_*cp*st;
    float ey=(float)cam_target_.y+cam_dist_*sp;
    float ez=(float)cam_target_.z+cam_dist_*cp*ct;
    float fx=(float)cam_target_.x-ex, fy=(float)cam_target_.y-ey, fz=(float)cam_target_.z-ez;
    float fl=std::sqrt(fx*fx+fy*fy+fz*fz); fx/=fl; fy/=fl; fz/=fl;
    float rx=fy*1-fz*0, ry=fz*0-fx*1, rz=fx*0-fy*0;
    // right = f × (0,1,0)
    rx = fy*0-fz*1; ry = fz*0-fx*0; rz = fx*1-fy*0;
    float rl=std::sqrt(rx*rx+ry*ry+rz*rz); rx/=rl; ry/=rl; rz/=rl;
    float tux=ry*fz-rz*fy, tuy=rz*fx-rx*fz, tuz=rx*fy-ry*fx;
    mat4_identity(m);
    m[0]=rx; m[4]=ry; m[8]=rz;   m[12]=-(rx*ex+ry*ey+rz*ez);
    m[1]=tux;m[5]=tuy;m[9]=tuz;  m[13]=-(tux*ex+tuy*ey+tuz*ez);
    m[2]=-fx;m[6]=-fy;m[10]=-fz; m[14]=(fx*ex+fy*ey+fz*ez);
    m[3]=0; m[7]=0; m[11]=0; m[15]=1;
}

void ViewportWidget::computeProjMatrix(float* m, float aspect) const
{
    float fovRad=cam_fov_*(float)M_PI/180.0f;
    float f=1.0f/std::tan(fovRad/2.0f);
    float near=0.05f, far=500.0f;
    for (int i=0;i<16;++i) m[i]=0;
    m[0]=f/aspect; m[5]=f;
    m[10]=(far+near)/(near-far); m[11]=-1.0f;
    m[14]=2.0f*far*near/(near-far);
}

float ViewportWidget::camEyeX() const {
    return (float)cam_target_.x+cam_dist_*std::cos(cam_phi_)*std::sin(cam_theta_); }
float ViewportWidget::camEyeY() const {
    return (float)cam_target_.y+cam_dist_*std::sin(cam_phi_); }
float ViewportWidget::camEyeZ() const {
    return (float)cam_target_.z+cam_dist_*std::cos(cam_phi_)*std::cos(cam_theta_); }

QPointF ViewportWidget::worldToScreen(const scene::Vec3d& wp) const
{
    float view[16], proj[16], vpm[16];
    computeViewMatrix(view);
    computeProjMatrix(proj, (float)width() / std::max((float)height(),1.0f));
    mat4_mul(vpm, proj, view);
    float x=(float)wp.x, y=(float)wp.y, z=(float)wp.z;
    float clip[4] = {
        vpm[0]*x+vpm[4]*y+vpm[8]*z+vpm[12],
        vpm[1]*x+vpm[5]*y+vpm[9]*z+vpm[13],
        vpm[2]*x+vpm[6]*y+vpm[10]*z+vpm[14],
        vpm[3]*x+vpm[7]*y+vpm[11]*z+vpm[15]
    };
    if (std::abs(clip[3])<1e-6f) return {-1,-1};
    return { (clip[0]/clip[3]+1.0f)*0.5f*width(),
             (1.0f-clip[1]/clip[3])*0.5f*height() };
}

// ============================================================================
// Draw primitives — sphere
// ============================================================================
void ViewportWidget::drawSphere(const scene::Vec3d& center, float radius,
                                float r, float g, float b)
{
    float model[16],scale[16],trans[16],mvp[16],view[16],proj[16];
    mat4_translate(trans,(float)center.x,(float)center.y,(float)center.z);
    mat4_scale(scale,radius,radius,radius);
    mat4_mul(model,trans,scale);
    computeViewMatrix(view);
    computeProjMatrix(proj,(float)width()/std::max((float)height(),1.0f));
    float vp[16]; mat4_mul(vp,proj,view);
    mat4_mul(mvp,vp,model);
    float nm[9]={model[0],model[1],model[2],model[4],model[5],model[6],model[8],model[9],model[10]};
    float camPos[3]={camEyeX(),camEyeY(),camEyeZ()};
    glUseProgram(shader_prog_);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_,"uMVP"),1,GL_FALSE,mvp);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_,"uModel"),1,GL_FALSE,model);
    glUniformMatrix3fv(glGetUniformLocation(shader_prog_,"uNormalMat"),1,GL_FALSE,nm);
    glUniform3f(glGetUniformLocation(shader_prog_,"uColor"),r,g,b);
    glUniform3fv(glGetUniformLocation(shader_prog_,"uCamPos"),1,camPos);
    glBindVertexArray(sphere_vao_);
    glDrawElements(GL_TRIANGLES,sphere_idx_count_,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}

// ============================================================================
// Draw primitives — cylinder
// ============================================================================
void ViewportWidget::drawCylinder(const scene::Vec3d& a, const scene::Vec3d& b,
                                  float radius, float r, float g, float bl)
{
    scene::Vec3d d{b.x-a.x,b.y-a.y,b.z-a.z};
    float h=(float)scene::distance({0,0,0},d);
    if (h<1e-6f) return;
    float dx=(float)d.x/h,dy=(float)d.y/h,dz=(float)d.z/h;
    float ax=-dz, ay=0.0f, az=dx;
    float sinA=std::sqrt(ax*ax+az*az), cosA=dy;
    float model[16]; mat4_identity(model);
    if (sinA>1e-6f) {
        ax/=sinA; az/=sinA;
        float c=cosA,s=sinA,t2=1.0f-c;
        model[0]=t2*ax*ax+c;    model[4]=t2*ax*ay-s*az; model[8]=t2*ax*az+s*ay;
        model[1]=t2*ay*ax+s*az; model[5]=t2*ay*ay+c;    model[9]=t2*ay*az-s*ax;
        model[2]=t2*az*ax-s*ay; model[6]=t2*az*ay+s*ax; model[10]=t2*az*az+c;
    } else if (dy<0) { model[5]=-1.0f; }
    float sm[16]; mat4_identity(sm); sm[0]=radius; sm[5]=h; sm[10]=radius;
    float rs[16]; mat4_mul(rs,model,sm);
    rs[12]=(float)a.x; rs[13]=(float)a.y; rs[14]=(float)a.z;
    float view[16],proj[16],vp[16],mvp[16];
    computeViewMatrix(view);
    computeProjMatrix(proj,(float)width()/std::max((float)height(),1.0f));
    mat4_mul(vp,proj,view); mat4_mul(mvp,vp,rs);
    float nm[9]={rs[0],rs[1],rs[2],rs[4],rs[5],rs[6],rs[8],rs[9],rs[10]};
    float camPos[3]={camEyeX(),camEyeY(),camEyeZ()};
    glUseProgram(shader_prog_);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_,"uMVP"),1,GL_FALSE,mvp);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_,"uModel"),1,GL_FALSE,rs);
    glUniformMatrix3fv(glGetUniformLocation(shader_prog_,"uNormalMat"),1,GL_FALSE,nm);
    glUniform3f(glGetUniformLocation(shader_prog_,"uColor"),r,g,bl);
    glUniform3fv(glGetUniformLocation(shader_prog_,"uCamPos"),1,camPos);
    glBindVertexArray(cyl_vao_);
    glDrawElements(GL_TRIANGLES,cyl_idx_count_,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}

// ============================================================================
// Draw primitives — lines (dynamic VBO upload)
// ============================================================================
void ViewportWidget::drawLines(const std::vector<float>& xyz,
                               float r, float g, float b, float lineWidth)
{
    if (xyz.empty()) return;
    float view[16],proj[16],vp[16];
    computeViewMatrix(view);
    computeProjMatrix(proj,(float)width()/std::max((float)height(),1.0f));
    mat4_mul(vp,proj,view);
    glLineWidth(lineWidth);
    glUseProgram(line_shader_);
    glUniformMatrix4fv(glGetUniformLocation(line_shader_,"uMVP"),1,GL_FALSE,vp);
    glUniform3f(glGetUniformLocation(line_shader_,"uColor"),r,g,b);
    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER,line_vbo_);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(xyz.size()*sizeof(float)),xyz.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES,(GLint)0,(GLsizei)(xyz.size()/3));
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

// ============================================================================
// Draw — gradient background
// ============================================================================
void ViewportWidget::drawBackground()
{
    glDisable(GL_DEPTH_TEST);
    glUseProgram(bg_shader_);
    glBindVertexArray(bg_vao_);
    glDrawArrays(GL_TRIANGLES,0,6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// Draw — selection highlight (draw slightly-larger sphere in highlight color)
// ============================================================================
void ViewportWidget::drawSelectionHighlight(const scene::Vec3d& pos, float radius)
{
    drawSphere(pos, radius * 1.18f, 1.0f, 0.85f, 0.10f);
}

// ============================================================================
// Draw — atoms
// ============================================================================
void ViewportWidget::drawAtoms(const scene::FrameData& f)
{
    for (int i=0; i<(int)f.atoms.size(); ++i) {
        const auto& a = f.atoms[i];
        float r,g,b; atomColor(a.Z,r,g,b);
        float rad = atomDisplayRadius(a.Z);
        if (i == selected_atom_) drawSelectionHighlight(a.pos, rad);
        drawSphere(a.pos, rad, r, g, b);
    }
}

// ============================================================================
// Draw — bonds
// ============================================================================
void ViewportWidget::drawBonds(const scene::FrameData& f)
{
    if (render_style_ == RenderStyle::SpaceFill) return; // no bonds in CPK
    float brad = (render_style_ == RenderStyle::Sticks) ? 0.04f : 0.08f;
    for (const auto& bond : f.bonds) {
        if (bond.i<0||bond.j<0||bond.i>=f.atom_count()||bond.j>=f.atom_count()) continue;
        const auto& ai = f.atoms[bond.i];
        const auto& aj = f.atoms[bond.j];
        scene::Vec3d mid{(ai.pos.x+aj.pos.x)*0.5,(ai.pos.y+aj.pos.y)*0.5,(ai.pos.z+aj.pos.z)*0.5};
        float ri,gi,bi,rj,gj,bj;
        atomColor(ai.Z,ri,gi,bi);
        atomColor(aj.Z,rj,gj,bj);
        // Two-tone bond: each atom's half uses its own color
        drawCylinder(ai.pos, mid, brad, ri,gi,bi);
        drawCylinder(mid, aj.pos, brad, rj,gj,bj);
        // Double bond: extra parallel cylinder offset slightly
        if (bond.order >= 1.9) {
            // compute perpendicular offset ~0.12 Å
            scene::Vec3d d{aj.pos.x-ai.pos.x,aj.pos.y-ai.pos.y,aj.pos.z-ai.pos.z};
            float h=(float)scene::distance({0,0,0},d);
            if (h>1e-6f) {
                float offX=-d.z/h*0.12f, offY=0.0f, offZ=d.x/h*0.12f;
                scene::Vec3d a2{ai.pos.x+offX,ai.pos.y+offY,ai.pos.z+offZ};
                scene::Vec3d b2{aj.pos.x+offX,aj.pos.y+offY,aj.pos.z+offZ};
                scene::Vec3d m2{mid.x+offX,mid.y+offY,mid.z+offZ};
                drawCylinder(a2,m2,brad*0.7f,ri,gi,bi);
                drawCylinder(m2,b2,brad*0.7f,rj,gj,bj);
            }
        }
    }
}

// ============================================================================
// Draw — periodic cell box
// ============================================================================
void ViewportWidget::drawCellBox(const scene::BoxInfo& box)
{
    if (!box.enabled) return;
    const auto& a=box.a, &b=box.b, &c=box.c;
    // 8 corners
    scene::Vec3d O{0,0,0};
    scene::Vec3d A{a.x,a.y,a.z};
    scene::Vec3d B{b.x,b.y,b.z};
    scene::Vec3d C{c.x,c.y,c.z};
    scene::Vec3d AB{A.x+B.x,A.y+B.y,A.z+B.z};
    scene::Vec3d AC{A.x+C.x,A.y+C.y,A.z+C.z};
    scene::Vec3d BC{B.x+C.x,B.y+C.y,B.z+C.z};
    scene::Vec3d ABC{A.x+B.x+C.x,A.y+B.y+C.y,A.z+B.z+C.z};
    auto push=[](std::vector<float>& v, const scene::Vec3d& p){
        v.push_back((float)p.x); v.push_back((float)p.y); v.push_back((float)p.z);
    };
    std::vector<float> lines;
    // 12 edges
    for (auto [p,q] : std::initializer_list<std::pair<scene::Vec3d,scene::Vec3d>>{
        {O,A},{O,B},{O,C},{A,AB},{A,AC},{B,AB},{B,BC},{C,AC},{C,BC},{AB,ABC},{AC,ABC},{BC,ABC}})
    { push(lines,p); push(lines,q); }
    drawLines(lines, 0.5f,0.7f,1.0f, 1.5f);
}

// ============================================================================
// paintGL — main render pass
// ============================================================================
void ViewportWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackground();

    const scene::FrameData* f = currentFrame();
    if (!f || f->atoms.empty()) {
        // QPainter overlay for "empty" message
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QColor(100,100,110));
        p.setFont(QFont("Segoe UI", 14));
        p.drawText(rect(), Qt::AlignCenter, "No structure loaded\nUse File → Open XYZ…");
        return;
    }

    if (render_style_ == RenderStyle::Wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    drawBonds(*f);
    drawAtoms(*f);

    if (show_box_ && f->box.enabled)
        drawCellBox(f->box);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // QPainter overlays (labels + axes)
    if (show_labels_ || show_axes_) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (show_labels_) drawLabels(p, *f);
        if (show_axes_)   drawAxesOverlay(p);
    }
}

// ============================================================================
// Labels overlay (QPainter)
// ============================================================================
void ViewportWidget::drawLabels(QPainter& p, const scene::FrameData& f)
{
    QFont font("Consolas", 8);
    font.setBold(true);
    p.setFont(font);
    for (int i=0; i<(int)f.atoms.size(); ++i) {
        const auto& a = f.atoms[i];
        QPointF sc = worldToScreen(a.pos);
        if (sc.x() < 0 || sc.x() > width() || sc.y() < 0 || sc.y() > height()) continue;
        // Shadow
        p.setPen(QColor(0,0,0,160));
        p.drawText(sc + QPointF(1,1), QString::fromStdString(
            a.symbol.empty() ? (a.Z>=1&&a.Z<=NUM_ELEM ? ELEM_SYM[a.Z-1] : "?") : a.symbol));
        // Label
        float r,g,b; atomColor(a.Z,r,g,b);
        p.setPen(QColor((int)(r*255),(int)(g*255),(int)(b*255)));
        p.drawText(sc, QString::fromStdString(
            a.symbol.empty() ? (a.Z>=1&&a.Z<=NUM_ELEM ? ELEM_SYM[a.Z-1] : "?") : a.symbol));
    }
}

// ============================================================================
// Axes overlay — bottom-left corner using QPainter projection
// ============================================================================
void ViewportWidget::drawAxesOverlay(QPainter& p)
{
    int cx = 50, cy = height() - 50;
    float len = 30.0f;

    // Extract camera rotation from view matrix (upper-left 3x3 rows)
    float view[16]; computeViewMatrix(view);
    // row0 (right), row1 (up), row2 (back) of view = columns of rotation^-1
    // To project world-space axis to screen: multiply by transpose of view rotation
    // X axis world = (1,0,0); Y = (0,1,0); Z = (0,0,1)
    // Screen x = dot(axis, view_row0); screen y = -dot(axis, view_row1)

    auto project = [&](float wx, float wy, float wz) -> QPointF {
        float sx =  view[0]*wx + view[4]*wy + view[8]*wz;
        float sy = -(view[1]*wx + view[5]*wy + view[9]*wz);
        return QPointF(cx + sx*len, cy + sy*len);
    };

    QPointF o(cx, cy);
    QPointF xEnd = project(1,0,0);
    QPointF yEnd = project(0,1,0);
    QPointF zEnd = project(0,0,1);

    p.setFont(QFont("Segoe UI", 8, QFont::Bold));
    auto drawAxis = [&](QPointF end, QColor col, const char* lbl) {
        p.setPen(QPen(col, 2));
        p.drawLine(o, end);
        p.setPen(col.lighter(140));
        p.drawText(end + QPointF(3,-3), lbl);
    };
    drawAxis(xEnd, QColor(220,60,60),  "X");
    drawAxis(yEnd, QColor(60,200,80),  "Y");
    drawAxis(zEnd, QColor(60,140,220), "Z");

    // Circle origin
    p.setPen(QPen(QColor(200,200,200), 1));
    p.setBrush(QColor(200,200,200,180));
    p.drawEllipse(o, 3, 3);
    p.setBrush(Qt::NoBrush);
}

// ============================================================================
// Mouse interaction
// ============================================================================
void ViewportWidget::mousePressEvent(QMouseEvent* e)
{
    dragging_    = true;
    mouse_moved_ = false;
    drag_button_ = e->button();
    last_mouse_  = e->pos();
    press_pos_   = e->pos();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!dragging_) return;
    float dx=(float)(e->pos().x()-last_mouse_.x());
    float dy=(float)(e->pos().y()-last_mouse_.y());
    if (std::abs(dx)>1||std::abs(dy)>1) mouse_moved_ = true;
    last_mouse_ = e->pos();
    if (drag_button_ == Qt::LeftButton) {
        cam_theta_ -= dx*0.005f;
        cam_phi_   += dy*0.005f;
        cam_phi_ = std::clamp(cam_phi_, -1.5f, 1.5f);
    } else if (drag_button_ == Qt::RightButton || drag_button_ == Qt::MiddleButton) {
        float scale = cam_dist_*0.002f;
        cam_target_.x -= dx*scale;
        cam_target_.y += dy*scale;
    }
    update();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* e)
{
    dragging_ = false;
    if (!mouse_moved_ && e->button() == Qt::LeftButton)
        pickAtom(e->pos().x(), e->pos().y());
}

void ViewportWidget::wheelEvent(QWheelEvent* e)
{
    float delta = (float)e->angleDelta().y()/120.0f;
    cam_dist_ *= (1.0f - delta*0.08f);
    cam_dist_ = std::clamp(cam_dist_, 1.0f, 200.0f);
    update();
}

void ViewportWidget::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Left  || e->key() == Qt::Key_Comma)  stepBack();
    else if (e->key() == Qt::Key_Right || e->key() == Qt::Key_Period) stepForward();
    else if (e->key() == Qt::Key_Space) { if (isPlaying()) pause(); else play(); }
    else if (e->key() == Qt::Key_Escape) clearSelection();
    else QOpenGLWidget::keyPressEvent(e);
}

// ============================================================================
// Atom picking — ray-sphere intersection, pick closest atom
// ============================================================================
void ViewportWidget::pickAtom(int sx, int sy)
{
    const scene::FrameData* f = currentFrame();
    if (!f || f->atoms.empty()) return;

    // Build ray in world space
    float aspect = (float)width()/std::max((float)height(),1.0f);
    float fovRad = cam_fov_*(float)M_PI/180.0f;
    float ndcX = (2.0f*sx/width()-1.0f);
    float ndcY = (1.0f-2.0f*sy/height());
    float tanHalf = std::tan(fovRad/2.0f);
    float vx = ndcX*tanHalf*aspect;
    float vy = ndcY*tanHalf;

    // Camera basis from view matrix
    float view[16]; computeViewMatrix(view);
    // right = view row0, up = view row1, -forward = view row2
    float rightX=view[0],rightY=view[4],rightZ=view[8];
    float upX=view[1],    upY=view[5],    upZ=view[9];
    float fwdX=-view[2],  fwdY=-view[6],  fwdZ=-view[10];

    float rayX=fwdX+vx*rightX+vy*upX;
    float rayY=fwdY+vx*rightY+vy*upY;
    float rayZ=fwdZ+vx*rightZ+vy*upZ;
    float rl=std::sqrt(rayX*rayX+rayY*rayY+rayZ*rayZ);
    rayX/=rl; rayY/=rl; rayZ/=rl;

    float eyeX=camEyeX(), eyeY=camEyeY(), eyeZ=camEyeZ();

    int best = -1;
    float bestDist = 1e30f;
    for (int i=0; i<(int)f->atoms.size(); ++i) {
        const auto& a = f->atoms[i];
        float ox=(float)a.pos.x-eyeX, oy=(float)a.pos.y-eyeY, oz=(float)a.pos.z-eyeZ;
        float tca = ox*rayX+oy*rayY+oz*rayZ;
        if (tca<0) continue;
        float d2 = ox*ox+oy*oy+oz*oz - tca*tca;
        float rad = atomDisplayRadius(a.Z);
        if (d2 < rad*rad) {
            float thc = std::sqrt(rad*rad-d2);
            float t = tca-thc;
            if (t<bestDist) { bestDist=t; best=i; }
        }
    }
    if (best != selected_atom_) {
        selected_atom_ = best;
        emit atomSelected(selected_atom_);
        update();
    }
}

// ============================================================================
// Scene data
// ============================================================================
void ViewportWidget::setFrame(const scene::FrameData& frame)
{
    doc_ = std::make_shared<scene::SceneDocument>();
    doc_->frames.push_back(frame);
    current_frame_idx_ = 0;
    selected_atom_ = -1;
    fitCamera();
    emit frameChanged();
    emit frameIndexChanged(0,1);
    update();
}

void ViewportWidget::setDocument(std::shared_ptr<scene::SceneDocument> doc)
{
    doc_ = doc;
    current_frame_idx_ = doc_&&!doc_->empty() ? (int)doc_->frames.size()-1 : 0;
    selected_atom_ = -1;
    fitCamera();
    emit frameChanged();
    emit frameIndexChanged(current_frame_idx_, frameCount());
    update();
}

const scene::FrameData* ViewportWidget::currentFrame() const
{
    if (!doc_||doc_->empty()) return nullptr;
    if (current_frame_idx_<0||current_frame_idx_>=doc_->frame_count()) return nullptr;
    return &doc_->frame(current_frame_idx_);
}

void ViewportWidget::fitCamera()
{
    const scene::FrameData* f = currentFrame();
    if (!f||f->atoms.empty()) return;
    cam_target_ = f->centroid();
    cam_dist_   = (float)(f->bounding_radius()*3.0+3.0);
}

void ViewportWidget::resetCamera()
{
    cam_theta_=0.4f; cam_phi_=0.3f; cam_dist_=15.0f;
    cam_target_={0,0,0};
    fitCamera(); update();
}

void ViewportWidget::setWireframe(bool on)
{
    render_style_ = on ? RenderStyle::Wireframe : RenderStyle::BallAndStick;
    update();
}

void ViewportWidget::setRenderStyle(RenderStyle s)
{
    render_style_ = s; update();
}

// ============================================================================
// Trajectory playback
// ============================================================================
int ViewportWidget::frameCount() const
{
    return doc_ ? doc_->frame_count() : 0;
}

void ViewportWidget::setFrameIndex(int idx)
{
    if (!doc_||doc_->empty()) return;
    idx = std::clamp(idx, 0, doc_->frame_count()-1);
    if (idx != current_frame_idx_) {
        current_frame_idx_ = idx;
        emit frameChanged();
        emit frameIndexChanged(idx, frameCount());
        update();
    }
}

void ViewportWidget::play()
{
    if (frameCount()>1) play_timer_->start(1000/playback_fps_);
}

void ViewportWidget::pause()  { play_timer_->stop(); }

bool ViewportWidget::isPlaying() const { return play_timer_->isActive(); }

void ViewportWidget::stepForward()
{
    if (!doc_||doc_->empty()) return;
    setFrameIndex((current_frame_idx_+1) % doc_->frame_count());
}

void ViewportWidget::stepBack()
{
    if (!doc_||doc_->empty()) return;
    int n = doc_->frame_count();
    setFrameIndex((current_frame_idx_-1+n) % n);
}

void ViewportWidget::onPlayTimer() { stepForward(); }

void ViewportWidget::clearSelection()
{
    selected_atom_ = -1;
    emit atomSelected(-1);
    update();
}
'''

# ===========================================================================
# ObjectTree.h
# ===========================================================================
files[D + "/ObjectTree.h"] = r'''#pragma once
/**
 * ObjectTree.h — Left dock: hierarchical structure browser
 *
 * Three root categories:
 *   Molecules   — single-frame molecular structures
 *   Crystals    — crystal emitter results
 *   Results     — energy/trajectory outputs
 *
 * Each item stores type metadata in UserRole data.
 * Double-clicking an item activates it in the viewport.
 */

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>
#include <QString>

enum class TreeItemType { Molecule = 1, Crystal = 2, Result = 3, Trajectory = 4 };

class ObjectTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ObjectTree(QWidget* parent = nullptr);

    // --- Add items ---
    QTreeWidgetItem* addMolecule(const QString& name, int atomCount, int bondCount);
    QTreeWidgetItem* addCrystal(const QString& name, const QString& preset,
                                double nn_dist, int atomCount);
    QTreeWidgetItem* addResult(const QString& name, double energy,
                               int steps, double frms);
    QTreeWidgetItem* addTrajectory(const QString& name, int frameCount,
                                   double dt_fs);

    // --- Bulk operations ---
    void clear();
    void clearCategory(TreeItemType type);

    // --- Selection ---
    void setActiveItem(QTreeWidgetItem* item);

signals:
    void itemActivated(TreeItemType type, const QString& name,
                       QTreeWidgetItem* item);
    void itemRemoveRequested(QTreeWidgetItem* item);

private slots:
    void onDoubleClicked(QTreeWidgetItem* item, int col);
    void onContextMenu(const QPoint& pos);
    void onRemoveSelected();

private:
    QTreeWidgetItem* moleculesRoot_;
    QTreeWidgetItem* crystalsRoot_;
    QTreeWidgetItem* resultsRoot_;

    QAction* removeAct_;
};
'''

# ===========================================================================
# ObjectTree.cpp
# ===========================================================================
files[D + "/ObjectTree.cpp"] = r'''#include "ObjectTree.h"
#include <QHeaderView>

ObjectTree::ObjectTree(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderLabels({tr("Name"), tr("Details")});
    setColumnCount(2);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    moleculesRoot_ = new QTreeWidgetItem(this, {tr("Molecules")});
    crystalsRoot_  = new QTreeWidgetItem(this, {tr("Crystals")});
    resultsRoot_   = new QTreeWidgetItem(this, {tr("Results")});

    for (auto* root : {moleculesRoot_, crystalsRoot_, resultsRoot_}) {
        QFont f = root->font(0);
        f.setBold(true);
        root->setFont(0, f);
        root->setExpanded(true);
    }

    removeAct_ = new QAction(tr("Remove"), this);
    connect(removeAct_, &QAction::triggered, this, &ObjectTree::onRemoveSelected);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &ObjectTree::onDoubleClicked);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &ObjectTree::onContextMenu);
}

QTreeWidgetItem* ObjectTree::addMolecule(const QString& name, int atomCount, int bondCount)
{
    auto* item = new QTreeWidgetItem(moleculesRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 atoms, %2 bonds").arg(atomCount).arg(bondCount));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Molecule);
    moleculesRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addCrystal(const QString& name, const QString& preset,
                                        double nn_dist, int atomCount)
{
    auto* item = new QTreeWidgetItem(crystalsRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 | nn=%.3f Å | %2 atoms")
        .arg(preset).arg(nn_dist).arg(atomCount));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Crystal);
    crystalsRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addResult(const QString& name, double energy,
                                       int steps, double frms)
{
    auto* item = new QTreeWidgetItem(resultsRoot_);
    item->setText(0, name);
    item->setText(1, QString("E=%.4f | steps=%1 | Frms=%.2e")
        .arg(energy).arg(steps).arg(frms));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Result);
    resultsRoot_->setExpanded(true);
    return item;
}

QTreeWidgetItem* ObjectTree::addTrajectory(const QString& name, int frameCount,
                                           double dt_fs)
{
    auto* item = new QTreeWidgetItem(resultsRoot_);
    item->setText(0, name);
    item->setText(1, QString("%1 frames | dt=%.1f fs").arg(frameCount).arg(dt_fs));
    item->setData(0, Qt::UserRole, (int)TreeItemType::Trajectory);
    resultsRoot_->setExpanded(true);
    return item;
}

void ObjectTree::clear()
{
    while (moleculesRoot_->childCount()>0) delete moleculesRoot_->takeChild(0);
    while (crystalsRoot_->childCount()>0)  delete crystalsRoot_->takeChild(0);
    while (resultsRoot_->childCount()>0)   delete resultsRoot_->takeChild(0);
}

void ObjectTree::clearCategory(TreeItemType type)
{
    QTreeWidgetItem* root = nullptr;
    if (type==TreeItemType::Molecule)   root = moleculesRoot_;
    else if (type==TreeItemType::Crystal) root = crystalsRoot_;
    else root = resultsRoot_;
    if (root) while (root->childCount()>0) delete root->takeChild(0);
}

void ObjectTree::setActiveItem(QTreeWidgetItem* item)
{
    if (item) { setCurrentItem(item); scrollToItem(item); }
}

void ObjectTree::onDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item || !item->parent()) return; // ignore root items
    auto type = (TreeItemType)item->data(0, Qt::UserRole).toInt();
    emit itemActivated(type, item->text(0), item);
}

void ObjectTree::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item || !item->parent()) return;
    QMenu menu(this);
    menu.addAction(removeAct_);
    menu.exec(viewport()->mapToGlobal(pos));
}

void ObjectTree::onRemoveSelected()
{
    QTreeWidgetItem* item = currentItem();
    if (!item || !item->parent()) return;
    emit itemRemoveRequested(item);
    delete item;
}
'''

# ===========================================================================
# PropertiesPanel.h
# ===========================================================================
files[D + "/PropertiesPanel.h"] = r'''#pragma once
/**
 * PropertiesPanel.h — Right dock: properties inspector
 *
 * Sections:
 *   Identity     — formula, atom count, bond count
 *   Energy       — total + breakdown (LJ, Coulomb, bonded)
 *   Geometry     — selected atom info, bond angles
 *   Lattice      — a, b, c, alpha, beta, gamma (if PBC)
 *   Simulation   — steps, temperature, dt, Frms
 *   Frame        — frame index / total, step, time
 */

#include <QWidget>
#include <QScrollArea>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QString>

class PropertiesPanel : public QScrollArea
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    // --- Identity ---
    void setFormula(const QString& formula);
    void setAtomCount(int n);
    void setBondCount(int n);

    // --- Energy ---
    void setEnergy(double total_kcal);
    void setEnergyBreakdown(double lj, double coul, double bond);
    void setForceRMS(double rms);

    // --- Lattice ---
    void setLattice(double a, double b, double c,
                    double alpha=90, double beta=90, double gamma=90);
    void clearLattice();

    // --- Simulation ---
    void setSimParams(int steps, double temp_K, double dt_fs);
    void setFrameInfo(int idx, int total, double time_ps);

    // --- Selected atom ---
    void setSelectedAtom(int atomIdx, int Z, const QString& symbol,
                         double x, double y, double z);
    void clearSelectedAtom();

    // --- Bulk ---
    void clearAll();

private:
    QWidget* content_;

    // Identity group
    QGroupBox* identityGroup_;
    QLabel*    formulaLabel_;
    QLabel*    atomCountLabel_;
    QLabel*    bondCountLabel_;

    // Energy group
    QGroupBox* energyGroup_;
    QLabel*    energyLabel_;
    QLabel*    ljLabel_;
    QLabel*    coulLabel_;
    QLabel*    bondedLabel_;
    QLabel*    frmsLabel_;

    // Lattice group
    QGroupBox* latticeGroup_;
    QLabel*    latticeALabel_;
    QLabel*    latticeBLabel_;
    QLabel*    latticeCLabel_;
    QLabel*    latticeAngLabel_;

    // Simulation group
    QGroupBox* simGroup_;
    QLabel*    stepsLabel_;
    QLabel*    tempLabel_;
    QLabel*    dtLabel_;
    QLabel*    frameLabel_;

    // Selection group
    QGroupBox* selGroup_;
    QLabel*    selIdxLabel_;
    QLabel*    selSymLabel_;
    QLabel*    selPosLabel_;

    static QLabel* makeValueLabel();
};
'''

# ===========================================================================
# PropertiesPanel.cpp
# ===========================================================================
files[D + "/PropertiesPanel.cpp"] = r'''#include "PropertiesPanel.h"

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    content_ = new QWidget;
    auto* vbox = new QVBoxLayout(content_);
    vbox->setContentsMargins(6,6,6,6);
    vbox->setSpacing(8);

    // ---- Identity ----
    identityGroup_ = new QGroupBox(tr("Identity"));
    auto* ig = new QFormLayout(identityGroup_);
    ig->addRow(tr("Formula:"),    formulaLabel_    = makeValueLabel());
    ig->addRow(tr("Atoms:"),      atomCountLabel_  = makeValueLabel());
    ig->addRow(tr("Bonds:"),      bondCountLabel_  = makeValueLabel());
    vbox->addWidget(identityGroup_);

    // ---- Energy ----
    energyGroup_ = new QGroupBox(tr("Energy"));
    auto* eg = new QFormLayout(energyGroup_);
    eg->addRow(tr("Total:"),       energyLabel_ = makeValueLabel());
    eg->addRow(tr("  LJ:"),        ljLabel_     = makeValueLabel());
    eg->addRow(tr("  Coulomb:"),   coulLabel_   = makeValueLabel());
    eg->addRow(tr("  Bonded:"),    bondedLabel_ = makeValueLabel());
    eg->addRow(tr("Frms:"),        frmsLabel_   = makeValueLabel());
    vbox->addWidget(energyGroup_);

    // ---- Lattice ----
    latticeGroup_ = new QGroupBox(tr("Lattice"));
    auto* lg = new QFormLayout(latticeGroup_);
    lg->addRow(tr("a, b, c:"),    latticeALabel_   = makeValueLabel());
    lg->addRow(tr(""),            latticeBLabel_   = makeValueLabel());
    lg->addRow(tr(""),            latticeCLabel_   = makeValueLabel());
    lg->addRow(tr("α, β, γ:"),   latticeAngLabel_ = makeValueLabel());
    latticeGroup_->setVisible(false);
    vbox->addWidget(latticeGroup_);

    // ---- Simulation ----
    simGroup_ = new QGroupBox(tr("Simulation"));
    auto* sg = new QFormLayout(simGroup_);
    sg->addRow(tr("Steps:"),       stepsLabel_ = makeValueLabel());
    sg->addRow(tr("T (K):"),       tempLabel_  = makeValueLabel());
    sg->addRow(tr("dt (fs):"),     dtLabel_    = makeValueLabel());
    sg->addRow(tr("Frame:"),       frameLabel_ = makeValueLabel());
    vbox->addWidget(simGroup_);

    // ---- Selection ----
    selGroup_ = new QGroupBox(tr("Selected Atom"));
    auto* sl = new QFormLayout(selGroup_);
    sl->addRow(tr("Index:"),       selIdxLabel_ = makeValueLabel());
    sl->addRow(tr("Element:"),     selSymLabel_ = makeValueLabel());
    sl->addRow(tr("Position:"),    selPosLabel_ = makeValueLabel());
    selGroup_->setVisible(false);
    vbox->addWidget(selGroup_);

    vbox->addStretch();
    setWidget(content_);
    clearAll();
}

QLabel* PropertiesPanel::makeValueLabel()
{
    auto* l = new QLabel("—");
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

void PropertiesPanel::setFormula(const QString& f)     { formulaLabel_->setText(f); }
void PropertiesPanel::setAtomCount(int n)              { atomCountLabel_->setText(QString::number(n)); }
void PropertiesPanel::setBondCount(int n)              { bondCountLabel_->setText(QString::number(n)); }

void PropertiesPanel::setEnergy(double e)
{
    energyLabel_->setText(QString("%1 kcal/mol").arg(e, 0, 'f', 4));
}

void PropertiesPanel::setEnergyBreakdown(double lj, double coul, double bond)
{
    ljLabel_    ->setText(QString("%1").arg(lj,   0, 'f', 4));
    coulLabel_  ->setText(QString("%1").arg(coul, 0, 'f', 4));
    bondedLabel_->setText(QString("%1").arg(bond, 0, 'f', 4));
}

void PropertiesPanel::setForceRMS(double rms)
{
    frmsLabel_->setText(QString("%1 kcal/mol/Å").arg(rms, 0, 'e', 3));
}

void PropertiesPanel::setLattice(double a, double b, double c,
                                  double alpha, double beta, double gamma)
{
    latticeALabel_  ->setText(QString("a = %1 Å").arg(a, 0, 'f', 4));
    latticeBLabel_  ->setText(QString("b = %1 Å").arg(b, 0, 'f', 4));
    latticeCLabel_  ->setText(QString("c = %1 Å").arg(c, 0, 'f', 4));
    latticeAngLabel_->setText(QString("%1°  %2°  %3°")
        .arg(alpha,0,'f',2).arg(beta,0,'f',2).arg(gamma,0,'f',2));
    latticeGroup_->setVisible(true);
}

void PropertiesPanel::clearLattice()        { latticeGroup_->setVisible(false); }

void PropertiesPanel::setSimParams(int steps, double temp, double dt)
{
    stepsLabel_->setText(QString::number(steps));
    tempLabel_ ->setText(QString("%1 K").arg(temp, 0, 'f', 1));
    dtLabel_   ->setText(QString("%1 fs").arg(dt,  0, 'f', 2));
}

void PropertiesPanel::setFrameInfo(int idx, int total, double time_ps)
{
    frameLabel_->setText(QString("%1 / %2  (t = %3 ps)")
        .arg(idx+1).arg(total).arg(time_ps, 0, 'f', 3));
}

void PropertiesPanel::setSelectedAtom(int atomIdx, int Z, const QString& symbol,
                                       double x, double y, double z)
{
    selIdxLabel_->setText(QString("%1  (Z=%2)").arg(atomIdx).arg(Z));
    selSymLabel_->setText(symbol);
    selPosLabel_->setText(QString("(%1, %2, %3) Å")
        .arg(x,0,'f',3).arg(y,0,'f',3).arg(z,0,'f',3));
    selGroup_->setVisible(true);
}

void PropertiesPanel::clearSelectedAtom() { selGroup_->setVisible(false); }

void PropertiesPanel::clearAll()
{
    formulaLabel_  ->setText("—");
    atomCountLabel_->setText("—");
    bondCountLabel_->setText("—");
    energyLabel_   ->setText("—");
    ljLabel_       ->setText("—");
    coulLabel_     ->setText("—");
    bondedLabel_   ->setText("—");
    frmsLabel_     ->setText("—");
    stepsLabel_    ->setText("—");
    tempLabel_     ->setText("—");
    dtLabel_       ->setText("—");
    frameLabel_    ->setText("—");
    clearLattice();
    clearSelectedAtom();
}
'''

# ===========================================================================
# ConsolePanel.h
# ===========================================================================
files[D + "/ConsolePanel.h"] = r'''#pragma once
/**
 * ConsolePanel.h — Bottom dock: timestamped log + command input
 *
 * Color coding:
 *   info    — default (light gray)
 *   success — green
 *   warning — amber
 *   error   — red
 *
 * History: up/down arrows navigate previous commands.
 */

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDateTime>
#include <QKeyEvent>
#include <QStringList>

class CommandLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit CommandLineEdit(QWidget* parent = nullptr);
    void addToHistory(const QString& cmd);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    QStringList history_;
    int         histIdx_ = -1;
};

// ---------------------------------------------------------------------------

class ConsolePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget* parent = nullptr);

    void log(const QString& msg);
    void logSuccess(const QString& msg);
    void logWarning(const QString& msg);
    void logError(const QString& msg);
    void clear();

signals:
    void commandSubmitted(const QString& cmd);

private slots:
    void onReturn();

private:
    QTextEdit*       output_;
    CommandLineEdit* input_;

    void append(const QString& html);
    static QString timestamp();
};
'''

# ===========================================================================
# ConsolePanel.cpp
# ===========================================================================
files[D + "/ConsolePanel.cpp"] = r'''#include "ConsolePanel.h"

// ============================================================================
// CommandLineEdit — history-aware input
// ============================================================================
CommandLineEdit::CommandLineEdit(QWidget* parent) : QLineEdit(parent) {}

void CommandLineEdit::addToHistory(const QString& cmd)
{
    if (!cmd.isEmpty() && (history_.isEmpty() || history_.last() != cmd))
        history_.append(cmd);
    histIdx_ = -1;
}

void CommandLineEdit::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Up) {
        if (history_.isEmpty()) return;
        if (histIdx_ < history_.size()-1) ++histIdx_;
        setText(history_[history_.size()-1-histIdx_]);
    } else if (e->key() == Qt::Key_Down) {
        if (histIdx_ > 0) { --histIdx_; setText(history_[history_.size()-1-histIdx_]); }
        else { histIdx_=-1; clear(); }
    } else {
        QLineEdit::keyPressEvent(e);
    }
}

// ============================================================================
// ConsolePanel
// ============================================================================
ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);

    output_ = new QTextEdit;
    output_->setReadOnly(true);
    output_->setFont(QFont("Consolas, 'Courier New', monospace", 9));
    output_->document()->setMaximumBlockCount(2000);
    layout->addWidget(output_);

    input_ = new CommandLineEdit;
    input_->setPlaceholderText(tr("Enter command… (relax | md | sp | reset | help)"));
    connect(input_, &QLineEdit::returnPressed, this, &ConsolePanel::onReturn);
    layout->addWidget(input_);

    log("VSEPR Desktop — Console ready.");
    log("Commands: <b>relax</b>, <b>md</b>, <b>sp</b>, <b>reset</b>, <b>help</b>");
}

QString ConsolePanel::timestamp()
{
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

void ConsolePanel::append(const QString& html)
{
    output_->append(html);
    output_->verticalScrollBar()->setValue(output_->verticalScrollBar()->maximum());
}

void ConsolePanel::log(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#c8c8c8'>%2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logSuccess(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#50c878'>✓ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logWarning(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#f0a830'>⚠ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::logError(const QString& msg)
{
    append(QString("<span style='color:#606060'>[%1]</span> "
                   "<span style='color:#e05050'>✗ %2</span>")
        .arg(timestamp(), msg.toHtmlEscaped()));
}

void ConsolePanel::clear()  { output_->clear(); }

void ConsolePanel::onReturn()
{
    QString cmd = input_->text().trimmed();
    if (cmd.isEmpty()) return;
    append(QString("<span style='color:#4a90d9'>&gt; %1</span>")
        .arg(cmd.toHtmlEscaped()));
    input_->addToHistory(cmd);
    input_->clear();
    emit commandSubmitted(cmd);
}
'''

print("Writing files...")
for path, content in files.items():
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\n') as f:
        f.write(content)
    print(f"  {os.path.relpath(path, ROOT)} ({len(content)} chars)")

print(f"\nDone — {len(files)} files written.")
