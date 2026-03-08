#include "ViewportWidget.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <array>
#include <map>

// ============================================================================
// Element data (CPK colors + covalent radii) — compact inline table
// ============================================================================

namespace {

struct ElemInfo { float r, g, b; float cov; float vdw; };

// Index = Z-1.  Only first ~54 populated; rest fall back to gray.
constexpr int NUM_ELEM = 54;
const ElemInfo ELEM[NUM_ELEM] = {
    // Z  r     g     b     cov   vdw
    {0.90f,0.90f,0.90f, 0.31f,1.20f}, //  1 H
    {0.85f,1.00f,1.00f, 0.28f,1.40f}, //  2 He
    {0.80f,0.50f,1.00f, 1.28f,1.82f}, //  3 Li
    {0.76f,1.00f,0.00f, 0.96f,1.53f}, //  4 Be
    {1.00f,0.71f,0.71f, 0.84f,1.92f}, //  5 B
    {0.20f,0.20f,0.20f, 0.76f,1.70f}, //  6 C
    {0.19f,0.31f,0.97f, 0.71f,1.55f}, //  7 N
    {0.90f,0.05f,0.05f, 0.66f,1.52f}, //  8 O
    {0.56f,0.88f,0.31f, 0.57f,1.47f}, //  9 F
    {0.70f,0.89f,0.96f, 0.58f,1.54f}, // 10 Ne
    {0.67f,0.36f,0.95f, 1.66f,2.27f}, // 11 Na
    {0.54f,1.00f,0.00f, 1.41f,1.73f}, // 12 Mg
    {0.75f,0.65f,0.65f, 1.21f,1.84f}, // 13 Al
    {0.94f,0.78f,0.63f, 1.11f,2.10f}, // 14 Si
    {1.00f,0.50f,0.00f, 1.07f,1.80f}, // 15 P
    {1.00f,1.00f,0.19f, 1.05f,1.80f}, // 16 S
    {0.12f,0.94f,0.12f, 1.02f,1.75f}, // 17 Cl
    {0.50f,0.82f,0.89f, 1.06f,1.88f}, // 18 Ar
    {0.56f,0.25f,0.83f, 2.03f,2.75f}, // 19 K
    {0.24f,1.00f,0.00f, 1.76f,2.31f}, // 20 Ca
    {0.90f,0.90f,0.90f, 1.70f,2.11f}, // 21 Sc
    {0.75f,0.76f,0.78f, 1.60f,2.00f}, // 22 Ti
    {0.65f,0.65f,0.67f, 1.53f,2.00f}, // 23 V
    {0.54f,0.60f,0.78f, 1.39f,2.00f}, // 24 Cr
    {0.61f,0.48f,0.78f, 1.39f,2.00f}, // 25 Mn
    {0.88f,0.40f,0.20f, 1.32f,2.00f}, // 26 Fe
    {0.94f,0.56f,0.63f, 1.26f,2.00f}, // 27 Co
    {0.31f,0.82f,0.31f, 1.24f,1.63f}, // 28 Ni
    {0.78f,0.50f,0.20f, 1.32f,1.40f}, // 29 Cu
    {0.49f,0.50f,0.69f, 1.22f,1.39f}, // 30 Zn
    {0.76f,0.56f,0.56f, 1.22f,1.87f}, // 31 Ga
    {0.40f,0.56f,0.56f, 1.20f,2.11f}, // 32 Ge
    {0.74f,0.50f,0.89f, 1.19f,1.85f}, // 33 As
    {1.00f,0.63f,0.00f, 1.20f,1.90f}, // 34 Se
    {0.65f,0.16f,0.16f, 1.20f,1.85f}, // 35 Br
    {0.36f,0.72f,0.82f, 1.16f,2.02f}, // 36 Kr
    {0.44f,0.18f,0.69f, 2.20f,3.03f}, // 37 Rb
    {0.00f,1.00f,0.00f, 1.95f,2.49f}, // 38 Sr
    {0.58f,1.00f,1.00f, 1.90f,2.00f}, // 39 Y
    {0.58f,0.88f,0.88f, 1.75f,2.00f}, // 40 Zr
    {0.45f,0.76f,0.79f, 1.64f,2.00f}, // 41 Nb
    {0.33f,0.71f,0.71f, 1.54f,2.00f}, // 42 Mo
    {0.23f,0.62f,0.62f, 1.47f,2.00f}, // 43 Tc
    {0.14f,0.56f,0.56f, 1.46f,2.00f}, // 44 Ru
    {0.04f,0.49f,0.55f, 1.42f,2.00f}, // 45 Rh
    {0.00f,0.41f,0.52f, 1.39f,1.63f}, // 46 Pd
    {0.75f,0.75f,0.75f, 1.45f,1.72f}, // 47 Ag
    {1.00f,0.85f,0.56f, 1.44f,1.58f}, // 48 Cd
    {0.65f,0.46f,0.45f, 1.42f,1.93f}, // 49 In
    {0.40f,0.50f,0.50f, 1.39f,2.17f}, // 50 Sn
    {0.62f,0.39f,0.71f, 1.39f,2.06f}, // 51 Sb
    {0.83f,0.48f,0.00f, 1.38f,2.06f}, // 52 Te
    {0.58f,0.00f,0.58f, 1.39f,1.98f}, // 53 I
    {0.26f,0.62f,0.69f, 1.40f,2.16f}, // 54 Xe
};

} // anonymous

