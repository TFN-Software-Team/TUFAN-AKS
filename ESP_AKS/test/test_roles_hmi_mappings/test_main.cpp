#include <unity.h>

extern void test_hmi_are_all_contactors_closed_roles_1_idle(void);
extern void test_hmi_are_all_contactors_closed_roles_1_ready(void);

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_hmi_are_all_contactors_closed_roles_1_idle);
    RUN_TEST(test_hmi_are_all_contactors_closed_roles_1_ready);
    return UNITY_END();
}
