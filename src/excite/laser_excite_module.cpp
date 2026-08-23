/**
 * src/excite/laser_excite_module.cpp  -  Laser excitation module
 * ==============================================================
 *
 * Implements IExciteModule for type = "laser".
 * Applies a pulse-shaped carrier wave along the declared axis.
 *
 * Physics:
 *   intensity(t) = envelope(t) * cos(omega * t)
 *   omega = photon_energy_eV * 0.15193   (eV -> rad/fs)
 *   envelope: gaussian (default), flat, or sech2 pulse shape
 *
 * Reference: FinalChapter/VSIM_IMPLEMENTATION_GUIDE.md §4
 * WO-75B  |  v5.0.0-main
 */

#include "vsim/excite_module.hpp"
#include "vsim/module_registry.hpp"
#include "vsim/vsim_document.hpp"

#include <cmath>
#include <string>
#include <string_view>

namespace {

class LaserExciteModule : public IExciteModule {
public:
	std::string_view type() const override { return "laser"; }
	std::string_view description() const override {
		return "Laser pulse (gaussian/flat/sech2 envelope + sinusoidal carrier)";
	}

	void configure(const vsim::ExciteEntry& e, double /*dt_fs*/) override {
		intensity_  = e.intensity;
		pulse_width_ = (e.pulse_width_fs > 0.0) ? e.pulse_width_fs : 100.0;
		photon_eV_   = e.photon_energy_eV;
		profile_     = e.profile.empty() ? "gaussian" : e.profile;
		start_t_     = 0.0;
		// Pulse centre at 2 * pulse_width_ from start
		centre_      = start_t_ + 2.0 * pulse_width_;
		sigma_       = pulse_width_ / 2.355;  // FWHM -> sigma
	}

	double intensity_at(double t_fs) const override {
		double env = envelope(t_fs);
		if (env < 1e-9) return 0.0;
		double omega   = photon_eV_ * 0.15193;  // eV -> rad/fs
		double carrier = std::cos(omega * t_fs);
		return intensity_ * env * carrier;
	}

private:
	double intensity_   = 1.0;
	double pulse_width_ = 100.0;
	double photon_eV_   = 0.0;
	double start_t_     = 0.0;
	double centre_      = 200.0;
	double sigma_       = 42.5;
	std::string profile_ = "gaussian";

	double envelope(double t_fs) const {
		if (profile_ == "flat") {
			return (t_fs >= start_t_ && t_fs <= start_t_ + pulse_width_) ? 1.0 : 0.0;
		}
		if (profile_ == "sech2") {
			double x = (t_fs - centre_) / sigma_;
			double c = std::cosh(x);
			return 1.0 / (c * c);
		}
		// gaussian (default)
		double x = (t_fs - centre_) / sigma_;
		return std::exp(-0.5 * x * x);
	}
};

// Self-registration
static vsim::AutoRegister<LaserExciteModule, IExciteModule> s_laser_reg("laser");

} // anonymous namespace
