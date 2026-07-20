#include <unity.h>

extern void test_preroll_first_sample_is_immediate(void);
extern void test_preroll_throttles_within_period(void);
extern void test_preroll_samples_at_and_past_period(void);
extern void test_preroll_pop_oldest_fifo_order(void);
extern void test_preroll_pop_oldest_empty_returns_false(void);
extern void test_preroll_capacity_matches_link_timeout_derivation(void);
extern void test_preroll_circulates_keeps_last_capacity_samples(void);
extern void test_preroll_reset_clears_state(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_preroll_first_sample_is_immediate);
    RUN_TEST(test_preroll_throttles_within_period);
    RUN_TEST(test_preroll_samples_at_and_past_period);
    RUN_TEST(test_preroll_pop_oldest_fifo_order);
    RUN_TEST(test_preroll_pop_oldest_empty_returns_false);
    RUN_TEST(test_preroll_capacity_matches_link_timeout_derivation);
    RUN_TEST(test_preroll_circulates_keeps_last_capacity_samples);
    RUN_TEST(test_preroll_reset_clears_state);
    return UNITY_END();
}
