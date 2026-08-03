#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Alan bazli minimum guncelleme araligi (SAF, donanimsiz, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: updateScreen 10 Hz'te (HMI_Task periyodu, main.cpp) cagrilir ve
// change-compare "deger degistiyse gonder" der. packv kaynagi deciV'dir, yani
// 0.1 V cozunurluk. SARJDA paket gerilimi bu cozunurlukte SUREKLI oynar
// (charger ripple + BMS olcum gurultusu), dolayisiyla alan saniyede ~10 kez
// yeni sayi yazar — ekranda okunamayacak kadar hizli zipliyor.
//
// COZUM: change-compare KORUNUR (deger hâlâ dogru ve gecikmesiz takip edilir),
// yalniz GONDERIM sikligi tavanlanir. Bu bir GOSTERIM cilalamasidir; degeri
// filtrelemez, yuvarlamaz, uydurmaz — sadece "en fazla N ms'te bir yaz" der.
// Atlanan tikte cache GUNCELLENMEZ (bkz. DisplayHMI.cpp), yani bekleyen
// degisiklik KAYBOLMAZ; pencere acilinca EN SON deger yazilir.
//
// NEDEN EMA/ORTALAMA DEGIL: HmiEmaFilter (HMIHelpers.h) degerin KENDISINI
// degistirir — ekranda BMS'in olcmedigi bir sayi gorunur. Sarj sonu CV/taper
// fazinda gercek gerilimi maskeler. Zaman tavani ise her zaman GERCEK bir
// ornegi gosterir, sadece daha seyrek.
//
// EMNIYET KATMANLARIYLA ILISKI — tavan bunlari BAGLAMAZ:
//   - forceFullRefresh (Nextion reset kurtarmasi) → ANINDA gonderilir,
//   - round-robin resync (ResyncPolicy.h) → ANINDA gonderilir.
// Aksi halde tespit edilemeyen bir ekran reset'inden sonra alan tavan suresi
// kadar bos/yanlis kalirdi. Gercek gonderim yapildiginda pencere yeniden
// baslar (hmi_throttle_stamp).
//
// Tick birimi agnostiktir (cagiran ayni birimi verir); unsigned cikarma
// (now - lastSendTick) sayac tasmasinda da dogru sonuc verir — ResyncPolicy.h /
// BmsFreshness.h ile ayni idiom. Durum cagirana aittir (DisplayHMI uye
// degiskeninde tutar).
//
// Testler: test/test_native_hmi_helpers/test_update_throttle.cpp.

// Gonderim penceresi acik mi? SAF SORGU — durum DEGISTIRMEZ.
// intervalTicks == 0 → tavan devre disi (her zaman true).
inline bool hmi_throttle_due(uint32_t now, uint32_t lastSendTick,
                             uint32_t intervalTicks) {
    if (intervalTicks == 0) return true;
    return (uint32_t)(now - lastSendTick) >= intervalTicks;
}

// Gercekten gonderim YAPILDIGINDA cagrilir; pencereyi yeniden baslatir.
// Sorgudan AYRI tutulur ki force/resync yollarindan gelen gonderimler de
// pencereyi damgalayabilsin (yoksa tavan iki katina cikardi).
inline void hmi_throttle_stamp(uint32_t now, uint32_t& lastSendTick) {
    lastSendTick = now;
}
