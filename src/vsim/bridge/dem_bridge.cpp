// WO-67N DEM Bridge validate + export stubs (Phase 1)
#include "vsim/bridge/dem_bridge.hpp"
#include "vsim/vsim_document.hpp"
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
namespace vsim {
std::vector<DEMBridgeDiagnostic> validate_dem_bridge(const DEMBridgeObject& dem, const VsimDocument&){
    std::vector<DEMBridgeDiagnostic> d;
    if(!dem.has_source()) d.push_back({70,"ERROR","VSIM-E070: DEMBridge '"+dem.path.path+"' has no source surface (set from/surface)",dem.source_line});
    if(!dem.has_geometry()) d.push_back({71,"ERROR","VSIM-E071: DEMBridge '"+dem.path.path+"' has no geometry assigned",dem.source_line});
    if(dem.packing_model.empty()) d.push_back({72,"WARN","VSIM-W072: DEMBridge '"+dem.path.path+"' packing_model not specified; defaulted to hard_sphere",dem.source_line});
    if(dem.contact_model.empty()) d.push_back({73,"WARN","VSIM-W073: DEMBridge '"+dem.path.path+"' contact_model not specified; defaulted to hertz_mindlin",dem.source_line});
    if(dem.particle_density_kg_m3<=0.0) d.push_back({75,"WARN","VSIM-W075: DEMBridge '"+dem.path.path+"' particle_density not specified; defaulted to 2500 kg/m3",dem.source_line});
    return d;}
void export_dem_bridge(const DEMBridgeObject& dem, const VsimDocument&, const std::string& out){
    std::filesystem::create_directories(out);
    {auto p=out+"/dem_bridge_manifest.json";FILE* f=std::fopen(p.c_str(),"w");if(f){
        std::time_t t=std::time(nullptr);char ts[32];std::strftime(ts,sizeof(ts),"%Y-%m-%dT%H:%M:%SZ",std::gmtime(&t));
        std::fprintf(f,"{\n  \"schema\": \"dem_bridge_manifest_v1\",\n  \"generated\": \"%s\",\n  \"phase\": 1,\n  \"phase_note\": \"Phase 1 stub. DEM solver deferred v5.2.0.\",\n  \"bridge_path\": \"%s\",\n  \"source_surface\": \"%s\",\n  \"geometry\": \"%s\",\n  \"packing_model\": \"%s\",\n  \"contact_model\": \"%s\",\n  \"friction\": %.4f,\n  \"restitution\": %.4f,\n  \"particle_density_kg_m3\": %.1f\n}\n",
            ts,dem.path.path.c_str(),dem.source_surface.path.c_str(),dem.geometry.path.c_str(),
            dem.packing_model.c_str(),dem.contact_model.c_str(),dem.friction,dem.restitution,dem.particle_density_kg_m3);
        std::fclose(f);}}
    if(dem.export_table){auto p=out+"/dem_packing_regions.tsv";FILE* f=std::fopen(p.c_str(),"w");if(f){
        std::fprintf(f,"region_id\tphi_hat\tJ_proxy\tP_bridge_Pa\tjamming_class\n");
        double phi=0.52,J=0.42,P=5e4;
        for(int i=0;i<3;++i)std::fprintf(f,"%d\t%.4f\t%.4f\t%.2e\t%s\n",i,phi+i*0.02,J+i*0.03,P*(1+i*0.1),i==0?"unjammed":i==1?"transitional":"jammed_proxy");
        std::fclose(f);}}
    {auto p=out+"/dem_boundary_conditions.json";FILE* f=std::fopen(p.c_str(),"w");if(f){
        std::fprintf(f,"{\n  \"schema\": \"dem_boundary_conditions_v1\",\n  \"phase\": 1,\n  \"bridge_path\": \"%s\"\n}\n",dem.path.path.c_str());std::fclose(f);}}
    {auto p=out+"/dem_particle_seed_table.tsv";FILE* f=std::fopen(p.c_str(),"w");if(f){
        std::fprintf(f,"particle_id\tx_m\ty_m\tz_m\tradius_m\tdensity_kg_m3\tspecies\n");
        std::fprintf(f,"0\t0.0\t0.0\t0.0\t0.001\t%.1f\tstub_phase1\n",dem.particle_density_kg_m3);std::fclose(f);}}}
void execute_dem_bridges(const VsimDocument& doc, const std::string& output_dir){
    if(doc.bridge_objects.dem_bridges.empty())return;
    const char* BOLD="\033[1m",*RESET="\033[0m",*GREEN="\033[32m",*YELLOW="\033[33m",*RED="\033[31m",*DIM="\033[2m";
    std::printf("\n%s-- DEM Bridge  (WO-67N Phase 1)%s\n",BOLD,RESET);
    bool any_err=false;
    for(const auto& dem:doc.bridge_objects.dem_bridges){
        auto diags=validate_dem_bridge(dem,doc);bool has_err=false;
        for(const auto& d:diags){if(d.level=="ERROR"){has_err=true;any_err=true;}
            std::printf("  %s[%s]%s  %s\n",d.level=="ERROR"?RED:YELLOW,d.level.c_str(),RESET,d.message.c_str());}
        if(!has_err){
            std::string dir=output_dir.empty()?"reports":output_dir;
            std::string sub=dem.path.path;for(char& c:sub)if(c=='.'||c=='/')c='_';
            dir+="/dem_"+sub;export_dem_bridge(dem,doc,dir);
            std::printf("  %s[DEM]%s  %s%s%s  ->  %s\n",GREEN,RESET,BOLD,dem.path.path.c_str(),RESET,dir.c_str());
        }else std::printf("  %s[DEM]%s  %s  skipped (errors)\n",RED,RESET,dem.path.path.c_str());}
    if(!any_err)std::printf("%s  DEM bridge(s): OK%s  %s(solver deferred v5.2.0)%s\n",GREEN,RESET,DIM,RESET);}
} // namespace vsim