/**
 * VSEPR-Sim Triple Output System - Authoritative Architecture
 * 
 * One Simulation. Three Render Targets. No Physics Duplication.
 * 
 * This document defines the architectural approach to visualization
 * that acknowledges platform realities rather than fighting them.
 */

# TRIPLE OUTPUT SYSTEM - AUTHORITATIVE ARCHITECTURE

## Problem Statement

**The Reality**:
- Native OpenGL + GLFW + GLEW can break due to platform/loader incompatibilities
- HTML + WebGL works flawlessly in any modern browser
- Some environments have no GPU or can't load GL context
- Forcing all rendering through one brittle path causes cascading failures

**The Wrong Approach**:
Building elaborate workarounds to force all rendering through a single pipeline, then:
- Spending weeks debugging platform-specific linker issues
- Creating platform-specific code paths
- Duplicating physics simulation logic for different renderers
- Having different visualization paths produce different results

**The Right Approach**:
Acknowledge that different rendering targets have different characteristics, and build
architecture that leverages each appropriately.

---

## The Three Outputs

### 1. NATIVE OPENGL OUTPUT (Authoritative Performance Path)

**The path you use when everything works**

Purpose: Maximum performance, deterministic frame timing, full shader pipeline

When it's used:
- Developer machines with C++/GLFW/GLEW toolchain
- Production desktop builds (Windows/Linux/macOS)
- CI visual regression testing
- Headless rendering when needed

Characteristics:
- GLFW window management (or headless context)
- Platform-appropriate GL loader (GLEW/GLAD/GLCore)
- Direct GPU pipeline (no serialization overhead)
- Fixed 60+ FPS deterministic timing
- Full shader system (PBR, deferred rendering, compute)
- Real-time input handling

Failure mode: Falls through to WebGL (system keeps working)

```cpp
// This is your PRIMARY rendering path
class NativeGLRenderer {
    GLContext gl_context;
    SceneGraph scene;
    
    bool initialize() {
        try {
            gl_context.create_window(1280, 720);
            gl_context.load_gl_extensions();
            compile_shaders();
            return true;
        } catch (...) {
            return false;  // Fall through to WebGL
        }
    }
    
    void render() {
        gl_context.clear_buffers();
        for (auto& entity : scene.entities) {
            entity.bind_material();
            entity.bind_mesh();
            glDrawElements(...);
        }
        gl_context.swap_buffers();
    }
};
```

---

### 2. HTML/WEBGL OUTPUT (Universal Guaranteed Path)

**The path that always works**

Purpose: Cross-platform visualization that requires zero native build/compilation

Why this exists:
- Browsers solved graphics 10+ years ago
- Works on any machine with a browser (which is all of them)
- Perfect for demos, debugging, sanity checks
- Identical camera + material semantics to OpenGL
- Data streamed from simulation or exported once

When it's used:
- Demo presentations
- CI automated visual tests (headless Chrome)
- Remote visualization (SSH tunnel + browser)
- Fallback when native GL fails
- Scientific report generation

Characteristics:
- WebGL2 via Three.js or Babylon.js
- Data pipeline: Simulation → JSON/glTF → WebGL
- Runs in browser (no native compilation)
- Network streaming capable
- Built-in screenshot/video export

Example flow:
```
Simulation (C++)
    ↓
Export to JSON/glTF
    ↓
HTTP Server (localhost:8000)
    ↓
Browser (Firefox/Chrome)
    ↓
Three.js WebGL2 Renderer
    ↓
Interactive visualization
```

Failure mode: Fallback to ASCII/SVG (system keeps working)

```cpp
// This is your UNIVERSAL renderer
class WebGLExporter {
    void render(const SceneGraph& scene) {
        // Export to JSON
        nlohmann::json root;
        for (const auto& entity : scene.entities) {
            root["objects"].push_back(entity.to_json());
        }
        std::ofstream file("scene.json");
        file << root.dump(2);
        
        // Generate HTML with Three.js
        generate_viewer_html("index.html");
        
        // Serve
        std::cout << "Open http://localhost:8000 to view\n";
    }
};
```

---

### 3. FALLBACK/MINIMAL OUTPUT (Safety Net)

**The path for when graphics truly fail**

Purpose: Never let a failed GPU renderer silence the science

When it's used:
- GL context creation fails completely
- WebGL not available (headless CI, no GPU)
- User requests diagnostic output
- Emergency "something's broken, show me the data" mode

Examples:
- ASCII projection of molecule
- SVG wireframe dump
- CPU-rasterized PNG preview
- JSON scene graph (for debugging)
- Plain text system state report

Why this matters:
```
"If your simulation can't explain itself without GPU acceleration,
 it's not a simulation. It's a screensaver."
```

If graphics break, the science should STILL be visible.

