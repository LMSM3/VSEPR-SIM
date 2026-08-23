/**
 * cmd_batch_sweep.cpp  -  Gaussian / multi-distribution parameter sweep engine
 * WO-BATCH-SWEEP  |  v5.0.0
 */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif
#include "cli/cmd_batch_sweep.hpp"
#include "cli/cmd_run_vsim.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace vsepr::cli {
namespace {
#ifdef _WIN32
static bool vt_sw = []()->bool{
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE); DWORD m=0;
    if(GetConsoleMode(h,&m)){SetConsoleMode(h,m|ENABLE_VIRTUAL_TERMINAL_PROCESSING);return true;}
    return false;}();
#endif
static const char* CR="\033[0m";
static const char* CB="\033[1m";
static const char* CD="\033[2m";
static const char* CG="\033[0;32m";
static const char* CY="\033[1;33m";
static const char* CE="\033[0;31m";

static std::string sw_trim(const std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,b-a+1);}
static std::string sw_lower(const std::string& s){
    std::string r=s;for(auto&c:r)c=static_cast<char>(std::tolower((unsigned char)c));return r;}
static std::vector<std::string> sw_split(const std::string& s,char d){
    std::vector<std::string> v;std::stringstream ss(s);std::string t;
    while(std::getline(ss,t,d))v.push_back(t);return v;}

// Minimal JSON helpers
static bool jstr(const std::string& j,const std::string& k,std::string& out){
    std::string n="\""+k+"\"";
    auto p=j.find(n);
    while(p!=std::string::npos){
        auto c=j.find(':',p+n.size());if(c==std::string::npos)break;
        auto vs=c+1;while(vs<j.size()&&std::isspace((unsigned char)j[vs]))++vs;
        if(vs>=j.size())break;
        if(j[vs]=='"'){auto ve=j.find('"',vs+1);if(ve==std::string::npos)break;
            out=j.substr(vs+1,ve-vs-1);return true;}
        p=j.find(n,p+1);}return false;}
static bool jnum(const std::string& j,const std::string& k,double& out){
    std::string n="\""+k+"\"";auto p=j.find(n);
    while(p!=std::string::npos){
        auto c=j.find(':',p+n.size());if(c==std::string::npos)break;
        auto vs=c+1;while(vs<j.size()&&std::isspace((unsigned char)j[vs]))++vs;
        char*e=nullptr;double v=std::strtod(j.c_str()+vs,&e);
        if(e!=j.c_str()+vs){out=v;return true;}
        p=j.find(n,p+1);}return false;}
static bool jint(const std::string& j,const std::string& k,int& out){
    double d=0;if(jnum(j,k,d)){out=static_cast<int>(std::lround(d));return true;}return false;}
static bool jarr(const std::string& j,const std::string& k,std::string& out){
    std::string n="\""+k+"\"";auto p=j.find(n);if(p==std::string::npos)return false;
    auto b=j.find('[',p+n.size());if(b==std::string::npos)return false;
    int d=1;auto c=b+1;while(c<j.size()&&d>0){if(j[c]=='[')++d;else if(j[c]==']')--d;++c;}
    out=j.substr(b,c-b);return true;}
static std::vector<std::string> jobjs(const std::string& a){
    std::vector<std::string> v;int d=0;size_t s=std::string::npos;
    for(size_t i=0;i<a.size();++i){
        if(a[i]=='{'){if(d==0)s=i;++d;}
        else if(a[i]=='}'){--d;if(d==0&&s!=std::string::npos){v.push_back(a.substr(s,i-s+1));s=std::string::npos;}}}
    return v;}

// Distribution helpers
static SweepDist pdist(const std::string& s){
    auto l=sw_lower(sw_trim(s));
    if(l=="normal"||l=="gaussian")return SweepDist::Normal;
    if(l=="uniform")return SweepDist::Uniform;
    if(l=="log_normal"||l=="lognormal")return SweepDist::LogNormal;
    if(l=="truncated_normal"||l=="truncnorm")return SweepDist::TruncatedNormal;
    return SweepDist::Normal;}
