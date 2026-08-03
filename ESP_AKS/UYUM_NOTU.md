> # 🗄️ TARİHSEL KAYIT — GÜNCEL DEĞİL, AKSİYON GEREKTİRMEZ
>
> **BU DOKÜMAN E32 DÖNEMİNE AİTTİR ve E22 GEÇİŞİ SONRASI GEÇERLİLİĞİNİ
> YİTİRMİŞTİR.** Aşağıdaki `SPED=0xC4`, `OPTION=0x47`, "E32 config dogrulandi"
> gibi tüm değerler **ESKİ E32-433T30D modülüne** aittir ve bugünkü firmware
> ile eşleşmez — buradaki hiçbir değeri koda veya bench prosedürüne
> KOPYALAMAYIN.
>
> Güncel karşılıkları:
>
> | Konu | Güncel kaynak |
> | --- | --- |
> | E22 register adres/değer sözleşmesi | `ESP_AKS/include/E22Regs.h` (tek doğruluk kaynağı) |
> | Hava hızı / TX periyodu / link zaman aşımı | [`Documents/LoRa_Link_Analysis.md`](../Documents/LoRa_Link_Analysis.md) |
> | Protokol / alan sözleşmesi | [`Documents/UKS_LoRa_Protocol.md`](../Documents/UKS_LoRa_Protocol.md) |
> | Bench'te register teyidi prosedürü | [`Documents/BENCH_E22_TEYIT.md`](../Documents/BENCH_E22_TEYIT.md) |
> | CRYPT senkronu (G7) | [`Documents/E22_CRYPT_SENKRON.md`](../Documents/E22_CRYPT_SENKRON.md) |
>
> **Düzeltme (03.08.2026):** Bu banner eskiden güncel E22 detayları için
> `Documents/README.md`'ye yönlendiriyordu; o dosya E22 ile ilgili DEĞİLDİR
> (kök README'nin bayat bir kopyasıdır), bu yüzden yönlendirme yukarıdaki
> tabloyla değiştirildi.
>
> Aşağıdaki "Saha Menzil Testi" prosedürünün *yöntemi* (kademeli mesafe +
> paket kaybı eşikleri) hâlâ mantıklıdır, ancak içindeki SPED/OPTION
> değerleri E32'ye aittir — menzil testi yapılacaksa eşikler korunup
> register değerleri güncel E22 kaynaklarından alınmalıdır.

# AKS–UKS LoRa Uyum Notları

## SPED=0xC4 Değişikliği (SPED=0x1A yerine)

| Alan | Eski (0x1A) | Yeni (0xC4) |
|------|------------|-------------|
| UART baud (bit[7:6]) | 00 → 1200 baud | 11 → 9600 baud |
| Parity (bit[5:3]) | — | 000 → 8N1 |
| Air data rate (bit[2:0]) | 010 → 2.4 kbps | 100 → 9.6 kbps |

**Gerekçe:** 19 alan / ~78 byte v2 telemetri paketi 2.4 kbps'de ~260 ms havada kalır;
5 Hz (200 ms) periyoda sığmaz. 9.6 kbps'de ~65 ms'ye düşer, bol marj kalır.
Ayrıca SPED=0x1A yanlış bit mapping nedeniyle UART baud'u 9600 değil 1200 olarak
ayarlıyordu; şeffaf modda MCU↔E32 iletişimi kesiliyordu.

**ÖNEMLİ — UKS tarafı (U-ALIGN adımı):**
UKS lora.h dosyasında da `SPED = 0xC4` yapılmalıdır.
Eski değerler 0x1A veya 0xC2 geçersizdir. İki taraf aynı SPED değerini taşımadan
hiçbir paket iletilmez.

---

## Adım 3 — Donanım Doğrulaması (Ekip Sahada Yapacak)

1. **AKS seri monitörü** (115200 baud) açılınca şu satır görünmeli:
   ```
   E32 config dogrulandi: SPED=0xC4 CHAN=0x17 OPTION=0x47
   ```
   Görünmüyorsa: AUX pin pull-up'ını kontrol et (GPIO35 harici 10k pull-up gerekli).

2. **UKS seri monitörü**'nde gerçek AKS'ten `TEL,...` satırı görünmeli (UKS güncellemesi
   bittikten sonra doğru parse edilir).

---

## Adım 9.2.k — Saha Menzil Testi (Claude Code Otomatik Yapamaz)

SPED=0xC4 ile hava hızı artığından E32 teorik hassasiyeti biraz düşebilir.
Aşağıdaki prosedürü ekip sahada yapmalı.

### Prosedür

- **Ortam:** Açık alan (yarış pisti benzeri), bina / orman engeli olmadan.
- **Ölçüm:** UKS dashboard'undaki `seq_gaps / good_packets` sayacı ile paket kaybı oranı.
- **Kademeli mesafe:** 200 m → 500 m → 1000 m

| Mesafe | Paket kaybı (%) | Kabul eşiği |
|--------|----------------|-------------|
| 200 m  | ...            | < %1        |
| 500 m  | ...            | < %2        |
| 1000 m | ...            | < %5        |

### Başarısızlık durumunda

1. `OPTION=0x47` bit[1:0]=11 → 21 dBm olduğunu datasheet'ten teyit et.
   30 dBm olmasını istiyorsan bit[1:0]=00 → `OPTION=0x44` yapılabilir; ancak
   bu güç tüketimini artırır ve lokal kurallara göre izin gerektirebilir.
2. Hava hızını 9.6 kbps yerine 4.8 kbps'e çek (bit[2:0]=011 → `SPED=0xC3`);
   UKS tarafını aynı anda güncelle.

### Test kaydı

```
Menzil testi: <tarih>, <mesafe>, <paket kaybı %>
Örnek: 2026-07-15, 1000 m, %2.1
```
