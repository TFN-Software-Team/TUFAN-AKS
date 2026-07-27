#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// HMI Round-Robin Resync (SAF, donanımsız, native test edilebilir).
// ---------------------------------------------------------------------------
// SORUN: Nextion reset kurtarması (NextionResetDetect.h) Startup event'inin
// (00 00 00 FF FF FF) RX hattından SAĞLAM gelmesine dayanır. Ancak besleme
// çökmesi (brown-out) sırasında RX hattının kendisi de güvenilmezdir — event
// bozulup KAYBOLABİLİR. O durumda dedektör hiç tetiklenmez, change-cache'ler
// eski değerleri tutar ve ekran kalıcı olarak yarı-dolu kalır (olay tabanlı
// tespitin kör noktası).
//
// ÇÖZÜM: olaydan bağımsız, periyodik kendi kendini onaran emniyet katmanı:
// her HMI_RESYNC_INTERVAL_MS'te bir, skalar alanlardan yalnızca SIRADAKİ
// TEKİ change-cache'e BAKILMAKSIZIN zorla gönderilir ve sıra bir sonraki
// alana ilerler (round-robin). Burst YOKTUR: tetik başına tek alan → UART
// bütçesi asla aşılmaz (bkz. SystemConfig.h static_assert). Ekran, tespit
// edilemeyen bir reset sonrasında bile en kötü ihtimalle
//     HMI_RESYNC_FIELD_COUNT × HMI_RESYNC_INTERVAL_MS
// içinde kendini toparlar (13 × 500 ms = 6.5 sn). 13 slotun 1'i (MOTOR_ERR)
// ATIL'dır — gönderimi yoruma alındı (bkz. DisplayHMI.cpp) — yani fiilen
// yenilenen alan sayısı 12'dir; slot yerinde bırakıldı çünkü enum sırası ile
// updateScreen gönderim sırası arasındaki birebir eşleşme invaryantını
// kaydırmak riskli, tam tur süresi ise değişmiyor.
//
// Tick birimi agnostiktir (çağıran aynı birimi verir); unsigned çıkarma
// (now - lastResyncTick) sayaç taşmasında da doğru sonuç verir — bkz.
// AutobaudPolicy.h / BmsFreshness.h ile aynı idiom. Durum çağırana aittir
// (DisplayHMI üye değişkenlerde tutar), fonksiyon çağrı başına en fazla BİR
// alan döndürdüğünden aralık yanlışlıkla görev periyodunun altına ayarlansa
// bile yük kendiliğinden görev frekansıyla sınırlanır.
//
// Referans desen: lib/CanManager/AutobaudPolicy.h. Testler:
// test/test_native_hmi_helpers/test_resync_policy.cpp.

// updateScreen'deki gönderim sırasıyla BİREBİR aynı alan sırası (Documents/
// HMI_Field_Map.md "Nextion Object Names" tablosuyla uyumlu tutulmalı).
enum HMI_ResyncField : uint8_t {
    HMI_RESYNC_SPEED = 0,
    HMI_RESYNC_BAT,
    HMI_RESYNC_RPM,
    HMI_RESYNC_TORQUE,
    HMI_RESYNC_TEMP,
    HMI_RESYNC_PACKV,
    HMI_RESYNC_PACKA,
    HMI_RESYNC_STATE,
    HMI_RESYNC_MOTOR_ERR,  // ATIL SLOT — gönderim yoruma alındı (bkz. DisplayHMI.cpp)
    HMI_RESYNC_VALID,
    HMI_RESYNC_CONTACTOR,
    HMI_RESYNC_CHG,         // chg.val — şarj/deşarj durumu (ChargeState.h)
    HMI_RESYNC_HEADLIGHT,   // far.pic durum göstergesi (şartname B2 9.19.c)
    HMI_RESYNC_FIELD_COUNT  // = 13 (1'i atıl: MOTOR_ERR → fiilen 12 alan yenilenir)
};

// Resync vadesi geldiyse zorla gönderilecek alanın indeksini (0..fieldCount-1)
// döndürür ve sırayı ilerletir; vade dolmadıysa -1 döner (durum DEĞİŞMEZ).
inline int hmi_resync_due_field(uint32_t now, uint32_t& lastResyncTick,
                                uint8_t& nextField, uint8_t fieldCount,
                                uint32_t intervalTicks) {
    if (fieldCount == 0) return -1;
    if ((uint32_t)(now - lastResyncTick) < intervalTicks) return -1;
    lastResyncTick = now;
    const int field = nextField;
    nextField = (uint8_t)((nextField + 1u) % fieldCount);
    return field;
}