static const char* dname(SweepDist d){
    switch(d){case SweepDist::Normal:return "normal";case SweepDist::Uniform:return "uniform";
    case SweepDist::LogNormal:return "log_normal";case SweepDist::TruncatedNormal:return "truncated_normal";}
    return "normal";}

// Gaussian quantile (Beasley-Springer-Moro approximation)
static double nq(double p){
    static const double a[]={2.50662823884,-18.61500062529,41.39119773534,-25.44106049637};
    static const double b[]={-8.47351093090,23.08336743743,-21.06224101826,3.13082909833};
    static const double c[]={0.3374754822726147,0.9761690190917186,0.1607979714918209,
        0.0276438810333863,0.0038405729373609,0.0003951896511349,
        0.0000321767881768,0.0000002888167364,0.0000003960315187};
    double q=p-0.5;
    if(std::abs(q)<=0.42){double r=q*q;return q*(((a[3]*r+a[2])*r+a[1])*r+a[0])/((((b[3]*r+b[2])*r+b[1])*r+b[0])*r+1.0);}
    double r=(q>0)?std::log(-std::log(1.0-p)):std::log(-std::log(p));
    double x=c[0]+r*(c[1]+r*(c[2]+r*(c[3]+r*(c[4]+r*(c[5]+r*(c[6]+r*(c[7]+r*c[8])))))));
    return (q>0)?x:-x;}
static double ncdf(double x){return 0.5*std::erfc(-x/std::sqrt(2.0));}
// Sampler
static std::vector<double> sample_axis(const SweepAxis& ax,std::mt19937_64& rng,SweepDesign design,int global_n){
    int n=(design==SweepDesign::Factorial)?ax.n:global_n;
    std::vector<double> vals;vals.reserve(static_cast<size_t>(n));
    const double lo=ax.min_val,hi=ax.max_val;
    const bool hlo=lo>-1e29,hhi=hi<1e29;
    auto clamp=[&](double v)->double{if(hlo&&v<lo)v=lo;if(hhi&&v>hi)v=hi;return v;};
    if(design==SweepDesign::LatinHypercube){
        std::uniform_real_distribution<double> jit(0.0,1.0/n);
        for(int i=0;i<n;++i){
            double u=(i+jit(rng))/n,v=0.0;
            switch(ax.dist){
            case SweepDist::Normal:v=clamp(ax.mean+ax.sigma*nq(u));break;
            case SweepDist::TruncatedNormal:{double pa=hlo?ncdf((lo-ax.mean)/ax.sigma):0.0;double pb=hhi?ncdf((hi-ax.mean)/ax.sigma):1.0;double pu=pa+u*(pb-pa);v=clamp(ax.mean+ax.sigma*nq(std::max(1e-9,std::min(1.0-1e-9,pu))));break;}
            case SweepDist::Uniform:{double ul=hlo?lo:ax.mean-3.0*ax.sigma,uh=hhi?hi:ax.mean+3.0*ax.sigma;v=ul+u*(uh-ul);break;}
            case SweepDist::LogNormal:v=clamp(std::exp(ax.mean+ax.sigma*nq(u)));break;}
            vals.push_back(ax.integer_valued?std::round(v):v);}
        std::shuffle(vals.begin(),vals.end(),rng);
    }else{
        std::normal_distribution<double> nd(ax.mean,ax.sigma);
        double ul=hlo?lo:ax.mean-3.0*ax.sigma,uh=hhi?hi:ax.mean+3.0*ax.sigma;
        std::uniform_real_distribution<double> ud(ul,uh);
        for(int i=0;i<n;++i){
            double v=0.0;
            switch(ax.dist){
            case SweepDist::Normal:case SweepDist::TruncatedNormal:{int t=0;do{v=nd(rng);++t;}while(t<1000&&((hlo&&v<lo)||(hhi&&v>hi)));v=clamp(v);break;}
            case SweepDist::Uniform:v=ud(rng);break;
            case SweepDist::LogNormal:{std::lognormal_distribution<double> lnd(ax.mean,ax.sigma);v=clamp(lnd(rng));break;}}
            vals.push_back(ax.integer_valued?std::round(v):v);}}
    return vals;}