```cpp
// This is your GUARANTEED renderer
class FallbackRenderer {
    void render_ascii(const SceneGraph& scene, std::ostream& out) {
        const int width = 80, height = 24;
        std::vector<std::string> canvas(height, std::string(width, ' '));
        
        for (const auto& entity : scene.entities) {
            int x = static_cast<int>((entity.position.x + 1) * width / 2);
            int y = static_cast<int>((entity.position.y + 1) * height / 2);
            if (x >= 0 && x < width && y >= 0 && y < height) {
                canvas[y][x] = '@';
            }
        }
        
        for (const auto& line : canvas) out << line << "\n";
    }
    
    void render_svg(const SceneGraph& scene, const std::string& path) {
        std::ofstream svg(path);
        svg << "<svg width='800' height='600'>\n";
        for (const auto& entity : scene.entities) {
            float x = (entity.position.x + 1) * 400;
            float y = (entity.position.y + 1) * 300;
            svg << "<circle cx='" << x << "' cy='" << y 
                << "' r='10' fill='red'/>\n";
        }
        svg << "</svg>\n";
    }
};
```

---

## Unified Core Principle

**One simulation. One scene graph. Three translation layers.**

```
┌─────────────────────────────┐
│   Simulation Core           │
│   (Physics, Dynamics)       │
├─────────────────────────────┤
│   Scene Graph               │
│   • Entities                │
│   • Transforms              │
│   • Materials               │
│   • Lighting                │
│   • Camera State            │
└──────────┬──────────────────┘
           │
    ┌──────┴──────┬──────────┬──────────┐
    │             │          │          │
    ▼             ▼          ▼          ▼
Native GL     WebGL      Fallback    Multi-Export
(60 FPS)    (30 FPS)     (ASCII)     (All three)
```

**Key Rules**:

1. **No duplication of physics**
   - Single MD engine produces positions/forces
   - Scene graph generated once
   - All renderers read the same scene graph

2. **No duplicated rendering logic**
   - Material calculations happen once
   - Lighting calculations happen once
   - Only the TRANSLATION to GPU format differs

3. **Only translation layers**
   - Native GL: Scene Graph → VAO/VBO/Shader Uniforms
   - WebGL: Scene Graph → JSON → Three.js objects
   - Fallback: Scene Graph → ASCII/SVG/JSON

4. **Graceful degradation**
   - Try Native GL (best)
   - If that fails, try WebGL (universal)
   - If that fails, try Fallback (guaranteed)
   - System ALWAYS produces output

---

## Implementation Pattern

### Architecture in Code

```cpp
// Abstract interface
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool initialize() = 0;
    virtual void render(const SceneGraph& scene) = 0;
    virtual bool is_available() const = 0;
};

// Concrete implementations
class NativeGLRenderer : public Renderer { /* ... */ };
class WebGLExporter : public Renderer { /* ... */ };
class FallbackRenderer : public Renderer { /* ... */ };

// Composite that tries all
class VisualizationSystem {
    std::vector<std::unique_ptr<Renderer>> renderers;
    Renderer* primary = nullptr;
    
    bool initialize() {
        // Try each in order
        auto gl = std::make_unique<NativeGLRenderer>();
        if (gl->initialize()) {
            renderers.push_back(std::move(gl));
            primary = renderers.back().get();
            return true;
        }
        
        auto web = std::make_unique<WebGLExporter>();
        if (web->initialize()) {
            renderers.push_back(std::move(web));
            primary = renderers.back().get();
            return true;
        }
        
        auto fallback = std::make_unique<FallbackRenderer>();
        if (fallback->initialize()) {
            renderers.push_back(std::move(fallback));
            primary = renderers.back().get();
            return true;
        }
        
        // Should never reach here
        return false;
    }
    
    void render(const SceneGraph& scene) {
        if (primary) primary->render(scene);
    }
};
```

### Failure Cascade

```
initialize() {
    try_native_gl();           // Best (60 FPS)
    |
    ├─ Success? → Use it ✓
    │
    └─ Fails? → try_webgl()    // Universal (30 FPS)
        |
        ├─ Success? → Use it ✓
        │
        └─ Fails? → try_fallback()  // Guaranteed
            |
            ├─ Success? → Use it ✓
            │  (ASCII output, JSON export)
            │
            └─ Fails? → Error
               (Should never happen)
}

Result: System ALWAYS visualizes something
        User ALWAYS sees output
        Science never blocked by graphics
```

---

## Advantages of Triple Output

### 1. Resilience
- One renderer fails → system keeps working
- No single point of failure
- Graceful degradation

### 2. Development Workflow
```
Local dev (fast iteration):
  Native GL for 60 FPS interactive development
  
Demo/Presentation:
  WebGL for cross-platform, no setup required
  
CI/Headless/Remote:
  WebGL export works anywhere
  
Debugging graphics issues:
  Fallback ASCII shows whether problem is rendering or data
```

