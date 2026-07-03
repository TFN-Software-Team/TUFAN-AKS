// motor_discovery.cpp
// Geçici keşif aracı: E32 motor sürücünün CAN hattında yayınladığı TÜM
// frame'leri (ID + ham baytlar) filtresiz olarak seri porta basar. Amaç,
// CAN_ID_MOTOR_STATUS varsayımını (0x200) doğrulamak ve parseMotorStatus
// için gerçek bayt-değer eşleştirmesini çıkarmaktır.
//
// Etkinleştirme: -D MOTOR_DISCOVERY_MODE derleme flag'i (bkz. platformio.ini
// [env:motor_discovery] ortamı). Bu flag olmadan dosya tamamen boş derlenir;
// normal firmware'e hiçbir etkisi yoktur. (e22_diagnostic.cpp ile aynı desen.)
//
// Kullanım:
//   pio run -e motor_discovery --target upload
//   pio device monitor
//   ... veri toplama bitince ...
//   pio run -e esp32dev --target upload   (normal firmware geri döner)
//
// Çıktı formatı (pandas-uyumlu, bkz. altta "CSV" bölümü):
//   ms,id_hex,ext,dlc,b0,b1,b2,b3,b4,b5,b6,b7        <- her frame bir satır
//   # ...                                             <- özet/bilgi satırları
//   Python: pd.read_csv("log.txt", comment="#")
//
// Baud taraması: Araç hattı CanManager'da 500 kbps varsayılmış; motor sürücü
// aynı hattaysa 500k'da frame gelmeli. Gelmezse 250k -> 125k -> 1M sırayla
// denenir (Çin menşeli sürücülerde 250k yaygındır). Frame yakalanan ilk
// hızda kilitlenir.
//
// LISTEN_ONLY: ESP32 TWAI dinleme modunda hatta hiçbir bit (ACK dahil)
// basmaz — yanlış baud'da bile sürücünün hata sayaçları etkilenmez.
// DİKKAT: Bench'te hat SADECE sürücü + ESP32'den oluşuyorsa frame'ler ACK
// alamaz ve sürücü yayını kesebilir. Bu durumda aşağıdaki
// MOTOR_DISCOVERY_ACK_MODE flag'ini de ekleyerek NORMAL modda derleyin
// (yalnızca baud kesinleştikten sonra önerilir).

#ifdef MOTOR_DISCOVERY_MODE

#include <cstdio>
#include <cstring>

#include "SystemConfig.h"
#include "driver/twai.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr const char* TAG = "MOTOR_DISC";

// --- Ayarlar -----------------------------------------------------------
static constexpr int      MD_SCAN_WINDOW_MS   = 8000;  // baud başına dinleme
static constexpr int      MD_SUMMARY_PERIOD_MS = 10000;
static constexpr uint8_t  MD_MAX_IDS          = 64;

// --- ID istatistikleri ---------------------------------------------------
struct MD_IdStats {
    uint32_t id;
    bool     extended;
    uint8_t  dlc;
    uint32_t count;
    int64_t  firstSeenMs;
    int64_t  lastSeenMs;
    uint8_t  lastData[8];
    uint8_t  changeMask;  // bit i = 1 ise bayt[i] en az bir kez değişti
};

static MD_IdStats s_stats[MD_MAX_IDS];
static uint8_t    s_statCount = 0;
static uint16_t   s_markerNo  = 0;

