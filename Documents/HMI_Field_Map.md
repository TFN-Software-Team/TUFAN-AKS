# HMI Field Map

This document defines the current AKS -> Nextion field mapping used by the firmware in Phase 4.

## Runtime Data Model

The firmware now builds one `HMI_DisplayData` snapshot per HMI refresh cycle.

Fields currently included:

| Firmware Field | Source | Description |
| --- | --- | --- |
| `HMI_currentSpeed` | `TEL_motorRpm` | Main speed / RPM value shown on screen |
| `HMI_currentBattery` | `TEL_bmsSoc` | Battery state of charge |
| `HMI_motorRpm` | `TEL_motorRpm` | Raw motor RPM |
| `HMI_motorTorqueFeedback` | `TEL_motorTorqueFeedback` | Signed torque feedback |
| `HMI_motorErrorFlags` | `TEL_motorErrorFlags` | Motor driver error flags |
| `HMI_motorDataValid` | `TEL_motorDataValid` | Freshness indicator |
| `HMI_motorTimeoutActive` | `TEL_motorTimeoutActive` | Timeout indicator |
| `HMI_bmsTemperatureC` | `TEL_bmsTemperatureC` | BMS temperature |
| `HMI_bmsPackVoltageDeciV` | `TEL_bmsPackVoltageDeciV` | BMS pack voltage |
| `HMI_bmsPackCurrentCentiA` | `TEL_bmsCurrentCentiA` | BMS pack current |
| `HMI_bmsDataValid` | `TEL_bmsDataValid` | BMS freshness — `chg` NO_DATA dalının girdisi |
| `HMI_bmsTimeoutActive` | `TEL_bmsTimeoutActive` | BMS post-reception timeout — `chg` NO_DATA dalının girdisi |
| `HMI_chargerActive` | `TEL_chargerActive` | Araç şarjda mı — `chg` CHARGING dalının girdisi. **Yalnız gösterim**; LoRa wire formatına serialize EDİLMEZ |
| `HMI_contactorClosed` | `RelayManager::getRelayState()` | True if all positive contactors are closed |
| `HMI_vcuState` | `VcuLogic::getState()` | Current VCU state |
| `HMI_headlightOn` | `VcuLogic::isHeadlightOn()` | Headlight state (physical switch). Screen only **shows** it; `false` whenever `RELAY_ROLES_ASSIGNED=0` (honest state) |

## Nextion Object Names

The current firmware expects these object names on the Nextion page:

| Nextion Object | Type | Firmware Command |
| --- | --- | --- |
| `speed` | numeric | `speed.val=<value>` |
| `bat` | numeric | `bat.val=<value>` |
| `rpm` | numeric | `rpm.val=<value>` |
| `torque` | numeric | `torque.val=<value>` |
| `temp` | numeric | `temp.val=<value>` |
| `packv` | float (1 dp) | `packv.val=<deciV>` |
| `packa` | float (2 dp) | `packa.val=<centiA>` |
| `state` | text | `state.txt="..."` |
| ~~`motorErr`~~ | text | **DEVRE DIŞI** — gönderim yoruma alındı (aşağıya bkz.) |
| `valid` | text | `valid.txt="..."` |
| `contactor` | text | `contactor.txt="..."` — ⚠️ ekran projesinde obje **HENÜZ YOK** |
| `chg` | numeric (gizli) | `chg.val=<0..3>` — şarj/deşarj durumu (aşağıya bkz.) |
| `far` | **Picture** | `far.pic=<ID>` — headlight status indicator (see below) — ⚠️ ekranda `pFar` adıyla duruyor, isim tutmuyor |

### ⚠️ Ekran projesi tarafı eksikleri (firmware HAZIR, ekran DEĞİL)

Firmware aşağıdaki komutları **doğru biçimde gönderiyor**, ancak ekran
projesinde (`EV_Dashboard_v01__14_.HMI`) karşılık gelen obje yok/yanlış —
komutlar `bkcmd=0` altında **sessizce yutuluyor**:

