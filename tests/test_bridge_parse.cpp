// test_bridge_parse.cpp -- WO-67N/67O bridge parser regression tests (B1-B15)
#include "vsim/vsim_parser.hpp"
#include "vsim/objects/bridge_objects.hpp"
#include "vsim/bridge/dem_bridge.hpp"
#include "vsim/bridge/fea_bridge.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
using namespace vsim;
static int g_pass=0, g_fail=0;
static void PASS(const char* n){std::printf("  [PASS] %s\n",n);++g_pass;}
static void FAIL(const char* n,const char* r){std::fprintf(stderr,"  [FAIL] %s  -  %s\n",n,r);++g_fail;}
#define REQUIRE(name,cond,msg) do{if(!(cond)){FAIL(name,msg);return;}}while(0)
#define REQUIRE_EQ(name,got,expected,msg) REQUIRE(name,(got)==(expected),msg)
#define REQUIRE_NEAR(name,got,expected,tol,msg) do{if(std::abs((double)(got)-(double)(expected))>(tol)){FAIL(name,msg);return;}}while(0)
static VsimDocument parse_str(const std::string& s){return VsimParser::parse_string(s);}
static void test_B1(){const char* N="B1  DEMBridge basic parse";
    try{auto d=parse_str("[bridge]\ndem.pipe = DEMBridge(from = system.surface.wall, geometry = system.geometry.pipe)\n");
    REQUIRE(N,d.bridge_objects.dem_bridges.size()==1,"expected 1");
    REQUIRE_EQ(N,d.bridge_objects.dem_bridges[0].path.path,"dem.pipe","path");
    REQUIRE_EQ(N,d.bridge_objects.dem_bridges[0].source_surface.path,"system.surface.wall","src");
    REQUIRE_EQ(N,d.bridge_objects.dem_bridges[0].geometry.path,"system.geometry.pipe","geom");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B2(){const char* N="B2  DEMBridge from alias";
    try{auto d=parse_str("[bridge]\ndem.p = DEMBridge(from = surf.wall, geometry = geo.pipe)\n");
    REQUIRE(N,!d.bridge_objects.dem_bridges.empty(),"empty");
    REQUIRE_EQ(N,d.bridge_objects.dem_bridges[0].source_surface.path,"surf.wall","alias");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B3(){const char* N="B3  DEMBridge numeric fields";
    try{auto d=parse_str("[bridge]\ndem.p = DEMBridge(from = s, geometry = g, packing_model = soft_sphere, contact_model = linear_spring, friction = 0.45, restitution = 0.30)\n");
    const auto& b=d.bridge_objects.dem_bridges[0];
    REQUIRE_EQ(N,b.packing_model,"soft_sphere","packing");
    REQUIRE_NEAR(N,b.friction,0.45,1e-9,"friction");
    REQUIRE_NEAR(N,b.restitution,0.30,1e-9,"restitution");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B4(){const char* N="B4  DEMBridge missing source -> E070";
    try{auto d=parse_str("[bridge]\ndem.p = DEMBridge(geometry = g.pipe)\n");
    auto dg=validate_dem_bridge(d.bridge_objects.dem_bridges[0],d);
    bool f=false;for(const auto& x:dg)if(x.code==70&&x.level=="ERROR")f=true;
    REQUIRE(N,f,"E070 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B5(){const char* N="B5  DEMBridge missing geometry -> E071";
    try{auto d=parse_str("[bridge]\ndem.p = DEMBridge(from = s.wall)\n");
    auto dg=validate_dem_bridge(d.bridge_objects.dem_bridges[0],d);
    bool f=false;for(const auto& x:dg)if(x.code==71&&x.level=="ERROR")f=true;
    REQUIRE(N,f,"E071 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B6(){const char* N="B6  empty packing_model -> W072";
    try{auto d=parse_str("[bridge]\ndem.p = DEMBridge(from = s, geometry = g)\n");
    auto dem=d.bridge_objects.dem_bridges[0];dem.packing_model="";
    auto dg=validate_dem_bridge(dem,d);
    bool f=false;for(const auto& x:dg)if(x.code==72&&x.level=="WARN")f=true;
    REQUIRE(N,f,"W072 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B7(){const char* N="B7  FEABridge basic parse";
    try{auto d=parse_str("[bridge]\nfea.wall = FEABridge(from = system.surface.wall, target = system.geometry.pipe)\n");
    REQUIRE(N,d.bridge_objects.fea_bridges.size()==1,"expected 1");
    REQUIRE_EQ(N,d.bridge_objects.fea_bridges[0].path.path,"fea.wall","path");
    REQUIRE_EQ(N,d.bridge_objects.fea_bridges[0].source_surface.path,"system.surface.wall","src");
    REQUIRE_EQ(N,d.bridge_objects.fea_bridges[0].target_geometry.path,"system.geometry.pipe","tgt");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B8(){const char* N="B8  FEABridge surface/geometry aliases";
    try{auto d=parse_str("[bridge]\nfea.w = FEABridge(surface = s.wall, geometry = g.pipe)\n");
    const auto& f=d.bridge_objects.fea_bridges[0];
    REQUIRE_EQ(N,f.source_surface.path,"s.wall","surface alias");
    REQUIRE_EQ(N,f.target_geometry.path,"g.pipe","geom alias");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B9(){const char* N="B9  FEABridge material/criterion/fatigue/cavitation_k";
    try{auto d=parse_str("[bridge]\nfea.w = FEABridge(from = s, target = g, material = inconel625, criterion = von_mises, fatigue = true, cavitation_k = 0.22)\n");
    const auto& f=d.bridge_objects.fea_bridges[0];
    REQUIRE_EQ(N,f.material,"inconel625","material");
    REQUIRE_EQ(N,f.criterion,"von_mises","criterion");
    REQUIRE(N,f.fatigue_enabled,"fatigue");
    REQUIRE_NEAR(N,f.cavitation_k,0.22,1e-9,"cavitation_k");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B10(){const char* N="B10 FEABridge missing source -> E080";
    try{auto d=parse_str("[bridge]\nfea.w = FEABridge(target = g.pipe)\n");
    auto dg=validate_fea_bridge(d.bridge_objects.fea_bridges[0],d);
    bool f=false;for(const auto& x:dg)if(x.code==80&&x.level=="ERROR")f=true;
    REQUIRE(N,f,"E080 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B11(){const char* N="B11 FEABridge missing target -> E081";
    try{auto d=parse_str("[bridge]\nfea.w = FEABridge(from = s.wall)\n");
    auto dg=validate_fea_bridge(d.bridge_objects.fea_bridges[0],d);
    bool f=false;for(const auto& x:dg)if(x.code==81&&x.level=="ERROR")f=true;
    REQUIRE(N,f,"E081 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B12(){const char* N="B12 FEABridge absent yield_strength -> W085";
    try{auto d=parse_str("[bridge]\nfea.w = FEABridge(from = s, target = g, criterion = von_mises)\n");
    auto dg=validate_fea_bridge(d.bridge_objects.fea_bridges[0],d);
    bool f=false;for(const auto& x:dg)if(x.code==85&&x.level=="WARN")f=true;
    REQUIRE(N,f,"W085 not raised");PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B13(){const char* N="B13 Multiple DEM+FEA bridges parsed independently";
    try{auto d=parse_str("[bridge]\ndem.alpha = DEMBridge(from = s, geometry = g)\nfea.beta = FEABridge(from = s, target = g)\ndem.gamma = DEMBridge(from = s2, geometry = g2, friction = 0.55)\n");
    REQUIRE(N,d.bridge_objects.dem_bridges.size()==2,"2 DEM");
    REQUIRE(N,d.bridge_objects.fea_bridges.size()==1,"1 FEA");
    REQUIRE_NEAR(N,d.bridge_objects.dem_bridges[1].friction,0.55,1e-9,"gamma friction");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B14(){const char* N="B14 Unknown bridge constructor stored raw no crash";
    try{auto d=parse_str("[bridge]\nfuture.obj = SPHBridge(from = s, geometry = g)\n");
    REQUIRE(N,d.bridge_objects.dem_bridges.empty(),"no DEM");
    REQUIRE(N,d.bridge_objects.fea_bridges.empty(),"no FEA");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
static void test_B15(){const char* N="B15 BridgeObjectStore::empty() when no bridges";
    try{auto d=parse_str("[project]\nname = \"b15\"\n");
    REQUIRE(N,d.bridge_objects.empty(),"not empty");
    PASS(N);}catch(const std::exception& e){FAIL(N,e.what());}}
int main(){
    std::printf("\n=== WO-67N/67O Bridge Parse Tests ===\n\n");
    test_B1();test_B2();test_B3();test_B4();test_B5();
    test_B6();test_B7();test_B8();test_B9();test_B10();
    test_B11();test_B12();test_B13();test_B14();test_B15();
    std::printf("\n  Results: %d passed, %d failed\n\n",g_pass,g_fail);
    return (g_fail>0)?1:0;}