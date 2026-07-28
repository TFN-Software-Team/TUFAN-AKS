# AKS Board Bring-Up Checklist

Bu doküman, ESP-AKS kodunun harici breadboard/ESP32'den, araçtaki **gerçek AKS anakartına** (dahili ESP32, dahili TWAI, TJA1050) taşınması sırasında donanım ve yazılımın doğrulanması için hazırlanmıştır.

## 1. Donanım Pin Bağlantıları (SystemConfig.h Referansı)

| Birim | ESP32 Pini (GPIO) | Kart/Şema Etiketi | Açıklama |
| :--- | :--- | :--- | :--- |
| **CAN_TX (TWAI)** | `GPIO_NUM_5` | TX | ESP32'nin dâhili CAN kontrolcüsü TX çıkışı, TJA1050'ye gider. |
| **CAN_RX (TWAI)** | `GPIO_NUM_4` | RX | ESP32'nin dâhili CAN kontrolcüsü RX girişi, TJA1050'den gelir. |
| **HMI_TX (Nextion)** | `GPIO_NUM_33` | screen_RX | ESP32 TX → Nextion Ekran RX |
| **HMI_RX (Nextion)** | `GPIO_NUM_32` | screen_TX | Nextion Ekran TX → ESP32 RX |
| **LORA_TX** | `GPIO_NUM_16` | LR_RXD | ESP32 TX → LoRa RX (UART2) |
| **LORA_RX** | `GPIO_NUM_17` | LR_TXD | LoRa TX → ESP32 RX (UART2) |

> ⚠️ **UYARI (Nextion):** Şemada `screen_RX` / `screen_TX` etiketleri kafa karıştırıcı olabilir (ESP açısından mı, konnektör açısından mı?). Eğer ekrana veri gitmiyorsa, kodda GPIO32 ve GPIO33'ü yer değiştirip deneyin.

## 2. Yazılım Yapılandırması ve Auto-Baudrate

- Koddan eski `MCP2515` veya harici SPI-CAN kütüphanesi tamamen kaldırıldı; sistem ESP-IDF'in resmi **TWAI** (Two-Wire Automotive Interface) donanım sürücüsü üzerinde çalışıyor.
- `CanManager::begin()` içinde **Auto-Baudrate Detection** döngüsü mevcuttur. Kod sırasıyla `500kbps`, `125kbps` ve `250kbps` hızlarını dener. CAN bus üzerindeki cihazlardan sinyal yakaladığı (1 saniye içinde geçerli paket okuduğu) hızı kilitler.

## 3. Beklenen İlk Log Çıktısı (Boot Sequence)

Seri portu (UART0 - 115200 baud) bilgisayara bağlayıp kartı başlattığınızda aşağıdakilere benzer loglar akmalıdır:

```text
I (xx) DisplayHMI: Initialized on UART1 (TX=IO33, RX=IO32)
I (xx) CanManager: CAN bitrate auto-detect deneniyor: 500kbps
I (xx) CanManager: CAN bitrate BASARIYLA bulundu: 500kbps  <-- VEYA 125kbps
...
D (xx) CanManager: LB-E000: packV=790 deciV (79.0 V)
D (xx) CanManager: LB-E001: temp1=25 C, temp2=24 C
```

## 4. Sorun Giderme (Troubleshooting)

Eğer `CAN bitrate auto-detect basarisiz oldu! Fallback: 500kbps` görüyorsanız ve sonrasında hiç `LB-E000` logu gelmiyorsa:

1. **Fiziksel Hata:** TJA1050'nin beslemesi (5V) var mı? TJA1050, 3.3V ile çalışmaz (RX/TX 3.3V tolere eder ama VCC 5V olmalıdır).
2. **Kablo & Terminasyon:** CAN_H ve CAN_L kabloları BMS'e doğru mu bağlı? Hattın iki ucunda 120 ohm terminasyon direnci (toplam eşdeğer ~60 ohm) var mı? Multimetreyle CAN kapalıyken direnç ölçün.
3. **Pin Tersliği:** `CAN_TX (5)` ve `CAN_RX (4)` fiziksel olarak transceiver'ın yanlış pinlerine gidiyor olabilir mi?
4. **BMS Uykuda:** BMS güç tuşu/anahtarı açık mı? Şarj/Deşarj hattında bir işlem yapılmadığı için BMS uyku modunda olabilir.
5. **Nextion Ekranında Değerler Yok:** CAN'den log geliyor ama ekranda `--` veya `0` varsa; J8 soketindeki TX/RX (32/33) kablolarını çaprazlayın (Rx-Tx yer değiştirin).