| Obje | Durum | Yapılacak |
| --- | --- | --- |
| `contactor` | Obje **yok** | Text bileşeni eklenecek (`txt_maxl≥8`, varsayılan `--`) |
| `warn` | Obje **yok** (BMS paneli, `buildBmsNextionCommands` gönderiyor) | Number bileşeni eklenecek (varsayılan `3`) |
| `far` | `pFar` adıyla var; ayrıca touch event'inde **yerel durum** tutuyor (`vaFarState.val++`) — sözleşme ihlali | `far` olarak yeniden adlandırılacak, event **boşaltılacak** |
| `chg` | Obje **var** (`chg`/`chgtxt`/`tm0`), ama `tm0` 3. durumu tanımıyor ve varsayılan `0` | Varsayılan `3` yapılacak, `tm0`'a `chg.val==3` dalı eklenecek |
| `motorErr` | Varsayılan metni `"Herhangi bir hata bulunamadi..."` — reset anında "hata yok" yalanı | Varsayılan `--` yapılacak |

Adım adım talimat: [NEXTION_EKRAN_YAPILACAKLAR.md](NEXTION_EKRAN_YAPILACAKLAR.md).

### `chg` — şarj / deşarj / boşta göstergesi

`chg` gizli bir **Number** bileşenidir; ekrandaki `tm0` timer'ı değerini
`chgtxt` metnine çevirir. Karar mantığı **saf** ve native testlidir:
`lib/HMIHelpers/ChargeState.h::hmi_chargeState`
(`test/test_native_hmi_helpers/test_charge_state.cpp`).

| `chg.val` | `chgtxt` | Koşul |
| --- | --- | --- |
| `0` | `Bosta` | Akım ölü bandın içinde, şarj yok |
| `1` | `Sarj Oluyor` | `TEL_chargerActive` |
| `2` | `Desarj` | `packCurrentCentiA <= -HMI_CHG_DISCHARGE_DEADBAND_CENTI_A` |
| `3` | `--` | **BMS verisi geçersiz VEYA bayat — durum BİLİNMİYOR** |

Karar sırası (ilk eşleşen kazanır): NO_DATA → CHARGING → DISCHARGING → IDLE.

- **Şarj kararı yeniden üretilmez.** Kaynak `TEL_chargerActive`tır; o bayrak
  zaten iki bağımsız göstergenin OR'udur (charger frame tazeliği **veya**
  `ChargeDetect` akım-işareti tespiti — eşik + debounce + hareketsizlik).
- **Deşarj için ölü bant ŞART:** saha ölçümünde boşta akım `-0.1 A` (Y20).
  Ölü bant olmadan araç dururken ekran "Desarj" yazar.
  `HMI_CHG_DISCHARGE_DEADBAND_CENTI_A = 100` (1.0 A) — **CONFIG, gösterim
  eşiği**, güvenlik eşiği değil (bkz. [Threshold_Ownership.md](Threshold_Ownership.md)).
- **Rejeneratif frenleme** bataryaya pozitif akım basar (`MOTOR_DRIVER_PRESENT=0`
  olduğu için bugün yok). Şarj kararı `ChargeDetect`'in hareketsizlik
  katmanından geçtiği için rejen "şarj" sayılmaz; deşarj dalı yalnız negatif
  akıma baktığı için "deşarj" da göstermez — en fazla `IDLE` görünür.

### `motorErr` — DEVRE DIŞI (MOTOR_DRIVER_PRESENT=0)

Bu alanın gönderimi `DisplayHMI::updateScreen` içinde **yoruma alındı**
(silinmedi). Gerekçe:

- `0x200` frame'ini bugün **hall-effect hız sensörü** üretiyor ve
  `data[7]=0x00` gönderiyor → alan yapısal olarak hep `"0x00"` basıyordu.
- Format ham hex; bit anlamları belgelenmemiş
  ([CAN_Message_Table.md](CAN_Message_Table.md) byte 7: `bit0`=çalışıyor,
  `bit[7:1]`=hata, tek tek anlam **BİLİNMİYOR**).
