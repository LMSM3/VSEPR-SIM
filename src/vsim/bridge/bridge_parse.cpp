/**
 * bridge_parse.cpp -- [bridge] section constructor-line parser
 * WO-67N / WO-67O | Phase 1 | v5.1.5
 */
#include "vsim/vsim_parser.hpp"
#include "vsim/objects/bridge_objects.hpp"
#include "vsim/objects/object_path.hpp"
#include <sstream>
#include <string>
#include <vector>
namespace vsim {
static std::string bp_trim(const std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return{};
    size_t b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
static std::vector<std::string> tokenise_args(const std::string& ar){
    std::vector<std::string> t;std::string c;int d=0;
    for(char ch:ar){if(ch=='('){++d;c+=ch;}else if(ch==')'){--d;c+=ch;}
    else if(ch==','&&d==0){t.push_back(bp_trim(c));c.clear();}else c+=ch;}
    if(!bp_trim(c).empty())t.push_back(bp_trim(c));return t;}
static DEMBridgeObject parse_dem_bridge(const std::string& path_str,const std::string& rhs,int line_no){
    DEMBridgeObject obj;obj.path=ObjectPath(path_str);obj.source_line=line_no;
    size_t paren=rhs.find('(');size_t close=rhs.rfind(')');
    if(paren==std::string::npos)return obj;
    std::string ar=rhs.substr(paren+1,close!=std::string::npos?close-paren-1:std::string::npos);
    for(const auto& tok:tokenise_args(ar)){
        size_t eq=tok.find('=');if(eq==std::string::npos)continue;
        std::string k=bp_trim(tok.substr(0,eq)),v=bp_trim(tok.substr(eq+1));
        if(v.size()>=2&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);
        if(k=="from"||k=="surface"||k=="src")obj.source_surface=ObjectPath(v);
        else if(k=="geometry"||k=="geom")obj.geometry=ObjectPath(v);
        else if(k=="inlet")obj.inlet=ObjectPath(v);
        else if(k=="outlet")obj.outlet=ObjectPath(v);
        else if(k=="carrier"||k=="ambient")obj.ambient=ObjectPath(v);
        else if(k=="packing_model")obj.packing_model=v;
        else if(k=="contact_model")obj.contact_model=v;
        else if(k=="friction"){try{obj.friction=std::stod(v);}catch(...){}}
        else if(k=="restitution"){try{obj.restitution=std::stod(v);}catch(...){}}
        else if(k=="particle_density"||k=="particle_density_kg_m3"){try{obj.particle_density_kg_m3=std::stod(v);}catch(...){}}
        else if(k=="export"||k=="export_manifest")obj.export_manifest=(v=="true"||v=="1");
        else if(k=="export_table")obj.export_table=(v=="true"||v=="1");
        else if(k=="export_format")obj.export_format=v;
    }
    return obj;}
static FEABridgeObject parse_fea_bridge(const std::string& path_str,const std::string& rhs,int line_no){
    FEABridgeObject obj;obj.path=ObjectPath(path_str);obj.source_line=line_no;
    size_t paren=rhs.find('(');size_t close=rhs.rfind(')');
    if(paren==std::string::npos)return obj;
    std::string ar=rhs.substr(paren+1,close!=std::string::npos?close-paren-1:std::string::npos);
    for(const auto& tok:tokenise_args(ar)){
        size_t eq=tok.find('=');if(eq==std::string::npos)continue;
        std::string k=bp_trim(tok.substr(0,eq)),v=bp_trim(tok.substr(eq+1));
        if(v.size()>=2&&v.front()=='"'&&v.back()=='"')v=v.substr(1,v.size()-2);
        if(k=="from"||k=="surface"||k=="src")obj.source_surface=ObjectPath(v);
        else if(k=="target"||k=="geometry"||k=="geom")obj.target_geometry=ObjectPath(v);
        else if(k=="material")obj.material=v;
        else if(k=="map"||k=="mapping_mode")obj.mapping_mode=v;
        else if(k=="criterion")obj.criterion=v;
        else if(k=="yield_strength"||k=="yield_strength_Pa"){try{obj.yield_strength_Pa=std::stod(v);}catch(...){}}
        else if(k=="fatigue"||k=="fatigue_enabled")obj.fatigue_enabled=(v=="true"||v=="1");
        else if(k=="cavitation_k"){try{obj.cavitation_k=std::stod(v);}catch(...){}}
        else if(k=="export_json")obj.export_json=(v=="true"||v=="1");
        else if(k=="export_tsv")obj.export_tsv=(v=="true"||v=="1");
        else if(k=="export_vtk"||k=="export_vtk_proxy")obj.export_vtk=(v=="true"||v=="1");
    }
    return obj;}
void VsimParser::apply_bridge_constructor_line(const std::string& lhs,const std::string& rhs,int line_no){
    std::string r=rhs;
    size_t s=r.find_first_not_of(" \t");if(s!=std::string::npos)r=r.substr(s);
    if(r.rfind("DEMBridge",0)==0)
        doc_.bridge_objects.dem_bridges.push_back(parse_dem_bridge(lhs,r,line_no));
    else if(r.rfind("FEABridge",0)==0)
        doc_.bridge_objects.fea_bridges.push_back(parse_fea_bridge(lhs,r,line_no));
    else
        doc_.raw_sections["bridge"][lhs]=rhs;}
} // namespace vsim