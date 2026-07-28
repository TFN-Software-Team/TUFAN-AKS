# Motor Sürücüsü Entegrasyon Notu (G2)

> Kapsam: Bu belge, motor sürücüsü araca **entegre edilmeden önceki** durumu
> dürüstçe belgeler ve entegrasyon günü yapılacakları listeler. Sürücü geldiğinde
> `MOTOR_DRIVER_PRESENT` bayrağı `1` yapılınca iskelet devreye girer.

## 1. G2 riski (mevcut durum, dürüst özet)

Motor sürücüsü **henüz hazır değil ve araca bağlı değil**. Bu yüzden:

- Hiçbir gerçek torque komutu gönderilmiyor (`CanManager::sendTorqueCommand`
  bayrak 0 iken frame üretmez, yalnız bir kez uyarı loglar; bayrak 1 olsa
  bile frame içeriği henüz DOĞRULANMADIĞI için gönderim yine gerçekleşmez —
  aşağıya bkz.).
- E-STOP / FAULT durumunda `VcuLogic::handleEmergencyStop` /`handleFault`
  güvenli kapanış sırasını (sıfır-tork → bekle → kontaktör aç) **çağrı olarak
  kurar**, ama sıfır-tork adımı bayrak 0'da gerçek frame üretmediğinden
  kontaktörler **yük altında açılabilir**.
- `VCU_CONTACTOR_OPEN_DELAY_MS = 20 ms` **semboliktir** — gerçek tork sönüm
  süresine göre kalibre edilmemiştir.
- **2026-07-28 DÜZELTME — gecikme fiilen ÇALIŞMIYORDU:** sıra "çağrı olarak"
  kuruluydu ama `E-STOP`/`FAULT` yolunda (1) sıfır-tork ve (3) kontaktör açma
  **AYNI tick'e** düşüyordu, `STOP` yolunda ise ikisi **ardışık iki satırdı** —
  yani `VCU_CONTACTOR_OPEN_DELAY_MS` fiilen **0**'dı. Ayrıntı ve düzeltme için
  aşağıdaki §7'ye bakın. Bugün (`MOTOR_DRIVER_PRESENT=0`) bunun sahada bir
  etkisi yoktu (tork zaten üretilmiyor); **motor sürücüsü entegre edilince
  doğrudan ark/kontak kaynaması riski olurdu.**
- **2026-07-13 GÜNCELLEME (thread-safety hazırlığı, madde 4 çözüldü):**
  `CanManager::sendTorqueCommand` artık VCU task'inden çağrıldığında
  `twai_transmit`'i ASLA doğrudan çağırmaz — istek bir
  `TorqueRequestQueue`'ya (`lib/CanManager/TorqueRequestQueue.h`, saf/atomic,
  FreeRTOS/twai bağımsız) yazılır; gerçek gönderim CAN task döngüsünde
  (`processRxMessages()` içinden, her tik) `drainTorqueQueue()` ile çekilip
  yapılacak. Bu, tasarımı ŞİMDİDEN doğru task'e bağlıyor; frame İÇERİĞİ hâlâ
  TODO (aşağıya bkz.) — motor sürücü spec'i gelmeden `drainTorqueQueue()`
  içindeki gövde derlenmez bile (bkz. madde 3'teki `#error` guard'ı).

**Saha riski:** Motor tork üretirken kontaktör açılırsa **ark, kontak kaynaması
ve regen aşırı gerilimi** oluşabilir. Bu risk yalnızca motor sürücüsü entegre
edildiğinde gerçek olur (şu an sürücü yokken tehlike oluşmaz).

## 2. `MOTOR_DRIVER_PRESENT` bayrağının kapsadığı yerler

Bayrak `include/SystemConfig.h` içinde tanımlı (varsayılan `0`). Bayrak `1`
yapıldığında etkilenen tüm noktalar:

