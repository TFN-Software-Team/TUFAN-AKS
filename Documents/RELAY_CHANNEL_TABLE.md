# Relay Channel Table

All relay outputs are driven through the MCP23S17 and are active-low at the hardware level.

Current software meaning:

- `RelayManager::allOn()` closes every channel in `RELAY_CONTACTOR_BANK_MASK`.
- `RelayManager::allOff()` de-energizes every channel in `RELAY_CONTACTOR_BANK_MASK` (safety open — şartname Bölüm 3, 8.2.a.vi).
- Channels **outside** the bank mask are untouched by `allOn`/`allOff`; their last commanded state is preserved in the shadow register and remains consistent with the `verifyOutputs()` readback check. Flasher (OUT5), cooling fan (OUT7) and headlight (OUT2) are all out-of-bank.
- `VcuLogic::handleReady()` closes **only** `RELAY_DRIVE_BANK_MASK` (S2 + HV−), never `allOn()`.

## Channel decisions (hardware-team approved)

| OUT | Function | Bank membership |
| --- | --- | --- |
| OUT0 | **S1 — charge-line contactor** | Contactor bank (**not** drive bank) |
| OUT1 | **HV− contactor** — opens/closes together with S2 in the drive bank | Drive bank + contactor bank |
| OUT2 | **Headlight (far)** — controlled by a **physical switch** (`HEADLIGHT_SWITCH_PIN` = GPIO27, INPUT_PULLUP), BMS-independent; the screen only **shows** its status (`far.pic`) | **Out of bank** |
| OUT3 | empty / spare — **not wired to any load** | Contactor bank only (**not** drive bank) |
| OUT4 | **S2 — drive-line contactor** | Drive bank + contactor bank |
| OUT5 | **Warning flasher** (audible+visual) | **Out of bank** |
| OUT6 | empty / spare — **not wired to any load** | Contactor bank only (**not** drive bank) |
| OUT7 | **Cooling fan** — automatic, temperature-driven | **Out of bank** |
| OUT8 | empty / spare — **not wired to any load** | Contactor bank only (**not** drive bank) |
| OUT9 | empty / spare — **not wired to any load** | Contactor bank only (**not** drive bank) |

**Not connected to the AKS (documented here, no firmware):**
- **Horn (korna) + wiper (silecek): AKS DIŞI — donanımsal devre (B2 9.17 / B2 9.12.c).** Wired as a stand-alone hardware circuit; the AKS drives no channel for them.
- **Brake lamp (fren lambası): AKS'ye bağlanması YASAK — B2 9.19.b + 9.7.f-g (mekanik NC anahtar zorunlu).** Must be driven by a mechanical normally-closed switch, never by firmware.

## `RELAY_ROLES_ASSIGNED` build flag (SystemConfig.h)

The channel→physical-load mapping (Faz 2, harness) has **not** been confirmed by the hardware team yet, but the S1/S2/flasher/fan/headlight role logic is already enabled: the compile-time flag `RELAY_ROLES_ASSIGNED` in `SystemConfig.h` is currently **1**. Setting it back to `0` restores the legacy single-bank behavior.

| Flag | `RELAY_CONTACTOR_BANK_MASK` | Behavior |
| --- | --- | --- |
| `0` | `0x3FF` (all 10 channels) | Legacy single-bank behavior, byte-for-byte identical to before. A `#warning` is emitted at compile time. Flasher / fan / headlight / S1-S2 logic is compiled out. `RELAY_DRIVE_BANK_MASK` is **not defined**; `HMI_areAllContactorsClosed` falls back to the contactor mask in every state. |
| `1` (current default) | `0x35B` (flasher 5 + fan 7 + headlight 2 excluded) | S1/S2 mode switching (şartname 8.2.a) + temperature warning flasher (6.e.ii) + cooling fan (B3 7.a-b) + headlight (B2 9.19.c) active. `RELAY_DRIVE_BANK_MASK = 0x012` (**only** S2 ch4 + HV− ch1). |

### Why the two masks are asymmetric

`RELAY_DRIVE_BANK_MASK` (0x012) is **not** simply `RELAY_CONTACTOR_BANK_MASK` minus S1 — the four unwired spares (ch3/6/8/9) are in the contactor mask but **not** in the drive mask:

