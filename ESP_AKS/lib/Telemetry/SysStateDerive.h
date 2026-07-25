#pragma once
#include <cstdint>

#include "SystemConfig.h"  // SYSSTATE_DERIVE_FROM_CURRENT, SYSSTATE_CURRENT_IDLE_BAND_CENTI_A
#include "VehicleData.h"   // TelemetryData

// SysStateDerive — DOĞRULANMIŞ akım sinyalinden (0xE000 byte[0:1],
// TEL_bmsCurrentCentiA) türetilen `sysState` (TEL 12. alan) ÇALIŞMA MODU.
//
// Y33 KARARI (24.07.2026) — bu yol artık VARSAYILAN AÇIK
// (SYSSTATE_DERIVE_FROM_CURRENT=1):
// BMS'in sistem-durumunu (OK/FAULT) yayınladığı bir CAN ID'ye ULAŞILAMADI ve
// aranmaya devam edilmeyecek. Alan hiçbir zaman parse edilemediği için ham
// değer hep 0 kalıyordu; eski sanitize kuralı (geçersiz -> 4) bunu FAULT'a
// çeviriyor ve UKS/Monitor ekranlarında BMS DAİMA "FAULT" görünüyordu. Bu
// YANILTICIYDI. AKS-17 ham 0'ı nötr 2'ye çevirerek yanıltıcı gösterimi
// durdurdu, ama alan o haliyle HİÇBİR BİLGİ TAŞIMIYORDU (sabit 2).
//
// Yerine geçen mantık, ekibin PCAN saha gözlemine dayanır (Y20): şarjda akım
// +9.8 A (POZİTİF), şarjda değilken -0.1 A, sürüşte -5.6 A (NEGATİF). Yani
// akım İŞARETİ bataryanın çalışma modunu güvenilir biçimde belli eder. Akım
// alanının kaynağı (0xE000 byte[0:1]) DOĞRULANMIŞ durumdadır — bu bir hipotez
// değil, ölçülmüş bir sinyaldir.
//
// KAPSAM SINIRI: bu alan bataryanın ÇALIŞMA MODUNU söyler, BMS'in SAĞLIĞINI
// değil. İkisi aynı şey DEĞİLDİR; BMS sağlığı için bir kaynağımız yok ve bu
// alan öyleymiş gibi davranmaz (FAULT üretmez, aşağıya bkz.).
//
// "BMS VERİSİ YOK" bilgisi bu alandan DEĞİL, TEL frame'inin 16. alanı
// (bmsValid) üzerinden gider — o alan zaten TEL_bmsDataValid'i taşır. UKS ve
// Monitor gösterimi önce bmsValid'e bakar; 0 ise "VERİ YOK" yazar ve bu alanı
// yok sayar. Böylece hem çalışma modu hem bağlantı canlılığı ayrı ayrı ve
// dürüstçe raporlanır.
//
// EK B GÜVEN KURALI: bu türetilmiş değer YALNIZCA UKS telemetri gösterimi
// içindir — VCU karar mantığına (FAULT/kontaktör) BAĞLANMAZ. Çağıran taraf
// (main.cpp LoRa_txSend) bunu yalnızca LoRa TX paketleme yolunda, VcuLogic'in
// okuduğu paylaşılan TelemetryData kopyasına DOKUNMADAN uygular.
//
// FAULT(4) NEDEN ÜRETİLMEZ: akımdan "hata" çıkarılamaz (akım normal aralıkta
// olsa bile BMS başka bir nedenle arızada olabilir, ya da tam tersi) — bu,
// ölçülen sinyalin söyleyebileceğinin ÖTESİNDE bir iddia olurdu. E032/E033
// (alarm/uyarı bitfield adayı, bkz. CAN_Message_Table.md ve BENI_OKU.md 5.1)
// doğrulanırsa FAULT girdisi BURAYA (aşağıdaki deriveFromCurrentImpl
// çağrısından önce/sonra bir kontrol olarak) bağlanabilir — bu, o doğrulama
// tamamlanana kadar bilinçli olarak AÇIK bırakılmış bir genişletme noktasıdır.
//
// HİSTEREZİS: bilinçli olarak EKLENMEDİ. Gerekçe: (1) bu değer yalnızca UKS
// operatör ekranındaki bir GÖSTERİM alanını besler, hiçbir kontaktör/FAULT
// kararını etkilemez — bant sınırında nadir bir 2↔3 titremesi güvenlik
// sonucu doğurmaz, yalnızca kozmetik bir görüntü kararsızlığıdır; (2)
// histerezis EKLEMEK bu saf/stateless fonksiyonu STATEFUL yapardı (önceki
// durumu bir yerde saklamak gerekir), bu da gösterim alanı için gereğinden
// fazla karmaşıklık/test yüzeyi eklerdi; (3) TX periyodu zaten 2 Hz
// (LORA_TX_PERIOD_MS=500) — insan operatörün fark edeceği bir çırpınma
// oranı değil. Ayrıca Y20 ölçümleri (boşta -0.1 A, sürüş -5.6 A, şarj +9.8 A)
// ±0.5 A bandından belirgin biçimde uzaktır; sınırda salınım pratikte
// beklenmez. Ekip histerezis isterse bu karar gözden geçirilebilir.
namespace SysStateDerive {

// UKS sysState sözleşmesi: 1=Deşarj, 2=Boşta, 3=Şarj, 4=FAULT (burada
// ÜRETİLMEZ). idleBandCentiA parametreli (test edilebilir) çekirdek —
// üretim kodu aşağıdaki deriveFromCurrent() sarmalayıcısını kullanır
// (rpmToSpeedKmhX10Impl/rpmToSpeedKmhX10 ile aynı desen, bkz. Telemetry.h).
inline uint8_t deriveFromCurrentImpl(int32_t bmsCurrentCentiA,
                                     int32_t idleBandCentiA) {
    if (bmsCurrentCentiA > idleBandCentiA) return 3;   // Charge
    if (bmsCurrentCentiA < -idleBandCentiA) return 1;  // Discharge
    return 2;                                          // IDLE (|akım| <= bant)
}

inline uint8_t deriveFromCurrent(int32_t bmsCurrentCentiA) {
    return deriveFromCurrentImpl(bmsCurrentCentiA,
                                 SYSSTATE_CURRENT_IDLE_BAND_CENTI_A);
}

// Tek çağrı noktası: main.cpp LoRa_txSend içinde, sanitizeForUplink'ten
// ÖNCE çağrılır (sıra önemli — sanitize önce çalışırsa ham 0'ı 2'ye çevirir
// ve aşağıdaki "yalnız 0 ise uygula" koşulu bir daha tutmaz).
// SYSSTATE_DERIVE_FROM_CURRENT==1 (Y33 sonrası varsayılan) iken yalnızca
// TEL_bmsSystemState HALA 0 ise (gerçek parse henüz eklenmemişse) uygular;
// gerçek bir parse eklenip alan doldurulmuşsa (!=0) ONU EZMEZ. ==0 yapılırsa
// hiçbir şey YAPMAZ (eski davranışa dönüş yolu açık bırakıldı).
//
// BAYAT VERİ KORUMASI: BMS verisi taze değilken (TEL_bmsDataValid==false)
// TEL_bmsCurrentCentiA son GÖRÜLEN değeri tutar. O bayat akımdan mod türetmek
// "şarj ediliyor" gibi YANLIŞ bir şey söyleyebilirdi; bu yüzden bu durumda
// nötr 2 (BOŞTA) yazılır. Operatörün gördüğü gerçek bilgi, aynı frame'deki
// bmsValid=0 alanıdır ("VERİ YOK").
inline void applyIfEnabled(TelemetryData& d) {
#if SYSSTATE_DERIVE_FROM_CURRENT
    if (d.TEL_bmsSystemState == 0) {
        d.TEL_bmsSystemState =
            d.TEL_bmsDataValid ? deriveFromCurrent(d.TEL_bmsCurrentCentiA)
                               : 2U;  // bayat akımdan mod türetme
    }
#else
    (void)d;
#endif
}

}  // namespace SysStateDerive
