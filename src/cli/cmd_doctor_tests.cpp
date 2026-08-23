/**
 * cmd_doctor_tests.cpp
 * --------------------
 * Implements `vsepr doctor integratedtest` and `vsepr doctor benchmark`.
 *
 * Test ordering (dependency-first):
 *   Group A  -  primitives         (Vec3 math, BoxOrtho)
 *   Group B  -  State construction (particle assembly, mass/charge/edge)
 *   Group C  -  Statistics         (OnlineStats, OnlineVec3Stats)
 *   Group D  -  Formula parsing    (PeriodicTable load, parse_formula)
 *   Group E  -  Neighbor graph     (build_neighbor_graph, CN)
 *   Group F  -  Fingerprinting     (ProtoFingerprint from State)
 *   Group G  -  Clustering         (deterministic cluster assignment)
 *   Group H  -  Pipeline smoke     (validate + run on demo script if present)
 *
 * Benchmarks cover the same groups with iteration counts chosen to run
 * in <2 s total on a modern workstation.
 */

#include "cli/cmd_doctor_tests.hpp"

#include "atomistic/core/state.hpp"
#include "atomistic/core/statistics.hpp"
#include "atomistic/classify/fingerprints.hpp"
#include "atomistic/classify/cluster.hpp"
#include "core/math_vec3.hpp"
#include "box/pbc.hpp"
#include "cli/cmd_validate.hpp"
#include "cli/cmd_run_vsim.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr::cli {

