#include <unity.h>

extern void test_hmi_are_all_contactors_closed_roles_1_idle(void);
extern void test_hmi_are_all_contactors_closed_roles_1_ready(void);
extern void test_hmi_ready_ignores_unwired_spare_channels(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_hmi_are_all_contactors_closed_roles_1_idle);
    RUN_TEST(test_hmi_are_all_contactors_closed_roles_1_ready);
    RUN_TEST(test_hmi_ready_ignores_unwired_spare_channels);
    return UNITY_END();
}