- "Hata yok" ile "veri yok" ayrımı bu alanda **YOK** — o ayrım `valid`
  alanındadır (`VALID`/`INVALID`/`TIMEOUT`).

**Korunanlar:** `HMI_formatErrorText` helper'ı ve testleri, `HMI_DisplayData::
HMI_motorErrorFlags` alanı ve `main.cpp`'deki ataması (veri akmaya devam
ediyor, yalnız gösterilmiyor), `HMI_RESYNC_MOTOR_ERR` enum girdisi (**ATIL
SLOT** — sıra kaydırmamak için yerinde).

**GERİ AÇMADAN ÖNCE:** (1) sürücü dokümanından bit haritasını çıkar ve
`CAN_Message_Table.md`'ye işle, (2) `HMI_formatErrorText`'i insan-okur metne
çevir, (3) veri yokken `--` sentinel'i bas, (4) `txt_maxl` ve
`HMI_RESYNC_CMD_MAX_BYTES` bütçesini yeni metin uzunluğuna göre kontrol et.

> `VcuLogic`'in motor hata bayrağı → FAULT yolu (`MotorFaultDebounce.h`)
> bundan **tamamen ayrıdır ve DEĞİŞTİRİLMEDİ**. Kapatılan yalnızca
> **gösterim** katmanıdır.

### Bilinen Sınırlamalar

- **`warn=2` iki durumu birden temsil ediyor:** gerçek kritik eşik aşımı
  **ve** `isValid=false` → `makeSafeInvalid()` (BMS verisi geçersiz).
  Operatör bu ikisini `warn` alanına bakarak ayırt **edemez**; `valid`
  alanına (`VALID`/`INVALID`/`TIMEOUT`) da bakması gerekir. İleride ayrı bir
  seviyeyle (ör. `4` = INVALID) çözülmesi önerilir — çözülürse ekran
  tarafındaki eşleme tablosu ve `BmsComputed.h` birlikte güncellenmelidir.
- **`contactor` bir KOMUT durumudur, geri besleme değildir.**
  `RelayManager`'ın bildiği şey verilen komuttur; kontaktör yardımcı
  kontağından bağımsız geri besleme **okunmuyor**. Yapışmış/açılmamış bir
  kontaktör ekranda yine `CLOSED` görünür.

### Float (xfloat) fields

`packv` is a Nextion **float** component with 1 decimal place (`"00.0"`) and
`packa` a **float** component with 2 decimal places (`"00.00"`). A Nextion
xfloat interprets the integer `.val` it receives as `display_value × 10^(decimal places)`:

- `packv`: source is deci-volts (`× 0.1 V`), which already equals `V × 10`, so `.val` is sent unscaled (e.g. `790` → `79.0`).
- `packa`: source is centi-amps (`× 0.01 A`), which already equals `A × 100`, so `.val` is sent unscaled (e.g. `1250` → `12.50`, `-2000` → `-20.00`).

Scaling lives in `HMI_packVoltageToXfloat` / `HMI_packCurrentToXfloat`
(`HMIHelpers.h`). `temp` remains an integer `number` component and is not scaled.

## Text Formatting Rules

| UI Field | Output |
| --- | --- |
| `state` | `INIT`, `IDLE`, `READY`, `DRIVE`, `ESTOP`, `FAULT` |
| ~~`motorErr`~~ | ~~Hex string such as `0x00`, `0x04`, `0xFF`~~ — **DEVRE DIŞI**, gönderilmiyor |
| `valid` | `VALID`, `INVALID`, or `TIMEOUT` |
| `contactor` | `CLOSED` or `OPEN` |
| `chg` | `0`=Bosta, `1`=Sarj Oluyor, `2`=Desarj, `3`=`--` (veri yok) |

## Refresh Behavior

- HMI refresh task runs at `10 Hz`.
- The display driver caches the last transmitted snapshot.
- A Nextion field is only updated if its value changed, the screen is being populated for the first time, or its round-robin resync slot is due (see below).

### Nextion reset (brown-out) recovery

A Nextion brown-out/reset reverts every component to its Editor defaults while
the ESP-side caches (`HMI_lastScreenData`, `BmsNextionCache`) still hold the
last transmitted values — without recovery, unchanged fields would never be
re-sent and the screen would stay at defaults (observed in the field).

The firmware detects the reset and repopulates the screen:

- **Detection** — the Nextion *Startup* event (`0x00 0x00 0x00 0xFF 0xFF 0xFF`)
  it emits on power-up is caught by a pure byte-stream state machine
  (`lib/DisplayHMI/NextionResetDetect.h`) wired in parallel to the
  `readTouchCommand()` RX path. It tolerates fragmented arrival and does not
  interfere with the `0x5A CMD ~CMD` touch-frame parser.
- **Recovery** — on detection the driver (`DisplayHMI::HMI_handleNextionReset`):
  1. re-sends `bkcmd=0` (it is not persistent across a Nextion reset),
  2. invalidates its scalar-field cache (`forceFullRefresh()`), so the next
     `updateScreen()` re-sends all fields exactly like the first call after boot,
  3. raises a one-shot flag consumed by the HMI task via
     `DisplayHMI::consumeResetFlag()`, which resets `BmsNextionCache` and
     re-arms `BMS_firstRun` — this flag affects **only** the 24-cell BMS panel.
- **TX budget** — the cell/bar/balance repopulation is spread across multiple
  10 Hz cycles by the existing `buildBmsNextionCommands` `maxBytes=90` budget
  (cache sentinels + `isWarm=false`, same mechanism as boot), keeping each
  cycle within the 9600-baud UART budget (~96 bytes per 100 ms).
- **Logging** — a rate-limited WARN (`Nextion reset algilandi ...`, at most one
  per `HMI_RESET_WARN_LOG_INTERVAL_MS`, total counter included) is emitted.

This recovery is fully local to the HMI path; LoRa telemetry
(`lib/Telemetry`, `UplinkScheduler`) is unaffected.

### Round-robin resync (safety net for undetected resets)

The Startup event itself can be corrupted or lost while the supply is
collapsing (the RX line is unreliable during a brown-out), in which case the
reset detector never fires. A periodic, event-independent resync layer covers
this blind spot (`lib/DisplayHMI/ResyncPolicy.h`):

- Every `HMI_RESYNC_INTERVAL_MS` (default 500 ms, `SystemConfig.h`) exactly
  **one** scalar field is force-sent regardless of the change cache, then the
  rotation advances: `speed → bat → rpm → torque → temp → packv → packa →
  state → (motorErr: ATIL SLOT) → valid → contactor → chg → far (headlight pic)
  → back to start`.
- **13 slots, 1 of them dead.** `HMI_RESYNC_MOTOR_ERR` is an **ATIL SLOT** —
  the `motorErr` send is commented out (see above), so that trigger emits
  nothing. The real number of refreshed fields is **12**. The slot was kept in
  place because the enum order must match the `updateScreen` send order
  **one-to-one**; shifting the indices would risk that invariant, and the full
  rotation time is unaffected either way.
- No bursts: one command (≤ `HMI_RESYNC_CMD_MAX_BYTES` = 26 B) per trigger,
  ~52 B/s at the default interval — enforced against the UART budget by a
  `static_assert` in `SystemConfig.h`. The `static_assert` formula is
  **independent of the field count** (one field per trigger), so growing the
  rotation from 12 to 13 slots does not change the peak UART load.
  (`"chg.val=3"` = 12 B, well under the 26 B cap.)
- Worst-case self-heal time after an undetected reset:
  `13 slots × 500 ms = 6.5 s`. This is exactly why the headlight is displayed
  through a resync-covered field and the screen holds **no** local headlight
  state: after a Nextion brown-out the `far.pic` indicator re-syncs to the real
  ESP-owned state within one rotation instead of showing a stale icon.
- **Yeni alan eklerken:** `HMI_RESYNC_HEADLIGHT` **son eleman kalmalıdır**
  (native test `test_resync_field_count_is_thirteen_headlight_last` bunu
  kilitler); yeni slotlar onun **önüne** eklenir ve `updateScreen`'deki
  gönderim sırası aynı noktaya taşınır.

The 24-cell BMS panel has its own rotation
(`lib/BmsAlgo/BmsNextionPacket.h::bmsNextionCacheInvalidateSlot`):

- Every `BMS_RESYNC_INTERVAL_MS` (default 1000 ms) one of 27 slots (24 cell
  triples `cellN/jN/balN` + `cellmax` + `cellmin` + `warn`) has its
  `BmsNextionCache` entries invalidated with values that cannot occur in
  production (65534 mV / 255); the existing change-compare + `maxBytes=90`
  path then re-emits that slot.
- Invalidation is *sticky*: if the byte budget runs out in a cycle, the cache
  mismatch persists and the slot is re-emitted on a later cycle — a resync is
  never lost.
- Cell slots wait for the next 1 Hz `updateCells` tick (≤ 1 s extra latency);
  summary slots re-emit on the next cycle.
- Worst-case self-heal time for the full panel:
  `27 slots × 1000 ms + ~2 s tail ≈ 29 s` (the safety-critical scalar fields
  above heal in ≤ 6.5 s; the slower detail-panel tour is a deliberate budget
  trade-off, proven by a combined `static_assert` in `SystemConfig.h`:
  52 B/s + 48 B/s ≤ 15% of the 960 B/s raw UART capacity).

## Command Inputs

Touch commands are 3-byte frames `0x5A <CMD> <~CMD>` (header, command, bitwise-NOT
checksum) parsed by `DisplayHMI::readTouchCommand`. IDs currently recognized by firmware:

| Command ID | Meaning | Full frame (header CMD ~CMD) |
| --- | --- | --- |
| `1` | `START` | `0x5A 0x01 0xFE` |
| `2` | `RESET` | `0x5A 0x02 0xFD` |
| `3` | `EMERGENCY_STOP` | `0x5A 0x03 0xFC` |
| `4` | `DRIVE_ENABLE` | `0x5A 0x04 0xFB` |
| `5` | **UNUSED — RESERVED** (do not reassign) | — |

### Command `5` — unused / reserved

Command `5` was previously `HEADLIGHT_TOGGLE` (frame `0x5A 0x05 0xFA`). The
headlight is now controlled by a **physical switch** (`HEADLIGHT_SWITCH_PIN`,
şartname B2 9.19.c), so the screen no longer sends any headlight command — the
firmware no longer handles command 5. The ID is **kept permanently free**: if it
were reassigned to another command, an old screen project still emitting
`0x5A 0x05 0xFA` would trigger the wrong action. Command IDs 1-4 remain
START/RESET/EMERGENCY_STOP/DRIVE_ENABLE.

### `far` (Picture) — headlight status indicator contract

The headlight is displayed with a Nextion **Picture** component, **objname
`far`**. The firmware sends `far.pic=<ID>` where `<ID>` is
`HMI_PIC_HEADLIGHT_ON` / `HMI_PIC_HEADLIGHT_OFF` (`SystemConfig.h`, **CONFIG** —
placeholder `1`/`0`, to be replaced with the real Nextion resource IDs once the
screen project is prepared). The command is generated by the pure helper
`HMI_formatPicCommand` (`HMIHelpers.h`, native-tested) and gated by the same
change-compare + resync path as every other field (sent when the value changes,
on the first refresh, or when the `HMI_RESYNC_HEADLIGHT` resync slot is due).

**The screen does NOT control the headlight and holds NO local headlight
state.** The single owner of the state is the ESP (`VcuLogic::isHeadlightOn`,
driven by the physical switch). This is deliberate: a Nextion brown-out reset is
an observed event in this system — if the screen kept its own headlight state,
the icon would diverge from reality after a reset. Because the ESP owns the
state and it rides the round-robin resync (`far` is a resync field), the
indicator re-syncs to the true state within one resync rotation (≤ 6 s) instead
of showing a stale icon. The Picture component's touch event must be left
**empty** in the Nextion project. See `RELAY_CH_HEADLIGHT` and the headlight
switch table in [RELAY_CHANNEL_TABLE.md](RELAY_CHANNEL_TABLE.md).
