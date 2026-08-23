// tests/test_uless_indicator.cpp
// Unit test for the Uless observation-length indicator model.

#include "vis/uless_indicator.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
	using vsepr::vis::uless::ObservationBar;

	{
		ObservationBar bar{0.5, 1.0, "obs"};
		assert(std::fabs(bar.ratio() - 0.5) < 1e-9);
		assert(bar.value_text() == std::string("obs 0.50"));
	}

	{
		ObservationBar bar{2.0, 1.0, "t"};
		assert(std::fabs(bar.ratio() - 1.0) < 1e-9);
	}

	{
		ObservationBar bar{-1.0, 1.0, "t"};
		assert(std::fabs(bar.ratio() - 0.0) < 1e-9);
	}

	{
		ObservationBar bar{0.75, 0.0, "bad"};
		assert(std::fabs(bar.ratio() - 0.0) < 1e-9);
	}

	std::cout << "ULESS_INDICATOR_OK\n";
	return EXIT_SUCCESS;
}
