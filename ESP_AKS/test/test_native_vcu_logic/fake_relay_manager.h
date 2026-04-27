#pragma once
// Fake RelayManager için global sayaçlar — testlerden include edilir.
extern unsigned g_fake_relay_allOn_count;
extern unsigned g_fake_relay_allOff_count;
extern unsigned g_fake_relay_setRelay_count;

// Tüm sayaçları sıfırlar; setUp() içinde çağrılmalıdır.
void fake_relay_reset(void);
