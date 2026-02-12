#include "report_md.hpp"
#include <sstream>
#include <iomanip>

namespace atomistic {

std::string fire_report_md(const State& s, const FIREStats& st) {
    std::ostringstream o;
    o << "# FIRE Minimization Report\n\n";

    o << "## State\n";
    o << "- $N=" << s.N << "$\n";
    o << "- $U=" << std::setprecision(12) << st.U << "$\n";
    o << "- $\\|F\\|_{RMS}=" << st.Frms << "$\n";
    o << "- $\\Delta U/N=" << st.dU_per_atom << "$\n";
    o << "- $\\alpha=" << st.alpha << "$, $\\Delta t=" << st.dt << "$\n\n";

    o << "## Energy decomposition\n";
    o << "- $U_{vdW}=" << s.E.UvdW << "$\n";
    o << "- $U_{Coul}=" << s.E.UCoul << "$\n\n";

    o << "## Math\n";
    o << "$$F = -\\nabla_X U(S)$$\n";
    o << "$$X_{t+1} = X_t + \\Delta t\\,V_t$$\n";
    o << "$$V_{t+1} = (1-\\alpha)V_t + \\alpha\\,\\frac{F_t}{\\|F_t\\|}\\,\\|V_t\\|$$\n";
    o << "$$\\text{stop if } \\|F\\|_{RMS}<\\varepsilon_F \\;\\lor\\; \\frac{|U_t-U_{t-1}|}{N}<\\varepsilon_U$$\n";

    return o.str();
}

} // namespace atomistic