// vsim text patcher
static bool is_sec_hdr(const std::string& line,std::string& name){
    size_t a=line.find('[');if(a==std::string::npos)return false;
    for(size_t i=0;i<a;++i)if(!std::isspace((unsigned char)line[i]))return false;
    size_t b=line.rfind(']');if(b==std::string::npos||b<=a)return false;
    size_t ia=a,ib=b;while(ia<line.size()&&line[ia]=='[')++ia;while(ib>0&&line[ib]==']')--ib;
    if(ib<ia)return false;name=sw_trim(line.substr(ia,ib-ia+1));return true;}
struct PT{std::string sec,sub,key;bool has_sub=false;};
static PT parse_tgt(const std::string& t){
    PT pt;
    auto br=t.find('[');
    if(br!=std::string::npos){auto cl=t.find(']',br);pt.sec=t.substr(0,br);pt.sub=t.substr(br+1,cl-br-1);pt.key=t.substr(cl+2);pt.has_sub=true;return pt;}
    auto d=t.find('.');if(d==std::string::npos){pt.sec=pt.key=t;return pt;}
    pt.sec=t.substr(0,d);std::string rest=t.substr(d+1);auto d2=rest.find('.');
    if(d2!=std::string::npos){pt.sub=rest.substr(0,d2);pt.key=rest.substr(d2+1);pt.has_sub=true;}else pt.key=rest;return pt;}
static std::string fmtv(double v,bool iv){
    if(iv){std::ostringstream o;o<<static_cast<long long>(std::llround(v));return o.str();}
    std::ostringstream o;o<<std::setprecision(10)<<v;return o.str();}
static std::string patch_vsim(const std::string& text,const PT& pt,double value,bool iv){
    std::vector<std::string> lines;{std::istringstream is(text);std::string l;while(std::getline(is,l))lines.push_back(l);}
    const std::string vs=fmtv(value,iv);
    const std::string& sec=pt.sec,&sub=pt.sub,&key=pt.key;
    bool is_mol=(sec=="molecule");
    auto bsn=[&]()->std::string{if(is_mol)return "simulation.molecule";if(pt.has_sub&&!sub.empty())return sec+"."+sub;return sec;};
    std::string tsec=bsn();int mif=0,mit=0;
    if(is_mol){try{mit=std::stoi(sub);}catch(...){mit=0;}}
    bool in_t=false;int il=-1,rl=-1;
    for(size_t i=0;i<lines.size();++i){
        std::string sn;
        if(is_sec_hdr(lines[i],sn)){
            if(in_t&&rl==-1&&il==-1)il=static_cast<int>(i);
            in_t=false;
            if(sw_lower(sn)==sw_lower(tsec)){if(is_mol){if(mif==mit)in_t=true;++mif;}else in_t=true;}
        }else if(in_t){
            std::string s=sw_trim(lines[i]);
            if(s.size()>key.size()){std::string lk=sw_lower(s.substr(0,key.size()));std::string tk=sw_lower(key);
                if(lk==tk){size_t p=key.size();while(p<s.size()&&std::isspace((unsigned char)s[p]))++p;if(p<s.size()&&s[p]=='='){rl=static_cast<int>(i);break;}}}}}
    if(rl>=0){auto&l=lines[static_cast<size_t>(rl)];auto eq=l.find('=');
        if(eq!=std::string::npos){std::string lhs=l.substr(0,eq+1),cmt;auto h=l.find('#',eq+1);if(h!=std::string::npos)cmt="  "+sw_trim(l.substr(h));l=lhs+" "+vs+cmt;}}
    else{if(il<0&&in_t)il=static_cast<int>(lines.size());
        if(il>=0)lines.insert(lines.begin()+il,key+" = "+vs);
        else{lines.push_back("");
            if(is_mol)lines.push_back("[[simulation.molecule]]");
            else if(pt.has_sub&&!sub.empty())lines.push_back("["+sec+"."+sub+"]");
            else lines.push_back("["+sec+"]");
            lines.push_back(key+" = "+vs);}}
    std::ostringstream out;for(size_t i=0;i<lines.size();++i){out<<lines[i];if(i+1<lines.size())out<<"\n";}return out.str();}

