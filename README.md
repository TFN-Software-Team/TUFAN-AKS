> ⚠️ **BU DİZİN (repo kökü) ASIL ARAÇ FİRMWARE'İNİ İÇERMEZ.** Bu, bataryayı ilk keşif/test amaçlı kullanılan bir CAN sniffer aracıdır. Asıl AKS firmware'i için `ESP_AKS/` dizinine bakınız (`ESP_AKS/README.md`).

# TUFAN-AKS

TUFAN-AKS is the Vehicle Control Unit (VCU / AKS) firmware for the TUFAN electric vehicle platform. The project targets ESP32 with ESP-IDF through PlatformIO, uses FreeRTOS for task-based concurrency, and keeps all application code in C++17 without Arduino core dependencies.

## Build Environment

- PlatformIO environment: `env:esp32dev`
- Framework: ESP-IDF
- Language standard: C++17
- Main transport interfaces: CAN, SPI, UART

## Sistem Ne Yapar

AKS (Araç Kontrol Sistemi), aracın **kontrol birimi** ve **telemetri
kaynağıdır**. Üç işi vardır:

1. **Kontrol** — kontaktörleri süren, 6 durumlu bir güvenlik durum makinesi
   çalıştırır: `INIT → IDLE → READY → DRIVE`, artı her an girilebilen
   `EMERGENCY_STOP` ve `FAULT`.
2. **Ölçüm** — CAN üzerinden BMS (Lithium Balance cBMS24) ve motor
   sürücüsünü dinler, doğrulanmış sinyalleri tek bir veri sözleşmesinde
   (`TelemetryData`) toplar.
3. **Yayın** — bu veriyi LoRa ile UKS'e (yer istasyonu) gönderir ve Nextion
   ekranına basar. Bağlantı koparsa veriyi tamponlar, dönünce replay eder.

## RTOS Görevleri (4 task)

`src/main.cpp` içinde `xTaskCreatePinnedToCore` ile kurulur:

| Task | İşi |
| --- | --- |
| `CAN_Task` | TWAI RX/TX, BMS + motor frame ayrıştırma, tazelik izleme |
| `VCU_Task` | Durum makinesi, güvenlik eşikleri, röle/kontaktör kararları |
| `HMI_Task` | Nextion ekranı: gösterim + dokunmatik komut okuma |
| `LoRa_Task` | UKS uplink zamanlaması, offline tampon/replay, heartbeat |

## Depo Yerleşimi

- `src/main.cpp` — giriş noktası, task kurulumu, watchdog, kuyruk bağlantıları
- `include/SystemConfig.h` — pin haritası, CAN ID'leri, zamanlama sabitleri,
  eşikler, derleme bayrakları
- `lib/` — **13 kütüphane**, ikiye ayrılır:
  - **Donanıma dokunanlar:** `CanManager` (TWAI), `RelayManager` (SPI/MCP23S17),
    `DisplayHMI` (Nextion UART), `Telemetry` + `LoraLink` + `E22Config` (LoRa)
  - **Saf / native test edilebilir:** `CanParse`, `VcuLogic`, `BmsAlgo`,
    `BmsModel`, `HMIHelpers`, `OfflineBuffer`, `VehicleData`
- `test/` — native (host) test paketleri; beklenen sayılar
  [TEST_BASELINE.md](TEST_BASELINE.md) içinde
- `tools/e2e/` — AKS↔UKS sözleşme (drift) testleri, pytest

## Derleme ve Yükleme

```bash
# Derle
pio run -e esp32dev

# Karta yükle
pio run -e esp32dev --target upload

# Seri monitör (115200)
pio device monitor
```

## Testler

```bash
pio test -e native          # varsayılan derleme (RELAY_ROLES_ASSIGNED=0)
pio test -e native_roles    # rol mantığı varyantı (bayrak=1)
pytest tools/e2e/ -v        # AKS↔UKS sözleşme testleri
```

**Beklenen test sayıları [TEST_BASELINE.md](TEST_BASELINE.md) içindedir.**
Sayı düştüyse bir paket sessizce çalışmıyor olabilir — o dosyaya bakın.

## Derleme Bayrakları

| Bayrak | Varsayılan | Anlamı |
| --- | --- | --- |
| `RELAY_ROLES_ASSIGNED` | **0** | S1/S2 ayrımı, flaşör, fan, far mantığı. **0 çünkü Faz 2 (klemens↔yük) kablolaması bitmedi** — aşağıya bkz. |
| `MOTOR_DRIVER_PRESENT` | **0** | Motor sürücüsü henüz yok; tork komutu ÜRETİLMEZ **ve** motor CAN'i (`0x200`) karar mantığına bağlı DEĞİL — frame'i bugün hall-effect hız sensörü üretiyor, akışın kesilmesi FAULT/kontaktör açma sebebi değil (bkz. [MOTOR_ENTEGRASYON_NOTU.md](Documents/MOTOR_ENTEGRASYON_NOTU.md) §6) |
| `SYSSTATE_DERIVE_FROM_CURRENT` | **1** | BMS durum alanı, doğrulanmış akımdan çalışma modu taşır (Y33) |