- **Closing (READY entry) is narrow.** `handleReady()` energizes S2 + HV− only. Including the spares would draw four pointless coil currents on every START, stretch the staggered close from 2 to 6 steps (`4 × RELAY_STAGGER_STEP_MS` = 120 ms of extra READY latency), and make the HMI contactor indicator (`contactor.txt`, via `HMI_areAllContactorsClosed`) depend on relays that are not wired to anything.
- **Opening (`allOff` safety open) stays wide.** Şartname 8.2.a.vi requires *everything* to open on a safety problem, so `allOff()` keeps de-energizing the whole contactor bank, spares included — whatever a spare happens to be driving is forced to the safe state.

The asymmetry is safe in this direction only: opening more than necessary is harmless, energizing more than necessary is not. Both invariants are locked by `static_assert` in `SystemConfig.h` (drive mask ⊆ contactor mask; S1 ∉ drive mask).

### NOTE — adding a spare to the drive bank later

When the hardware team wires one of `RELAY_CH_SPARE_3` / `_6` / `_8` / `_9`:

1. **Is the new load part of the drive line** (must be CLOSED in READY/DRIVE)? Then add it to the drive mask in `SystemConfig.h`:
   ```c
   #define RELAY_DRIVE_BANK_MASK \
       ((1u << RELAY_CH_S2_DRIVE) | (1u << RELAY_CH_HVNEG) | (1u << RELAY_CH_SPARE_3))
   ```
   and rename the `RELAY_CH_SPARE_n` macro to its real role. The channel must remain in `RELAY_CONTACTOR_BANK_MASK` so `allOff()` can open it — the subset `static_assert` fails at compile time if it does not.
2. **Is it an auxiliary load** (second headlight, horn, pump — must survive a safety open)? Then do **not** add it to either mask; drive it with an explicit `setRelay()` like the fan/headlight, and extend the "out of bank" `static_assert` group instead.
3. Update the hex values in this table, `HMI_Field_Map.md`, and the `test_mask_contract_values` assertions in `test_roles_relay_mask` / `test_roles_vcu_logic` in the **same commit** (CLAUDE.md Rule 5).

## Channel Map

| Channel Macro | Index | Role (when `RELAY_ROLES_ASSIGNED=1`) | Şartname | Physical Load |
| --- | --- | --- | --- | --- |
| `RELAY_CH_S1_CHARGE` (`RELAY_CH_POS_0`) | 0 | **S1 — charge-line contactor** (closed in IDLE while charger CAN stream is fresh; open in READY/DRIVE; open on FAULT/E-STOP). In the contactor bank, **outside** the drive bank. | 8.2.a.iii / 8.2.a.vii / 8.2.a.vi | TBD during harness validation |
| `RELAY_CH_HVNEG` (`RELAY_CH_POS_1`) | 1 | **HV− contactor** — drive-bank member; opens/closes together with S2 | 8.2.a | TBD during harness validation |
| `RELAY_CH_HEADLIGHT` (`RELAY_CH_POS_2`) | 2 | **Headlight (far)** — driven by `VcuLogic` from a **physical switch** on `HEADLIGHT_SWITCH_PIN` (**GPIO27**, direct ESP32 GPIO with INPUT_PULLUP; active-low, switch to GND). Debounce `HEADLIGHT_DEBOUNCE_MS`=40 ms; switch type `HEADLIGHT_SWITCH_LATCHING` (default **1** = latching/maintained). Latching → far follows the switch **position** (survives ESP reset — if the switch is still "on", far comes back on). BMS-independent. **Outside** the bank mask — `allOff`/`allOn` (FAULT/E-STOP/READY) never change it. The screen no longer controls the headlight; it only **shows** the state (`far.pic`). Pure decision logic: `lib/VcuLogic/HeadlightSwitch.h`. | B2 9.19.c | TBD during harness validation |
| `RELAY_CH_SPARE_3` (`RELAY_CH_POS_3`) | 3 | empty / spare — **not wired**. In the contactor bank (so `allOff` forces it open) but **not** in the drive bank (READY does not energize it). | — | Boş/yedek |
| `RELAY_CH_S2_DRIVE` (`RELAY_CH_POS_4`) | 4 | **S2 — drive-line contactor** | 8.2.a.vii: closed in drive; 8.2.a.iii: open while charging; 8.2.a.vi: open on safety problem | TBD during harness validation |
| `RELAY_CH_FLASHER` (`RELAY_CH_POS_5`) | 5 | **Temperature warning flasher** (audible+visual). Driven by `VcuLogic` from the verified BMS max temperature: ON at ≥55 °C, OFF below 53 °C (`FLASHER_HYSTERESIS_C=2`). **Outside** the contactor bank mask — `allOff()` never extinguishes it, so it stays on through FAULT/E-STOP while the temperature holds. | 6.e.ii / 6.e.iii | TBD during harness validation |
| `RELAY_CH_SPARE_6` (`RELAY_CH_POS_6`) | 6 | empty / spare — **not wired**. Contactor bank only, same as ch3. | — | Boş/yedek |
| `RELAY_CH_FAN` (`RELAY_CH_POS_7`) | 7 | **Cooling fan** — driven by `VcuLogic` from the verified BMS max temperature: ON at ≥40 °C (`FAN_ON_TEMP_C`), OFF at ≤35 °C (`FAN_OFF_TEMP_C`). **Outside** the bank mask — stays on through FAULT/E-STOP so a hot pack keeps cooling. Stale/timed-out BMS data leaves it untouched. Fan is wired to the **NO (normally-open) terminal** (updated in commit 865f8f6; `RELAY_CH_FAN_NC_WIRED=0`); no polarity inversion is needed. | B3 7.a-b | **NO (normally-open) terminal, updated 2026-07-29** |
| `RELAY_CH_SPARE_8` (`RELAY_CH_POS_8`) | 8 | empty / spare — **not wired**. Contactor bank only, same as ch3. | — | Boş/yedek |
| `RELAY_CH_SPARE_9` (`RELAY_CH_POS_9`) | 9 | empty / spare — **not wired**. Contactor bank only, same as ch3. | — | Boş/yedek |