// Job execution
struct SweepJob{std::string path,label;std::vector<std::pair<std::string,double>> params;int exit_code=-1;double elapsed_ms=0.0;bool ran=false;};
static void run_sj(SweepJob& j){auto t0=std::chrono::steady_clock::now();j.ran=true;j.exit_code=vsepr::cli::cmd_run_vsim({j.path});auto t1=std::chrono::steady_clock::now();j.elapsed_ms=std::chrono::duration<double,std::milli>(t1-t0).count();}
static std::mutex g_smtx;
static void print_sj(size_t i,size_t t,const SweepJob& j,bool q){if(q)return;std::lock_guard<std::mutex> lk(g_smtx);const char*sc=(j.exit_code==0)?CG:CE,*ss=(j.exit_code==0)?"PASS":"FAIL";std::fprintf(stdout,"  %s[%zu/%zu]%s  %s%-4s%s  %s%.0fms%s  %s\n",CD,i,t,CR,sc,ss,CR,CD,j.elapsed_ms,CR,j.label.c_str());std::fflush(stdout);}

// Cartesian product
static void cexp(const std::vector<std::vector<double>>& s,std::vector<std::vector<double>>& c,std::vector<double>& cur,size_t d){if(d==s.size()){c.push_back(cur);return;}for(auto v:s[d]){cur.push_back(v);cexp(s,c,cur,d+1);cur.pop_back();}}

// JSON report
static std::string jesc(const std::string& s){std::string r;for(char c:s){if(c=='"')r+="\\\"";else if(c=='\\')r+="\\\\";else if(c=='\n')r+="\\n";else r+=c;}return r;}
static void write_report(const std::string& path,const std::vector<SweepJob>& jobs,const SweepConfig& cfg,double wms){
    std::ofstream f(path);if(!f){std::fprintf(stderr,"[sweep] cannot write report: %s\n",path.c_str());return;}
    int p=0,fl=0,sk=0;for(auto&j:jobs){if(!j.ran)++sk;else if(j.exit_code==0)++p;else++fl;}
    auto now=std::time(nullptr);char ts[32];std::strftime(ts,sizeof(ts),"%Y-%m-%dT%H:%M:%S",std::localtime(&now));
    f<<"{\n"<<"  \"label\": \""<<jesc(cfg.label.empty()?"vsepr-sweep":cfg.label)<<"\",\n"
     <<"  \"timestamp\": \""<<ts<<"\",\n"<<"  \"base_vsim\": \""<<jesc(cfg.base_vsim)<<"\",\n"
     <<"  \"total_ms\": "<<std::fixed<<std::setprecision(1)<<wms<<",\n"
     <<"  \"summary\": { \"passed\": "<<p<<", \"failed\": "<<fl<<", \"skipped\": "<<sk<<" },\n"
     <<"  \"axes\": [\n";
    for(size_t ai=0;ai<cfg.axes.size();++ai){auto&ax=cfg.axes[ai];
        f<<"    { \"name\": \""<<jesc(ax.name)<<"\", \"target\": \""<<jesc(ax.target)<<"\", \"distribution\": \""<<dname(ax.dist)<<"\", \"mean\": "<<ax.mean<<", \"sigma\": "<<ax.sigma<<", \"n\": "<<ax.n<<" }";
        if(ai+1<cfg.axes.size())f<<",";f<<"\n";}
    f<<"  ],\n  \"jobs\": [\n";
    for(size_t i=0;i<jobs.size();++i){auto&j=jobs[i];
        f<<"    { \"label\": \""<<jesc(j.label)<<"\", \"exit_code\": "<<j.exit_code<<", \"elapsed_ms\": "<<std::fixed<<std::setprecision(1)<<j.elapsed_ms<<", \"ran\": "<<(j.ran?"true":"false")<<", \"params\": {";
        for(size_t pi=0;pi<j.params.size();++pi){f<<" \""<<jesc(j.params[pi].first)<<"\": "<<j.params[pi].second;if(pi+1<j.params.size())f<<",";}
        f<<" } }";if(i+1<jobs.size())f<<",";f<<"\n";}
    f<<"  ]\n}\n";}

