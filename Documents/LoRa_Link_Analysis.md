# LoRa Link Analysis

This note captures the current AKS-side telemetry bandwidth estimate and the practical assumptions for Phase 3 reliability work.

> **Merge note (2026-07-03):** an earlier revision on `main` (commit
> `594a93e`) briefly replaced the payload described below with a
> semicolon-separated `zaman_ms;hiz_kmh;T_bat_C;V_bat_C;kalan_enerji_Wh`
> format. That format is the **TUFAN-Monitor CSV log line**, not the
> AKS→UKS LoRa wire format — UKS's `telemetry.c::Decode_Line` parser
> requires the 19-field `TEL,...` frame described here (see
> `tools/e2e/contract.py`). This doc has been corrected back to describe
> the actual wire format; the merge kept the tested `TEL,...` encoder.

> **Link flapping fix (2026-07-07):** field logs showed the AKS↔UKS link
> constantly cycling DOWN→UP (`LINK: UKS heartbeat timeout` every ~5-6 s).
> Root cause: on the single-frequency half-duplex E22 channel, AKS's
> continuous 5 Hz TX left almost no gap for UKS's 1 Hz `0xB0` heartbeat to
> get through, so it only arrived every ~5-6 s — longer than the old
> `LINK_TIMEOUT_MS = 3000`. Fix: `LORA_TX_PERIOD_MS` lowered `200 -> 500`
> (5 Hz → 2 Hz, opening up channel time for the heartbeat) and
> `LINK_TIMEOUT_MS` raised `3000 -> 9000` (margin over the observed ~5-6 s
> heartbeat interval). The bandwidth figures below are updated for the new
> 2 Hz rate.

> **Air rate revision (2026-07-17):** field range testing found the link
> unreliable at `1.5 km`, well beyond the actual required range (max `500 m`
> for this course). Air rate was first dropped `9.6 -> 2.4 kbps` (with TX
> rate temporarily at `1 Hz`); that intermediate 2.4 kbps/1 Hz configuration
> was never deployed to the field — the same day it was revised again to
> `2.4 -> 4.8 kbps` (`REG0 = 0x63`, bit[2:0] = `011`) with TX rate restored
> to `2 Hz` (`LORA_TX_PERIOD_MS = 500`), trading some of the extra range
> margin back for sensitivity/precision headroom since 500 m does not need
> the full range 2.4 kbps would have bought. UKS's `TEL_LINK_TIMEOUT_MS`
> stayed at `2000 ms` throughout. AKS commits: `9230936` (9.6->2.4 kbps),
> `c083139` (2.4->4.8 kbps); synchronized with UKS the same day.