| Yer | Bayrak 0 (mevcut) | Bayrak 1 (entegrasyon) |
|-----|-------------------|------------------------|
| `VcuLogic.h::isReadyEntryPermitted` (P1 READY interlock) | motor verisi READY girişini bloklamaz | ek şart: `TEL_motorDataValid == true` |
| `VcuLogic.cpp::readyRejectReason` | `motorDataValid` reddedilme nedeni değil | `motorDataValid=0` READY reddi nedeni olur |
| `VcuLogic.h::hasCriticalCondition` — `TEL_motorErrorFlags != 0` | **kritik SAYILMAZ** (bkz. §6) | kritik → FAULT / kontaktör açma |
| `VcuLogic.h::hasCriticalCondition` — `TEL_motorTimeoutActive` | **kritik SAYILMAZ** (bkz. §6) | IDLE dışında kritik → FAULT |
| `VcuLogic.h::isResetInterlockSatisfied` — `TEL_motorErrorFlags != 0` | reset'i **bloklamaz** | reset'i bloklar |
| `VcuLogic.h::isResetInterlockSatisfied` — `!motorDataValid \|\| motorTimeoutActive` | reset'i **bloklamaz** | AKS-04 fail-safe: reset'i bloklar |
| `VcuLogic.h::isResetInterlockSatisfied` — RPM (`VCU_RESET_MAX_RPM`) | **bayraktan bağımsız** — taze veri varken hareket halinde reset yasak | aynı (kaynak sensör yerine motor sürücüsü) |
| `CanManager.cpp::sendTorqueCommand` | frame YOK, bir kez uyarı loglar, `false` döner, kuyruğa yazmaz | İsteği `TorqueRequestQueue`'ya yazar (`true` döner) — gerçek `twai_transmit` CAN task'inde `drainTorqueQueue()` ile yapılır; frame içeriği **TODO** (`#error` guard `MOTOR_TORQUE_FRAME_DEFINED` tanımlanana kadar derlemeyi engeller) |
| `lib/CanManager/MotorTorque.h::frameEnabled()` | `false` | `true` |
| `test/test_native_ready_motor/` | — | flag=1 derlemesiyle predicate + gate testleri |

## 3. Entegrasyon günü yapılacaklar (checklist)

1. **`sendTorqueCommand`/`drainTorqueQueue` frame formatı** (`CanManager.cpp`,
   `drainTorqueQueue()` içindeki `#if MOTOR_DRIVER_PRESENT` bloğu): motor
   sürücü spec'inden CAN ID, DLC ve byte düzenini **doğrula ve UYDURMADAN**
   doldur; `twai_transmit` çağrısını aç. `CAN_ID_TORQUE_CMD` şu an
   `SystemConfig.h`'de yorumda — doğrulanınca aç. Format tamamlanınca
   `SystemConfig.h`'ye `#define MOTOR_TORQUE_FRAME_DEFINED 1` ekle (aksi
   halde `MOTOR_DRIVER_PRESENT=1` yapıldığında derleme `#error` ile
   BİLEREK kırılır — bkz. madde 3 altındaki not, thread-safety kalemi ÇÖZÜLDÜ).
2. **Delay kalibrasyonu:** `VCU_CONTACTOR_OPEN_DELAY_MS`'i (SystemConfig.h)
   gerçek tork sönüm süresine göre ayarla. Motor RPM/akımının sıfır-tork
   komutundan sonra ne kadar sürede düştüğünü ölç; delay bu süreden büyük
   olsun. **EK (kuyruklama sonrası):** ölçülecek süre artık yalnızca
   "sıfır-tork komutundan motor tepkisine" değil, "VCU'nun `requestZeroTorque`
   çağrısından (t=0) motorun GERÇEKTEN sıfır tork uygulamasına" kadar geçen
   süre olmalı — bu, CAN task'inin `drainTorqueQueue()`'yu bir sonraki tik'te
   çekme gecikmesini de (CAN task döngü periyodu kadar, en kötü durum) İÇERİR.
   Delay'i yalnızca motor tork sönüm süresine göre değil, bu ek gecikmeyi de
   ekleyerek kalibre et.