// Help
static void print_sweep_help(const char* prog){
    std::cout<<CB<<"vsepr-batch sweep"<<CR<<"  --  Gaussian parameter sweep engine  (v5.0.0)\n\n"
        <<CB<<"Usage\n"<<CR<<"  "<<prog<<" --base <script.vsim> [--axis SPEC ...] [options]\n"
        <<"  "<<prog<<" --sweep-config <file.bsweep> [options]\n\n"
        <<CB<<"Input\n"<<CR
        <<"  --base  <path.vsim>              Base script to clone and patch\n"
        <<"  --sweep-config  <file.bsweep>    JSON config (base + axes + design)\n\n"
        <<CB<<"Axis spec  (--axis, repeatable)\n"<<CR
        <<"  \"name:target:dist:mean:sigma[:min:max[:N]]\"\n"
        <<"  target    dot-path, e.g. run.temperature_K\n"
        <<"  dist      normal | uniform | log_normal | truncated_normal\n"
        <<"  mean/sigma/min/max/N as appropriate\n\n"
        <<CB<<"Sweep design  (--sweep-design)\n"<<CR
        <<"  factorial        Cartesian product  (runs = prod Ni * replicates)\n"
        <<"  joint            zip axes           (runs = max Ni * replicates)\n"
        <<"  latin_hypercube  stratified joint   (runs = N * replicates)\n\n"
        <<CB<<"Options\n"<<CR
        <<"  --replicates N  --sweep-out <dir>  --sweep-seed N  --max-jobs N\n"
        <<"  --keep-generated  --jobs N  --stop-on-fail  --dry-run\n"
        <<"  --report <path>  --label <text>  --verbose/-v  --quiet/-q\n\n"
        <<CB<<"Supported targets\n"<<CR
        <<"  run.temperature_K  run.pressure_GPa  run.max_steps  run.dt_fs\n"
        <<"  simulation.box_size_ang  simulation.fire_dt_fs  simulation.fire_max_steps\n"
        <<"  simulation.ewald_alpha  simulation.ewald_rcut  simulation.ewald_kmax\n"
        <<"  environment.temperature  environment.pressure  environment.humidity\n"
        <<"  environment.field_x  environment.field_y  environment.field_z\n"
        <<"  molecule[N].temperature_K  molecule[N].count  molecule[N].velocity_drift\n"
        <<"  molecule[N].n_layers  seed.foundation  cell.lx  cell.ly  cell.lz\n"
        <<"  excite.<name>.intensity  excite.<name>.fluence  excite.<name>.pulse_width_fs\n\n"
        <<CB<<"Example\n"<<CR
        <<"  "<<prog<<" --base demo.vsim \\\n"
        <<"    --axis \"T:run.temperature_K:normal:300:80:100:900:9\" \\\n"
        <<"    --axis \"P:run.pressure_GPa:uniform:0:0:0:2:5\" \\\n"
        <<"    --sweep-design factorial --jobs 4 --report out/sweep.json\n";}

} // anonymous namespace