namespace {

// --- Terminal colour helpers ------------------------------------------------
const char* GRN  = "\033[0;32m";
const char* YEL  = "\033[1;33m";
const char* RED  = "\033[0;31m";
const char* CYN  = "\033[36m";
const char* DIM  = "\033[2m";
const char* BOLD = "\033[1m";
const char* RST  = "\033[0m";

constexpr const char* OK   = "  [ok]    ";
constexpr const char* FAIL = "  [FAIL]  ";
constexpr const char* SKIP = "  [skip]  ";
constexpr const char* BNK  = "  [bench] ";

// --- Tiny test harness ------------------------------------------------------

struct TestResult {
	int passed = 0;
	int failed = 0;
	int skipped = 0;
};

#define IT_PASS(label) \
	do { std::printf("%s%s%s%s\n", GRN, OK, RST, (label)); ++result.passed; } while(0)

#define IT_FAIL(label, reason) \
	do { std::printf("%s%s%s%s  -  %s\n", RED, FAIL, RST, (label), (reason)); ++result.failed; } while(0)

#define IT_SKIP(label, reason) \
	do { std::printf("%s%s%s%s  -  %s\n", YEL, SKIP, RST, (label), (reason)); ++result.skipped; } while(0)

#define IT_CHECK(cond, label) \
	do { if (cond) { IT_PASS(label); } else { IT_FAIL(label, "assertion false"); } } while(0)

#define IT_NEAR(a, b, eps, label) \
	do { if (std::abs((a)-(b)) <= (eps)) { IT_PASS(label); } \
		 else { std::snprintf(_errbuf, sizeof(_errbuf), \
					"got %.10g, expected %.10g (delta %.3g)", (double)(a), (double)(b), std::abs((a)-(b))); \
				IT_FAIL(label, _errbuf); } } while(0)

char _errbuf[256];

// --- Helper: build a small NaCl-like diatomic State ------------------------
static atomistic::State make_diatomic(double d = 2.4) {
	atomistic::State s;
	s.N = 2;
	s.X = { {0.0, 0.0, 0.0}, {d, 0.0, 0.0} };
	s.V = { {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0} };
	s.Q = { +1.0, -1.0 };
	s.M = { 22.99, 35.45 };
	s.type = { 11, 17 };
	s.F.resize(2, {0.0, 0.0, 0.0});
	s.B.push_back({ 0, 1 });
	return s;
}

// Build a small H2O-like triatomic
static atomistic::State make_triatomic() {
	atomistic::State s;
	s.N = 3;
	const double bond = 0.96; // Å  O-H
	const double angle = 104.5 * (3.14159265358979 / 180.0);
	s.X = {
		{0.0, 0.0, 0.0},
		{ bond * std::cos(angle / 2.0),  bond * std::sin(angle / 2.0), 0.0},
		{ bond * std::cos(angle / 2.0), -bond * std::sin(angle / 2.0), 0.0}
	};
	s.V.resize(3, {0.0, 0.0, 0.0});
	s.Q = { -0.83, +0.415, +0.415 };
	s.M = { 15.999, 1.008, 1.008 };
	s.type = { 8, 1, 1 };
	s.F.resize(3, {0.0, 0.0, 0.0});
	s.B.push_back({ 0, 1 });
	s.B.push_back({ 0, 2 });
	return s;
}

// --- Group A: Vec3 / BoxOrtho primitives -------------------------------------
static void group_a(TestResult& result) {
	std::printf("\n%sGroup A  -  primitives%s\n", BOLD, RST);

	using vsepr::Vec3;

	Vec3 a{1.0, 2.0, 3.0};
	Vec3 b{4.0, 5.0, 6.0};

	IT_NEAR(vsepr::dot(a, b), 32.0, 1e-12, "Vec3 dot product");
	IT_NEAR(vsepr::norm(a), std::sqrt(14.0), 1e-12, "Vec3 norm");

	Vec3 c = a + b;
	IT_CHECK(c.x == 5.0 && c.y == 7.0 && c.z == 9.0, "Vec3 operator+");

	Vec3 d = a * 2.0;
	IT_CHECK(d.x == 2.0 && d.y == 4.0 && d.z == 6.0, "Vec3 scalar multiply");

	// BoxOrtho default: PBC disabled
	vsepr::BoxOrtho box;
	IT_CHECK(!box.enabled, "BoxOrtho default disabled");

	// PBC wrap: particle at x=L+0.5 should wrap to 0.5
	box.set_dimensions(10.0, 10.0, 10.0);
	Vec3 pos1 = box.wrap({10.5, 5.0, 5.0});
	IT_NEAR(pos1.x, 0.5, 1e-10, "BoxOrtho wrap x > L");
	Vec3 pos2 = box.wrap({-0.1, 5.0, 5.0});
	IT_NEAR(pos2.x, 9.9, 1e-10, "BoxOrtho wrap x < 0");
}

// --- Group B: State construction ---------------------------------------------
static void group_b(TestResult& result) {
	std::printf("\n%sGroup B  -  State construction%s\n", BOLD, RST);

	auto s = make_diatomic();
	IT_CHECK(s.N == 2, "diatomic N == 2");
	IT_CHECK(s.X.size() == 2, "positions vector size");
	IT_CHECK(s.M[0] > 0.0 && s.M[1] > 0.0, "masses > 0");
	IT_CHECK(s.B.size() == 1, "one bond edge");
	IT_NEAR(s.Q[0] + s.Q[1], 0.0, 1e-12, "charge neutrality");

	auto w = make_triatomic();
	IT_CHECK(w.N == 3, "triatomic N == 3");
	IT_CHECK(w.B.size() == 2, "triatomic bond count");

	// Bond length check
	Vec3 d = w.X[1] - w.X[0];
	IT_NEAR(vsepr::norm(d), 0.96, 1e-6, "O-H bond length");
}

// --- Group C: Statistics -----------------------------------------------------
static void group_c(TestResult& result) {
	std::printf("\n%sGroup C  -  statistics%s\n", BOLD, RST);

	atomistic::OnlineStats stats;
	for (int i = 1; i <= 100; ++i) stats.add_sample(static_cast<double>(i));

	IT_NEAR(stats.get_mean(), 50.5, 1e-9, "OnlineStats mean [1..100]");
	IT_NEAR(stats.get_variance(), 841.6666666666667, 1e-4, "OnlineStats variance [1..100]");
	IT_CHECK(stats.count() == 100, "OnlineStats count");

	// Constant series: variance should be ~0
	atomistic::OnlineStats flat;
	for (int i = 0; i < 50; ++i) flat.add_sample(3.14);
	IT_NEAR(flat.get_variance(), 0.0, 1e-12, "OnlineStats variance constant series");

	// Vec3 stats: centroid of unit cube corner
	atomistic::OnlineVec3Stats vs;
	vs.add_sample({0.0, 0.0, 0.0});
	vs.add_sample({2.0, 0.0, 0.0});
	vs.add_sample({0.0, 2.0, 0.0});
	vs.add_sample({0.0, 0.0, 2.0});
	auto mean3 = vs.get_mean();
	IT_NEAR(mean3.x, 0.5, 1e-10, "OnlineVec3Stats mean x");
	IT_NEAR(mean3.y, 0.5, 1e-10, "OnlineVec3Stats mean y");
	IT_NEAR(mean3.z, 0.5, 1e-10, "OnlineVec3Stats mean z");
}

// --- Group D: Neighbor graph --------------------------------------------------
static void group_d(TestResult& result) {
	std::printf("\n%sGroup D  -  neighbor graph%s\n", BOLD, RST);

	auto s = make_diatomic(2.4);
	auto ng = atomistic::classify::build_neighbor_graph(s, 3.5);
	IT_CHECK(ng.N == 2, "neighbor graph N");
	IT_CHECK(ng.CN[0] == 1 && ng.CN[1] == 1, "diatomic CN == 1");
	IT_NEAR(ng.dist[0][0], 2.4, 1e-6, "diatomic bond distance in graph");

	// Atoms far apart: no neighbors within cutoff
	auto sfar = make_diatomic(10.0);
	auto ng2 = atomistic::classify::build_neighbor_graph(sfar, 3.5);
	IT_CHECK(ng2.CN[0] == 0 && ng2.CN[1] == 0, "isolated atoms CN == 0");

	// Triatomic: central atom (O) should have CN=2
	auto w = make_triatomic();
	auto ng3 = atomistic::classify::build_neighbor_graph(w, 3.5);
	IT_CHECK(ng3.CN[0] == 2, "H2O central atom CN == 2");
}

// --- Group E: Fingerprinting --------------------------------------------------
static void group_e(TestResult& result) {
	std::printf("\n%sGroup E  -  fingerprinting%s\n", BOLD, RST);

	auto s1 = make_diatomic(2.4);
	auto s2 = make_diatomic(2.4);
	auto s3 = make_diatomic(5.0); // same topology, different distance

	auto ng1 = atomistic::classify::build_neighbor_graph(s1, 3.5);
	auto ng2 = atomistic::classify::build_neighbor_graph(s2, 3.5);
	auto ng3 = atomistic::classify::build_neighbor_graph(s3, 3.5);

	auto fp1 = atomistic::classify::compute_proto_fingerprint(s1, ng1);
	auto fp2 = atomistic::classify::compute_proto_fingerprint(s2, ng2);
	auto fp3 = atomistic::classify::compute_proto_fingerprint(s3, ng3);

	IT_CHECK(fp1.topology_hash == fp2.topology_hash,
			 "identical states produce identical topology hash");
	IT_CHECK(!fp1.CN_histogram.empty(),
			 "CN histogram non-empty");
	IT_CHECK(!fp1.RDF_histogram.empty(),
			 "RDF histogram non-empty");

	// fp3 has bond length 5.0 Å > 3.5 Å cutoff -> zero neighbors -> different topology
	IT_CHECK(fp1.topology_hash != fp3.topology_hash,
			 "bond beyond cutoff produces distinct topology hash");
}

// --- Group F: Clustering ------------------------------------------------------
static void group_f(TestResult& result) {
	std::printf("\n%sGroup F  -  clustering%s\n", BOLD, RST);

	auto s1 = make_diatomic(2.4);
	auto s2 = make_diatomic(2.4);
	auto s3 = make_diatomic(5.0);

	// cluster_by_proto takes States and builds fingerprints internally
	std::vector<atomistic::State> structures = { s1, s2, s3 };
	auto cr  = atomistic::classify::cluster_by_proto(structures, 0.1);
	auto cr2 = atomistic::classify::cluster_by_proto(structures, 0.1);

	IT_CHECK(cr.cluster_ids.size() == 3, "cluster result size == input size");
	IT_CHECK(cr.cluster_ids[0] == cr.cluster_ids[1],
			 "identical states in same cluster");
	IT_CHECK(cr.num_clusters() >= 1, "at least one cluster formed");
	IT_CHECK(cr.cluster_ids == cr2.cluster_ids,
			 "clustering is deterministic across runs");
}

// --- Group G: Pipeline smoke (needs demo scripts on disk) ---------------------
static void group_g(TestResult& result, const std::filesystem::path& scriptsDir) {
	std::printf("\n%sGroup G  -  pipeline smoke%s\n", BOLD, RST);

	namespace fs = std::filesystem;
	fs::path nacl = scriptsDir / "demo_01_nacl_level0.vsim";
	if (!fs::exists(nacl)) {
		IT_SKIP("validate pipeline", "demo_01_nacl_level0.vsim not found");
		IT_SKIP("run pipeline",      "demo_01_nacl_level0.vsim not found");
		return;
	}

	{
		std::streambuf* buf = std::cout.rdbuf(nullptr);
		int rc = vsepr::cli::cmd_validate({ nacl.string() });
		std::cout.rdbuf(buf);
		if (rc == 0) IT_PASS("validate pipeline (demo_01_nacl_level0.vsim)");
		else         IT_FAIL("validate pipeline", "cmd_validate returned non-zero");
	}
	{
		std::streambuf* buf = std::cout.rdbuf(nullptr);
		int rc = vsepr::cli::cmd_run_vsim({ nacl.string() });
		std::cout.rdbuf(buf);
		if (rc == 0) IT_PASS("run pipeline (demo_01_nacl_level0.vsim)");
		else         IT_FAIL("run pipeline", "cmd_run_vsim returned non-zero");
	}
}

// --- Benchmark helpers --------------------------------------------------------
using Clock = std::chrono::steady_clock;

static void bench_report(const char* label, long long iters, double elapsed_ns) {
	double per_iter_ns = elapsed_ns / static_cast<double>(iters);
	double per_iter_us = per_iter_ns / 1000.0;
	if (per_iter_us < 1.0)
		std::printf("%s%s%s%-42s  %6lld iters  %8.1f ns/iter\n",
					CYN, BNK, RST, label, iters, per_iter_ns);
	else
		std::printf("%s%s%s%-42s  %6lld iters  %8.2f µs/iter\n",
					CYN, BNK, RST, label, iters, per_iter_us);
}

template <typename Fn>
static void bench(const char* label, long long iters, Fn&& fn) {
	// Warm up
	for (int w = 0; w < 3; ++w) fn();
	auto t0 = Clock::now();
	for (long long i = 0; i < iters; ++i) fn();
	auto t1 = Clock::now();
	double elapsed_ns = static_cast<double>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
	bench_report(label, iters, elapsed_ns);
}

} // anonymous namespace