With `RELAY_ROLES_ASSIGNED=0` all ten channels are plain positive-contactor bank outputs (previous table).

## Per-channel polarity — `RELAY_INVERT_MASK` (2026-07-29, SORUN 2)

**Field symptom:** the cooling fan was spinning while the HMI read 32 °C.

**Software was clean.** `RELAY_CH_FAN` is written from exactly one place
(`VcuLogic.cpp::run()` → `fanDesiredState`), and at 32 °C that function returns
`false` (32 ≤ `FAN_OFF_TEMP_C`=35) — the firmware was commanding the fan **off**.

**Root cause was electrical:** the fan load is wired to the relay's **NC
(normally-closed)** terminal. Commanding the relay "off" leaves the coil
de-energized, which leaves NC **closed** — so the fan ran. The logic was exactly
inverted for that one channel.

`RELAY_INVERT_MASK` (`SystemConfig.h`) inverts the logical→pin mapping per channel;
`RelayManager::hwFromLogical()` is the single conversion point used by `begin()`,
`setRelay()`, `allOn()`, `allOff()`, `reinitAndReassert()` and `verifyOutputs()`:

| Channel class | Mapping | Meaning |
| --- | --- | --- |
| Not in mask (NO terminal, default) | pin = `!logical` | active-low driver: load ON = coil energized = pin LOW |
| In mask (NC terminal) | pin = `logical` | load ON = coil **de-energized** = pin HIGH |

Upper layers (`VcuLogic`, HMI, tests, `getRelayState()`) always speak in terms of
the **load**, never the coil; the wiring difference is closed in this one mask.

- Enabled by `RELAY_CH_FAN_NC_WIRED` (default `0` — fan is wired to the NO terminal, commit 865f8f6) → `RELAY_INVERT_MASK = 0u` (no channel is inverted).
- A `static_assert` **forbids** putting any contactor-bank channel in the mask —
  inverting a contactor would make `allOff()` *close* it (the opposite of 8.2.a.vi).
- `begin()` no longer writes a hard-coded `0xFF`; it writes `hwFromLogical(0)` so
  "all loads off" is correct from the first millisecond even on an inverted channel.

**Hardware follow-up:** while the fan stays on NC, its coil is continuously energized
whenever the pack is below 40 °C (i.e. almost always) — wasted current and coil/contact
life. Moving the wire to the **NO** terminal is the permanent fix; then set
`RELAY_CH_FAN_NC_WIRED 0` and nothing else changes.

## S1/S2 mode summary (`RELAY_ROLES_ASSIGNED=1`)