### 3. Zero Duplication
- Physics runs once
- Scene graph built once
- Only renderers differ

### 4. Validation
```
Scientific validation:
  Run simulation
  Export to all three formats
  If all three agree → results are correct
  If they differ → found your bug
```

### 5. Cross-Platform Consistency
- Same camera behavior in GL, WebGL, ASCII
- Same material properties
- Same coordinate system
- Results are identical regardless of renderer

---

## Real-World Example: Molecule Visualization

```
User runs: vsepr visualize water.xyz

System attempt:
  1. Try Native GL
     ├─ GLFW init... ✓
     ├─ GLEW init... ✗ (incompatible driver)
     └─ Fall through
     
  2. Try WebGL
     ├─ Generate scene.json... ✓
     ├─ Generate index.html... ✓
     ├─ Start HTTP server... ✓
     └─ SUCCESS: Open http://localhost:8000
     
User sees: Interactive 3D molecule in browser
System state: Working perfectly

---

Alternative scenario (headless CI):

System attempt:
  1. Try Native GL
     ├─ No display server ✗
     └─ Fall through
     
  2. Try WebGL
     ├─ No browser available ✗
     └─ Fall through
     
  3. Try Fallback
     ├─ Generate water.svg... ✓
     ├─ Generate water.json... ✓
     └─ SUCCESS: Diagnostic output saved
     
CI output:
  ✓ water.svg (wireframe visualization)
  ✓ water.json (complete scene data)
  ✓ Can be viewed/analyzed in post-processing

Science is never blocked by graphics infrastructure.
```

---

## Integration with Existing Code

### Modify Application Lifecycle

```cpp
// Before: "Create GL window or die"
int main() {
    GLWindow window(1280, 720);  // Fails if GL unavailable
    Simulation sim;
    
    while (!window.should_close()) {
        sim.step(dt);
        window.render(sim.scene());
    }
}

// After: "Try GL, fallback gracefully"
int main() {
    Simulation sim;
    VisualizationSystem viz;
    
    if (!viz.initialize()) {
        std::cerr << "No visualization available\n";
        return 1;
    }
    
    // Now we have SOME renderer
    if (auto* gl = dynamic_cast<NativeGLRenderer*>(viz.primary())) {
        // Native GL: interactive loop
        while (!gl->should_close()) {
            sim.step(dt);
            gl->render(sim.scene());
        }
    } else {
        // WebGL or Fallback: non-interactive export
        for (int step = 0; step < 1000; ++step) {
            sim.step(dt);
            viz.render(sim.scene());  // Exports to JSON each frame
        }
        std::cout << "Animation exported to ./outputs/\n";
    }
}
```

---

## Configuration & Modes

```cpp
enum class VisualizationStrategy {
    NATIVE_GL_ONLY,      // Crash if GL fails (dev mode)
    PREFER_NATIVE,       // Try GL, fall back gracefully (production)
    PREFER_WEB,          // Export to browser (demos)
    ALWAYS_ALL_THREE,    // Render to all targets (validation)
    HEADLESS_EXPORT      // WebGL/Fallback only (CI)
};

// User selects via config
VisualizationConfig config;
config.strategy = VisualizationStrategy::PREFER_NATIVE;
config.webgl_export.enabled = true;
config.webgl_export.output_dir = "./outputs";
config.fallback.include_ascii = true;
config.fallback.include_svg = true;

VisualizationSystem viz;
viz.initialize(config);
```

---

## Performance Characteristics

| Renderer | FPS | Latency | Headless | Scale |
|----------|-----|---------|----------|-------|
| Native GL | 60+ | Immediate | Yes | 100K+ atoms |
| WebGL | 30 | Network+browser | Yes | 10K atoms |
| Fallback | Instant | File I/O | Yes | Any |

Choose renderer based on use case:
- **Interactive dev** → Native GL (best performance)
- **Demo/Presentation** → WebGL (works everywhere)
- **CI/Testing** → Fallback (deterministic, no GPU)

---

## Conclusion

**This is not a workaround. This is architecture.**

By acknowledging that different rendering targets have different strengths:
- Native GL for performance
- WebGL for universality  
- Fallback for robustness

We build a system that is resilient, cross-platform, and scientifically sound.

One simulation. Three outputs. Zero duplication.

```
"Optimize for the happy path, handle the sad paths gracefully."
```

When Native GL works → blazing fast interactive development
When Native GL fails → seamless fallback to browser-based visualization
When everything fails → ASCII diagram proves the data is still there

This is professional software architecture.

---

**Implementation Status**: Ready for Phase 2  
**Key Pattern**: Adapter Pattern (Renderer interface with three implementations)  
**Integration Effort**: Minimal (fits existing architecture cleanly)  
**Payoff**: Entire class of platform-specific bugs eliminated
