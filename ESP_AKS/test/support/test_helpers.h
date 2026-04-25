#pragma once
#include "Telemetry.h"

namespace test_helpers {

// "Tüm değerler güvenli aralıkta, hiçbir uyarı/kritik koşul tetiklenmez"
// referans bir TelemetryData üretir. Tek bir alanı edit edip threshold
// kenarlarını izole etmek kolaylaşsın diye tüm alanlar nominal seçilir.
//
// Notlar:
// - SOC, sıcaklık ve voltaj VCU eşiklerinin tam orta bandında (25°C, 8.0V).
// - Akım sıfır → ne charge ne discharge warning/critical.
// - Tüm error flag'ler 0; data valid; timeout aktif değil.
inline TelemetryData makeTelemetryDataValid() {
    TelemetryData d{};
    d.TEL_motorRpm = 0;
    d.TEL_motorTorqueFeedback = 0;
    d.TEL_motorErrorFlags = 0;
    d.TEL_motorDataValid = true;
    d.TEL_motorTimeoutActive = false;

    d.TEL_bmsSoc = 80;
    d.TEL_bmsCurrentDeciA = 0;
    d.TEL_bmsTemperatureC = 25;
    d.TEL_bmsPackVoltageDeciV = 80;          // 8.0 V — bandın ortası
    d.TEL_bmsAverageCellVoltageMv = 4000;
    d.TEL_bmsErrorFlags = 0;
    d.TEL_bmsDataValid = true;
    return d;
}

}  // namespace test_helpers