// ===========================================================================
// Public implementations
// ===========================================================================
bool load_sweep_config(const std::string& path,SweepConfig& out){
    std::ifstream f(path);if(!f){std::fprintf(stderr,"[sweep] cannot open config: %s\n",path.c_str());return false;}
    std::string j((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    jstr(j,"base",out.base_vsim);
    {std::string ds;if(jstr(j,"design",ds)){auto l=sw_lower(sw_trim(ds));if(l=="joint")out.design=SweepDesign::Joint;else if(l=="latin_hypercube")out.design=SweepDesign::LatinHypercube;else out.design=SweepDesign::Factorial;}}
    jint(j,"replicates",out.replicates);jint(j,"max_jobs",out.max_jobs);
    {std::string od;if(jstr(j,"out_dir",od))out.out_dir=od;}
    {double sd=0;if(jnum(j,"sweep_seed",sd))out.sweep_seed=static_cast<uint64_t>(sd);}
    std::string at;if(!jarr(j,"axes",at)){std::fprintf(stderr,"[sweep] no axes array\n");return false;}
    for(auto&obj:jobjs(at)){SweepAxis ax;jstr(obj,"name",ax.name);jstr(obj,"target",ax.target);
        {std::string ds;if(jstr(obj,"distribution",ds))ax.dist=pdist(ds);}
        jnum(obj,"mean",ax.mean);jnum(obj,"sigma",ax.sigma);
        {double v;if(jnum(obj,"min",v))ax.min_val=v;if(jnum(obj,"max",v))ax.max_val=v;}
        {int n=ax.n;jint(obj,"n",n);ax.n=n;}
        if(ax.target.empty()){std::fprintf(stderr,"[sweep] axis missing target -- skipped\n");continue;}
        if(ax.name.empty())ax.name=ax.target;out.axes.push_back(ax);}
    return !out.axes.empty();}

bool parse_axis_spec(const std::string& spec,SweepAxis& out){
    auto p=sw_split(spec,':');if(p.size()<5){std::fprintf(stderr,"[sweep] --axis needs name:target:dist:mean:sigma\n");return false;}
    out.name=sw_trim(p[0]);out.target=sw_trim(p[1]);out.dist=pdist(p[2]);
    try{out.mean=std::stod(p[3]);out.sigma=std::stod(p[4]);}catch(...){std::fprintf(stderr,"[sweep] --axis parse error: %s\n",spec.c_str());return false;}
    if(p.size()>=7){try{out.min_val=std::stod(p[5]);out.max_val=std::stod(p[6]);}catch(...){}}
    if(p.size()>=8){try{out.n=std::stoi(p[7]);}catch(...){}}
    if(out.name.empty())out.name=out.target;return true;}

int cmd_batch_sweep(const std::vector<std::string>& args){
    SweepConfig cfg;std::string scp;
    for(size_t i=0;i<args.size();++i){
        const auto&a=args[i];
        auto next=[&]()->std::string{if(i+1<args.size())return args[++i];std::fprintf(stderr,"[sweep] %s requires an argument\n",a.c_str());std::exit(2);};
        if(a=="--help"||a=="-h"){print_sweep_help("vsepr-batch");return 0;}
        else if(a=="--base")cfg.base_vsim=next();
        else if(a=="--sweep-config")scp=next();
        else if(a=="--sweep-out")cfg.out_dir=next();
        else if(a=="--sweep-seed"){try{cfg.sweep_seed=std::stoull(next());}catch(...){}}
        else if(a=="--max-jobs"){try{cfg.max_jobs=std::stoi(next());}catch(...){}}
        else if(a=="--replicates"){try{cfg.replicates=std::stoi(next());}catch(...){}}
        else if(a=="--jobs"){try{cfg.jobs=std::stoi(next());}catch(...){}}
        else if(a=="--report")cfg.report_path=next();
        else if(a=="--label")cfg.label=next();
        else if(a=="--sweep-design"){std::string d=sw_lower(sw_trim(next()));if(d=="joint")cfg.design=SweepDesign::Joint;else if(d=="latin_hypercube")cfg.design=SweepDesign::LatinHypercube;else cfg.design=SweepDesign::Factorial;}
        else if(a=="--axis"){SweepAxis ax;if(parse_axis_spec(next(),ax))cfg.axes.push_back(ax);}
        else if(a=="--stop-on-fail")cfg.stop_fail=true;
        else if(a=="--dry-run")cfg.dry_run=true;
        else if(a=="--keep-generated")cfg.keep=true;
        else if(a=="--verbose"||a=="-v")cfg.verbose=true;
        else if(a=="--quiet"||a=="-q")cfg.quiet=true;
        else{std::fprintf(stderr,"[sweep] unknown option: %s\n",a.c_str());return 2;}}

    if(!scp.empty()){SweepConfig fc;if(!load_sweep_config(scp,fc))return 2;
        if(cfg.base_vsim.empty())cfg.base_vsim=fc.base_vsim;
        if(cfg.design==SweepDesign::Factorial&&fc.design!=SweepDesign::Factorial)cfg.design=fc.design;
        if(cfg.replicates==1&&fc.replicates>1)cfg.replicates=fc.replicates;
        if(cfg.max_jobs==2000&&fc.max_jobs!=2000)cfg.max_jobs=fc.max_jobs;
        std::vector<SweepAxis> m=fc.axes;for(auto&ax:cfg.axes)m.push_back(ax);cfg.axes=std::move(m);}

    if(cfg.base_vsim.empty()){std::fprintf(stderr,"[sweep] no base .vsim -- use --base or --sweep-config\n");return 2;}
    if(cfg.axes.empty()){std::fprintf(stderr,"[sweep] no axes -- use --axis or --sweep-config\n");return 2;}
    if(cfg.label.empty())cfg.label="vsepr-sweep";
    if(cfg.jobs<1)cfg.jobs=1;if(cfg.jobs>64)cfg.jobs=64;if(cfg.replicates<1)cfg.replicates=1;

    // Read base .vsim
    std::string base_text;
    {std::ifstream bf(cfg.base_vsim);if(!bf){std::fprintf(stderr,"[sweep] cannot open base: %s\n",cfg.base_vsim.c_str());return 2;}
    base_text=std::string((std::istreambuf_iterator<char>(bf)),std::istreambuf_iterator<char>());}

    // RNG
    std::mt19937_64 rng;
    if(cfg.sweep_seed==0)rng.seed(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    else rng.seed(cfg.sweep_seed);

    // Sample axes
    int gn=9;if(cfg.design!=SweepDesign::Factorial)for(auto&ax:cfg.axes)gn=std::max(gn,ax.n);
    std::vector<std::vector<double>> as;for(auto&ax:cfg.axes)as.push_back(sample_axis(ax,rng,cfg.design,gn));

    // Expand combinations
    std::vector<std::vector<double>> combos;
    if(cfg.design==SweepDesign::Factorial){std::vector<double> cur;cexp(as,combos,cur,0);}
    else{for(int i=0;i<gn;++i){std::vector<double> row;for(size_t ai=0;ai<as.size();++ai)row.push_back(as[ai][std::min((size_t)i,as[ai].size()-1)]);combos.push_back(row);}}

    // Replicates
    size_t bc=combos.size();for(int r=1;r<cfg.replicates;++r)for(size_t c=0;c<bc;++c)combos.push_back(combos[c]);

    // Safety cap
    if(static_cast<int>(combos.size())>cfg.max_jobs){std::fprintf(stderr,"[sweep] %zu combinations exceeds --max-jobs %d\n",combos.size(),cfg.max_jobs);return 2;}

    // Output dir
    if(cfg.out_dir.empty()){auto now=std::time(nullptr);char ts[20];std::strftime(ts,sizeof(ts),"%Y%m%d_%H%M%S",std::localtime(&now));cfg.out_dir="batch_out/sweep_"+std::string(ts);}
    std::error_code ec;fs::create_directories(cfg.out_dir,ec);
    if(ec){std::fprintf(stderr,"[sweep] cannot create output dir: %s\n",cfg.out_dir.c_str());return 2;}

    // Generate .vsim files
    std::vector<SweepJob> jobs;jobs.reserve(combos.size());
    for(size_t ci=0;ci<combos.size();++ci){
        SweepJob sj;std::string pat=base_text;std::ostringstream lbl;
        lbl<<"run_"<<std::setw(4)<<std::setfill('0')<<(ci+1);
        for(size_t ai=0;ai<cfg.axes.size();++ai){auto&ax=cfg.axes[ai];double v=combos[ci][ai];
            PT pt=parse_tgt(ax.target);pat=patch_vsim(pat,pt,v,ax.integer_valued);
            sj.params.push_back({ax.name,v});lbl<<"_"<<ax.name<<"="<<fmtv(v,ax.integer_valued);}
        sj.label=lbl.str();sj.path=(fs::path(cfg.out_dir)/(sj.label+".vsim")).string();
        {std::ofstream out(sj.path);if(!out){std::fprintf(stderr,"[sweep] cannot write: %s\n",sj.path.c_str());return 2;}out<<pat;}
        jobs.push_back(std::move(sj));}

    // Header
    if(!cfg.quiet){
        const char* dn=(cfg.design==SweepDesign::Factorial)?"factorial":(cfg.design==SweepDesign::Joint)?"joint":"lhs";
        std::cout<<CB<<"VSEPR Sweep Inlet"<<CR<<"  ["<<cfg.label<<"]  design="<<dn<<"  axes="<<cfg.axes.size()<<"  runs="<<jobs.size()<<"  threads="<<cfg.jobs<<"\n";
        std::printf("%s%-20s%-30s%-18s%-12s%-12s  N%s\n",CB,"Axis","Target","Distribution","Mean","Sigma",CR);
        std::cout<<std::string(96,'-')<<"\n";
        for(auto&ax:cfg.axes)std::printf("%-20s%-30s%-18s%-12g%-12g  %d\n",ax.name.c_str(),ax.target.c_str(),dname(ax.dist),ax.mean,ax.sigma,ax.n);
        std::cout<<std::string(96,'-')<<"\n";}

    // Dry run
    if(cfg.dry_run){
        for(size_t i=0;i<jobs.size();++i){std::cout<<"  ["<<(i+1)<<"] "<<jobs[i].label<<"\n";for(auto&p:jobs[i].params)std::cout<<"        "<<CD<<p.first<<" = "<<p.second<<CR<<"\n";}
        std::cout<<"(dry-run; files in "<<cfg.out_dir<<")\n";
        if(!cfg.keep){std::error_code re;fs::remove_all(cfg.out_dir,re);}return 0;}

    // Execute
    auto wt0=std::chrono::steady_clock::now();
    std::atomic<bool> af{false};std::atomic<size_t> nj{0};const size_t tot=jobs.size();
    auto worker=[&](){while(true){if(af.load())break;size_t idx=nj.fetch_add(1);if(idx>=tot)break;run_sj(jobs[idx]);print_sj(idx+1,tot,jobs[idx],cfg.quiet);if(jobs[idx].exit_code!=0&&cfg.stop_fail)af.store(true);}};
    if(cfg.jobs==1){worker();}else{std::vector<std::thread> th;th.reserve(static_cast<size_t>(cfg.jobs));for(int t=0;t<cfg.jobs;++t)th.emplace_back(worker);for(auto&t:th)t.join();}
    auto wt1=std::chrono::steady_clock::now();double wms=std::chrono::duration<double,std::milli>(wt1-wt0).count();
    for(auto&j:jobs)if(!j.ran)j.exit_code=-1;
    int p=0,fl=0,sk=0;for(auto&j:jobs){if(!j.ran)++sk;else if(j.exit_code==0)++p;else++fl;}
    if(!cfg.quiet){
        std::cout<<std::string(96,'-')<<"\n";
        std::printf("  %s%-8s%s  %s%-8s%s  %s%-8s%s  wall %.0fms\n",CG,(std::to_string(p)+" pass").c_str(),CR,CE,(std::to_string(fl)+" fail").c_str(),CR,CY,(std::to_string(sk)+" skip").c_str(),CR,wms);
        if(fl>0){std::cout<<"\n"<<CE<<CB<<"Failed runs:\n"<<CR;for(auto&j:jobs)if(j.ran&&j.exit_code!=0)std::cout<<"  "<<j.label<<"\n";}
        std::cout<<"\n";}
    if(!cfg.keep&&fl==0){std::error_code re;fs::remove_all(cfg.out_dir,re);}
    else if(!cfg.keep&&!cfg.quiet)std::cout<<CD<<"[sweep] keeping files in "<<cfg.out_dir<<" (failures)\n"<<CR;
    if(!cfg.report_path.empty())write_report(cfg.report_path,jobs,cfg,wms);
    return (fl>0)?1:0;}

} // namespace vsepr::cli
