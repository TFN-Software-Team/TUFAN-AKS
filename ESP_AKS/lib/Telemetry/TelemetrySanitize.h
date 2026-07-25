#pragma once

// Saf, donanimsiz telemetri sanitizasyon fonksiyonlari.
//
// UKS parser'i (TUFAN-UKS-TELEMETRY Core/Src/telemetry.c, Decode_Line)
// her alani ayri ayri sert aralik kontrolunden (Parse_Int) gecirir ve
// TEK alan aralik disindaysa TUM frame'i reddeder (parse_fail) — yani
// RPM, gerilim, sicaklik gibi diger tum gecerli alanlar da birlikte
// kaybolur. Sartname 9.2.b/9.2.d telemetri akisinin surekliligini
// zorunlu kildigi icin AKS, UKS'in kabul araligi disina KESINLIKLE
// cikmayan degerler uretmelidir.
//
// Bu dosya donanim/log bagimliligi olmadan (yalniz <cstdint>/<climits>)
// native ortamda test edilebilir olacak sekilde tasarlandi. Cagiran
// taraf (CanManager) throttle'li WARN log'u kendi bunyesinde uretir.
#include <cstdint>
#include <climits>

#include "VehicleData.h"  // TelemetryData (M3)

namespace TelemetrySanitize {

// UKS Decode_Line: Parse_Int(f[12], min=1, max=4) — bu aralik DISINDA bir
// deger TUM frame'i reddettirir (parse_fail), bu yuzden cikti HER ZAMAN
// 1..4 icinde kalmalidir.
//
// ALAN ANLAMI (Y33 karari, 24.07.2026 — bkz. SysStateDerive.h):
//   1 = DESARJ  (batarya akim VERIYOR — negatif akim)
//   2 = BOSTA   (|akim| <= SYSSTATE_CURRENT_IDLE_BAND_CENTI_A) VEYA
//               BMS verisi taze degil (SysStateDerive::applyIfEnabled)
//   3 = SARJ    (batarya akim ALIYOR — pozitif akim)
//   4 = FAULT   — AKS bu degeri ARTIK URETMEZ. Gecerli aralikta birakildi ki
//                 ileride gercek bir BMS-durum CAN ID'si bulunursa (E032/E033
//                 teyidi, bkz. BENI_OKU.md 5.1) bu sanitizer onu SESSIZCE
//                 YUTMASIN.
//
// KAYNAK YOK: BMS'in SAGLIK durumunu (OK/FAULT) yayinladigi bir CAN ID'ye
// ULASILAMADI. Alan parse edilemedigi icin ham deger hep 0 kalir; bu yuzden
// gercek deger SysStateDerive tarafindan DOGRULANMIS akimdan turetilir ve
// buraya 1/2/3 olarak gelir. "BMS verisi yok" bilgisi bu alandan DEGIL,
// TEL frame'inin 16. alanindan (bmsValid) gider.
//
// FALLBACK: gecersiz/bilinmeyen ham deger 4 (FAULT) DEGIL 2 (BOSTA) verir —
// aralik icinde, notr ve hicbir ariza IDDIASI tasimayan tek deger. Onceki
// davranis 0 -> 4 (FAULT) idi ve UKS'te "BMS DAIMA FAULT" yaniltici
// gosterimine yol aciyordu. Bkz. bulgu T3-21/T2-22, AKS-17, Y33.
inline uint8_t sanitizeSystemState(uint8_t raw) {
    return (raw >= 1U && raw <= 4U) ? raw : 2U;
}

// UKS Decode_Line: Parse_Int(f[15], min=0, max=10000).
inline uint16_t sanitizeSoc(uint16_t raw) {
    return (raw > 10000U) ? 10000U : raw;
}

// UKS Decode_Line: Parse_Int(f[14], min=-2147483647, max=2147483647) —
// tam int32_t araliginin degil, INT32_MIN'i (bir eksik ucta) HARIC
// tutan simetrik bir aralik. INT32_MIN fiziksel olarak anlamsiz bir
// akim degeri oldugundan (sensor/CAN bozulma senaryosu), UKS sinirina
// gore +1 kaydirmanin fiziksel anlam kaybi yoktur.
inline int32_t sanitizeCurrent(int32_t raw) {
    return (raw == INT32_MIN) ? (INT32_MIN + 1) : raw;
}

// UKS Decode_Line: Parse_Int(f[4]="torque", min=-32768, max=32767) —
// int16_t. BILINEN SEMANTIK UYUMSUZLUK: sozlesme bu alani "torque" olarak
// adlandirir, ancak AKS encoder'i (Telemetry.cpp::sendStatus) buraya FIILEN
// TEL_motorVoltageDeciV (motor voltaji, deciV, uint16_t) yazar — gercek tork
// degil. TEL_motorVoltageDeciV 32767'yi asabilir (ornek: 40000 deciV =
// 4000.0 V), asarsa UKS Parse_Int TUM frame'i reddeder (parse_fail) ve rpm,
// BMS vb. diger tum gecerli alanlar da kaybolur. Kalici cozume kadar (bkz.
// Documents/TORQUE_ALAN_KARAR_NOTU.md) bu deger 32767'ye KIRPILIR — bu
// yalnizca frame reddini engeller, "dogru tork degeri" uretmez.
inline uint16_t sanitizeMotorVoltForTorqueField(uint16_t raw) {
    return (raw > 32767U) ? 32767U : raw;
}

inline int16_t sanitizeRpm(int16_t raw) {
    constexpr int16_t TEL_RPM_MAX = 20000;
    if (raw < 0) return 0;
    if (raw > TEL_RPM_MAX) return TEL_RPM_MAX;
    return raw;
}

// Tek ortak sanitize kapısı: canlı VE replay (OfflineBuffer'dan gelen)
// paketler, UKS'e gitmeden hemen önce (sendStatus çağrısının hemen
// öncesinde) buradan geçer — böylece ikisi de aynı garantiye sahip olur
// (S4). Yukarıdaki alan-bazlı fonksiyonların KENDİSİ değiştirilmedi;
// bu yalnızca onları tek noktada birleştiren bir sarmalayıcıdır.
inline TelemetryData sanitizeForUplink(const TelemetryData& raw) {
    TelemetryData out = raw;
    out.TEL_motorRpm           = sanitizeRpm(out.TEL_motorRpm);
    out.TEL_bmsSystemState     = sanitizeSystemState(out.TEL_bmsSystemState);
    out.TEL_bmsSocHundredths   = sanitizeSoc(out.TEL_bmsSocHundredths);
    out.TEL_bmsCurrentCentiA  = sanitizeCurrent(out.TEL_bmsCurrentCentiA);
    out.TEL_motorVoltageDeciV  = sanitizeMotorVoltForTorqueField(out.TEL_motorVoltageDeciV);
    return out;
}

}  // namespace TelemetrySanitize
