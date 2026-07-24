"""AKS vTask_LoRa_UKS ana donguyusunun (src/main.cpp, satir ~505-635) saf
Python, sanal-saatli simulasyonu.

Bu, AKS firmware kodunun kendisini COPYALAMAZ/CALISTIRMAZ (donanimsiz
kisitlamasi) — ayni sozlesme davranisini (2 Hz canli TX, kesintide 1 Hz
seyreltilmis ornekleme, link-up sonrasi tik basina <=REPLAY_BURST_PER_TICK
replay + 1 canli) contract.py sabitleriyle ve OfflineBufferSim ile modelleyip
her "TX edilen" paket icin GERCEK UKS kabul kurallarindan (contract.
parse_uks_frame) ve GERCEK Monitor csv_logger fonksiyonlarindan gecirir.

Gercek zaman beklenmez — nowMs bir Python int sayaci olarak ilerletilir.

AKS-07 look-back tamponlamasi: firmware (UplinkScheduler.cpp recordLookback +
becameDown dali) link UP iken de her ornekleme periyodunda (1 Hz) canli
okumayi 16 slotluk dairesel tampona yazar. Link DOWN'a gecis ANINDA
(LINK_TIMEOUT_MS/1000)+2 = 11 kaydı offline buffer'in BASINA aktarir. Bu
mekanizma, link-down tespitinin LINK_TIMEOUT_MS kadar gecikmesinden kaynaklanan
zaman cizelgesi boşluğunu kapatir.
"""

from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass, field

import contract
from offline_buffer_sim import OfflineBufferSim


@dataclass
class EmittedPacket:
    tick_now_ms: int
    kind: str          # "replay" ya da "live"
    fields: dict        # UKS'in kabul ettigi parse_uks_frame() sekli


@dataclass
class SimResult:
    packets: list = field(default_factory=list)          # EmittedPacket, TX sirasiyla
    buffered_outage_ts: list = field(default_factory=list)  # kesintide buffer'a giren ts_ms'ler
    outage_start_ms: int = 0
    outage_end_ms: int = 0


class _LookbackRing:
    """UplinkScheduler::m_lookbackBuf'in Python eslenigi (16 slotluk dairesel
    tampon). Kaynak: UplinkScheduler.h satir 103-107, .cpp recordLookback()."""
    CAPACITY = 16

    def __init__(self, sample_period_ms: int):
        self._buf = [None] * self.CAPACITY
        self._head = 0
        self._count = 0
        self._last_sample_ms = 0
        self._sample_period_ms = sample_period_ms

    def record(self, now_ms: int, reading: dict) -> None:
        """UplinkScheduler::recordLookback eslenigi."""
        if self._last_sample_ms == 0 or (now_ms - self._last_sample_ms) >= self._sample_period_ms:
            self._last_sample_ms = now_ms
            self._buf[self._head] = dict(reading)  # kopya
            self._head = (self._head + 1) % self.CAPACITY
            if self._count < self.CAPACITY:
                self._count += 1

    @property
    def last_sample_ms(self) -> int:
        return self._last_sample_ms

    def drain(self, n: int) -> list[dict]:
        """Halkadaki son min(n, count) kaydi kronolojik sirada doner
        (UplinkScheduler::updateLink becameDown dalindaki aktarim
        mantigi — .cpp satir 47-64)."""
        items_to_take = min(n, self._count)
        if items_to_take == 0:
            return []
        start_idx = (self._head - items_to_take + self.CAPACITY) % self.CAPACITY
        result = []
        for i in range(items_to_take):
            idx = (start_idx + i) % self.CAPACITY
            result.append(self._buf[idx])
        return result


def _sensor_reading(now_ms: int) -> dict:
    """O anki "canli" sensor okumasi — deterministik, her zaman UKS'in kabul
    aralığında (sanitize gerektirmeyen normal calisma degerleri)."""
    return {
        "rpm": 1200,
        "torque": 100,
        "motor_err": 0,
        "motor_valid": 1,
        "motor_timeout": 0,
        "cell_vmax": 37500,
        "cell_vmin": 37400,
        "temp_h": 28,
        "temp_l": 26,
        "sys_state": 2,        # IDLE
        "pack_v": 780,
        "current": 5000,
        "soc": 8000,
        "bms_valid": 1,
        "ts_ms": now_ms,
        "spd_x10": 250,
    }


def _sanitize(reading: dict) -> dict:
    """TelemetrySanitize::sanitizeForUplink Python eslenigi (bkz.
    contract.sanitize_system_state/sanitize_soc/sanitize_current)."""
    out = dict(reading)
    out["sys_state"] = contract.sanitize_system_state(out["sys_state"])
    out["soc"] = contract.sanitize_soc(out["soc"])
    out["current"] = contract.sanitize_current(out["current"])
    return out


