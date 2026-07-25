#pragma once
//
// BmsComputed.h — BmsPackData'nın YORUMLANMIŞ (karar verilmiş) sonucu.
//
// Rol 2 (Gömülü Sistem & Algoritma) çıktısının veri sözleşmesi. Ham hücre
// verisi (BmsPackData) computePack() ile bu yapıya dönüştürülür: paket
// gerilimi, min/max hücre, dengeleme bayrakları, SoC ve uyarı seviyesi.
//
// Donanım/IDF bağımlılığı YOKTUR — saf C++17. Hem ESP32 firmware'inde hem de
// native (host) Unity testlerinde derlenebilir.
//
#include <cstdint>

#include "BmsModel.h"  // BMS_CELL_COUNT, BmsPackData (PAYLAŞILAN sözleşme)

// --- Uyarı seviyeleri (warningLevel alanı) ---
// Tek bir bütünleşik ciddiyet derecesi; ekrandaki tehlike metnini de bu belirler.
static constexpr uint8_t BMS_WARN_OK = 0;        // Her şey nominal
static constexpr uint8_t BMS_WARN_WARNING = 1;   // Dikkat — eşiğe yaklaşıldı
static constexpr uint8_t BMS_WARN_CRITICAL = 2;  // Kritik — koruma gerekebilir
// Bu anlık görüntüde 24 hücrenin TAMAMI henüz taze/tam değilse (cellDataValid
// =false — kaynak mapping'i DOĞRULANDI, E015-E020, G8/M4 FIX; false burada
// yalnız "henüz tüm CAN ID'leri gelmedi / freshness timeout" anlamına gelir)
// uyarı seviyesi hesaplanamaz. "Sağlıklı" (OK) göstermek YANLIŞ GÜVEN yaratır;
// CRITICAL göstermek de yalancı alarmdır. Bu sentinel "veri yok / nötr"
// anlamına gelir. NOT: Nextion warn bileşeni bu değeri (3) nötr/"--" olarak
// göstermelidir (ekran/.HMI dosyası işi).
static constexpr uint8_t BMS_WARN_NO_DATA = 3;

// --- Yorumlanmış paket durumu ---
// computePack() bu yapıyı eksiksiz doldurur. Tüketiciler (HMI paketleyici,
// orchestrator) yalnızca bu yapıyı okur; ham eşik mantığını tekrar etmez.
struct BmsComputed {
    uint32_t packVoltageMv;  // 24 hücrenin gerilim toplamı, milivolt.
                             // uint32: 24*4200=100800 mV uint16'ya sığmaz,
                             // nominal 24*3650=87600 mV bile taşardı.

    uint16_t cellMaxMv;       // En yüksek hücre gerilimi, mV
    uint16_t cellMinMv;       // En düşük hücre gerilimi, mV
    uint8_t cellMaxIndex;     // En yüksek hücrenin indeksi [0..23]
    uint8_t cellMinIndex;     // En düşük hücrenin indeksi [0..23]
    uint16_t cellDeltaMv;     // cellMaxMv - cellMinMv (dengesizlik göstergesi)

    // Ortalama hücre geriliminden LİNEER tahmin edilen şarj durumu, 0..100.
    //
    // *** GÖSTERİLEN SoC BU DEĞİLDİR (Y8 kararı, 24.07.2026). ***
    // Ekranda ve telemetride gösterilen TEK SoC, BMS'in kendi raporladığı
    // TEL_bmsSocHundredths'tir (0xE000 byte[4:5], DOĞRULANDI) — üretici hesabı
    // daha güvenilirdir. Bu alan YEDEK/tanı amaçlıdır ve bugün hiçbir gösterim
    // yolunda TÜKETİLMEZ (yalnız native testler okur).
    //
    // Neden duruyor: LiFePO4'ün düz OCV bölgesinde bu tahmin KABADIR (bkz.
    // BmsAlgo.h), ama BMS SoC alanı bir gün kaybolursa/geçersizleşirse elde
    // tutulacak tek yedektir. Buraya bir tüketici bağlanacaksa ÖNCE
    // VehicleData.h'deki tek-kaynak kuralı güncellenmelidir — aksi halde
    // ekranda İKİ FARKLI yüzde görünür ve operatör hangisine güveneceğini
    // bilemez (Y8'in kapatmak istediği tam da budur).
    uint8_t socPercent;

    // Dengeleme (pasif deşarj) bayrakları. balanceFlag[i]=true => i. hücrenin
    // deşarj direnci aktif (o hücre fazla dolu).
    //
    // YALNIZCA GÖSTERİM — AKS DENGELEME YAPMAZ (Y8). Dengeleme cBMS24'ün
    // DONANIM özelliğidir (pasif/dissipative, 200 mA @ 4.2 V, datasheet) ve
    // BMS tarafından kendi başına yürütülür. Bu bayraklar bir KOMUT değildir;
    // BmsNextionPacket yalnızca ekranda hangi hücrenin dengelendiğini
    // göstermek için okur. AKS'ten BMS'e dengeleme komutu GÖNDERİLMEZ.
    bool balanceFlag[BMS_CELL_COUNT];

    uint8_t warningLevel;  // BMS_WARN_OK / WARNING / CRITICAL

    int16_t tempMaxC;  // En yüksek paket sıcaklığı, °C (BmsPackData::packTempMaxC'den — per-hücre sıcaklık kaynağı yok)
    int16_t tempMinC;  // En düşük paket sıcaklığı, °C (BmsPackData::packTempMinC'den)
};
