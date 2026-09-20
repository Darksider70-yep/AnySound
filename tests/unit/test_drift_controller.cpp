// NOLINTBEGIN
#include <catch2/catch_test_macros.hpp>
#include <chorus/sync/drift_controller.hpp>

#include <cmath>

TEST_CASE("DriftController PI convergence and clamping", "[sync][drift]") {
    chorus::DriftController controller(200.0, 30000, 0.05, 0.005);
    REQUIRE(controller.resampler_ratio() == 1.0);
    REQUIRE_FALSE(controller.needs_hard_resync());

    // Positive phase error of +1000 us (1 ms behind, needs to speed up)
    for (int i = 0; i < 20; ++i) {
        controller.update(1000, 20.0, 0.02);  // 20 ppm skew, 20ms steps
    }

    REQUIRE_FALSE(controller.needs_hard_resync());
    REQUIRE(controller.total_correction_ppm() > 20.0);
    REQUIRE(controller.total_correction_ppm() <= 200.0);
    REQUIRE(controller.resampler_ratio() > 1.0);
}

TEST_CASE("DriftController hard resync on large phase excursion (> 30 ms)", "[sync][drift]") {
    chorus::DriftController controller(200.0, 30000);

    // Large sudden excursion of 40 ms
    controller.update(40000, 0.0, 0.02);
    REQUIRE(controller.needs_hard_resync());

    controller.clear_resync();
    REQUIRE_FALSE(controller.needs_hard_resync());
}
// NOLINTEND