3. **E-STOP altında düşüş doğrulaması:** E-STOP tetikle, sıfır-tork komutu
   sonrası **motor RPM ve akımının kontaktör açılmadan ÖNCE düştüğünü**
   osiloskop/CAN log ile doğrula. Bu doğrulama yapılmadan sahaya/piste çıkma.
4. ~~**Torque'un CAN task'i dışından gönderimi (thread-safety):**~~ **ÇÖZÜLDÜ
   (2026-07-13).** Sıfır-tork isteği VcuLogic (VCU task) → sink →
   `CanManager::sendTorqueCommand` (CAN task'ine ait örnek) yolundan gelir;
   artık `sendTorqueCommand` isteği yalnızca bir `TorqueRequestQueue`'ya
   (atomic, kilitsiz) yazar — gerçek `twai_transmit` HER ZAMAN CAN task
   döngüsünden (`processRxMessages()` → `drainTorqueQueue()`) çağrılır.
   Native test: `test/test_native_vcu_logic/test_state_machine.cpp`
   `test_estop_zero_torque_reaches_can_queue_before_contactor_open` — E-STOP
   sırasında isteğin kuyruğa kontaktör açılmadan ÖNCE ulaştığını VE
   kuyruklamanın bu sırayı bozmadığını doğrular. Kalan iş: yalnızca madde 1
   (frame İÇERİĞİ) ve madde 2'deki ek gecikme kalibrasyonu.
5. **DC-link kapasitesi / precharge kararı:** Motor sürücüsünün DC-link
   kondansatör kapasitesini kontrol et. **Bataryada precharge devresi YOK**;
   risk yük tarafındaki (motor sürücü) kondansatörden gelir ve motor sürücü
   entegre edilmeden **sorun oluşmaz**. Anlamlı bir DC-link kapasitesi varsa,
   **precharge (donanım devresi) + sıralı kapatma (yazılım)** kararı donanım
   ekibiyle **birlikte** verilecek. (Bu projede precharge rolü tanımlı değildir;
   bkz. SystemConfig.h röle kanal tablosu.)

## 4. İlgili dosyalar

- `include/SystemConfig.h` — `MOTOR_DRIVER_PRESENT`, `VCU_CONTACTOR_OPEN_DELAY_MS`,
  (entegrasyon günü eklenecek) `MOTOR_TORQUE_FRAME_DEFINED`
- `lib/CanManager/CanManager.cpp` / `.h` — `sendTorqueCommand`,
  `drainTorqueQueue` (CAN task döngüsü, `processRxMessages()` içinden
  çağrılır), dosya başındaki `#error` guard'ı
- `lib/CanManager/TorqueRequestQueue.h` — SAF (atomic, FreeRTOS/twai
  bağımsız) VCU task → CAN task tork isteği kuyruğu; native testlerde
  bağımsız test edilir
- `lib/CanManager/MotorTorque.h` — saf frame-gate (`frameEnabled()`)
- `lib/VcuLogic/VcuLogic.h` — motor kaynaklı karar girdilerini kapsayan
  `#if MOTOR_DRIVER_PRESENT` blokları (`hasCriticalCondition`,
  `isResetInterlockSatisfied`, `isReadyEntryPermitted`) — bkz. §6
- `lib/VcuLogic/VcuLogic.cpp` — `handleEmergencyStop` / `handleFault` kapanış
  sırası, `requestZeroTorque`, torque sink hook, `readyRejectReason`
- `src/main.cpp` — `CAN_torqueSink` köprüsü, `VcuLogic::setTorqueSink`
- `test/test_native_vcu_logic/` — E-STOP/FAULT çağrı sırası testleri +
  `test_torque_request_queue.cpp` (kuyruk unit testleri) +
  `test_estop_zero_torque_reaches_can_queue_before_contactor_open`
  (kuyruklamanın E-STOP sırasını bozmadığının entegrasyon testi)

---

## 5. Hız Sensörü CAN Entegrasyonu (2026-07-17)

> Kapsam: Hall-effect hız sensörü ünitesi (esp32-canbus-speed-sensor) artık
> AKS kartına **doğrudan CAN hattı üzerinden** bağlıdır. Geçici test düzeneğinde
> kullanılan 2. bilgisayar (motor-surucu-test reposu, ESP32 + MCP2515 alıcı)
> **devre dışı bırakılmıştır** — artık gerekli değildir.

**Mevcut topoloji:**
```
[Hall sensör ünitesi (ESP32+MCP2515+2 mıknatıs)]
     --- CAN_H / CAN_L (500 kbps, STD 11-bit, ID 0x200) ---
[AKS kartı (ESP32 + TJA1050 TWAI)]
     --- UART1 (9600 baud) ---
[Nextion HMI Ekranı]
```

**Sensör tarafı:** `CAN_MSG_ID = 0x200` (`CAN_ID_MOTOR_STATUS` ile eşleştirildi).
Frame formatı: `data[0:1]` = RPM (big-endian uint16), `data[2:7]` = 0x00, DLC=8,
100 ms periyot (10 Hz). `data[7]` bit0 (isRunning) = 0 bırakıldı — motor sürücüsü
entegre değil (`MOTOR_DRIVER_PRESENT=0`), bu bit VCU karar mantığını ETKİLEMEZ.

**AKS tarafı:** `CanManager::processRxMessages()` → `handleMotorStatus()` →
`CanParse::parseMotorStatus()` → `TEL_motorRpm` → `rpmToSpeedKmhX10()`
(`VehicleParams.h`: D=0.56 m, GR=1.0, direkt tahrik) → `TEL_speedKmhX10` →
`vTask_HMI_Display` (EMA filtre) → Nextion `speed.val=...`. Zincir doğrulandı,
native test eklendi (`test_hall_sensor_rpm850_parse_and_speed` vb.).

---

## 6. Motor CAN'i karar mantığından ayrıldı (2026-07-28) — **ENTEGRASYONDA DEĞİŞİR**

> **Bu bölüm entegrasyon günü DAVRANIŞ DEĞİŞİKLİĞİ anlamına gelir.**
> `MOTOR_DRIVER_PRESENT`'i `1` yapmak, aşağıdaki üç kontrolü tek seferde geri
> açar — bayrağı açmadan önce §3 checklist'iyle birlikte bunu da okuyun.

### Neden

§5'te anlatıldığı gibi, bayrak `0` iken `0x200`'ü **motor sürücüsü değil,
hall-effect hız sensörü ünitesi** üretiyor. Buna rağmen `VcuLogic` iki karar
noktasında bu frame'i hâlâ bir *motor sürücüsü* sinyali gibi ele alıyordu:

- **Sahte FAULT:** sensör bir an sussa (`TEL_motorTimeoutActive`),
  `hasCriticalCondition` READY/DRIVE'da kritik döndürüp **kontaktör
  açtırıyordu** — oysa kaybolan şey yalnızca hız göstergesidir.
- **KALICI kilitlenme:** aynı koşul `isResetInterlockSatisfied`'ı da
  reddettiği için araç `EMERGENCY_STOP`/`FAULT`'tan **çıkamıyordu** — RESET
  butonu her seferinde reddediliyor, otomatik reset de takılıyordu.
- **Doğrulanmamış sinyalden kontaktör:** `TEL_motorErrorFlags` (frame
  `data[7]`) bit anlamları gerçek motor sürücüsü için doğrulanmış değil
  (bkz. `Documents/CAN_Message_Table.md` `0x200`) — karar mantığına bağlı
  olması `CLAUDE.md` Kural 4 / Ek B ihlaliydi.

### Ne yapıldı

`isReadyEntryPermitted`'deki mevcut desen (`#if MOTOR_DRIVER_PRESENT`) diğer
iki karar noktasına da uygulandı. Bayrak `0` iken motor kaynaklı girdiler
**hiçbir** FAULT/kontaktör/reset kararına girmez; bayrak `1` olduğunda
**tamamı geri gelir** (§2 tablosuna bakın).

**Tek istisna — RPM kontrolü bayraktan BAĞIMSIZ kaldı:** hareket halinde
RESET yasağı (`VCU_RESET_MAX_RPM`, AKS-04) sürüyor, çünkü RPM'i bugün hall
sensörü doğrulanmış biçimde besliyor. Kontrol yalnızca `TEL_motorDataValid`
iken uygulanır: freshness kaybında `CanManager` `TEL_motorRpm`'i **sıfırlamaz**
(son değer donar), dolayısıyla bayat bir RPM'e bakmak sensör yüksek hızda
sustuğunda reset'i yeniden **kalıcı** olarak bloklardı.

### Entegrasyon günü ek adımlar

1. **Bayrağı açmadan önce:** `0x200`'ün kaynağı gerçekten motor sürücüsüne
   geçmiş olmalı. Sürücü ile hall sensörü ünitesi **aynı anda** `0x200`
   yayınlarsa CAN çakışması olur — sensör ünitesi ya devre dışı bırakılmalı ya
   da farklı bir ID'ye alınmalıdır (o durumda `CAN_ID_MOTOR_STATUS` ve
   `rpmToSpeedKmhX10` yolu gözden geçirilir).