## Güncel CAN Kapsamı

Tam ve tek doğruluk kaynağı:
[Documents/CAN_Message_Table.md](Documents/CAN_Message_Table.md).

Ayrıştırılan frame'ler:

| ID | İçerik | Durum |
| --- | --- | --- |
| `0x200` | Motor durumu (RPM, gerilim, hata bayrakları) | RPM DOĞRULANDI; `data[7]` hata bitleri HİPOTEZ — karar mantığına bağlı DEĞİL (`MOTOR_DRIVER_PRESENT=0`) |
| `0x100` | Tork komutu (TX) | `MOTOR_DRIVER_PRESENT=0` — gönderilmiyor |
| `0xE000` | Pack gerilimi, akım, SoC 1, SoC 2 | DOĞRULANDI |
| `0xE001` | Hücre min/max/ort özeti + 2 sıcaklık | DOĞRULANDI |
| `0xE015`–`0xE020` | 24 hücrenin tekil voltajları (4 hücre/frame) | DOĞRULANDI |
| `0x1806E5F4` | **BMS→Charger setpoint broadcast'i** (yalnız dinlenir) — ⚠️ şarj göstergesi DEĞİL, karar mantığına bağlı DEĞİL | DOĞRULANDI (decode) |
| `0xE002`–`0xE006`, `0xE032`, `0xE033` | Alan anlamları çözülemedi | BİLİNMİYOR — karar mantığına bağlı DEĞİL |

## LoRa / E22 Baseline

The radio module is E22-400T30D-V2 (SX1268), a pin-compatible successor to
the retired E32-433T30D. Pin assignments are unchanged; the register-based
configuration protocol and config-mode pin levels are not (see
`include/E22Regs.h` for the address/value contract and
`lib/E22Config` for the pure command-build / response-parse helpers).

Startup mode for normal (transparent) operation:

- `M0 = 0`
- `M1 = 0`

Config mode (boot-time register sync against the contract):

- `M0 = 0`
- `M1 = 1`

Integration notes:

- Configure `LORA_M0_PIN` and `LORA_M1_PIN` before UART traffic starts.
- Use `LORA_AUX_PIN` as a readiness gate before telemetry transmission.
- Keep telemetry TX in transparent UART mode; radio register configuration
  happens once at boot in `vTask_LoRa_UKS` before entering the main loop.

## Relay Mapping Status

Röle katmanı artık şartname Bölüm 3'e (6.e.ii/6.e.iii sıcaklık uyarı flaşörü, 8.2.a S1/S2 kontaktör rolleri) + soğutma fanı (B3 7.a-b) + far (B2 9.19.c) rol makrolarıyla tanımlıdır — `RELAY_CH_S1_CHARGE=0` (şarj hattı), `RELAY_CH_HVNEG=1` (HV−, S2 ile birlikte sürüş bankı), `RELAY_CH_HEADLIGHT=2` (far, bank dışı), `RELAY_CH_S2_DRIVE=4` (sürüş hattı), `RELAY_CH_FLASHER=5` (uyarı flaşörü, bank dışı), `RELAY_CH_FAN=7` (soğutma fanı, bank dışı), `RELAY_CH_SPARE_3/6/8/9` boş/yedek (kontaktör bankı içinde, sürüş bankı DIŞINDA). Korna+silecek AKS DIŞI (donanımsal devre), fren lambası AKS'ye bağlanması YASAK (mekanik NC anahtar). Ayrıntılı tablo ve mod özeti: [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md).

Doğrulama iki aşamalıdır: **Faz 1 (yazılım kanalı ↔ kart klemensi) DOĞRULANDI** — 2026-07-22, çıplak kartta (klemensler boş, HV ayrık) 10 kanal sırayla sürülüp durum LED'iyle eşlendi, 10/10 şemayla birebir uyuştu (ch0→OUT0/D8 … ch9→OUT9/D23; ⚠️ röle ref. K1/K3… klemens sırasıyla karışık — kablolamada OUT etiketine bakılır). **Faz 2 (klemens ↔ fiziksel yük kablolaması) HENÜZ DEĞİL** — donanım ekibi harness'i çekince tamamlanacak. Rol mantığı `RELAY_ROLES_ASSIGNED` derleme bayrağının arkasındadır; bayrak `SystemConfig.h` içinde **şu anda `1`** ("fiziksel röle yük eşlemesi aktifleştirildi") — `0` yapılırsa eski tek-bank davranışına dönülür ve `#warning` basılır. ⚠️ Faz 2 harness'i çekilmeden HV ile sahada çalıştırmadan önce yük kablolaması `RELAY_CHANNEL_TABLE.md` Faz 2 tablosuyla teyit edilmelidir. LED/test-noktası tablosu ve Faz 2 yük atamaları: [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md).