void ViewportWidget::atomColor(int Z, float& r, float& g, float& b)
{
    if (Z >= 1 && Z <= NUM_ELEM) {
        r = ELEM[Z-1].r; g = ELEM[Z-1].g; b = ELEM[Z-1].b;
    } else {
        r = 0.6f; g = 0.6f; b = 0.6f;
    }
}

float ViewportWidget::atomRadius(int Z)
{
    if (Z >= 1 && Z <= NUM_ELEM) return ELEM[Z-1].cov;
    return 1.5f;
}

// ============================================================================
// Construction
// ============================================================================

ViewportWidget::ViewportWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

ViewportWidget::~ViewportWidget()
{
    makeCurrent();
    if (sphere_vao_) glDeleteVertexArrays(1, &sphere_vao_);
    if (sphere_vbo_) glDeleteBuffers(1, &sphere_vbo_);
    if (sphere_ebo_) glDeleteBuffers(1, &sphere_ebo_);
    if (cyl_vao_)    glDeleteVertexArrays(1, &cyl_vao_);
    if (cyl_vbo_)    glDeleteBuffers(1, &cyl_vbo_);
    if (cyl_ebo_)    glDeleteBuffers(1, &cyl_ebo_);
    if (shader_prog_) glDeleteProgram(shader_prog_);
    doneCurrent();
}

// ============================================================================
// Shaders — minimal Blinn-Phong, no external files
// ============================================================================

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
    vec3 L = normalize(vec3(0.6, 0.8, 0.5));   // key light direction
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 H = normalize(L + V);

    // Ambient + diffuse + specular (Blinn-Phong)
    float ambient  = 0.15;
    float diffuse  = max(dot(N, L), 0.0) * 0.65;
    float spec     = pow(max(dot(N, H), 0.0), 64.0) * 0.35;

    // Fill light (softer, from opposite side)
    vec3 L2 = normalize(vec3(-0.4, 0.3, -0.6));
    float fill = max(dot(N, L2), 0.0) * 0.20;

    // Rim light
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.10;

    vec3 color = uColor * (ambient + diffuse + fill) + vec3(spec + rim);

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
)";