2. **`data[7]` bit haritasını doğrula:** sürücü spec'inden hangi bitin hangi
   arızaya karşılık geldiğini teyit et ve `CAN_Message_Table.md`'yi güncelle.
   Bayrak `1` bu bitleri **doğrudan kontaktör açma yetkisine** bağlar.
3. **Timeout süresini kalibre et:** `CAN_MOTOR_STATUS_TIMEOUT_MS` (1500 ms) hall
   sensörünün 100 ms periyoduna göre seçilmişti; sürücünün yayın periyoduna göre
   yeniden değerlendir — bayrak `1` iken bu timeout READY/DRIVE'da FAULT üretir.
4. **Testler:** bayrak `0` davranışı `test/test_native_vcu_logic/`
   (`*_when_flag0` adlı case'ler), bayrak `1` davranışı
   `test/test_native_ready_motor/` içinde kilitlidir. Bayrağı açtığınızda
   `*_when_flag0` testleri **artık geçerli değildir** — bunları `ready_motor`
   paketindeki karşılıklarıyla birlikte gözden geçirip güncelleyin.

---

## 7. Güvenli kapanış SIRASI düzeltildi (2026-07-28)

> **Özet:** "sıfır tork → `VCU_CONTACTOR_OPEN_DELAY_MS` bekle → kontaktör aç"
> sırası **üç yolun da hiçbirinde fiilen çalışmıyordu**. Gecikme artık kod
> tarafından garanti ediliyor. Bugün (`MOTOR_DRIVER_PRESENT=0`) **davranış
> değişmedi** — sıfır tork zaten no-op; değişen yalnızca SIRA GARANTİSİ.

### Neden çalışmıyordu

| Yol | Eski davranış |
|-----|---------------|
| `E-STOP` (`handleEmergencyStop`) | Sıfır tork, handler'ın "ilk tick" guard'ına (`s_stateTimer <= TASK_PERIOD_MS`) bağlıydı. `transitionTo` sonrası `run()` **return** ettiği için handler ilk kez `s_stateTimer == 20` ile çalışıyordu; `VCU_CONTACTOR_OPEN_DELAY_MS == TASK_PERIOD_MS == 20` olduğundan `>= 20` açma koşulu da **aynı tick'te** sağlanıyordu. |
| `FAULT` (`handleFault`) | Aynı hata, aynı gerekçe. |
| `STOP` (`run()` içindeki `STOP_REQUEST` dalı) | `requestZeroTorque()` ve `s_relays->allOff(false)` **ardışık iki satırdı** — arada hiçbir bekleme yoktu. Dosya başındaki `#if MOTOR_DRIVER_PRESENT #warning` tam olarak bunu tarif ediyordu. |

Her üç durumda da gerçekleşen gecikme **0 ms**'ti.

### Nasıl düzeltildi

1. **Adım (1) geçişe taşındı:** sıfır tork artık handler'da değil,
   `transitionTo(EMERGENCY_STOP|FAULT)` içinde (`beginSafeShutdown()`) **t=0'da**
   isteniyor ve `s_uptimeMs` damgası kaydediliyor.
2. **Adım (3) damgaya bağlandı:** kontaktör açma artık
   `contactorOpenDelayElapsed()` kapısından geçiyor — "sıfır tork istendi **ve**
   üzerinden `>= VCU_CONTACTOR_OPEN_DELAY_MS` geçti".
3. **Derleme-zamanı kilidi:** `static_assert(VCU_CONTACTOR_OPEN_DELAY_MS >=
   TASK_PERIOD_MS)`. Bu sayede "gecikme ≥ bir tik" garanti; yani (3) her zaman
   (1)'den **en az bir tick sonra** çalışır. Delay'i §3 madde 2'ye göre
   kalibre ederken bu alt sınırın altına **inmeyin**.
4. **`STOP` de aynı sıraya alındı:** `STOP_REQUEST` yalnızca sıfır torku ister
   ve `s_stopPendingOpen` bayrağını kurar; kontaktör açma + `IDLE` dönüşü bir
   sonraki uygun tick'te tamamlanır. **VCU task'i `vTaskDelay` ile
   BLOKLANMAZ** — bloklamak E-STOP bayrağının, flaşör/fan mantığının ve
   aktüatör doğrulamasının tepkisini de geciktirirdi.
5. **Bekleyen `STOP` iptal edilebilir:** `transitionTo` her durum değişiminde
   `s_stopPendingOpen`'ı temizler; böylece `STOP` beklerken gelen bir
   `E-STOP`/`FAULT` kazanır ve bayat bir `STOP` güvenlik durumunu `IDLE`'a
   **düşüremez**.
6. **`#warning` kaldırıldı** (açık kapandı); yerine gerekçeyi anlatan bir yorum
   bırakıldı.

### Zamanlama sonucu

`E-STOP` isteğinden kontaktör açılmasına kadar geçen süre **1 tick (20 ms)**
olarak KALDI — sıfır tork geçiş tick'inde istendiği için gecikme uzamadı.
Sıfır-tork ile açma arasındaki mesafe ise 0 ms'ten **20 ms**'e çıktı.

### Testler (regresyon)

- `test_state_machine.cpp` — `test_estop_requests_zero_torque_before_opening_contactors`,
  `test_fault_requests_zero_torque_before_opening_contactors`: **1. tick'ten
  sonra `allOff` çağrılmamış olmalı**, 2. tick'te çağrılmalı.
- `test_stop_request.cpp` — `test_stop_zero_torque_precedes_contactor_open_by_a_tick`,
  `test_repeated_stop_does_not_postpone_contactor_open`,
  `test_estop_during_pending_stop_wins_and_cancels_it`.

> **Not:** yalnızca çağrı SIRA NUMARASI karşılaştıran eski testler bu hatayı
> yakalayamıyordu — aynı tick içinde de tork önce çağrıldığından `seq(torque) <
> seq(allOff)` iddiası eskiden de geçiyordu. Kilitlenmesi gereken şey
> **tick ayrımıdır**, sıra numarası değil.