- **Bayrak=0 (eski davranış):** 10 kanalın tamamı tek pozitif kontaktör bankıdır (`RELAY_CONTACTOR_BANK_MASK=0x3FF`); `allOn()`/`allOff()` davranışı önceki sürümle bayt-bayt aynıdır. Flaşör / fan / far ve S1/S2 mantığı derlenmez.
- **Bayrak=1 (kodda şu anki varsayılan):** `RELAY_CONTACTOR_BANK_MASK=0x35B` (flaşör 5 + fan 7 + far 2 hariç) olur; `allOff()` güvenlik açması S1+S2+HV−+yedekleri açar ama flaşörü/fanı/farı söndürmez. Flaşör, doğrulanmış BMS sıcaklığından 55 °C'de yanar, 53 °C altında söner (`FLASHER_HYSTERESIS_C=2`). Soğutma fanı (şartname B3 7.a-b) aynı desenle 40 °C'de açılır, 35 °C'ye inince kapanır (`FAN_ON_TEMP_C`/`FAN_OFF_TEMP_C`, CONFIG); FAULT/E-STOP dahil her durumda çalışır (sıcak batarya soğutması kesilmez), bayat BMS verisinde dokunulmaz. Far (şartname B2 9.19.c) artık **fiziksel bir düğmeyle** (`HEADLIGHT_SWITCH_PIN`=GPIO27, doğrudan ESP32 GPIO + INPUT_PULLUP, SPI'dan bağımsız) kontrol edilir; ekran farı KONTROL ETMEZ, yalnız durumunu GÖSTERİR (`far.pic`, Nextion Picture bileşeni "far"). Düğme tipi `HEADLIGHT_SWITCH_LATCHING` (varsayılan 1 = kalıcı/anahtarlı, otomotiv normu — ESP reset'inde far anahtar konumundan geri gelir), debounce `HEADLIGHT_DEBOUNCE_MS`=40 ms; saf karar mantığı `lib/VcuLogic/HeadlightSwitch.h` (native test edilir). BMS'ten bağımsız, FAULT/E-STOP/READY'de korunur (bank dışı kanal). ⚠️ **AÇIK ÇELİŞKİ — ekran komutu 5 (far toggle):** Bu satır eskiden "komut 5 KALDIRILDI ve kalıcı olarak rezerve edildi" diyordu, ama **kod hâlâ komut 5'i işliyor** (`src/main.cpp` → `case 5` → `VcuEvent::HEADLIGHT_TOGGLE`, `VcuLogic.cpp` içinde `RELAY_ROLES_ASSIGNED` arkasında). Yani ekran farı hâlâ toggle edebiliyor. Üstelik fiziksel düğme mantığı her tick'te far durumunu anahtar konumuna eşitlediğinden (latching mod), ekrandan yapılan toggle bir sonraki tick'te **geri alınır** — iki sürücü aynı kanala yazıyor. Karar verilip tek yol bırakılmalı (bkz. `BENI_OKU.md`). S1/S2 mod anahtarlaması: **IDLE'da S1 koşulsuz kapalıdır** ("şarja hazır" duruşu — 2026-07-29 politika değişikliği: şarj tespiti artık yalnız akım tabanlı olduğundan S1'i o bayrağa bağlamak kilitlenme yaratırdı, akım akması için S1'in kapalı olması gerekir); şarj akımı tespit edilirken (`TEL_chargerActive`) ayrıca READY reddedilir (8.2.a.iii); READY/DRIVE'da S1 açıkça açılır + sürüş bankı (`RELAY_DRIVE_BANK_MASK=0x012` — YALNIZ S2 + HV−) kapalı (8.2.a.vii); güvenlik probleminde hepsi açık (8.2.a.vi). Soğutma fanı kanalı (OUT7) **NC klemense** bağlıdır; `RELAY_INVERT_MASK` o kanalın mantıksal→pin çevrimini tersler (bkz. [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md) "Per-channel polarity"). İki maske bilinçli olarak ASİMETRİKTİR: kablosuz yedek kanallar (ch3/6/8/9) kontaktör bankının içinde ama sürüş bankının dışındadır — READY onları gereksiz yere enerjilendirmez, `allOff` güvenlik açması yine de açar. Ayrıntı: [RELAY_CHANNEL_TABLE.md](Documents/RELAY_CHANNEL_TABLE.md).

Sıcaklık eşikleri 55/70 °C (uyarı/kapanma, 15 °C sabit aralık — şartname 6.e.iii) `SystemConfig.h` ve `VcuLogic.h` içindeki `static_assert`'lerle derleme zamanında kilitlidir. Bayrak=1 varyantının testleri `pio test -e native_roles` ile çalışır (`test_roles_*` suite'leri).