static int64_t MD_nowMs() {
    return (int64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static MD_IdStats* MD_statsFor(uint32_t id, bool ext, uint8_t dlc) {
    for (uint8_t i = 0; i < s_statCount; i++) {
        if (s_stats[i].id == id) {
            s_stats[i].dlc = dlc;
            return &s_stats[i];
        }
    }
    if (s_statCount >= MD_MAX_IDS)
        return nullptr;
    MD_IdStats& s = s_stats[s_statCount++];
    memset(&s, 0, sizeof(s));
    s.id = id;
    s.extended = ext;
    s.dlc = dlc;
    s.firstSeenMs = MD_nowMs();
    s.lastSeenMs = s.firstSeenMs;
    return &s;
}

// --- Özet tablosu --------------------------------------------------------
// '#' önekli satırlar pandas'ta comment="#" ile atlanır, CSV bozulmaz.
static void MD_printSummary() {
    if (s_statCount == 0) {
        printf("# [OZET] Hic frame yok. Kontrol: kontak acik mi? CAN-H/L "
               "ters mi? Terminasyon ~60 ohm mu? Transceiver besleniyor mu?\r\n");
        return;
    }
    printf("# [OZET] %u farkli ID | id / tip / adet / ort.periyot / "
           "degisen baytlar\r\n",
           (unsigned)s_statCount);
    for (uint8_t i = 0; i < s_statCount; i++) {
        const MD_IdStats& s = s_stats[i];
        const int64_t span = s.lastSeenMs - s.firstSeenMs;
        const long avgP = (s.count > 1) ? (long)(span / (s.count - 1)) : 0;
        char mask[9];
        for (uint8_t b = 0; b < 8; b++)
            mask[b] = (b < s.dlc) ? ((s.changeMask >> b) & 1 ? 'X' : '.') : ' ';
        mask[8] = '\0';
        printf("#   0x%08lX %s %7lu %6ld ms [%s]\r\n", (unsigned long)s.id,
               s.extended ? "EXT" : "STD", (unsigned long)s.count, avgP, mask);
    }
}

// --- Seri komutlar (UART0): m = marker, s = ozet ------------------------
static void MD_pollSerial() {
    uint8_t c;
    while (uart_read_bytes(UART_NUM_0, &c, 1, 0) == 1) {
        if (c == 'm') {
            s_markerNo++;
            printf("# MARKER %u @ %lld ms\r\n", (unsigned)s_markerNo,
                   (long long)MD_nowMs());
        } else if (c == 's') {
            MD_printSummary();
        }
    }
}

// --- Belirli bir baud'da dinlemeyi dene ---------------------------------
// true dönerse: bu hızda en az bir frame yakalandı, sürücü kurulu kalır.
static bool MD_tryBaud(const twai_timing_config_t& t_config,
                       const char* label) {
#ifdef MOTOR_DISCOVERY_ACK_MODE
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
#else
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
#endif
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        ESP_LOGE(TAG, "twai_driver_install basarisiz (%s)", label);
        return false;
    }
    if (twai_start() != ESP_OK) {
        ESP_LOGE(TAG, "twai_start basarisiz (%s)", label);
        twai_driver_uninstall();
        return false;
    }

    ESP_LOGI(TAG, "Deneniyor: %s — %d ms dinlenecek...", label,
             MD_SCAN_WINDOW_MS);

    const int64_t t0 = MD_nowMs();
    twai_message_t msg;
    while (MD_nowMs() - t0 < MD_SCAN_WINDOW_MS) {
        if (twai_receive(&msg, pdMS_TO_TICKS(100)) == ESP_OK) {
            ESP_LOGI(TAG, "FRAME YAKALANDI — %s kilitlendi.", label);
            return true;  // sürücü kurulu kalıyor, ana döngü devralacak
        }
    }

    // Bu hızda hiçbir şey yok — hata sayaçlarını raporla ve sök
    twai_status_info_t st;
    if (twai_get_status_info(&st) == ESP_OK) {
        ESP_LOGW(TAG, "%s: frame yok (bus_err=%lu rx_err=%lu)", label,
                 (unsigned long)st.bus_error_count,
                 (unsigned long)st.rx_error_counter);
    }
    twai_stop();
    twai_driver_uninstall();
    return false;
}

// ---------------------------------------------------------------------------
// app_main — tek giriş noktası (sadece bu env derleniyor)
// ---------------------------------------------------------------------------
extern "C" void app_main() {
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "  MOTOR CAN KESIF MODU — filtresiz ham dinleme");
#ifdef MOTOR_DISCOVERY_ACK_MODE
    ESP_LOGW(TAG, "  ACK MODU AKTIF (TWAI_MODE_NORMAL) — hatta ACK basilir!");
#else
    ESP_LOGI(TAG, "  LISTEN-ONLY — hatta hicbir bit basilmaz.");
#endif
    ESP_LOGI(TAG, "  Seri komutlar: m = marker bas, s = ozet bas");
    ESP_LOGI(TAG, "================================================");

    // UART0'i komut okumak icin kur (loglar etkilenmez)
    uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);

    // Baud taramasi: proje varsayimi 500k oncelikli, sonra alternatifler
    struct {
        twai_timing_config_t cfg;
        const char* label;
    } MD_baudList[] = {
        {TWAI_TIMING_CONFIG_500KBITS(), "500 kbps"},
        {TWAI_TIMING_CONFIG_250KBITS(), "250 kbps"},
        {TWAI_TIMING_CONFIG_125KBITS(), "125 kbps"},
        {TWAI_TIMING_CONFIG_1MBITS(), "1 Mbps"},
    };

    bool locked = false;
    while (!locked) {
        for (auto& b : MD_baudList) {
            if (MD_tryBaud(b.cfg, b.label)) {
                locked = true;
                break;
            }
        }
        if (!locked) {
            ESP_LOGW(TAG, "Hicbir hizda frame yok — tarama basa donuyor. "
                          "Kablolama/kontak/terminasyon kontrol edin.");
        }
    }

    // CSV başlığı (çıplak printf — pandas dogrudan okuyabilsin)
    printf("ms,id_hex,ext,dlc,b0,b1,b2,b3,b4,b5,b6,b7\r\n");

    int64_t lastSummary = MD_nowMs();
    twai_message_t msg;

    while (true) {
        MD_pollSerial();

        if (MD_nowMs() - lastSummary >= MD_SUMMARY_PERIOD_MS) {
            lastSummary = MD_nowMs();
            MD_printSummary();
        }

        if (twai_receive(&msg, pdMS_TO_TICKS(50)) != ESP_OK)
            continue;

        const int64_t nowMs = MD_nowMs();
        const bool ext = msg.extd;
        const uint8_t dlc =
            (msg.data_length_code > 8) ? 8 : msg.data_length_code;

        MD_IdStats* s = MD_statsFor(msg.identifier, ext, dlc);
        if (s != nullptr) {
            if (s->count > 0) {
                for (uint8_t i = 0; i < dlc; i++)
                    if (msg.data[i] != s->lastData[i])
                        s->changeMask |= (1 << i);
            }
            memcpy(s->lastData, msg.data, 8);
            s->lastSeenMs = nowMs;
            s->count++;
        }

        // CSV satırı: bir frame = bir satır
        printf("%lld,%08lX,%d,%d", (long long)nowMs,
               (unsigned long)msg.identifier, ext ? 1 : 0, (int)dlc);
        for (uint8_t i = 0; i < 8; i++) {
            if (i < dlc)
                printf(",%d", (int)msg.data[i]);
            else
                printf(",");
        }
        printf("\r\n");
    }
}

#endif  // MOTOR_DISCOVERY_MODE
