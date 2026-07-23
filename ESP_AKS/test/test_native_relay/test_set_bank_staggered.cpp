#include <unity.h>

#include "RelayManager.h"
#include "fake_spi.h"

namespace {
void primeRelay() {
    fake_spi_reset();
    RelayManager::instance().resetForTest();
    RelayManager::instance().begin();
    fake_spi_reset();
}
}  // namespace

// ---------------------------------------------------------------------------
// setBankStaggered(mask, delay)
// ---------------------------------------------------------------------------
void test_setBankStaggered_drives_selected_channels(void) {
    primeRelay();
    
    // Test with mask = 0x03 (Channels 0 and 1)
    uint16_t mask = 0x03;
    RelayManager::instance().setBankStaggered(mask, 30);

    // Channel 0 and 1 should be TRUE
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(0));
    TEST_ASSERT_TRUE(RelayManager::instance().getRelayState(1));
    TEST_ASSERT_FALSE(RelayManager::instance().getRelayState(2));
    
    // Expected writes:
    // For ch=0: read/writes -> write OLATA (low bit 0)
    // For ch=1: read/writes -> write OLATA (low bit 1)
    // We can just verify the final register values or write count
    // The delay itself is mocked and just returns immediately in this test.
    TEST_ASSERT_GREATER_THAN(0, fake_spi_write_count());
}
