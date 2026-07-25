# TEST TABANI — Beklenen Test Sayıları

> **Amaç: sessiz test kaybını yakalamak.** Bir test paketi derlenmez, link
> olmaz veya bir dosya yanlışlıkla çalıştırıcıya (runner) eklenmezse, toplam
> sayı düşer ama takım yine "PASSED" görünebilir. Bu dosya, her koşumda
> karşılaştırılacak **beklenen** sayıları tutar.
>
> **Son güncelleme:** 25.07.2026 · **Dal:** `okan/aks-kod-duzeltmeleri`
> · **Ölçüm:** aşağıdaki komutlar birebir koşturularak alındı.

---

## NEDEN BU DOSYA VAR — gerçek bir olay

Bu taban ölçülürken `native_roles` ortamındaki **iki paket hiç
çalışmıyordu**: `test_roles_vcu_logic` ve `test_roles_hmi_mappings` link
hatası (`undefined reference: uart_write_bytes`) yüzünden `ERRORED`
veriyordu. Paketler koşmadığı için içlerindeki **4 gerçek hata da
görünmüyordu**.

Bu, `BENI_OKU.md` 1.1'deki "bayrağı açınca `pio test -e native_roles`
koştur, 36 rol testi geçmeli" doğrulamasını anlamsız kılıyordu — komut
"geçti" demiyordu ama kimse sayıya bakmıyordu. Taban sayısı olsaydı fark
hemen görülürdü.

---

## KOMUTLAR

Üç paketin tamamı, `ESP_AKS/` dizininden:

```bash
pio test -e native
pio test -e native_roles
pytest tools/e2e/ -v          # TUFAN_UKS_REPO ortam değişkeni gerekebilir
```

---

## 1 · `pio test -e native` — **515 test / 18 paket**

Varsayılan derleme ortamı (`RELAY_ROLES_ASSIGNED=0`).

| Paket | Test |
|---|---:|
| `test_native_bms_algo` | 41 |
| `test_native_bms_freshness` | 6 |
| `test_native_can_parsing` | 76 |
| `test_native_charge_detect` | 14 |
| `test_native_e22_config` | 18 |
| `test_native_hmi` | 5 |
| `test_native_hmi_helpers` | 60 |
| `test_native_link_monitor` | 11 |
| `test_native_lora_rx_handler` | 7 |
| `test_native_motor_debounce` | 5 |
| `test_native_offline_buffer` | 18 |
| `test_native_ready_motor` | 2 |
| `test_native_relay` | 29 |
| `test_native_sysstate_derive_enabled` | 6 |
| `test_native_telemetry` | 63 |
| `test_native_uart_init_retry` | 8 |
| `test_native_uplink_scheduler` | 6 |
| `test_native_vcu_logic` | 140 |
| **TOPLAM** | **515** |

## 2 · `pio test -e native_roles` — **38 test / 3 paket**

`RELAY_ROLES_ASSIGNED=1` varyantı: S1/S2 mod anahtarlaması, flaşör, fan ve far
mantığı YALNIZ bu ortamda derlenir.

| Paket | Test |
|---|---:|
| `test_roles_hmi_mappings` | 2 |
| `test_roles_relay_mask` | 6 |
| `test_roles_vcu_logic` | 30 |
| **TOPLAM** | **38** |

> **NOT — BENI_OKU.md 1.1 güncellemesi:** orada "36 rol testi geçmeli"
> yazıyor. Doğru sayı artık **38**'dir (link düzeltildikten sonra
> `test_roles_hmi_mappings` de koşuyor). Bayrağı açmadan önce bu tabloya
> bakın, 36'ya değil.

## 3 · `pytest tools/e2e/` — **33 passed + 1 xfailed**

Çapraz-repo sözleşme (drift) testleri. `xfailed` olan **beklenen** bir
işaretçidir (`xfail(strict=True)`): işaretli alan gerçekten desteklenir hale
gelirse test **XPASS** ile takımı bilerek kırar — bu, izleyicinin
güncellenmesi gerektiğinin sinyalidir (bkz. `CLAUDE.md` §5).

| Dosya | Sonuç |
|---|---|
| `test_contract_drift.py` | 24 passed, 1 xfailed |
| `test_frame_contract.py` | 8 passed |
| `test_outage_simulation.py` | 1 passed |
| **TOPLAM** | **33 passed, 1 xfailed** |

---

## SAYI DEĞİŞTİĞİNDE NE YAPILIR

1. **Sayı DÜŞTÜYSE** → önce "hangi paket kayboldu?" diye sorun. Paket
   listesini yukarıdaki tabloyla karşılaştırın. Sık nedenler:
   - Paket link olmuyor (`ERRORED`) — çıktının en altındaki SUMMARY'de
     `PASSED` yerine `ERRORED` arayın.
   - Yeni bir `.cpp` eklendi ama `test_main.cpp` çalıştırıcısına
     `extern void` + `RUN_TEST` satırları eklenmedi.
2. **Sayı ARTTIYSA** ve bu bilinçliyse → **bu dosyayı aynı commit'te
   güncelleyin.** Taban güncellenmezse bir sonraki kişi gerçek bir kaybı
   "zaten fazlaydı" diye geçiştirir.
3. **`xfailed` sayısı değiştiyse** → `tools/e2e/test_contract_drift.py`
   içindeki `xfail` işaretçisine bakın; bir "AÇIK İŞ" kapanmış olabilir.

---

## İLGİLİ DOSYALAR

| Konu | Dosya |
|---|---|
| Kalan açık konular / teknik kontrol listesi | `BENI_OKU.md` |
| Protokol sözleşmesi ve test zorunluluğu | `CLAUDE.md` |
| Kanal-klemens eşlemesi | `RELAY_CHANNEL_TABLE.md` |