GLuint ViewportWidget::compileShader(const char* vert, const char* frag)
{
    auto compile = [&](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
            qWarning("Shader compile error: %s", log);
        }
        return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vert);
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log);
        qWarning("Shader link error: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ============================================================================
// Geometry generation — icosphere + cylinder
// ============================================================================

void ViewportWidget::buildSphereMesh(int subdivisions)
{
    // Unit icosahedron → subdivide
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<float> verts;
    std::vector<unsigned int> idxs;

    auto addV = [&](float x, float y, float z) {
        float len = std::sqrt(x*x + y*y + z*z);
        verts.push_back(x/len); verts.push_back(y/len); verts.push_back(z/len);
        // normals = positions for unit sphere
        verts.push_back(x/len); verts.push_back(y/len); verts.push_back(z/len);
        return (unsigned int)(verts.size()/6 - 1);
    };

    // 12 vertices of icosahedron
    addV(-1, t,0); addV(1, t,0); addV(-1,-t,0); addV(1,-t,0);
    addV(0,-1, t); addV(0, 1, t); addV(0,-1,-t); addV(0, 1,-t);
    addV( t,0,-1); addV( t,0, 1); addV(-t,0,-1); addV(-t,0, 1);

    // 20 faces
    unsigned int faces[] = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };
    for (auto f : faces) idxs.push_back(f);

    // Subdivision
    std::map<uint64_t, unsigned int> cache;
    auto midpoint = [&](unsigned int a, unsigned int b) -> unsigned int {
        uint64_t key = (uint64_t)std::min(a,b) << 32 | std::max(a,b);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        float mx = (verts[a*6]+verts[b*6])/2;
        float my = (verts[a*6+1]+verts[b*6+1])/2;
        float mz = (verts[a*6+2]+verts[b*6+2])/2;
        unsigned int idx = addV(mx, my, mz);
        cache[key] = idx;
        return idx;
    };

    for (int s = 0; s < subdivisions; ++s) {
        std::vector<unsigned int> newIdx;
        cache.clear();
        for (size_t i = 0; i < idxs.size(); i += 3) {
            unsigned int a = idxs[i], b = idxs[i+1], c = idxs[i+2];
            unsigned int ab = midpoint(a,b), bc = midpoint(b,c), ca = midpoint(c,a);
            newIdx.insert(newIdx.end(), {a,ab,ca, b,bc,ab, c,ca,bc, ab,bc,ca});
        }
        idxs = std::move(newIdx);
    }

    sphere_idx_count_ = (int)idxs.size();

    glGenVertexArrays(1, &sphere_vao_);
    glGenBuffers(1, &sphere_vbo_);
    glGenBuffers(1, &sphere_ebo_);

    glBindVertexArray(sphere_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.size()*sizeof(unsigned int), idxs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void ViewportWidget::buildCylinderMesh(int segments)
{
    std::vector<float> verts;
    std::vector<unsigned int> idxs;

    // Unit cylinder: height 1 along Y, radius 1
    for (int i = 0; i <= segments; ++i) {
        float a = 2.0f * M_PI * i / segments;
        float cx = std::cos(a), cz = std::sin(a);
        // bottom
        verts.insert(verts.end(), {cx, 0.0f, cz, cx, 0.0f, cz});
        // top
        verts.insert(verts.end(), {cx, 1.0f, cz, cx, 0.0f, cz});
    }
    for (int i = 0; i < segments; ++i) {
        unsigned int b = i*2, t = b+1, bn = (i+1)*2, tn = bn+1;
        idxs.insert(idxs.end(), {b, bn, t, t, bn, tn});
    }

    cyl_idx_count_ = (int)idxs.size();

    glGenVertexArrays(1, &cyl_vao_);
    glGenBuffers(1, &cyl_vbo_);
    glGenBuffers(1, &cyl_ebo_);

    glBindVertexArray(cyl_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, cyl_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cyl_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxs.size()*sizeof(unsigned int), idxs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// ============================================================================
// OpenGL lifecycle
// ============================================================================

void ViewportWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Neutral dark-gray viewport background — NOT green
    glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    shader_prog_ = compileShader(VERT_SRC, FRAG_SRC);
    buildSphereMesh(3);     // ~768 tris per sphere (MEDIUM quality)
    buildCylinderMesh(16);
}

void ViewportWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

// ============================================================================
// Matrix helpers (no GLM dependency for this minimal widget)
// ============================================================================

static void mat4_identity(float* m) {
    for (int i = 0; i < 16; ++i) m[i] = 0;
    m[0] = m[5] = m[10] = m[15] = 1;
}
static void mat4_mul(float* out, const float* a, const float* b) {
    float t[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            t[c*4+r] = 0;
            for (int k = 0; k < 4; ++k)
                t[c*4+r] += a[k*4+r] * b[c*4+k];
        }
    for (int i = 0; i < 16; ++i) out[i] = t[i];
}
static void mat4_translate(float* m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}
static void mat4_scale(float* m, float sx, float sy, float sz) {
    mat4_identity(m);
    m[0] = sx; m[5] = sy; m[10] = sz;
}

void ViewportWidget::computeViewMatrix(float* m) const
{
    // Orbit camera: eye = target + dist * spherical_to_cartesian(theta, phi)
    float sp = std::sin(cam_phi_), cp = std::cos(cam_phi_);
    float st = std::sin(cam_theta_), ct = std::cos(cam_theta_);

    float ex = (float)cam_target_.x + cam_dist_ * cp * st;
    float ey = (float)cam_target_.y + cam_dist_ * sp;
    float ez = (float)cam_target_.z + cam_dist_ * cp * ct;

    // lookAt
    float fx = (float)cam_target_.x - ex;
    float fy = (float)cam_target_.y - ey;
    float fz = (float)cam_target_.z - ez;
    float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    // right = f × up
    float ux = 0, uy = 1, uz = 0;
    float rx = fy*uz - fz*uy;
    float ry = fz*ux - fx*uz;
    float rz = fx*uy - fy*ux;
    float rlen = std::sqrt(rx*rx + ry*ry + rz*rz);
    rx /= rlen; ry /= rlen; rz /= rlen;

    // true up = r × f
    float tux = ry*fz - rz*fy;
    float tuy = rz*fx - rx*fz;
    float tuz = rx*fy - ry*fx;

    mat4_identity(m);
    m[0]=rx;  m[4]=ry;  m[ 8]=rz;  m[12]=-(rx*ex+ry*ey+rz*ez);
    m[1]=tux; m[5]=tuy; m[ 9]=tuz; m[13]=-(tux*ex+tuy*ey+tuz*ez);
    m[2]=-fx; m[6]=-fy; m[10]=-fz; m[14]= (fx*ex+fy*ey+fz*ez);
    m[3]=0;   m[7]=0;   m[11]=0;   m[15]=1;
}

void ViewportWidget::computeProjMatrix(float* m, float aspect) const
{
    float fovRad = cam_fov_ * (float)M_PI / 180.0f;
    float f = 1.0f / std::tan(fovRad / 2.0f);
    float near = 0.1f, far = 500.0f;
    for (int i = 0; i < 16; ++i) m[i] = 0;
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far+near)/(near-far);
    m[11] = -1.0f;
    m[14] = 2.0f*far*near/(near-far);
}

// ============================================================================
// Drawing primitives
// ============================================================================

void ViewportWidget::drawSphere(const scene::Vec3d& center, float radius,
                                float r, float g, float b)
{
    float model[16], scale[16], trans[16], mvp[16], view[16], proj[16];
    mat4_translate(trans, (float)center.x, (float)center.y, (float)center.z);
    mat4_scale(scale, radius, radius, radius);
    mat4_mul(model, trans, scale);

    computeViewMatrix(view);
    float aspect = (float)width() / std::max((float)height(), 1.0f);
    computeProjMatrix(proj, aspect);

    float vp[16];
    mat4_mul(vp, proj, view);
    mat4_mul(mvp, vp, model);

    // Normal matrix (upper-left 3×3 of model, assume uniform scale)
    float nm[9] = {
        model[0], model[1], model[2],
        model[4], model[5], model[6],
        model[8], model[9], model[10]
    };

    // Camera position for specular
    float sp = std::sin(cam_phi_), cp = std::cos(cam_phi_);
    float st = std::sin(cam_theta_), ct = std::cos(cam_theta_);
    float camPos[3] = {
        (float)cam_target_.x + cam_dist_ * cp * st,
        (float)cam_target_.y + cam_dist_ * sp,
        (float)cam_target_.z + cam_dist_ * cp * ct
    };

    glUseProgram(shader_prog_);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_, "uModel"), 1, GL_FALSE, model);
    glUniformMatrix3fv(glGetUniformLocation(shader_prog_, "uNormalMat"), 1, GL_FALSE, nm);
    glUniform3f(glGetUniformLocation(shader_prog_, "uColor"), r, g, b);
    glUniform3fv(glGetUniformLocation(shader_prog_, "uCamPos"), 1, camPos);

    glBindVertexArray(sphere_vao_);
    glDrawElements(GL_TRIANGLES, sphere_idx_count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void ViewportWidget::drawCylinder(const scene::Vec3d& a, const scene::Vec3d& b,
                                  float radius, float r, float g, float bl)
{
    scene::Vec3d d{b.x - a.x, b.y - a.y, b.z - a.z};
    float h = (float)scene::distance({0,0,0}, d);
    if (h < 1e-6f) return;

    // Build model matrix: translate to a, rotate Y-axis to d, scale
    float dx = (float)d.x/h, dy = (float)d.y/h, dz = (float)d.z/h;

    // Rodrigues: rotate (0,1,0) to (dx,dy,dz)
    // axis = (0,1,0) × dir, angle = acos(dy)
    float ax = -dz, ay = 0.0f, az = dx;
    float sinA = std::sqrt(ax*ax + az*az);
    float cosA = dy;

    float model[16];
    mat4_identity(model);

    if (sinA > 1e-6f) {
        ax /= sinA; az /= sinA;
        float c = cosA, s = sinA, t2 = 1.0f - c;
        // Rodrigues rotation matrix
        model[0] = t2*ax*ax + c;       model[4] = t2*ax*ay - s*az; model[8]  = t2*ax*az + s*ay;
        model[1] = t2*ay*ax + s*az;    model[5] = t2*ay*ay + c;    model[9]  = t2*ay*az - s*ax;
        model[2] = t2*az*ax - s*ay;    model[6] = t2*az*ay + s*ax; model[10] = t2*az*az + c;
    } else if (dy < 0) {
        // 180° flip
        model[5] = -1.0f;
    }

    // Apply scale (radius, height, radius) then translate to a
    float scaleModel[16];
    mat4_identity(scaleModel);
    scaleModel[0] = radius; scaleModel[5] = h; scaleModel[10] = radius;

    float rotScaled[16];
    mat4_mul(rotScaled, model, scaleModel);

    rotScaled[12] = (float)a.x;
    rotScaled[13] = (float)a.y;
    rotScaled[14] = (float)a.z;

    float view[16], proj[16], vp[16], mvp[16];
    computeViewMatrix(view);
    float aspect = (float)width() / std::max((float)height(), 1.0f);
    computeProjMatrix(proj, aspect);
    mat4_mul(vp, proj, view);
    mat4_mul(mvp, vp, rotScaled);

    float nm[9] = {
        rotScaled[0], rotScaled[1], rotScaled[2],
        rotScaled[4], rotScaled[5], rotScaled[6],
        rotScaled[8], rotScaled[9], rotScaled[10]
    };

    float sp = std::sin(cam_phi_), cp = std::cos(cam_phi_);
    float st = std::sin(cam_theta_), ct = std::cos(cam_theta_);
    float camPos[3] = {
        (float)cam_target_.x + cam_dist_ * cp * st,
        (float)cam_target_.y + cam_dist_ * sp,
        (float)cam_target_.z + cam_dist_ * cp * ct
    };

    glUseProgram(shader_prog_);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(shader_prog_, "uModel"), 1, GL_FALSE, rotScaled);
    glUniformMatrix3fv(glGetUniformLocation(shader_prog_, "uNormalMat"), 1, GL_FALSE, nm);
    glUniform3f(glGetUniformLocation(shader_prog_, "uColor"), r, g, bl);
    glUniform3fv(glGetUniformLocation(shader_prog_, "uCamPos"), 1, camPos);

    glBindVertexArray(cyl_vao_);
    glDrawElements(GL_TRIANGLES, cyl_idx_count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ============================================================================
// paintGL — main render pass
// ============================================================================

void ViewportWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const scene::FrameData* f = currentFrame();
    if (!f || f->atoms.empty()) return;

    if (wireframe_)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Draw bonds first (behind atoms)
    for (const auto& bond : f->bonds) {
        if (bond.i < 0 || bond.j < 0 ||
            bond.i >= f->atom_count() || bond.j >= f->atom_count()) continue;
        const auto& ai = f->atoms[bond.i];
        const auto& aj = f->atoms[bond.j];
        drawCylinder(ai.pos, aj.pos, 0.08f, 0.55f, 0.55f, 0.55f);
    }

    // Draw atoms
    for (const auto& atom : f->atoms) {
        float r, g, b;
        atomColor(atom.Z, r, g, b);
        float rad = atomRadius(atom.Z) * 0.4f;
        drawSphere(atom.pos, rad, r, g, b);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// ============================================================================
// Mouse interaction
// ============================================================================

void ViewportWidget::mousePressEvent(QMouseEvent* e)
{
    dragging_ = true;
    drag_button_ = e->button();
    last_mouse_ = e->pos();
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!dragging_) return;

    float dx = (float)(e->pos().x() - last_mouse_.x());
    float dy = (float)(e->pos().y() - last_mouse_.y());
    last_mouse_ = e->pos();

    if (drag_button_ == Qt::LeftButton) {
        // Orbit
        cam_theta_ -= dx * 0.005f;
        cam_phi_   += dy * 0.005f;
        cam_phi_ = std::clamp(cam_phi_, -1.5f, 1.5f);
    } else if (drag_button_ == Qt::RightButton || drag_button_ == Qt::MiddleButton) {
        // Pan
        float scale = cam_dist_ * 0.002f;
        cam_target_.x -= dx * scale;
        cam_target_.y += dy * scale;
    }
    update();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = false;
}

void ViewportWidget::wheelEvent(QWheelEvent* e)
{
    float delta = (float)e->angleDelta().y() / 120.0f;
    cam_dist_ *= (1.0f - delta * 0.08f);
    cam_dist_ = std::clamp(cam_dist_, 1.0f, 200.0f);
    update();
}

// ============================================================================
// Scene data (canonical model)
// ============================================================================

void ViewportWidget::setFrame(const scene::FrameData& frame)
{
    doc_ = std::make_shared<scene::SceneDocument>();
    doc_->frames.push_back(frame);
    current_frame_idx_ = 0;
    fitCamera();
    emit frameChanged();
    update();
}

void ViewportWidget::setDocument(std::shared_ptr<scene::SceneDocument> doc)
{
    doc_ = doc;
    current_frame_idx_ = doc_ && !doc_->empty() ? (int)doc_->frames.size() - 1 : 0;
    fitCamera();
    emit frameChanged();
    update();
}

const scene::FrameData* ViewportWidget::currentFrame() const
{
    if (!doc_ || doc_->empty()) return nullptr;
    if (current_frame_idx_ < 0 || current_frame_idx_ >= doc_->frame_count()) return nullptr;
    return &doc_->frame(current_frame_idx_);
}

void ViewportWidget::fitCamera()
{
    const scene::FrameData* f = currentFrame();
    if (!f || f->atoms.empty()) return;

    scene::Vec3d c = f->centroid();
    cam_target_ = c;
    double r = f->bounding_radius();
    cam_dist_ = (float)(r * 3.0 + 3.0);
}

void ViewportWidget::resetCamera()
{
    cam_theta_ = 0.4f;
    cam_phi_ = 0.3f;
    cam_dist_ = 15.0f;
    cam_target_ = {0, 0, 0};
    fitCamera();
    update();
}

void ViewportWidget::setWireframe(bool on)
{
    wireframe_ = on;
    update();
}