> **Air rate revision (2026-07-20):** field range testing (following the
> 2026-07-17 change above) found the link unreliable at `1.5 km` — the
> range target is no longer `500 m`. This reverts the `c083139` decision:
> air rate `4.8 -> 2.4 kbps` (`REG0 = 0x62`, bit[2:0] = `010`) and TX rate
> `2 Hz -> 1 Hz` (`LORA_TX_PERIOD_MS = 1000`), i.e. back to the
> mid-July intermediate configuration (equivalent to `9230936`). Team
> approved. Updated figures: a `~90 byte` TEL packet now spends `~380 ms`
> airtime at 2.4 kbps (was `~190 ms` at 4.8 kbps); live-only occupancy is
> unchanged at `~38%` of the (now longer) `1000 ms` tick, and replay-drain
> occupancy (live + 1 replay) is `~76%`. UKS's `TEL_LINK_TIMEOUT_MS` is
> recalibrated `2000 -> 4000 ms` in the same commit set, preserving the
> same `4x` margin over `LORA_TX_PERIOD_MS` (see "UKS-side TEL Timeout
> Margin" below — the invariant `TEL_LINK_TIMEOUT_MS >= 3 x
> LORA_TX_PERIOD_MS` is drift-guarded by
> `test_uks_tel_link_timeout_has_enough_margin_over_tx_period`).
> `OFFLINE_SAMPLE_PERIOD_MS` and `REPLAY_BURST_PER_TICK` are **not**
> changed by this revision — at 1 Hz TX, the link-UP drain rate (1
> record/s) now exactly equals the offline fill rate (1 record/s), so a
> flapping link can no longer *net* drain the buffer; the ring instead
> slides forward and overwrites its oldest records (see
> `lib/OfflineBuffer/OfflineBuffer.h`). This tradeoff was accepted by the
> team, not treated as a defect to fix here.
>
> **G10-b finding — ACCEPTED (2026-07-20 team decision), known constraint
> until the binary-frame transition:** at 2.4 kbps, one worst-case
> `LORA_TEL_FRAME_MAX_BYTES=120 B` frame takes `~500 ms` airtime; the
> busiest tick (live + 1 replay back-to-back) is therefore `~1000 ms` —
> **exactly the full `LORA_TX_PERIOD_MS` tick against the theoretical
> 120 B ceiling, i.e. zero margin on paper.** Unlike the `G10`
> `SystemConfig.h` `static_assert` (which only bounds the local ESP32<->E22
> UART line at 9600 baud and explicitly excludes air rate from its scope),
> this air-time budget has no compile-time enforcement — there is no
> air-rate constant in `SystemConfig.h` to assert against, so the
> derivation is documented as a comment (`SystemConfig.h`, "G10-b") rather
> than as a `static_assert`. Forcing it into a `static_assert` with any real
> safety margin would fail to compile at the current
> `REPLAY_BURST_PER_TICK=1` / `LORA_TX_PERIOD_MS=1000` values, so it was
> **not** done and the formula was **not** loosened to force a pass.
>
> **Why accepted rather than mitigated now:** the `120 B` figure is a
> theoretical ceiling (`LORA_TEL_FRAME_MAX_BYTES`, see the "worst-case digit
> width" comment in `SystemConfig.h`), not the typical case — a realistic
> `TEL` frame is closer to `~65 bytes` (see "Current AKS Telemetry Payload"
> above), giving a live+replay burst of `~540 ms` inside the `1000 ms` tick,
> i.e. `~46%` margin in the common case, not zero. On top of that, two
> independent layers of slack absorb an occasional saturated tick even when
> the ceiling is approached: (1) AUX back-pressure — a busy tick's next
> attempt is simply deferred, not lost (see "Can a TEL frame actually be
> skipped?" below); (2) UKS's `TEL_LINK_TIMEOUT_MS=4000` tolerates 3
> consecutive fully-skipped ticks (see the worst-case gap table below)
> before a false `LINK,DOWN`. Between these, an occasional single tick at
> the theoretical ceiling is not expected to cause field-visible link
> flapping. **`REPLAY_BURST_PER_TICK` and `LORA_TEL_FRAME_MAX_BYTES` are
> deliberately left unchanged.** The durable fix is the planned binary-frame
> transition (see "If Link Margin Is Poor" below), which shrinks the frame
> size enough to reopen real margin at the ceiling case too; this constraint
> is expected to close then, not before. Field validation of this
> assumption (2+ consecutive skipped ticks, replay-drain flapping) remains
> open — see "Recommended Field Checks" below.

## Current AKS Telemetry Payload

Current uplink format (AKS→UKS, v2):

- ASCII CSV line
- Prefix: `TEL,<version>,<sequence>,...` (19 comma-separated fields total, `TEL` tag included)
- Terminator: `\r\n`
- Rate: `1 Hz` (`LORA_TX_PERIOD_MS = 1000`, reverted from `2 Hz` on 2026-07-20)

Representative packet:

```text
TEL,2,0,1500,-250,5,1,0,37734,37422,32,31,2,780,-181610,6283,1,12345,1413\r\n
```

Typical payload size is approximately `50-80 bytes` depending on numeric field widths.
A conservative planning budget of `90 bytes` per packet gives:

- `90 bytes * 1 Hz = 90 bytes/s`
- `90 bytes/s * 10 bits/byte ~= 900 bit/s` on the UART side including start/stop overhead

On the air side, at the `2.4 kbps` E22 air data rate (`REG0 = 0x62`, bit[2:0] = `010`, reverted from `4.8 kbps` on 2026-07-20):

- `90 bytes * 10 bits/byte / 2400 bit/s ~= 380 ms` airtime per packet
- Live-only occupancy: `380 ms / 1000 ms tick ~= 38%`
- During offline-buffer drain (live + 1 replay frame back-to-back): `~760 ms / 1000 ms tick ~= 76%` peak occupancy

## Interpretation

Important distinction:

- The ESP32 <-> E22-400T30D-V2 UART link is configured for `9600 baud`.
- The radio air data rate of the E22 module (currently `2.4 kbps`, `REG0 = 0x62` bit[2:0] = `010`, see `E22Regs.h`) is lower than the UART baud.
- Therefore, final field testing must confirm that the selected E22 radio configuration can drain the UART input fast enough at the chosen packet rate.

## Current Reliability Policy

Implemented now:

- RF link is content-wise one-way: AKS→UKS carries telemetry, UKS→AKS carries only the `0xB0` heartbeat byte (no commands — see `LoraRxHandler.h`, 9.2.a).
- Link-down detection via heartbeat timeout (`LINK_TIMEOUT_MS = 9000`) with a boot-grace window (`BOOT_LINK_GRACE_MS = 5000`) so a UKS that's silent from power-on doesn't look falsely "up".
- OfflineBuffer ring buffer (`OB_CAPACITY = 600`) retains telemetry during link loss, sampled at 1 Hz (`OFFLINE_SAMPLE_PERIOD_MS = 1000`) while offline — 600 records @ 1 Hz gives ~10 minutes of coverage (record size 88 bytes, ~52,800 bytes static buffer).
- On reconnect: up to `REPLAY_BURST_PER_TICK = 1` buffered packets are replayed per TX tick, plus 1 live packet, until the buffer drains.
- **G11-b (2026-07-13) — LoRa UART init self-healing:** if `EspLoraHal::begin()`
  fails after its bounded `LORA_UART_MAX_INIT_ATTEMPTS` (G11), `vTask_LoRa_UKS`
  no longer parks in a permanent empty loop. It retries `begin()` (+
  `configureE22()`) every `LORA_INIT_RETRY_INTERVAL_MS` (30 s, see
  `lora_task_retry_due()` in `lib/LoraLink/UartInitRetry.h`, natively tested
  in `test/test_native_uart_init_retry`) until it succeeds. The watchdog is
  fed throughout the wait (both inside `EspLoraHal::begin()`'s own retry loop
  and the outer 30 s wait). `LoRa_IsTelemetryDisabled()` reports `true` for
  the whole disabled window and flips back to `false` on recovery — the
  vehicle is never affected either way (telemetry loss never triggers FAULT).
  On successful recovery, the `UplinkScheduler` (link FSM, offline buffer,
  replay) and the boot-grace timestamp are constructed **fresh, at the
  recovery moment** — they were already positioned after the init step in
  the task body, so no separate reset logic was needed; this means UKS is
  not falsely assumed "UP" using a stale boot time from before the outage.
  `LoRa_IsLinkDown()`'s cross-task pointer (`s_uplink`) is null-checked before
  dereferencing, so other tasks querying link state during the (re)init
  window get a safe default (`false`, i.e. "not down") rather than crashing.

## Replay-Mode Budget

When the link reconnects (link UP), the offline buffer begins to drain while the live stream continues. To prevent buffer overrun at the 9600 baud UART limit (`~960 bytes per 1000 ms tick`, up from `~480 bytes per 500 ms tick` now that the tick is lengthened to 1000 ms), the replay burst must be strictly limited.

- Live stream: `1 frame/tick` (`~90 bytes`)
- Replay stream: `REPLAY_BURST_PER_TICK = 1` (`~90 bytes`)
- Total TX load: `~180 bytes / tick`

This keeps the TX load well under the `960 bytes / tick` UART-hardware budget (previously `~192 bytes / 200 ms tick` before the tick period was first lengthened to 500 ms as part of the link-flapping fix, then to 1000 ms on 2026-07-20 — the UART-hardware budget only got roomier each time, so this does not reopen the issue described below). Previous configurations using a burst of 3 generated `~360 bytes / tick` at the old 200 ms tick, which caused the 256-byte TX buffer to fill, blocking the UART write. This blocking starved the LoRa RX heartbeat handler, causing phantom link timeouts (re-triggering a link DOWN state). With `REPLAY_BURST_PER_TICK = 1`, the task loop remains unblocked and RX processing (which is evaluated before TX) operates securely.

**Important:** the `~960 bytes / tick` figure above is the **local UART line** budget (ESP32<->E22, 9600 baud) — it does not account for the slower **air-rate** budget. At the current `2.4 kbps` air rate, the same `1 live + 1 replay` load takes `~1000 ms` of airtime, i.e. the entire tick with zero spare margin (see the "Open finding" note in the "Air rate revision (2026-07-20)" block above and `SystemConfig.h` "G10-b"). The two budgets are independent bottlenecks; this one (UART hardware) has roomy margin, the other (air rate) does not.
- All replayed and live packets pass through `TelemetrySanitize::sanitizeForUplink` immediately before `sendStatus` (S4) so UKS's accept ranges are never violated.
- No AKS retransmission / no AKS-level ACK handling.
- AUX gate checked before each TX attempt; if busy, TX for that tick is skipped (packet stays queued, not dropped).
- Sequence counter (`TEL_sequenceCounter`) increments only on actual TX (live or replay) — TUFAN-Monitor's `detect_new_boot` relies on this being strictly monotonic.

Implication:

- Packet loss is observable at UKS by sequence gaps.
- Lost packets (beyond `OB_CAPACITY`) are not resent — oldest buffered packet is dropped first.
- If AUX is busy, AKS retries the same sample next period.

## UKS-side TEL Timeout Margin

This section analyzes the **reverse-direction risk**: could the same kind of
half-duplex channel congestion that previously delayed the UKS→AKS heartbeat
(see "Link flapping fix" note above) also delay AKS→UKS `TEL` frames enough
to trip UKS's own link-down watchdog?

**UKS-side constant:** `TEL_LINK_TIMEOUT_MS = 4000` (`UKS-Telemetry/Core/Inc/telemetry.h`,
recalibrated `2000 -> 4000` on 2026-07-20 alongside the `LORA_TX_PERIOD_MS`
`500 -> 1000` reversion, same commit set). If no valid `TEL` frame arrives
for more than this duration, UKS declares `LINK,DOWN` (symmetric to AKS's
heartbeat-based detection, see `UYUM_NOTU.md` bölüm 2). Nominal `TEL`
cadence is `LORA_TX_PERIOD_MS = 1000` ms, so the nominal margin is
`4000 / 1000 = 4x` — unchanged from before the revision (the header
comment there calls this out explicitly, "4x marj").

### Can a TEL frame actually be skipped?

Per-tick TX (`UplinkScheduler::onTxTickLinkUp`, `src/main.cpp::LoRa_txSend`)
attempts up to `REPLAY_BURST_PER_TICK=1` replay frame **then** 1 live frame,
each gated by `isAuxReady()`. If AUX is busy, `LoRa_txSend` returns `false`
and that attempt is skipped for the tick — **not queued for later within the
same tick**, it simply waits for the next 1000 ms tick. Since the replay and
live checks happen back-to-back (microseconds apart), an AUX-busy tick
effectively skips **both** attempts for that tick, not just one. A skipped
live packet is not buffered as offline data since the link is still UP; the
next tick reads a fresh live sample. (The prior UART-ring-blocking hazard —
`uart_write_bytes` blocking and stalling the whole task loop, including RX/
heartbeat processing — was already closed by the G10 fix: `LoRa_txSend` now
checks `uart_get_tx_buffer_free_size` and **defers** (non-blocking) instead
of blocking when the ring lacks room for a full frame.)

### Worst-case gap vs. TEL_LINK_TIMEOUT_MS

| Consecutive fully-skipped ticks | Gap seen at UKS | vs. 4000 ms | Result |
|---|---|---|---|
| 0 | 1000 ms | — | LINK stays UP |
| 1 | 2000 ms | < 4000 | LINK stays UP |
| 2 | 3000 ms | < 4000 | LINK stays UP |
| 3 | 4000 ms | == 4000 (strict `>` check in `main.c`) | LINK stays UP (boundary, not triggered) |
| 4 | 5000 ms | > 4000 | **False `LINK,DOWN`** |

So the recalibrated configuration still tolerates **3 consecutive
fully-skipped TX ticks** before UKS would falsely declare the link down — the
same tolerance as before the revision, since the `4x` ratio was preserved.
In absolute time this is now 3 s of AUX-busy/deferred TX (was 1.5 s at
`500 ms` ticks); a 4th consecutive miss is still required to trip it.

### Why 3-in-a-row is considered low-probability today (not field-proven)

- **AUX-busy duration is air-time-bounded, not tick-period-bounded — and
  this assumption is tighter after the 2026-07-20 air-rate reversion, at
  the theoretical ceiling.** At the configured `2.4 kbps` air rate, one
  `LORA_TEL_FRAME_MAX_BYTES=120` frame takes ≈500 ms on air (was ≈200 ms at
  the retired `4.8 kbps`); against that ceiling, the busiest tick
  (live+replay during a replay drain) sends at most 2 frames ≈1000 ms —
  the entire `1000 ms` tick, zero slack on paper. **Team-accepted
  (2026-07-20, G10-b) rather than mitigated:** `120 B` is a worst-case
  digit-width ceiling, not the typical packet — a realistic `~65 B` TEL
  frame gives a live+replay burst of `~540 ms`, i.e. `~46%` slack in the
  common case (see "Air rate revision (2026-07-20)" note above). Even at
  the ceiling case, two layers still absorb an occasional saturated tick:
  (1) AUX back-pressure defers rather than drops the next attempt, and (2)
  `TEL_LINK_TIMEOUT_MS=4000` tolerates 3 consecutive fully-skipped ticks
  (table below) before a false `LINK,DOWN`. This does not change the
  worst-case-gap table below (that table is about the `TEL` frame arrival
  gap at UKS, bounded by `LORA_TX_PERIOD_MS` regardless of in-tick airtime
  headroom); it is a known, accepted constraint until the planned
  binary-frame transition shrinks the ceiling case enough to reopen real
  margin there too (see `SystemConfig.h` "G10-b" and "If Link Margin Is
  Poor" below). Field validation (2+ consecutive skipped ticks, replay-
  drain flapping) remains open — see "Recommended Field Checks".
- **Replay mode does not change the TX cadence.** It adds a second frame to
  the *same* 1000 ms tick (still one grid, see "Replay-Mode Budget" above),
  it does not insert extra ticks. The combined peak throughput at the
  **UART-hardware** level (`240 B/s ≤ 768 B/s` budget, `SystemConfig.h`
  `static_assert`) is well inside the UART's capacity — but see the bullet
  above: the **air-rate** level budget is a separate, much tighter
  constraint that this `static_assert` does not cover.
- **The original heartbeat-flapping mechanism doesn't have a direct mirror
  here.** That issue was UKS's *own* module being squeezed for airtime by
  AKS's then-continuous 5 Hz TX. AKS's TEL cadence, by contrast, is gated by
  **AKS's own local `AUX` pin** (its own module's busy/ready state), not by
  whether UKS happens to be transmitting. UKS only sends a single heartbeat
  byte at ~1 Hz (≈1 ms air time) — a brief, infrequent RX event on the AKS
  side — so it is not expected to compound into multi-tick AUX-busy stretches
  on the AKS→UKS direction.

**Residual risk:** this is a static/link-budget argument, not a field
measurement. "Recommended Field Checks" item 3 below (AUX-busy frequency)
should specifically also track whether **2+ consecutive** TX ticks are ever
skipped in practice; if UKS logs a `LINK,DOWN` while AKS's own tick log shows
no >4000 ms gap in its TX attempts, that indicates genuine RF/channel loss
rather than a false positive from local scheduling — worth distinguishing
before assuming the margin is real in the field.

**Constants changed by the 2026-07-20 revision:** `LORA_TX_PERIOD_MS`
(`500 -> 1000`) and `TEL_LINK_TIMEOUT_MS` (`2000 -> 4000`), recalibrated
together in the same commit set to preserve the `4x` margin ratio.
**Constant intentionally left unchanged:** `LINK_TIMEOUT_MS=9000` — this
prompt's scope explicitly excluded it, but the zero-margin air-rate finding
above means it may also warrant a future review; not done here, flagged in
`SystemConfig.h` next to its definition instead. This section's analysis is
protected by a drift-guard invariant (`tools/e2e/test_contract_drift.py`:
`TEL_LINK_TIMEOUT_MS ≥ 3 × LORA_TX_PERIOD_MS`) so that if someone later
raises `LORA_TX_PERIOD_MS` unilaterally without revisiting this margin, the
e2e suite fails instead of silently eroding the 3-tick tolerance computed
above.

## Recommended Field Checks

Before locking Phase 3 complete, validate:

1. ~~Actual E22 air data rate and module configuration in hardware (bench dump vs `E22Regs.h`)~~ — **DONE 2026-07-15**: bench dump matched `E22Regs.h`/`e22_regs.h` targets exactly, see `TEKNIK_KONTROL_PROVASI.md` §4 / `BENCH_E22_TEYIT.md` "Sonuç Kaydı" (P10). **NEEDS RE-VALIDATION** after the 2026-07-20 `REG0` change (`0x63 -> 0x62`) — this bench check predates the revision and has not been re-run against the new target.
2. Whether `1 Hz` (2026-07-20 reversion, was `2 Hz`) remains loss-free in practice.
3. Whether AUX busy events appear frequently under worst-case telemetry load — **now higher priority** given the "Open finding" above (zero in-tick airtime margin at 2.4 kbps during replay drain); watch specifically for **2+ consecutive** skipped TX ticks, not just isolated single skips.
4. Whether UKS parser cleanly handles skipped sequence numbers.
5. Replay drain sırasında (%76+ doluluk, ~10 dk) LINK DOWN→UP flapping'in geri gelmediğini teyit et (2026-07-07 vakasının tekrarı riski) — bkz. `lib/OfflineBuffer/OfflineBuffer.h` "DAVRANIŞ NOTU (2026-07-20)": 1 Hz'de drain hızı = doluş hızı olduğundan, flapping bir linkte buffer net boşalamayabilir; bu senaryoda gerçek link durumunun (UP/DOWN log'ları) beklenen davranışla eşleştiğini saha testinde doğrula.

## If Link Margin Is Poor

Preferred mitigation order:

1. Reduce telemetry field count.
2. Reduce transmit rate below `1 Hz` (current rate as of 2026-07-20).
3. Move from verbose ASCII to compact framed binary payloads.
4. Add selective ACK / retry only if field testing proves it necessary.
