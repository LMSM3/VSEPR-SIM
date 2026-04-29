// =============================================================================
// tests/test_vsim_post_step.cpp  WO-VSEPR-SIM-57E  (Group 34)
// =============================================================================
// PS1  disabled post_step.enabled = false returns empty
// PS2  empty script_block returns empty
// PS3  script executes, scope has pbc.distance value
// PS4  pbc.wrap in script
// PS5  pbc.distance minimum image across boundary
// PS6  particle.position readable
// PS7  multiple assignments execute in order
// PS8  bad expression throws VsimRuntimeError
// WO-VSEPR-SIM-57E  beta-8

#include <cassert>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_post_step.hpp"
#include "include/vsim/vsim_value.hpp"

using namespace vsim;

static VsimDocument make_doc(const std::string& sb, bool en = true) {
    VsimDocument doc;
    doc.cell.lx = 20.0; doc.cell.ly = 20.0; doc.cell.lz = 20.0;
    doc.boundary.x = "periodic"; doc.boundary.y = "periodic"; doc.boundary.z = "periodic";
    doc.pbc.minimum_image = true; doc.pbc.track_images = true;
    doc.post_step.enabled = en; doc.post_step.script_block = sb;
    return doc;
}

static std::vector<ScriptParticle> two_p(double ax,double ay,double az,double bx,double by,double bz) {
    ScriptParticle a; a.position={ax,ay,az}; a.symbol="Ar"; a.Z=18;
    ScriptParticle b; b.position={bx,by,bz}; b.symbol="Ar"; b.Z=18;
    return {a,b};
}

static void PS1() {
    auto doc = make_doc("d=pbc.distance(particle.position(1),particle.position(2))", false);
    auto s = run_post_step(doc, two_p(1,1,1,5,5,5));
    assert(s.empty()); std::puts("  PS1 PASS  disabled -> empty scope");
}
static void PS2() {
    auto doc = make_doc("", true);
    auto s = run_post_step(doc, two_p(1,1,1,5,5,5));
    assert(s.empty()); std::puts("  PS2 PASS  empty script -> empty scope");
}
static void PS3() {
    auto doc = make_doc("d=pbc.distance(particle.position(1),particle.position(2))");
    auto s = run_post_step(doc, two_p(0,0,0,3,0,0));
    assert(s.count("d"));
    double d = std::get<double>(s.at("d"));
    assert(std::fabs(d-3.0)<1e-9); std::puts("  PS3 PASS  scope['d']==3.0");
}
static void PS4() {
    auto doc = make_doc("w=pbc.wrap(xyzvec3(22.0,5.0,5.0))");
    auto s = run_post_step(doc, two_p(1,1,1,2,2,2));
    assert(s.count("w"));
    XYZVec3 w = std::get<XYZVec3>(s.at("w"));
    assert(std::fabs(w.x-2.0)<1e-9); std::puts("  PS4 PASS  pbc.wrap(22,5,5)->(2,5,5)");
}
static void PS5() {
    auto doc = make_doc("d=pbc.distance(particle.position(1),particle.position(2))");
    auto s = run_post_step(doc, two_p(0,0,0,18,0,0));
    double d = std::get<double>(s.at("d"));
    assert(std::fabs(d-2.0)<1e-9); std::puts("  PS5 PASS  minimum-image 2.0");
}
static void PS6() {
    auto doc = make_doc("p=particle.position(1)");
    auto s = run_post_step(doc, two_p(4,5,6,0,0,0));
    assert(s.count("p"));
    XYZVec3 p = std::get<XYZVec3>(s.at("p"));
    assert(std::fabs(p.x-4)<1e-9 && std::fabs(p.y-5)<1e-9 && std::fabs(p.z-6)<1e-9);
    std::puts("  PS6 PASS  particle.position(1)==(4,5,6)");
}
static void PS7() {
    std::string blk = "p1=particle.position(1)\np2=particle.position(2)\nd=pbc.distance(p1,p2)\n";
    auto doc = make_doc(blk);
    auto s = run_post_step(doc, two_p(0,0,0,7,0,0));
    assert(s.count("p1") && s.count("p2") && s.count("d"));
    double d = std::get<double>(s.at("d"));
    assert(std::fabs(d-7.0)<1e-9); std::puts("  PS7 PASS  multi-statement d==7.0");
}
static void PS8() {
    auto doc = make_doc("x=pbc.nonexistent_fn(1,2)");
    bool threw = false;
    try { run_post_step(doc, two_p(0,0,0,1,1,1)); } catch(const VsimRuntimeError&) { threw=true; }
    assert(threw); std::puts("  PS8 PASS  bad builtin throws VsimRuntimeError");
}

int main() {
    std::puts("=== test_vsim_post_step (Group 34 / WO-57E) ===\n");
    PS1(); PS2(); PS3(); PS4(); PS5(); PS6(); PS7(); PS8();
    std::puts("\n  8 / 8 passed\n");
    return 0;
}
