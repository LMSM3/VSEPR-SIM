#include "EngineAdapter.h"

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/crystal/unit_cell.hpp"
#include "atomistic/crystal/supercell.hpp"
#include "io/xyz_format.hpp"

#include <future>
#include <random>
#include <cmath>
#include <map>
#include <sstream>
#include <iomanip>

namespace bridge {

namespace detail {

// doc_to_state: SceneDocument frame -> atomistic::State + element names.
// Routes through parsers::from_xyz so mass/type-map logic lives in one place.
// Existing velocities are restored so an MD trajectory can be continued.
static std::pair<atomistic::State, std::vector<std::string>>
doc_to_state(const scene::SceneDocument& doc)
{
    const auto& f = doc.current_frame();

    vsepr::io::XYZMolecule mol;
    mol.atoms.reserve(f.atoms.size());
    for (const auto& a : f.atoms)
        mol.atoms.emplace_back(a.symbol.empty() ? "C" : a.symbol, a.pos.x, a.pos.y, a.pos.z);
    for (const auto& b : f.bonds)
        mol.bonds.emplace_back(b.i, b.j, b.order);

    atomistic::State s = atomistic::parsers::from_xyz(mol);

    if (!f.velocities.empty() && (int)f.velocities.size() == f.atom_count())
        for (uint32_t i = 0; i < s.N; ++i)
            s.V[i] = {f.velocities[i].x, f.velocities[i].y, f.velocities[i].z};

    std::vector<std::string> names;
    names.reserve(mol.atoms.size());
    for (const auto& a : mol.atoms) names.push_back(a.element);
    return {std::move(s), std::move(names)};
}

// state_to_frame: pack a State into a FrameData.
// Writes the full EnergyTerms decomposition and RMS force into the properties map.
static scene::FrameData
state_to_frame(const atomistic::State& s, const std::vector<std::string>& names, const std::string& mode)
{
    scene::FrameData f;
    f.source_mode = mode;
    f.atoms.reserve(s.N);
    for (uint32_t i = 0; i < s.N; ++i) {
        scene::AtomRecord a;
        a.symbol = (i < names.size()) ? names[i] : "C";
        a.Z      = 0;
        a.pos    = {s.X[i].x, s.X[i].y, s.X[i].z};
        f.atoms.push_back(a);
    }
    for (const auto& e : s.B)
        f.bonds.push_back({(int)e.i, (int)e.j, 1.0});
    if (!s.V.empty()) {
        f.velocities.resize(s.N);
        for (uint32_t i = 0; i < s.N; ++i) f.velocities[i] = {s.V[i].x, s.V[i].y, s.V[i].z};
    }
    if (!s.F.empty()) {
        f.forces.resize(s.N);
        for (uint32_t i = 0; i < s.N; ++i) f.forces[i] = {s.F[i].x, s.F[i].y, s.F[i].z};
    }
    f.properties["energy_total"] = s.E.total();
    f.properties["energy_bond"]  = s.E.Ubond;
    f.properties["energy_angle"] = s.E.Uangle;
    f.properties["energy_tors"]  = s.E.Utors;
    f.properties["energy_vdw"]   = s.E.UvdW;
    f.properties["energy_coul"]  = s.E.UCoul;
    f.properties["energy_pol"]   = s.E.Upol;
    if (!s.F.empty() && s.N > 0) {
        double sum = 0.0;
        for (const auto& fi : s.F) sum += fi.x*fi.x + fi.y*fi.y + fi.z*fi.z;
        f.properties["force_rms"] = std::sqrt(sum / s.N);
    }
    return f;
}

// velocities_are_zero: true when all V[i] are effectively zero.
// Decides whether to sample Maxwell-Boltzmann before an MD run.
static bool velocities_are_zero(const atomistic::State& s)
{
    for (const auto& v : s.V)
        if (v.x*v.x + v.y*v.y + v.z*v.z > 1e-20) return false;
    return true;
}

struct EngineCore {
    std::unique_ptr<atomistic::IModel> model;
    atomistic::ModelParams             mp;
    std::mt19937                       rng;
    EngineCore() : rng(std::random_device{}()) { model = atomistic::create_lj_coulomb_model(); mp.rc = 10.0; }
};

} // namespace detail

EngineAdapter::EngineAdapter()  : core_(std::make_unique<detail::EngineCore>()) {}
EngineAdapter::~EngineAdapter() = default;

KernelResult EngineAdapter::run(const KernelRequest& req, ProgressFn /*progress*/)
{
    KernelResult res;
    try {
        // Apply environment context from every request into the model params.
        // This is the Phase 10 wiring: desktop sets req.env, kernel reads mp.env.
        core_->mp.env = req.env;

        switch (req.op) {

        case KernelOp::LoadXYZ: {
            vsepr::io::XYZReader reader; vsepr::io::XYZMolecule mol;
            if (!reader.read(req.file_path, mol)) { res.message = "Read failed: " + reader.get_error(); return res; }
            reader.detect_bonds(mol);
            atomistic::State s = atomistic::parsers::from_xyz(mol);
            std::vector<std::string> names;
            names.reserve(mol.atoms.size());
            for (const auto& a : mol.atoms) names.push_back(a.element);
            auto doc = std::make_shared<scene::SceneDocument>();
            doc->provenance.mode        = "import";
            doc->provenance.source_file = req.file_path;
            doc->provenance.formula     = mol.formula;
            doc->frames.push_back(detail::state_to_frame(s, names, "import"));
            res.output = doc; res.success = true;
            res.message = "Loaded " + std::to_string(mol.atoms.size()) + " atoms, " + std::to_string(mol.bonds.size()) + " bonds";
            break;
        }

        case KernelOp::SaveXYZ: {
            if (!req.input || req.input->empty()) { res.message = "Nothing to save"; return res; }
            auto [s, names] = detail::doc_to_state(*req.input);
            vsepr::io::XYZMolecule mol = atomistic::compilers::to_xyz(s, names);
            vsepr::io::XYZWriter writer;
            if (!writer.write(req.file_path, mol)) { res.message = "Write failed: " + writer.get_error(); return res; }
            res.success = true;
            res.message = "Saved " + std::to_string(s.N) + " atoms to " + req.file_path;
            break;
        }

        case KernelOp::SinglePoint: {
            if (!req.input || req.input->empty()) { res.message = "No structure loaded"; return res; }
            auto [s, names] = detail::doc_to_state(*req.input);
            core_->model->eval(s, core_->mp);
            auto doc = std::make_shared<scene::SceneDocument>(*req.input);
            doc->frames.push_back(detail::state_to_frame(s, names, "single_point"));
            doc->provenance.mode = "single_point";
            std::ostringstream msg; msg << std::fixed << std::setprecision(4) << "E = " << s.E.total() << " kcal/mol";
            res.output = doc; res.success = true; res.message = msg.str();
            break;
        }

        case KernelOp::Relax: {
            if (!req.input || req.input->empty()) { res.message = "No structure loaded"; return res; }
            auto [s, names] = detail::doc_to_state(*req.input);
            atomistic::FIREParams fp;
            fp.max_steps = req.max_steps; fp.epsF = req.force_tol; fp.dt = 1e-3; fp.dt_max = 1e-1;
            atomistic::FIRE fire(*core_->model, core_->mp);
            const auto stats = fire.minimize(s, fp);
            auto doc = std::make_shared<scene::SceneDocument>(*req.input);
            auto frame = detail::state_to_frame(s, names, "fire");
            frame.step = stats.step;
            doc->frames.push_back(std::move(frame)); doc->provenance.mode = "fire";
            std::ostringstream msg;
            msg << "FIRE: " << stats.step << " steps" << std::fixed << std::setprecision(4)
                << ", E = " << stats.U << " kcal/mol, F_rms = " << std::scientific << std::setprecision(3) << stats.Frms;
            res.output = doc; res.success = true; res.message = msg.str();
            break;
        }

        case KernelOp::MD_NVE: {
            if (!req.input || req.input->empty()) { res.message = "No structure loaded"; return res; }
            auto [s, names] = detail::doc_to_state(*req.input);
            if (detail::velocities_are_zero(s)) atomistic::initialize_velocities_thermal(s, req.temperature, core_->rng);
            atomistic::VelocityVerletParams vvp;
            vvp.dt = req.dt; vvp.n_steps = req.max_steps;
            atomistic::VelocityVerlet vv(*core_->model, core_->mp);
            const auto stats = vv.integrate(s, vvp);
            auto doc = std::make_shared<scene::SceneDocument>(*req.input);
            auto frame = detail::state_to_frame(s, names, "md_nve");
            frame.step = stats.steps_completed;
            frame.properties["temperature"] = stats.T_avg;
            frame.properties["energy_kinetic"] = stats.KE_avg;
            doc->frames.push_back(std::move(frame)); doc->provenance.mode = "md_nve";
            std::ostringstream msg;
            msg << "NVE MD: " << stats.steps_completed << " steps" << std::fixed << std::setprecision(2)
                << ", T_avg = " << stats.T_avg << " K, E_drift = " << stats.E_drift << " kcal/mol";
            res.output = doc; res.success = true; res.message = msg.str();
            break;
        }

        case KernelOp::MD_NVT: {
            if (!req.input || req.input->empty()) { res.message = "No structure loaded"; return res; }
            auto [s, names] = detail::doc_to_state(*req.input);
            if (detail::velocities_are_zero(s)) atomistic::initialize_velocities_thermal(s, req.temperature, core_->rng);
            atomistic::LangevinParams lp;
            lp.dt = req.dt; lp.n_steps = req.max_steps; lp.T_target = req.temperature;
            atomistic::LangevinDynamics lang(*core_->model, core_->mp);
            const auto stats = lang.integrate(s, lp, core_->rng);
            auto doc = std::make_shared<scene::SceneDocument>(*req.input);
            auto frame = detail::state_to_frame(s, names, "md_nvt");
            frame.step = stats.steps_completed;
            frame.properties["temperature"]     = stats.T_avg;
            frame.properties["temperature_std"] = stats.T_std;
            frame.properties["energy_kinetic"]  = stats.KE_avg;
            doc->frames.push_back(std::move(frame)); doc->provenance.mode = "md_nvt";
            std::ostringstream msg;
            msg << "Langevin MD: " << stats.steps_completed << " steps" << std::fixed << std::setprecision(1)
                << ", T_avg = " << stats.T_avg << " K (target " << req.temperature << " K)";
            res.output = doc; res.success = true; res.message = msg.str();
            break;
        }

        case KernelOp::InferBonds: {
            if (!req.input || req.input->empty()) { res.message = "No structure loaded"; return res; }
            const auto& f = req.input->current_frame();
            vsepr::io::XYZMolecule mol;
            mol.atoms.reserve(f.atoms.size());
            for (const auto& a : f.atoms)
                mol.atoms.emplace_back(a.symbol.empty() ? "C" : a.symbol, a.pos.x, a.pos.y, a.pos.z);
            vsepr::io::XYZReader reader; reader.detect_bonds(mol);
            auto doc = std::make_shared<scene::SceneDocument>(*req.input);
            auto& bonds = doc->frames.back().bonds;
            bonds.clear(); bonds.reserve(mol.bonds.size());
            for (const auto& b : mol.bonds) bonds.push_back({b.atom_i, b.atom_j, b.bond_order});
            res.output = doc; res.success = true;
            res.message = "Inferred " + std::to_string(mol.bonds.size()) + " bonds";
            break;
        }

        case KernelOp::EmitCrystal: {
            // Look up preset by name and build a supercell.
            // Uses the same crystal::presets:: library validated in Phase 4/5.
            using namespace atomistic::crystal;
            UnitCell uc = [&]() -> UnitCell {
                const std::string& p = req.preset;
                if (p == "fcc_al"       || p == "Al") return presets::aluminum_fcc();
                if (p == "bcc_fe"       || p == "Fe") return presets::iron_bcc();
                if (p == "nacl"         || p == "NaCl") return presets::sodium_chloride();
                if (p == "diamond_si"   || p == "Si") return presets::silicon_diamond();
                if (p == "fcc_cu"       || p == "Cu") return presets::copper_fcc();
                if (p == "fcc_au"       || p == "Au") return presets::gold_fcc();
                if (p == "mgo"          || p == "MgO") return presets::magnesium_oxide();
                if (p == "cscl"         || p == "CsCl") return presets::cesium_chloride();
                throw std::runtime_error("Unknown crystal preset: " + p);
            }();

            int na = std::max(1, req.supercell[0]);
            int nb = std::max(1, req.supercell[1]);
            int nc = std::max(1, req.supercell[2]);

            SupercellResult sc = construct_supercell(uc, na, nb, nc);
            atomistic::State& s = sc.state;
            s.F.resize(s.N, {0,0,0});

            // Build element name list from atomic numbers
            std::vector<std::string> names(s.N);
            static const char* sym[] = {
                "","H","He","Li","Be","B","C","N","O","F","Ne",
                "Na","Mg","Al","Si","P","S","Cl","Ar","K","Ca",
                "Sc","Ti","V","Cr","Mn","Fe","Co","Ni","Cu","Zn",
                "Ga","Ge","As","Se","Br","Kr","","","","","",
                "","","","","","","","","","","","","","","","Au",
                ""  // placeholder up to index 79
            };
            for (uint32_t i = 0; i < s.N; ++i) {
                uint32_t Z = s.type[i];
                names[i] = (Z < sizeof(sym)/sizeof(sym[0]) && sym[Z][0])
                           ? sym[Z] : "X";
            }

            auto doc = std::make_shared<scene::SceneDocument>();
            doc->provenance.mode    = "crystal";
            doc->provenance.formula = uc.name;
            doc->frames.push_back(detail::state_to_frame(s, names, "crystal"));

            std::ostringstream msg;
            msg << uc.name << " " << na << "x" << nb << "x" << nc
                << ": " << s.N << " atoms";
            res.output = doc; res.success = true; res.message = msg.str();
            break;
        }

        default:
            res.message = "Operation not implemented";
            break;
        }
    } catch (const std::exception& e) {
        res.success = false;
        res.message = std::string("Engine error: ") + e.what();
    }
    return res;
}

std::future<KernelResult> EngineAdapter::runAsync(const KernelRequest& req, ProgressFn progress)
{
    return std::async(std::launch::async, [this, req, progress]() { return this->run(req, progress); });
}

} // namespace bridge