// ============================================================================
// cmd_integrated_test
// ============================================================================
int cmd_integrated_test(const std::vector<std::string>& /*args*/) {
	const std::string SEP(58, '-');

	std::printf("\n%sVSEPR-SIM  -  integrated test suite%s\n%s\n", BOLD, RST, SEP.c_str());
	std::printf("%s(dependency-ordered: primitives -> state -> stats -> graph -> fingerprint -> cluster -> pipeline)%s\n",
				DIM, RST);

	// Resolve scripts directory (same heuristic as doctor)
	namespace fs = std::filesystem;
	fs::path scriptsDir;
	{
		std::string localAppData;
		if (const char* p = std::getenv("LOCALAPPDATA")) localAppData = p;
		fs::path installRoot = localAppData.empty()
			? fs::current_path()
			: fs::path(localAppData) / "VSEPR-SIM";
		// Walk up from cwd to repo root
		for (fs::path p = fs::current_path(); p != p.parent_path(); p = p.parent_path()) {
			if (fs::exists(p / "CMakeLists.txt")) { scriptsDir = p / "scripts"; break; }
		}
		if (scriptsDir.empty())
			scriptsDir = installRoot / "examples";
	}

	TestResult result;

	group_a(result);
	group_b(result);
	group_c(result);
	group_d(result);
	group_e(result);
	group_f(result);
	group_g(result, scriptsDir);

	std::printf("\n%s\n", SEP.c_str());
	int total = result.passed + result.failed + result.skipped;
	if (result.failed == 0) {
		std::printf("%s  PASS%s  %d/%d tests passed",
					GRN, RST, result.passed, total);
		if (result.skipped > 0)
			std::printf(" (%d skipped)", result.skipped);
		std::printf("\n");
		return 0;
	} else {
		std::printf("%s  FAIL%s  %d test(s) failed  (%d passed, %d skipped)\n",
					RED, RST, result.failed, result.passed, result.skipped);
		return 1;
	}
}

