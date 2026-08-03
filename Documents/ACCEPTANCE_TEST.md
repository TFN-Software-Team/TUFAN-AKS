# Batarya (Lithium Balance c-BMS) Kabul Testi Prosedürü (Acceptance Test)

Bu belge, ESP-AKS donanımına Nextion HMI ve Lithium Balance c-BMS bağlandığında **uçtan uca doğrulama** için izlenecek adımları tanımlar.

## Test Ortamı
- ESP-AKS anakartı enerjili (12V) ve J8 konektörüne Nextion Ekran (UART1) bağlı.
- CAN bus hattına (CAN_H, CAN_L) c-BMS bağlı ve BMS uyanık durumda.
- Seri port (115200 baud) logları bir bilgisayardan izleniyor.

---

## Adım 1: Normal Çalışma Durumu (BMS Bağlı)

**İşlem:** Sistemi başlatın, BMS'in CAN verisi gönderdiğinden emin olun.
**Beklenen Seri Log:**
```text
I (xx) CanManager: CAN bitrate BASARIYLA bulundu: 500kbps (veya 125kbps)
D (xx) CanManager: LB-E000: packV=800 deciV (80.0 V)
D (xx) CanManager: LB-E001: temp1=25 C, temp2=24 C
```
**Beklenen Nextion Ekran Çıktısı:**
- **packv:** `80.00` (xfloat ölçeklemesiyle)
- **packa:** Deşarj anında negatif bir sayı (örn: `-1.10`)
- **temp:** `25` (maksimum hücre sıcaklığı `int16_t` tam sayı olarak)
- **bat (SoC):** `%98` (yüzdelik tam sayı)
- **Hücre Voltaj Barları:** **24 hücrenin GERÇEK değerleri dolu.** (Kaynak ID'ler 0xE015–0xE020 olarak DOĞRULANDI; `HMI_CELL_VOLTAGE_SOURCE_VERIFIED = true`, `lib/HMIHelpers/HMIHelpers.h:97`. Bu satır 03.08.2026'da güncellendi — eskiden "boş / `--`" bekleniyordu, o beklenti ARTIK YANLIŞTIR.)

---

## Adım 2: BMS Bağlantısının Kopması (Timeout Testi)

**İşlem:** Sistem çalışırken CAN_H / CAN_L kablolarını (veya BMS gücünü) sökün. Yaklaşık 2 saniye bekleyin.
**Beklenen Seri Log:**
```text
W (xx) CanManager: BMS data timeout (500 ms)
E (xx) VcuLogic: FAULT_DETECTED: BMS baglantisi koptu
```
**Beklenen Nextion Ekran Çıktısı:**
- **packv:** `--`
- **packa:** `--`
- **temp:** `--`
- **bat:** `--`
- **state:** `FAULT`
- Sistem kesinlikle `0` (sıfır) gibi sahte veriler BASTIRMAMALIDIR (Sentinel koruması test edilir).

---

## Adım 3: Hücre Voltajı Kısıtı (Spoofing Yasağı)

**İşlem:** Hücre voltajı göstergelerini inceleyin.

**Beklenen Durum (03.08.2026 itibarıyla):**
Hücre voltajlarının gerçek CAN kaynağı **bulundu** (0xE015–0xE020), bu yüzden
normal çalışmada 24 hücre **gerçek** değerlerini gösterir. Sınanan koruma
şudur: sistem hücre voltajını **packV / 24 olarak HESAPLAYIP ekrana SÜREMEZ**.

- BMS verisi taze iken: 24 hücre gerçek, birbirinden FARKLI değerler gösterir
  (hepsi birbirinin aynısı / tam olarak packV/24 ise bu bir SPOOFING
  belirtisidir ve kabul testi BAŞARISIZDIR).
- BMS verisi bayat/kopuk iken (Adım 2): hücreler `--` gösterir, `0` veya
  uydurma bir ortalama DEĞİL.

Bu, hatalı/dengesiz hücrelerin gözden kaçıp yangın riskine yol açmasını
engelleyen bir korumadır (G8/M4). Ayrıntı:
[CELL_VOLTAGE_INVESTIGATION.md](CELL_VOLTAGE_INVESTIGATION.md).
