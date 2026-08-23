// WO-72W — mlprop CLI command implementation
#include "cmd_mlprop.hpp"
#include "vsim/ml/ml.hpp"
#include <iostream>
#include <iomanip>
#include <stdexcept>

namespace vsepr::cli {

static void print_usage_mlprop() {
	std::cout <<
		"VSEPR-SIM mlprop commands:\n"
		"  vsepr mlprop recommend <property> <value>  Rank candidates near target\n"
		"  vsepr mlprop trends    <property>           Feature-property trend table\n"
		"  vsepr mlprop list                           List seed candidate set\n"
		"\nSupported properties: bandgap\n";
}

int cmd_mlprop(const std::vector<std::string>& args) {
	if (args.empty()) { print_usage_mlprop(); return 0; }
	const std::string sub = args[0];

	// ---- recommend ----------------------------------------------------------
	if (sub == "recommend") {
		if (args.size() < 3) {
			std::cerr << "Usage: vsepr mlprop recommend <property> <value>\n";
			return 1;
		}
		const std::string prop = args[1];
		double val = 0.0;
		try { val = std::stod(args[2]); }
		catch (...) { std::cerr << "Invalid value: " << args[2] << "\n"; return 1; }

		vsim::ml::RouteRecommender rr;
		rr.load_seed_set();
		auto recs = rr.recommend(prop, val, 10);
		if (recs.empty()) {
			std::cerr << "No candidates found for property: " << prop << "\n";
			return 1;
		}
		std::cout << "\nTop recommendations for " << prop << " = " << val << "\n"
				  << std::string(72, '-') << "\n"
				  << std::left
				  << std::setw(5)  << "Rank"
				  << std::setw(10) << "Score"
				  << std::setw(10) << "Delta"
				  << "Rationale\n"
				  << std::string(72, '-') << "\n";
		int rank = 1;
		for (auto& r : recs) {
			std::cout << std::setw(5) << rank++
					  << std::setw(10) << std::fixed << std::setprecision(3) << r.score
					  << std::setw(10) << std::setprecision(3) << r.delta
					  << r.rationale << "\n";
		}
		return 0;
	}

	// ---- trends -------------------------------------------------------------
	if (sub == "trends") {
		if (args.size() < 2) {
			std::cerr << "Usage: vsepr mlprop trends <property>\n";
			return 1;
		}
		const std::string prop = args[1];
		vsim::ml::RouteRecommender rr;
		rr.load_seed_set();

		// Extract matching candidates for trend analysis
		vsim::ml::PropertyTrendFinder tf;
		std::vector<vsim::ml::MaterialCandidate> pool;
		// Access through recommend with very wide window
		auto recs = rr.recommend(prop, 3.0, 100);
		for (auto& r : recs) pool.push_back(r.candidate);

		if (pool.empty()) { std::cerr << "No candidates for property: " << prop << "\n"; return 1; }
		auto trends = tf.find(pool);

		std::cout << "\nFeature-property trends: " << prop
				  << " (n=" << pool.size() << " candidates)\n"
				  << std::string(68, '-') << "\n"
				  << std::left
				  << std::setw(28) << "Feature"
				  << std::setw(12) << "Pearson r"
				  << std::setw(12) << "Slope"
				  << "RMSE\n"
				  << std::string(68, '-') << "\n";
		for (auto& t : trends) {
			std::cout << std::setw(28) << t.feature
					  << std::setw(12) << std::fixed << std::setprecision(3) << t.pearson_r
					  << std::setw(12) << std::setprecision(4) << t.slope
					  << std::setprecision(3) << t.rmse << "\n";
		}
		return 0;
	}

	// ---- list ---------------------------------------------------------------
	if (sub == "list") {
		vsim::ml::RouteRecommender rr;
		rr.load_seed_set();
		auto recs = rr.recommend("bandgap", 3.0, 100);
		std::cout << "\nSeed candidate set (" << recs.size() << " entries)\n"
				  << std::string(68, '-') << "\n"
				  << std::left
				  << std::setw(10) << "Formula"
				  << std::setw(24) << "Name"
				  << std::setw(12) << "Property"
				  << std::setw(10) << "Value"
				  << "Status\n"
				  << std::string(68, '-') << "\n";
		for (auto& r : recs) {
			auto& c = r.candidate;
			std::cout << std::setw(10) << c.formula
					  << std::setw(24) << c.name
					  << std::setw(12) << c.target_property
					  << std::setw(10) << c.target_value
					  << c.val_status << "\n";
		}
		return 0;
	}

	std::cerr << "Unknown mlprop sub-command: " << sub << "\n";
	print_usage_mlprop();
	return 1;
}

} // namespace vsepr::cli