**Bench'te yapılacaklar (HIL — kapsam dışı, native testlerle kanıtlanamaz):** G3 actuator geri-okuma doğrulaması (`RelayManager::verifyOutputs`, OLAT/IODIR readback + re-init/re-assert + actuator-fault) gerçek MCP23S17 ile bench'te doğrulanmalı: (1) çalışırken MCP23S17 gücünü kısa süre keserek brown-out reset tetikle, IODIR'in 0xFF'e döndüğünü ve VcuLogic'in FAULT'a geçtiğini osiloskopla/röle sesiyle teyit et; (2) MISO hattını fiziksel olarak ayırarak readback hatasında güvenli tarafa (kontaktör açık) düşüldüğünü doğrula.

## Contributors

Based on current git history, the repository contributors are:

- Sedat Ali Zevit - Seqat
- Şebnem Orel - sebnemorel
- incubation-0
- Mesalt-f4
- Order
- Nisa Köken - NisaKoken

## Development Rules

Contributor workflow and naming rules are documented in [CONTRIBUTING.md](CONTRIBUTING.md).

## Batarya Entegrasyonu Durumu

ESP-AKS ve Lithium Balance c-BMS entegrasyonu başarıyla devreye alınmıştır. Gerçek CAN sniffer loglarına göre yapılan son senkronizasyonların durumu aşağıdadır:

**Doğrulanan Veriler (DOĞRULANDI):**
- **0xE000**: Pack Voltajı (deciV), Pack Akımı (centiA, deşarjda negatif), SoC1 ve SoC2 (yüzde).
- **0xE001**: BMS Sıcaklığı (byte[6:7] üzerinden max/min seçimi) ve Hücre Özeti (byte[0:1]=min, byte[2:3]=max, byte[4:5]=avg hücre voltajı). **Pilot kabinindeki göstergedeki ÖZET min/max hücre gerilimi artık doğrudan bu mesajdan (BYS'nin kendi raporu) sürülür** (şartname B3 6.c); 24 hücre taraması yalnız E001 yokken fallback'tir. Deci-mV → mV çevrimi YUVARLAR (kesme değil).
- **0xE015–0xE020**: 24 Hücrenin Bireysel Voltajları (her CAN frame 4 hücre barındırır, raw→mV YUVARLAMA). Ekrandaki 24'lük bar paneli bu taramadan beslenir.
- **0x1806E5F4**: BMS'in şarj cihazına duyurduğu gerilim/akım setpoint'i (sadece okunuyor). ⚠️ **Bu frame bir "şarj var" göstergesi DEĞİLDİR:** BMS ayakta olduğu sürece, araç şarjda olsun olmasın ~100 ms'de bir kesintisiz yayınlanır (iki canlı CAN kaydıyla kanıtlandı, 2026-07-29). Eskiden tazeliği `TEL_chargerActive`e OR'lanıyordu ve sistem 7/24 yanlış pozitif şarj durumunda kalıp START/DRIVE'ı reddediyordu; bu bağ KOPARILDI. Şarj tespitinin tek kaynağı artık pack akımıdır (`0xE000` byte[0:1] > +2.0 A, araç duruyorken, debounce'lu — `lib/CanManager/ChargeDetect.h`). Ayrıntı: [CAN_Message_Table.md](Documents/CAN_Message_Table.md).

**Açık İşler ve Bilinmeyenler (BİLİNMİYOR):**
- **0xE002-0xE006, 0xE032, 0xE033**: BMS statik durumu / limit parametreleri / alarm bitfield adayı olduğu düşünülüyor; alan anlamları çözülemedi. Karar mantığını ETKİLEMEZ. Bkz. `BENI_OKU.md` 5.1.
- **BMS sağlık durumu (OK/FAULT) için CAN ID YOK** (Y33, 24.07.2026) ve aranmayacak. Telemetrideki `sysState` alanı bunun yerine **doğrulanmış akımdan türetilen çalışma modunu** taşır (1=Deşarj, 2=Boşta, 3=Şarj); FAULT üretilmez. "BMS verisi yok" bilgisi ayrı bir alandan (`bmsValid`) gider. Bkz. `lib/Telemetry/SysStateDerive.h`.
- **Bitrate:** 500 kbps kullanılıyor ve sahada çalışıyor. cBMS24 föyü 125–1000 kbit/s aralığını desteklediğinden bir çelişki YOK. `CanManager::begin()` içindeki **otomatik hız bulucu (auto-baud)** yine de kalıcı bir güvence olarak duruyor (bkz. [BRING_UP_CHECKLIST.md](Documents/BRING_UP_CHECKLIST.md)).