> **Fallback artık KALICI değildir.** Eskiden `Fallback: 500kbps` logundan sonra
> auto-baud bir daha asla yeniden denenmezdi — BMS boot anında sessizse
> (uykuda/geç açılıyor, madde 4) veya bus gerçekte 125/250 kbps ise AKS kalıcı
> olarak sağır kalıyordu. Artık bitrate doğrulanana (veya ilk geçerli frame
> alınana) kadar CAN task döngüsü her `CAN_AUTOBAUD_RETRY_INTERVAL_MS`
> (varsayılan 5 sn, `SystemConfig.h`) bir yeniden dener — tek tikte tüm 3 hız
> denenmez, rotasyonla tek hız denenir (`CanManager::retryAutobaudIfNeeded`,
> saf karar mantığı: `lib/CanManager/AutobaudPolicy.h`). Beklenen davranış:
> - Fallback'te kaldığı sürece en fazla 1 dakikada bir `CAN bitrate hala
>   dogrulanamadi (fallback 500kbps) — retry devam ediyor` WARN'ı görülür.
> - BMS sonradan uyanır/bus hızı yakalanırsa `CAN bitrate GEC de olsa
>   bulundu: <hız>` INFO logu ve ardından `LB-E000` akışı beklenir — kart
>   sıfırlanmadan kendi kendine düzelir.
> - Bir kez geçerli frame alındıktan (fallback hızında bile) SONRA kesilirse bu
>   artık bitrate sorunu değildir; BMS/E000-E001 timeout logları (mevcut
>   `TEL_bmsTimeoutActive` yolu) geçerlidir, autobaud retry devreye GİRMEZ.

## 5. Ekran Butonları Doğrulaması

Bu bölüm, Nextion ekranındaki her butonun (a) firmware'e ULAŞTIĞINI ve
(b) DOĞRU durum geçişini tetiklediğini bench'te tek tek doğrulamak içindir.
Ekran butonu 3 baytlık `[0x5A][CMD][~CMD]` çerçevesi gönderir
(bkz. `lib/DisplayHMI/DisplayHMI.cpp::readTouchCommand`); komut numaraları
`SystemConfig.h` içindeki `HMI_CMD_*` makrolarıdır.

### Hazırlık

- [ ] Seri port (UART0, **115200 baud**) bağlı ve log akıyor.
- [ ] Boot logunda `TUFAN-AKS <sürüm> (<git hash>)` satırı görülüyor
      (doğru firmware yüklü mü teyidi).
- [ ] **HV KAPALI / araç sehpada** — bu bölüm kontaktör kapatır (`START`),
      tekerlekler yerden kesik olmalıdır.
- [ ] Her butondan sonra `State: <eski> → <yeni>` satırı bekleniyor. Durum
      numaraları: `0=INIT 1=IDLE 2=READY 3=DRIVE 4=EMERGENCY_STOP 5=FAULT`
      (`lib/VcuLogic/VcuLogic.h::VcuState`). Ekrandaki `state.txt` karşılıkları:
      `INIT / IDLE / READY / DRIVE / ESTOP / FAULT`.

### 5.1 Buton → beklenen log + durum geçişi

| # | Buton | Çerçeve | Beklenen seri log satırı | Beklenen geçiş |
| :-- | :--- | :--- | :--- | :--- |
| 1 | START | `5A 01 FE` | `HMI command: START request` | `IDLE → READY` |
| 2 | DRIVE | `5A 02 FD` | `HMI command: DRIVE_ENABLE request` | `READY → DRIVE` |
| 3 | RESET | `5A 03 FC` | `HMI command: RESET request` | `FAULT/ESTOP → IDLE` |
| 4 | E-STOP | `5A 04 FB` | `HMI command: EMERGENCY_STOP request` | herhangi → `EMERGENCY_STOP` |
| 6 | STOP (DUR) | `5A 06 F9` | `HMI command: STOP (kontrollu durdurma) request` | `READY/DRIVE → IDLE` |

> ⚠️ **Komut 5 REZERVEDİR** (eski far toggle) ve bir butona ATANMAMALIDIR —
> bkz. 5.7.

### 5.2 START butonu (`HMI_CMD_START`)

Ön koşul: durum `IDLE` (ekranda `state.txt="IDLE"`), BMS verisi taze,
kritik eşik aşımı yok.

- [ ] `I (xx) APP_MAIN: HMI command: START request`
- [ ] `I (xx) VCU_LOGIC: State: 1 → 2`
- [ ] `I (xx) VCU_LOGIC: Surus banki kademeli kapatildi (S1 acik) — system READY`
      (`RELAY_ROLES_ASSIGNED=1` yolu; bayrak 0 iken
      `All contactors closed staggered — system READY`)
- [ ] Ekranda `state.txt` → `READY`, `contactor.txt` → `CLOSED`
- [ ] Kontaktör bobinleri fiziksel olarak çekti (S2 + HV−), **S1 AÇIK kaldı**
      (şartname 8.2.a.vii)

**Reddedilirse** — bu da geçerli bir sonuçtur, sebebi kaydedin:

- [ ] `W (xx) VCU_LOGIC: READY gecisi reddedildi: <sebep>` görüldü, sebep:
      `______________________` (ör. `actuator fault`, bayat BMS verisi,
      kritik eşik). Log en fazla 1 sn'de bir tekrarlanır (spam kısıtı).

### 5.3 DRIVE butonu (`HMI_CMD_DRIVE_ENABLE`)

Ön koşul: durum `READY`.

- [ ] `I (xx) APP_MAIN: HMI command: DRIVE_ENABLE request`
- [ ] `I (xx) VCU_LOGIC: State: 2 → 3`
- [ ] Ekranda `state.txt` → `DRIVE`
- [ ] `IDLE` iken basıldığında **hiçbir geçiş olmuyor** (komut logu düşer,
      `State:` satırı DÜŞMEZ) — DRIVE yalnız `READY`'de kabul edilir

### 5.4 STOP / "DUR" butonu (`HMI_CMD_STOP`, komut 6)

Ön koşul: durum `READY` veya `DRIVE`. STOP **E-STOP'un yerini TUTMAZ**: arıza
kaydı bırakmaz, RESET interlock'u gerektirmez.

- [ ] `I (xx) APP_MAIN: HMI command: STOP (kontrollu durdurma) request`
- [ ] `I (xx) VCU_LOGIC: STOP: sifir-tork istendi (DRIVE) — kontaktor acma 20 ms sonra`
      (`VCU_CONTACTOR_OPEN_DELAY_MS`)
- [ ] En az bir tik SONRA:
      `I (xx) VCU_LOGIC: STOP: kontrollu durdurma (DRIVE -> IDLE)`
- [ ] `I (xx) VCU_LOGIC: State: 3 → 1`
- [ ] Kontaktörler açtı; **fan / flaşör / far SÖNMEDİ** (bank maskesi dışı)
- [ ] `IDLE`/`FAULT`'ta basıldığında:
      `W (xx) VCU_LOGIC: STOP yok sayildi (durum <n> — yalniz READY/DRIVE)`
- [ ] STOP'a arka arkaya basmak açmayı ERTELEMİYOR (ikinci bası yok sayılır,
      ikinci bir "sifir-tork istendi" satırı DÜŞMEZ)

### 5.5 E-STOP butonu (`HMI_CMD_EMERGENCY_STOP`)

Her durumda çalışmalıdır (kuyruğu bypass eden atomic bayrak yolu).

- [ ] `I (xx) APP_MAIN: HMI command: EMERGENCY_STOP request`
- [ ] `I (xx) VCU_LOGIC: State: <eski> → 4`
- [ ] `E (xx) VCU_LOGIC: EMERGENCY STOP active — all relays off`
- [ ] Ekranda `state.txt` → `ESTOP`, `contactor.txt` → `OPEN`
- [ ] Kontaktörler ANINDA açtı (kademeli açma YOK — açma her zaman anlıktır)
- [ ] `DRIVE`'dan basıldığında da aynı satırlar düşüyor (durum bağımsızlığı)

### 5.6 RESET butonu (`HMI_CMD_RESET`)

Üç ayrı davranışı vardır; üçünü de deneyin.

`FAULT` / `EMERGENCY_STOP`'tan çıkış (interlock SAĞLANMIŞ — araç duruyor):

- [ ] `I (xx) APP_MAIN: HMI command: RESET request`
- [ ] `I (xx) VCU_LOGIC: State: 4 → 1` (veya `5 → 1`)
- [ ] Ekranda `state.txt` → `IDLE`

İnterlock SAĞLANMAMIŞ (motor hâlâ dönüyor / motor verisi bayat):

- [ ] `W (xx) VCU: RESET reddedildi: rpm=<n>, timeout=<0|1>`
- [ ] Durum DEĞİŞMEDİ (`State:` satırı düşmedi) — `VCU_RESET_MAX_RPM`=50

`IDLE`'da RESET (latch'lenmiş aktüatör fault'u için "tekrar dene"):

- [ ] `I (xx) VCU_LOGIC: IDLE: actuator fault temizlendi (RESET)`
- [ ] `State:` satırı DÜŞMEZ (zaten IDLE — bu bir geçiş değildir)
- [ ] Donanım gerçekten bozuksa bir sonraki `RELAY_VERIFY_PERIOD_MS` (100 ms)
      taraması fault'u yeniden latch'ler ve START tekrar reddedilir

### 5.7 Komut 5 — REZERVE (far toggle KALDIRILDI)

Sahadaki eski ekran projeleri hâlâ `5A 05 FA` gönderiyor olabilir. Ekranda
komut 5 gönderen bir buton **KALMAMALIDIR**; kalmışsa firmware onu güvenle
yok sayar. Doğrulamak için (varsa) o butona basın:

- [ ] `W (xx) APP_MAIN: Ignored/Unknown HMI command received: 5`
- [ ] Far rölesi (ch2) **DEĞİŞMEDİ** — far yalnız fiziksel düğmeyle sürülür
      (`HEADLIGHT_SWITCH_PIN` = GPIO27, şartname B2 9.19.c)
- [ ] Hiçbir `State:` satırı düşmedi

### 5.8 Far göstergesi (buton DEĞİL — fiziksel düğme + `far.pic`)

Far ekrandan KONTROL EDİLMEZ, yalnız GÖSTERİLİR.

- [ ] Fiziksel far düğmesi açıldığında:
      `I (xx) VCU_LOGIC: Far ACILDI (fiziksel dugme)`
- [ ] Far rölesi (ch2) çekti ve ekrandaki `far` Picture bileşeni "açık"
      resmine döndü
- [ ] ⚠️ **`far.pic` yanlış/alakasız resim gösteriyorsa** bu bir yazılım hatası
      DEĞİL, teyit edilmemiş bir CONFIG'dir — bkz. 5.9.

### 5.9 Boot'ta teyitsiz CONFIG özeti (YARIŞ ÖNCESİ ZORUNLU)

Firmware boot'ta hâlâ teyitsiz olan CONFIG değerlerini loglar. **Yarışa
çıkmadan önce bu satırların KAYBOLMASI hedeflenir.**

- [ ] Boot logunda far göstergesi uyarısı var mı?
      `W (xx) APP_MAIN: FAR GOSTERGESI TEYITSIZ: HMI_PIC_HEADLIGHT_OFF=0 ON=1
      PLACEHOLDER — EKRAN PROJESINDEN ALINACAK, YARIS ONCESI ZORUNLU`
      → Varsa: Nextion Editor'de `far` bileşeninin iki resminin gerçek
      resource ID'lerini `SystemConfig.h`'ye yazın ve
      `HMI_PIC_HEADLIGHT_CONFIRMED` → `1` yapın.
- [ ] Boot logundaki tek satırlık özeti okuyun ve listelenen her başlığı
      işaretleyin:
      `W (xx) APP_MAIN: TEYITSIZ CONFIG: <liste> — degerler girilene kadar
      bench/yaris davranisi TAHMINE dayali`

  | CONFIG | Bekleyen iş | Onaylanınca 1 yapılacak bayrak |
  | :--- | :--- | :--- |
  | `HMI_PIC_HEADLIGHT_ON/OFF` | Ekran projesinden resource ID | `HMI_PIC_HEADLIGHT_CONFIRMED` |
  | `FAN_ON_TEMP_C` / `FAN_OFF_TEMP_C` | Hücre datasheet + ekip onayı | `FAN_TEMP_CONFIRMED` |
  | `BMS_*_CHARGE_CURRENT_CENTI_A` | Saha kalibrasyonu + ekip onayı | `BMS_CURRENT_THRESHOLDS_CONFIRMED` |
  | `SYSSTATE_CURRENT_IDLE_BAND_CENTI_A` | Boşta akım gürültüsü ölçümü | `SYSSTATE_IDLE_BAND_CONFIRMED` |

- [ ] Hepsi teyit edildiğinde boot logunda WARN yerine
      `I (xx) APP_MAIN: CONFIG: yaris oncesi teyit bekleyen deger YOK.`
      görülüyor.
- [ ] `W (xx) APP_MAIN: Ignored/Unknown HMI command received: <n>` satırı
      beklenmedik bir `n` ile düşüyorsa ekran projesindeki buton komut
      numarası yanlıştır (`HMI_CMD_*` ile karşılaştırın).