def run_outage_simulation(
    pre_live_ms: int = 1000,
    outage_ms: int = 60000,
    post_live_ms: int | None = None,
) -> SimResult:
    """2 Hz canli -> 60 sn kesinti (1 Hz offline ornekleme) -> link up ->
    tik basina <=REPLAY_BURST_PER_TICK replay + 1 canli TX simulasyonu.

    post_live_ms=None birakilirsa, buffer'in tamamen bosalmasina yetecek sure
    contract sabitlerinden (OFFLINE_SAMPLE_PERIOD_MS, REPLAY_BURST_PER_TICK,
    LORA_TX_PERIOD_MS) turetilir (+%50 marj) — REPLAY_BURST_PER_TICK ileride
    tekrar degisirse sabit bir varsayimin sessizce bayatlamasini onler.

    Look-back tamponlamasi (AKS-07): firmware (UplinkScheduler.cpp) link UP
    iken de her offlineSamplePeriodMs'de canli okumayi 16 slotluk dairesel
    tampona yazar. Link DOWN'a GECIS ANINDA (LINK_TIMEOUT_MS/1000)+2 kaydi
    offline buffer'in BASINA aktarir — boylece link-down tespitindeki
    LINK_TIMEOUT_MS gecikme boslugu zaman cizelgesinde kapatilir.

    Donus: TX SIRASINA GORE tum emitted paketler (her biri gercek
    contract.parse_uks_frame() kabulunden gecmis, UKS'in kabul edecegi
    field dict'i tasir) + kesinti sirasinda buffer'a giren ts listesi.
    """
    if post_live_ms is None:
        max_buffered = outage_ms // contract.OFFLINE_SAMPLE_PERIOD_MS
        ticks_needed = math.ceil(max_buffered / contract.REPLAY_BURST_PER_TICK)
        post_live_ms = int(ticks_needed * contract.LORA_TX_PERIOD_MS * 1.5)  # %50 marj

    result = SimResult()
    buffer = OfflineBufferSim(contract.OB_CAPACITY)

    seq = 0
    tick_dt = contract.LORA_TX_PERIOD_MS

    total_ms = pre_live_ms + outage_ms + post_live_ms
    outage_start_ms = pre_live_ms
    outage_end_ms = pre_live_ms + outage_ms
    result.outage_start_ms = outage_start_ms
    result.outage_end_ms = outage_end_ms

    last_offline_sample_ms = 0
    has_offline_sample = False

    # --- Look-back halka tamponu (firmware eslenigi) ---
    lookback = _LookbackRing(contract.OFFLINE_SAMPLE_PERIOD_MS)
    link_was_down = False  # onceki tik'te link durumu

    # Link-down tespit anini firmware ile ayni sekilde hesapla:
    # link DOWN'a gecis, outage_start_ms + LINK_TIMEOUT_MS aninda olur.
    link_down_detect_ms = outage_start_ms + contract.LINK_TIMEOUT_MS

    now_ms = 0
    while now_ms < total_ms:
        link_down = link_down_detect_ms <= now_ms < outage_end_ms

        # --- Look-back: link UP iken her ornekleme periyodunda tampona yaz ---
        if not link_down:
            reading = _sensor_reading(now_ms)
            lookback.record(now_ms, reading)

        # --- becameDown: link UP -> DOWN gecisi, look-back'i offline buffer'a aktar ---
        if link_down and not link_was_down:
            # Firmware: int lookbackItems = (m_linkTimeoutMs / 1000) + 2;
            lookback_items = (contract.LINK_TIMEOUT_MS // 1000) + 2
            drained = lookback.drain(lookback_items)
            for entry in drained:
                buffer.push(entry)
                result.buffered_outage_ts.append(entry["ts_ms"])
            if drained:
                last_offline_sample_ms = lookback.last_sample_ms
                has_offline_sample = True

        link_was_down = link_down

        if not link_down:
            # --- NORMAL MOD: throttled replay + canli paket (S1) ---
            for _ in range(contract.REPLAY_BURST_PER_TICK):
                buffered = buffer.peek()
                if buffered is None:
                    break
                sent = _sanitize(buffered)
                sent["ver"] = contract.VER
                sent["seq"] = seq
                seq += 1
                result.packets.append(
                    EmittedPacket(tick_now_ms=now_ms, kind="replay", fields=sent)
                )
                buffer.drop_front()

            live = _sanitize(_sensor_reading(now_ms))
            live["ver"] = contract.VER
            live["seq"] = seq
            seq += 1
            result.packets.append(
                EmittedPacket(tick_now_ms=now_ms, kind="live", fields=live)
            )
        else:
            # --- OFFLINE MOD: canli TX yok; 1 Hz seyreltilmis ornekle ---
            reading = _sensor_reading(now_ms)
            if contract_offline_should_sample(
                now_ms, last_offline_sample_ms, has_offline_sample
            ):
                last_offline_sample_ms = now_ms
                has_offline_sample = True
                buffer.push(reading)
                result.buffered_outage_ts.append(reading["ts_ms"])

        now_ms += tick_dt

    return result


def contract_offline_should_sample(now_ms: int, last_sample_ms: int, has_sample: bool) -> bool:
    """OfflineBuffer.h::offline_should_sample'in Python eslenigi."""
    if not has_sample:
        return True
    return (now_ms - last_sample_ms) >= contract.OFFLINE_SAMPLE_PERIOD_MS