| Mode | S1 (ch 0) | Drive bank: S2 (ch 4) + HV− (ch 1) | Spares (ch 3,6,8,9) | Source |
| --- | --- | --- | --- | --- |
| IDLE (charging **or not**) | **CLOSED** — unconditional "charge-ready" posture | OPEN | OPEN | 8.2.a.iii |
| IDLE + charge current detected (`TEL_chargerActive`) | CLOSED | OPEN (START_REQUEST rejected: "sarj akimi tespit edildi (>2.0A) — sarj sirasinda READY yasak") | OPEN | 8.2.a.iii |
| READY / DRIVE | OPEN (explicitly commanded open on READY entry) | CLOSED (`RELAY_DRIVE_BANK_MASK` = 0x012, not `allOn`) | **untouched** — READY does not energize unwired channels | 8.2.a.vii |
| FAULT / EMERGENCY_STOP | OPEN | OPEN (`allOff` over `RELAY_CONTACTOR_BANK_MASK` = 0x35B) | OPEN — safety open covers the spares too | 8.2.a.vi |

> **S1 policy change (2026-07-29, SORUN 1).** S1 used to follow `TEL_chargerActive`.
> That flag is now derived **only** from pack current (`ChargeDetect`, > +2.0 A while
> stationary) — and current cannot flow unless S1 is already closed, so gating S1 on
> it would deadlock: charging could never start. Charging worked before only because
> `0x1806E5F4` freshness kept the flag permanently true (the false positive itself).
> S1 is therefore held closed in IDLE and opened explicitly on READY entry and by
> `allOff` in FAULT/E-STOP. **Hardware consequence:** the charge-port terminals sit at
> pack voltage while the vehicle is in IDLE. If a plug/proximity detect input is added
> later, S1 should follow that signal instead — only `VcuLogic.cpp::handleIdle()` changes.

Fan (OUT7) and headlight (OUT2) are **independent of the S1/S2 mode** — they are outside the bank mask, so none of the transitions above changes them. The fan tracks the verified BMS max temperature (40 °C ON / 35 °C OFF, hysteresis) in every state including FAULT/E-STOP; the headlight follows the driver's physical switch (`HEADLIGHT_SWITCH_PIN`), independent of BMS and vehicle state.

## Headlight physical switch (`HEADLIGHT_SWITCH_PIN`, `RELAY_ROLES_ASSIGNED=1`)

The headlight (OUT2) is controlled by a **physical driver switch**, not the touchscreen (şartname B2 9.19.c: "farlar sürücünün basacağı bir düğme ile açılıp kapanabilmeli"). The screen now only **displays** the state (`far.pic`), it never controls the light.

| Setting | Value | Notes |
| --- | --- | --- |
| `HEADLIGHT_SWITCH_PIN` | **GPIO27** | Direct ESP32 GPIO, INPUT_PULLUP. Chosen over the MCP23S17 J22 GPB4-GPB7 fallback so the input path is **independent of SPI** (works even if the relay expander resets). Free pin, not a strapping pin, supports internal pull-up. **CONFIG — awaiting hardware-team confirmation** (they route the switch wiring to this pin). |
| `HEADLIGHT_SWITCH_ACTIVE_LEVEL` | `0` | Active-low: switch to GND → LOW = "on" position / pressed. |
| `HEADLIGHT_SWITCH_LATCHING` | `1` (default) | `1` = maintained/latching (automotive norm, recommended): far follows the switch **position**; after an ESP reset the far comes back on if the switch is still on → post-reset desync impossible. `0` = momentary: toggle on the press edge, far OFF at boot. |
| `HEADLIGHT_DEBOUNCE_MS` | `40` | Read at the VCU task period (20 ms); unstable transitions are ignored. |

Wiring: connect the switch between GPIO27 and GND. The pure decision logic (debounce + latching/momentary) lives in `lib/VcuLogic/HeadlightSwitch.h` (native-tested); `main.cpp` binds `gpio_get_level(HEADLIGHT_SWITCH_PIN)` to the `VcuLogic` reader hook.

`TEL_chargerActive` is derived **only from pack current** (`ChargeDetect`: > +2.0 A, vehicle stationary, open/release debounced — see `lib/CanManager/ChargeDetect.h`) and is internal-only; it is never serialized into the LoRa TEL frame (19 fields, v2). It used to be OR'd with `0x1806E5F4` freshness (`CAN_chargerValid`); that term was removed on 2026-07-29 after two live CAN captures showed the frame streams continuously whether or not the vehicle is charging (see `CAN_Message_Table.md` § `0x1806E5F4`). `CAN_chargerValid` / `CAN_CHARGER_TIMEOUT_MS` still exist but only mark stored setpoint freshness — they feed no contactor, mode or interlock decision.

