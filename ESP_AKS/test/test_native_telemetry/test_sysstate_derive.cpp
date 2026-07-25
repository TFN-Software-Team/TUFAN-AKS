#include <unity.h>

#include "SysStateDerive.h"

// ===========================================================================
// SysStateDerive — akımdan türetilmiş sysState (Y33 kararı; bkz.
// SysStateDerive.h). Bu dosya, bayraktan (SYSSTATE_DERIVE_FROM_CURRENT)
// BAĞIMSIZ olan çekirdek matematiği (Impl + üretim sarmalayıcısı) VE
// varsayılan derleme ortamında (Y33 sonrası flag=1) applyIfEnabled'ın
// gerçekten UYGULANDIĞINI doğrular. Ezmeme kuralı ve bayat-veri koruması
// ayrıntılı olarak test_native_sysstate_derive_enabled'da ele alınır.
// ===========================================================================

// Bant içinde (|akım| <= bant) -> IDLE(2).
void test_derive_impl_zero_current_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(0, 50));
}

// Tam sınırda (+bant, -bant) -> HALA IDLE (kapsayıcı sınır: |akım| <= bant).
void test_derive_impl_at_positive_band_boundary_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(50, 50));
}

void test_derive_impl_at_negative_band_boundary_is_idle(void) {
    TEST_ASSERT_EQUAL_UINT8(2, SysStateDerive::deriveFromCurrentImpl(-50, 50));
}

// Sınırın hemen ÜSTÜNDE/ALTINDA -> Charge(3) / Discharge(1).
void test_derive_impl_just_above_positive_band_is_charge(void) {
    TEST_ASSERT_EQUAL_UINT8(3, SysStateDerive::deriveFromCurrentImpl(51, 50));
}

void test_derive_impl_just_below_negative_band_is_discharge(void) {
    TEST_ASSERT_EQUAL_UINT8(1, SysStateDerive::deriveFromCurrentImpl(-51, 50));
}

// Bandın çok üstünde/altında da aynı sınıflandırma geçerli.
void test_derive_impl_large_positive_current_is_charge(void) {
    TEST_ASSERT_EQUAL_UINT8(3, SysStateDerive::deriveFromCurrentImpl(990, 50));
}

void test_derive_impl_large_negative_current_is_discharge(void) {
    TEST_ASSERT_EQUAL_UINT8(1, SysStateDerive::deriveFromCurrentImpl(-2000, 50));
}

// Üretim sarmalayıcısı (deriveFromCurrent), SYSSTATE_CURRENT_IDLE_BAND_CENTI_A
// üretim sabitini kullanır — sınır davranışı Impl ile birebir aynı olmalı.
void test_derive_production_wrapper_matches_impl_at_production_band(void) {
    TEST_ASSERT_EQUAL_UINT8(
        2, SysStateDerive::deriveFromCurrent(SYSSTATE_CURRENT_IDLE_BAND_CENTI_A));
    TEST_ASSERT_EQUAL_UINT8(
        3,
        SysStateDerive::deriveFromCurrent(SYSSTATE_CURRENT_IDLE_BAND_CENTI_A + 1));
    TEST_ASSERT_EQUAL_UINT8(
        1,
        SysStateDerive::deriveFromCurrent(-SYSSTATE_CURRENT_IDLE_BAND_CENTI_A - 1));
}

// ---------------------------------------------------------------------------
// applyIfEnabled — Y33 sonrası SYSSTATE_DERIVE_FROM_CURRENT VARSAYILAN 1'dir
// (SystemConfig.h #ifndef guard). Bu native test binary'si bayrağı override
// ETMEZ, yani ÜRETİM ayarını doğrular: sysState alanı gerçekten akımdan
// türetilir. static_assert, bayrak ileride sessizce 0'a çekilirse bu testin
// yanıltıcı biçimde geçmesini ENGELLER.
// ---------------------------------------------------------------------------
void test_apply_derives_from_current_in_default_build(void) {
    static_assert(SYSSTATE_DERIVE_FROM_CURRENT == 1,
                  "Y33: varsayilan native derleme ortami "
                  "SYSSTATE_DERIVE_FROM_CURRENT=1 bekler");

    TelemetryData d = {};
    d.TEL_bmsSystemState = 0;          // "henuz parse edilmedi" durumu
    d.TEL_bmsDataValid = true;         // taze BMS verisi
    d.TEL_bmsCurrentCentiA = 990;      // acikca "Charge" sayilacak bir akim

    SysStateDerive::applyIfEnabled(d);

    TEST_ASSERT_EQUAL_UINT8(3, d.TEL_bmsSystemState);  // Charge
}