// ============================================================================
// cmd_benchmark
// ============================================================================
int cmd_benchmark(const std::vector<std::string>& /*args*/) {
	const std::string SEP(58, '-');

	std::printf("\n%sVSEPR-SIM  -  benchmark suite%s\n%s\n", BOLD, RST, SEP.c_str());
	std::printf("%s(timings: steady_clock, warmup=3, no skipping on slow results)%s\n\n", DIM, RST);

	using vsepr::Vec3;

	// -- A. Vec3 primitives -------------------------------------------------
	std::printf("%sA  -  Vec3 primitives%s\n", BOLD, RST);
	{
		Vec3 a{1.0, 2.0, 3.0}, b{4.0, 5.0, 6.0};
		bench("Vec3 dot product", 10'000'000, [&]() {
			volatile double r = vsepr::dot(a, b); (void)r;
		});
		bench("Vec3 norm", 10'000'000, [&]() {
			volatile double r = vsepr::norm(a); (void)r;
		});
		bench("Vec3 add + scale", 10'000'000, [&]() {
			volatile Vec3 r = (a + b) * 0.5; (void)r;
		});
	}

	// -- B. BoxOrtho wrap --------------------------------------------------
	std::printf("\n%sB  -  BoxOrtho PBC wrap%s\n", BOLD, RST);
	{
		vsepr::BoxOrtho box;
		box.set_dimensions(10.0, 10.0, 10.0);
		bench("BoxOrtho::wrap (single particle)", 10'000'000, [&]() {
			Vec3 r = box.wrap({10.5, -0.3, 5.0});
			volatile double rv = r.x; (void)rv;
		});
	}

	// -- C. OnlineStats ----------------------------------------------------
	std::printf("\n%sC  -  statistics%s\n", BOLD, RST);
	{
		bench("OnlineStats  -  1000 samples", 10'000, [&]() {
			atomistic::OnlineStats s;
			for (int i = 0; i < 1000; ++i) s.add_sample(static_cast<double>(i));
			volatile double r = s.get_mean(); (void)r;
		});
		bench("OnlineVec3Stats  -  1000 samples", 10'000, [&]() {
			atomistic::OnlineVec3Stats s;
			for (int i = 0; i < 1000; ++i) s.add_sample({(double)i, (double)i*2, (double)i*3});
			volatile double r = s.get_mean().x; (void)r;
		});
	}

	// -- D. State construction ---------------------------------------------
	std::printf("\n%sD  -  State construction%s\n", BOLD, RST);
	{
		bench("make_diatomic State", 100'000, [&]() {
			auto s = make_diatomic();
			volatile int n = s.N; (void)n;
		});
		bench("make_triatomic State", 100'000, [&]() {
			auto s = make_triatomic();
			volatile int n = s.N; (void)n;
		});
	}

	// -- E. Neighbor graph -------------------------------------------------
	std::printf("\n%sE  -  neighbor graph%s\n", BOLD, RST);
	{
		auto s2 = make_diatomic(2.4);
		auto s3 = make_triatomic();
		bench("build_neighbor_graph (diatomic)", 100'000, [&]() {
			auto ng = atomistic::classify::build_neighbor_graph(s2, 3.5);
			volatile int cn = ng.CN[0]; (void)cn;
		});
		bench("build_neighbor_graph (triatomic)", 100'000, [&]() {
			auto ng = atomistic::classify::build_neighbor_graph(s3, 3.5);
			volatile int cn = ng.CN[0]; (void)cn;
		});
	}

	// -- F. Fingerprinting -------------------------------------------------
	std::printf("\n%sF  -  fingerprinting%s\n", BOLD, RST);
	{
		auto s = make_diatomic(2.4);
		auto ng_s = atomistic::classify::build_neighbor_graph(s, 3.5);
		bench("compute_proto_fingerprint (diatomic)", 50'000, [&]() {
			auto fp = atomistic::classify::compute_proto_fingerprint(s, ng_s);
			volatile uint64_t h = fp.topology_hash; (void)h;
		});
		auto w = make_triatomic();
		auto ng_w = atomistic::classify::build_neighbor_graph(w, 3.5);
		bench("compute_proto_fingerprint (triatomic)", 50'000, [&]() {
			auto fp = atomistic::classify::compute_proto_fingerprint(w, ng_w);
			volatile uint64_t h = fp.topology_hash; (void)h;
		});
		bench("build_neighbor_graph overhead (diatomic)", 100'000, [&]() {
			auto ng = atomistic::classify::build_neighbor_graph(s, 3.5);
			volatile int cn = ng.CN[0]; (void)cn;
		});
	}

	// -- G. Clustering -----------------------------------------------------
	std::printf("\n%sG  -  clustering%s\n", BOLD, RST);
	{
		// cluster_by_proto takes States directly
		std::vector<atomistic::State> structures;
		for (int i = 0; i < 10; ++i) structures.push_back(make_diatomic(2.4));
		for (int i = 0; i < 10; ++i) structures.push_back(make_triatomic());

		bench("cluster_by_proto (20 structures)", 1'000, [&]() {
			auto cr = atomistic::classify::cluster_by_proto(structures, 0.1);
			volatile int nc = cr.num_clusters(); (void)nc;
		});
	}

	std::printf("\n%s\n", SEP.c_str());
	std::printf("%s  benchmark complete  -  all modules exercised%s\n", GRN, RST);
	return 0;
}

} // namespace vsepr::cli