## Faz 1 — kanal↔klemens doğrulaması (DOĞRULANDI, 2026-07-22)

**Yöntem:** Çıplak kartta (klemensler boş, HV ayrık), her yazılım kanalı sırayla tek tek sürüldü ve kartın durum LED'i ile eşlendi. **10 kanalın 10'u da şemayla BİREBİR uyuştu** — kart çizildiği gibi üretilmiş, sapma yok.

| Kanal | Klemens | Röle ref. | Durum LED'i | Test noktası |
| --- | --- | --- | --- | --- |
| ch0 | OUT0 | K1 | D8 | TP3 |
| ch1 | OUT1 | K3 | D13 | TP5 |
| ch2 | OUT2 | K6 | D19 | TP9 |
| ch3 | OUT3 | K9 | D24 | TP11 |
| ch4 | OUT4 | K2 | D9 | TP4 |
| ch5 | OUT5 | K4 | D14 | TP6 |
| ch6 | OUT6 | K7 | D20 | TP8 |
| ch7 | OUT7 | K10 | D25 | TP12 |
| ch8 | OUT8 | K5 | D15 | TP7 |
| ch9 | OUT9 | K8 | D23 | TP10 |

**⚠️ UYARI:** Röle referansları (K1, K3, K6 …) klemens sırasıyla **KARIŞIK** (ör. OUT0=K1 ama OUT4=K2, OUT8=K5). Kablo bağlarken röle numarasına (Kx) **DEĞİL**, klemens **OUT etiketine** bakılacak.

## Faz 2 — kablolama talimatı (yük atamaları, henüz çekilmedi)

Donanım ekibi harness'i çekerken her klemense aşağıdaki yük bağlanacak (klemens = **OUT etiketi**, röle numarası değil):

| Klemens | Fiziksel yük |
| --- | --- |
| OUT0 | **S1 şarj kontaktörü** |
| OUT1 | HV− kontaktörü |
| OUT2 | Far |
| OUT3 | boş / yedek |
| OUT4 | **S2 sürüş kontaktörü** |
| OUT5 | **Flaşör** |
| OUT6 | boş / yedek |
| OUT7 | Soğutma fanı |
| OUT8 | boş / yedek |
| OUT9 | boş / yedek |

> ⚠️ Bu tablo `SystemConfig.h` içindeki `RELAY_CH_*` makrolarından türetilmiştir
> (tek doğruluk kaynağı = kod, CLAUDE.md Kural 1). Daha eski bir sürümü
> S1'i OUT8'e, S2'yi OUT0'a, flaşörü OUT9'a atıyordu; bu eşleme commit
> `ed6e968` ile değişti (S1=OUT0, S2=OUT4, Flaşör=OUT5, Fan=OUT7) ve doküman
> geride kalmıştı. Kablolamadan önce donanım ekibiyle bu tablo teyit edilmeli.

Ayrıca **far düğmesi**: `HEADLIGHT_SWITCH_PIN` = **GPIO27** ile **GND** arasına bağlanacak (INPUT_PULLUP, aktif-düşük).

## Note

Doğrulama iki aşamalıdır; bu ikisi ayrı tutulmalıdır:

- **DOĞRULANDI (Faz 1, bu test — 2026-07-22):** yazılım kanalı `N` ↔ kart klemensi `OUT N` eşlemesi. Çıplak kartta LED tablosuyla 10/10 birebir uyuştu (yukarı).
- **HENÜZ DEĞİL (Faz 2, donanım ekibi):** kart klemensi ↔ fiziksel yük kablolaması. Harness çekilene kadar açık.

`SystemConfig.h` içinde **`RELAY_ROLES_ASSIGNED` şu anda `1`** ("Fiziksel röle yük eşlemesi aktifleştirildi") — yani yukarıdaki S1/S2/flaşör/fan/far rol mantığı derleniyor ve maskeler `0x35B` / `0x012` olarak etkin. Bu doküman daha önce bayrağın `0` kalacağını söylüyordu; kod bayrağı açtığı için (Kural 1: kod kazanır) metin koda hizalandı. **Faz 2 harness'i çekilmeden sahada HV ile çalıştırmadan önce yük kablolamasının yukarıdaki tabloyla eşleştiği teyit edilmelidir.**
