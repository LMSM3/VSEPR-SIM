// WO-67O FEA Bridge validate + export stubs (Phase 1)
#include "vsim/bridge/fea_bridge.hpp"
#include "vsim/vsim_document.hpp"
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
namespace vsim {
std::vector<FEABridgeDiagnostic> validate_fea_bridge(const FEABridgeObject& fea, const VsimDocument&){
    std::vector<FEABridgeDiagnostic> d;
    if(!fea.has_source()) d.push_back({80,"ERROR","VSIM-E080: FEABridge '"+fea.path.path+"' has no source surface (set from/surface)",fea.source_line});
    if(!fea.has_target()) d.push_back({81,"ERROR","VSIM-E081: FEABridge '"+fea.path.path+"' has no target geometry (set target/geometry)",fea.source_line});
    if(fea.material.empty()||fea.material=="unknown") d.push_back({82,"WARN","VSIM-W082: FEABridge '"+fea.path.path+"' material absent",fea.source_line});
    if(fea.mapping_mode.empty()) d.push_back({83,"WARN","VSIM-W083: FEABridge '"+fea.path.path+"' mapping_mode not specified; defaulted to surface_to_mesh",fea.source_line});
    if(fea.criterion=="von_mises") d.push_back({84,"WARN","VSIM-W084: FEABridge '"+fea.path.path+"' Von Mises values are Phase 1 proxy estimates",fea.source_line});
    if(!fea.has_yield_strength()) d.push_back({85,"WARN","VSIM-W085: FEABridge '"+fea.path.path+"' yield_strength absent",fea.source_line});
    return d;}
void export_fea_bridge(const FEABridgeObject& fea, const VsimDocument&, const std::string& out){
    std::filesystem::create_directories(out);
    {auto p=out+"/fea_bridge_manifest.json";FILE* f=std::fopen(p.c_str(),"w");if(f){
        std::time_t t=std::time(nullptr);char ts[32];std::strftime(ts,sizeof(ts),"%Y-%m-%dT%H:%M:%SZ",std::gmtime(&t));
        std::fprintf(f,"{\n  \"schema\": \"fea_bridge_manifest_v1\",\n  \"generated\": \"%s\",\n  \"phase\": 1,\n  \"phase_note\": \"Phase 1 stub. FEA solver deferred v5.2.0.\",\n  \"bridge_path\": \"%s\",\n  \"source_surface\": \"%s\",\n  \"target_geometry\": \"%s\",\n  \"material\": \"%s\",\n  \"mapping_mode\": \"%s\",\n  \"criterion\": \"%s\",\n  \"yield_strength_Pa\": %.1f,\n  \"fatigue_enabled\": %s\n}\n",
            ts,fea.path.path.c_str(),fea.source_surface.path.c_str(),fea.target_geometry.path.c_str(),
            fea.material.c_str(),fea.mapping_mode.c_str(),fea.criterion.c_str(),
            fea.yield_strength_Pa,fea.fatigue_enabled?"true":"false");std::fclose(f);}}
    if(fea.export_tsv){auto p=out+"/fea_surface_loads.tsv";FILE* f=std::fopen(p.c_str(),"w");if(f){
        bool hy=fea.has_yield_strength();
        if(hy)std::fprintf(f,"node_id\tpressure_Pa\tshear_Pa\tvon_mises_proxy_Pa\tyield_ratio_proxy\n");
        else  std::fprintf(f,"node_id\tpressure_Pa\tshear_Pa\tvon_mises_proxy_Pa\n");
        double P[]={101325,108000,104500,99800},S[]={1200,1450,1100,980};
        for(int i=0;i<4;++i){double vm=std::sqrt(P[i]*P[i]+3.0*S[i]*S[i]);
            if(hy)std::fprintf(f,"%d\t%.2f\t%.2f\t%.2f\t%.4f\n",i,P[i],S[i],vm,vm/fea.yield_strength_Pa);
            else  std::fprintf(f,"%d\t%.2f\t%.2f\t%.2f\n",i,P[i],S[i],vm);}std::fclose(f);}}
    if(fea.export_json){
        {auto p=out+"/fea_load_map.json";FILE* f=std::fopen(p.c_str(),"w");if(f){
            std::fprintf(f,"{\n  \"schema\": \"fea_load_map_v1\",\n  \"phase\": 1,\n  \"bridge_path\": \"%s\",\n  \"mapping_mode\": \"%s\"\n}\n",fea.path.path.c_str(),fea.mapping_mode.c_str());std::fclose(f);}}
        {auto p=out+"/fea_material_regions.json";FILE* f=std::fopen(p.c_str(),"w");if(f){
            std::fprintf(f,"{\n  \"schema\": \"fea_material_regions_v1\",\n  \"material\": \"%s\",\n  \"criterion\": \"%s\",\n  \"yield_strength_Pa\": %.1f,\n  \"regions\": []\n}\n",
                fea.material.c_str(),fea.criterion.c_str(),fea.yield_strength_Pa);std::fclose(f);}}}}
void execute_fea_bridges(const VsimDocument& doc, const std::string& output_dir){
    if(doc.bridge_objects.fea_bridges.empty())return;
    const char* BOLD="\033[1m",*RESET="\033[0m",*GREEN="\033[32m",*YELLOW="\033[33m",*RED="\033[31m",*DIM="\033[2m";
    std::printf("\n%s-- FEA Bridge  (WO-67O Phase 1)%s\n",BOLD,RESET);
    bool any_err=false;
    for(const auto& fea:doc.bridge_objects.fea_bridges){
        auto diags=validate_fea_bridge(fea,doc);bool has_err=false;
        for(const auto& d:diags){if(d.level=="ERROR"){has_err=true;any_err=true;}
            std::printf("  %s[%s]%s  %s\n",d.level=="ERROR"?RED:YELLOW,d.level.c_str(),RESET,d.message.c_str());}
        if(!has_err){
            std::string dir=output_dir.empty()?"reports":output_dir;
            std::string sub=fea.path.path;for(char& c:sub)if(c=='.'||c=='/')c='_';
            dir+="/fea_"+sub;export_fea_bridge(fea,doc,dir);
            std::printf("  %s[FEA]%s  %s%s%s  ->  %s\n",GREEN,RESET,BOLD,fea.path.path.c_str(),RESET,dir.c_str());
        }else std::printf("  %s[FEA]%s  %s  skipped (errors)\n",RED,RESET,fea.path.path.c_str());}
    if(!any_err)std::printf("%s  FEA bridge(s): OK%s  %s(solver deferred v5.2.0)%s\n",GREEN,RESET,DIM,RESET);}
} // namespace vsim