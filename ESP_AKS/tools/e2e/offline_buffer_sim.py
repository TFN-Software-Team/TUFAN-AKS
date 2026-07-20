"""OfflineBuffer.cpp'nin saf-Python FIFO eslenigi.

Kaynak: ESP_AKS lib/OfflineBuffer/OfflineBuffer.cpp (dairesel tampon,
dolu iken push en eskiyi düşürür). Bu, C++ kodun COPYASI degil, ayni
gozlemlenebilir davranisi tasiyan bagimsiz bir Python yeniden-yazimidir;
sozlesme uyumu (kapasite=OB_CAPACITY, FIFO, drop-oldest) test_contract_drift
tarafindan gercek kaynaktaki OB_CAPACITY sabitiyle capraz kontrol edilir.
"""

from __future__ import annotations

from collections import deque


class OfflineBufferSim:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self._items = deque()

    def push(self, item) -> None:
        if len(self._items) == self.capacity:
            self._items.popleft()  # en eskiyi dusur
        self._items.append(item)

    def peek(self):
        return self._items[0] if self._items else None

    def drop_front(self) -> bool:
        if not self._items:
            return False
        self._items.popleft()
        return True

    def count(self) -> int:
        return len(self._items)

    def is_empty(self) -> bool:
        return not self._items


class PrerollBufferSim:
    """PrerollBuffer.h'nin saf-Python eslenigi (R2).

    Link durumundan BAGIMSIZ, surekli sample() cagrilan (period_ms throttle
    ile) dairesel tampon; capacity dolunca en eski dusurulur. pop_oldest ile
    FIFO tuketilir (splice hazirligi) — OfflineBufferSim'e ekleme YAPMAZ,
    cagiran (run_outage_simulation) UplinkScheduler::updateLink'teki splice
    sirasini birebir taklit eder.
    """

    def __init__(self, capacity: int):
        self.capacity = capacity
        self._items: deque = deque()
        self._has_sample = False
        self._last_sample_ms = 0

    def sample(self, now_ms: int, item, period_ms: int) -> bool:
        if self._has_sample and (now_ms - self._last_sample_ms) < period_ms:
            return False
        self._last_sample_ms = now_ms
        self._has_sample = True
        if len(self._items) == self.capacity:
            self._items.popleft()
        self._items.append(item)
        return True

    def pop_oldest(self):
        return self._items.popleft() if self._items else None

    def count(self) -> int:
        return len(self._items)

    def reset(self) -> None:
        self._items.clear()
        self._has_sample = False
        self._last_sample_ms = 0
