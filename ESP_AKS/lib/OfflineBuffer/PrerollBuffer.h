#pragma once
#include <cstdint>
#include "SystemConfig.h"  // LINK_TIMEOUT_MS (kapasite turetme)
#include "VehicleData.h"   // TelemetryData (M3)

// PrerollBuffer — link durumundan BAĞIMSIZ, sürekli ~1 Hz'de son
// PREROLL_CAPACITY kaydı tutan dairesel tampon. Statik bellek, dinamik
// tahsis YOK. Thread-safe DEĞİL; çağıran senkronize eder (OfflineBuffer ile
// aynı desen — bkz. OfflineBuffer.h).
//
// GEREKÇE (9.2.e / 9.2.h): link DOWN, son heartbeat'ten LINK_TIMEOUT_MS
// (9 sn) geçmeden TESPİT EDİLMEZ — bu sürede UplinkScheduler hâlâ "link UP"
// sanıp canlı TX dener; bu paketler RF fiilen kopuksa havada kaybolur ve
// OfflineBuffer'a hiç yazılmaz (offlineSample() yalnız isLinkDown()==true
// iken çağrılır). PrerollBuffer bu "tespit gecikmesi" deliğini kapatır:
// link durumuna bakmadan sürekli örnekler; DOWN geçişi anında içeriği
// OfflineBuffer'ın önüne aktarılır (bkz. UplinkScheduler::updateLink),
// böylece kesintinin ilk ~LINK_TIMEOUT_MS/1000 saniyesi de kayda girer.
//
// KAPSAM SINIRI: bu yalnızca DEDEKTE EDİLEN (>=LINK_TIMEOUT_MS) kesintilerin
// tespit-öncesi penceresini kurtarır. Heartbeat boşluğu LINK_TIMEOUT_MS'e hiç
// ULAŞMAYAN (gerçekten <9 sn süren) bir RF blip'i link FSM'i hiçbir zaman
// DOWN görmez — becameDown hiç tetiklenmez, splice hiç çağrılmaz. Bu durumda
// o pencerede "canlı" sanılan TX'ler yine havada kaybolur ve kurtarılamaz;
// bu, tek yönlü/ACK'siz heartbeat tasarımının (9.2.a) yapısal bir sınırıdır,
// PrerollBuffer'ın kapsamı DIŞINDADIR (bkz. Documents/LoRa_Link_Analysis.md).
//
// Kapasite: LINK_TIMEOUT_MS/1000 + 2 sn'lik marj — derleme zamanında
// LINK_TIMEOUT_MS'ten türetilir (sabit literal YAZILMAZ); LINK_TIMEOUT_MS
// ileride değişirse kapasite otomatik senkron kalır.
#define PREROLL_CAPACITY ((unsigned)((LINK_TIMEOUT_MS) / 1000U) + 2U)

class PrerollBuffer {
   public:
    // Link durumuna BAKMAZ — çağıran her tick koşulsuz çağırır. periodMs
    // geçmediyse (ilk örnek hariç) örnek ALINMAZ, false döner. Dolu ise en
    // eski kayıt düşer (dairesel).
    bool sample(uint64_t nowMs, const TelemetryData& live, uint32_t periodMs) {
        if (m_hasSample && (nowMs - m_lastSampleMs) < (uint64_t)periodMs) {
            return false;
        }
        m_lastSampleMs = nowMs;
        m_hasSample = true;
        if (m_count == (int)PREROLL_CAPACITY) {
            m_head = (m_head + 1) % (int)PREROLL_CAPACITY;
            m_count--;
        }
        m_buf[m_tail] = live;
        m_tail = (m_tail + 1) % (int)PREROLL_CAPACITY;
        m_count++;
        return true;
    }

    int count() const { return m_count; }

    // En eskiyi çıkarır (FIFO) — kronolojik aktarım için. Boşsa false döner.
    bool popOldest(TelemetryData& out) {
        if (m_count == 0) return false;
        out = m_buf[m_head];
        m_head = (m_head + 1) % (int)PREROLL_CAPACITY;
        m_count--;
        return true;
    }

    // DOWN geçişinde aktarım SONRASI çağrılır: aynı pencerenin bir DAHA Kİ
    // geçişte tekrar aktarılmasını (ts tekrarını) önler — pre-roll o andan
    // itibaren sıfırdan birikmeye başlar.
    void reset() {
        m_head = 0;
        m_tail = 0;
        m_count = 0;
        m_hasSample = false;
        m_lastSampleMs = 0;
    }

   private:
    TelemetryData m_buf[PREROLL_CAPACITY];
    int m_head = 0;
    int m_tail = 0;
    int m_count = 0;
    bool m_hasSample = false;
    uint64_t m_lastSampleMs = 0;
};
