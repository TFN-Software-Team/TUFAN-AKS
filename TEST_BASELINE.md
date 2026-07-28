# TEST TABANI — Beklenen Test Sayıları

> **Amaç: sessiz test kaybını yakalamak.** Bir test paketi derlenmez, link
> olmaz veya bir dosya yanlışlıkla çalıştırıcıya (runner) eklenmezse, toplam
> sayı düşer ama takım yine "PASSED" görünebilir. Bu dosya, her koşumda
> karşılaştırılacak **beklenen** sayıları tutar.
>
> **Son güncelleme:** 28.07.2026 · **Dal:** `okan/aks-kod-duzeltmeleri`
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

## 1 · `pio test -e native` — **555 test / 18 paket**

Varsayılan derleme ortamı (`RELAY_ROLES_ASSIGNED=0`, `MOTOR_DRIVER_PRESENT=0`).

| Paket | Test | Önceki taban (25.07) |
|---|---:|---:|
| `test_native_bms_algo` | 41 | 41 |
| `test_native_bms_freshness` | 6 | 6 |
| `test_native_can_parsing` | 76 | 76 |
| `test_native_charge_detect` | 14 | 14 |
| `test_native_e22_config` | 18 | 18 |
| `test_native_hmi` | 18 | 5 † |
| `test_native_hmi_helpers` | 71 | 60 † |
| `test_native_link_monitor` | 11 | 11 |
| `test_native_lora_rx_handler` | 7 | 7 |
| `test_native_motor_debounce` | 5 | 5 |
| `test_native_offline_buffer` | 18 | 18 |
| `test_native_ready_motor` | 9 | 2 ‡ |
| `test_native_relay` | 29 | 29 |
| `test_native_sysstate_derive_enabled` | 6 | 6 |
| `test_native_telemetry` | 63 | 63 |
| `test_native_uart_init_retry` | 8 | 8 |
| `test_native_uplink_scheduler` | 6 | 6 |
| `test_native_vcu_logic` | 149 | 140 ‡ § ¶ |
| **TOPLAM** | **555** | **515** |

> **† Taban güncel değildi:** `test_native_hmi` (+13) ve `test_native_hmi_helpers`
> (+11) 25.07 ölçümünden bu yana büyümüş ama bu dosya güncellenmemiş. Bu 24 test
> **28.07 motor-gating çalışmasından ÖNCE** eklenmiş; burada yalnızca ölçülüp
> kayda geçirildi (kayıp yok, artış).
>
> **‡ 28.07 motor-gating değişikliği (+10):** motor kaynaklı karar girdileri
> `#if MOTOR_DRIVER_PRESENT` arkasına alındı (bkz.
> `Documents/MOTOR_ENTEGRASYON_NOTU.md` §6). Bayrak=0 davranışı
> `test_native_vcu_logic` içinde (+3, `*_when_flag0` case'leri), bayrak=1
> davranışı `test_native_ready_motor` içinde (+7) kilitlendi. Bayrak 1
> yapıldığında `*_when_flag0` testleri geçersizleşir — o gün gözden geçirin.
>
> **§ 28.07 IDLE-RESET düzeltmesi (+2):** latch'lenmiş actuator fault artık
> `IDLE`'da RESET ile temizlenebiliyor (önceden tek çıkış reboot'tu).
> `test_reset_interlock.cpp` içinde 2 yeni case. Ayrıca
> `test_idle_reset_is_noop` **yeniden adlandırıldı** →
> `test_idle_reset_does_not_change_state` (sayıyı değiştirmez; RESET artık
> IDLE'da tam bir no-op değil). Bkz. `Documents/HMI_Field_Map.md`
> "`RESET` (3) durum başına ne yapar".
>
> **¶ 28.07 güvenli-kapanış SIRASI düzeltmesi (+4):** sıfır-tork ile kontaktör
> açma aynı tick'e düşüyordu (gecikme fiilen 0 ms). `test_stop_request.cpp`'ye
> 3 yeni case eklendi. **+1'i yeni test DEĞİL:**
> `test_fault_latches_contactors_off_once_and_reasserts` zaten yazılmıştı ama
> `test_main.cpp`'ye hiç kaydedilmemişti — bu dosyanın uyardığı **sessiz test
> kaybının canlı bir örneği**. Kaydedilirken içindeki zaman aritmetiği de
> düzeltildi (re-assert sınırı t=1000 değil t=1020). Bkz.
> `Documents/MOTOR_ENTEGRASYON_NOTU.md` §7.

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
