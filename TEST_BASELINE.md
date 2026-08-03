# TEST TABANI — Beklenen Test Sayıları

> **Amaç: sessiz test kaybını yakalamak.** Bir test paketi derlenmez, link
> olmaz veya bir dosya yanlışlıkla çalıştırıcıya (runner) eklenmezse, toplam
> sayı düşer ama takım yine "PASSED" görünebilir. Bu dosya, her koşumda
> karşılaştırılacak **beklenen** sayıları tutar.
>
> **Son güncelleme:** 03.08.2026 · **Dal:** `main`
> · **Ölçüm:** `native` / `native_roles` sayıları bu güncellemede
> `test/` altındaki **`RUN_TEST` çağrıları statik olarak sayılarak** alındı
> (PlatformIO o makinede kurulu değildi). Tüm `RUN_TEST` satırlarının
> koşulsuz olduğu (yorum/`#if` arkasında olmadığı) tek tek doğrulandı, ama
> **bu bir koşum çıktısı değildir** — bir sonraki gerçek `pio test`
> koşumunda sayılar birebir teyit edilmeli, sapma varsa bu satır
> "koşturularak alındı" olarak güncellenmelidir. `tools/e2e` sayıları
> `def test_` sayımıyla doğrulandı ve DEĞİŞMEDİ.

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

## 1 · `pio test -e native` — **570 test / 18 paket**

Varsayılan derleme ortamı (`RELAY_ROLES_ASSIGNED=0`, `MOTOR_DRIVER_PRESENT=0`).

| Paket | Test | Önceki taban (28.07) |
|---|---:|---:|
| `test_native_bms_algo` | 41 | 41 |
| `test_native_bms_freshness` | 6 | 6 |
| `test_native_can_parsing` | 77 | 76 ◆ |
| `test_native_charge_detect` | 19 | 14 ◆ |
| `test_native_e22_config` | 18 | 18 |
| `test_native_hmi` | 18 | 18 |
| `test_native_hmi_helpers` | 79 | 71 ◆ |
| `test_native_link_monitor` | 11 | 11 |
| `test_native_lora_rx_handler` | 7 | 7 |
| `test_native_motor_debounce` | 5 | 5 |
| `test_native_offline_buffer` | 18 | 18 |
| `test_native_ready_motor` | 9 | 9 |
| `test_native_relay` | 29 | 29 |
| `test_native_sysstate_derive_enabled` | 6 | 6 |
| `test_native_telemetry` | 63 | 63 |
| `test_native_uart_init_retry` | 8 | 8 |
| `test_native_uplink_scheduler` | 6 | 6 |
| `test_native_vcu_logic` | 150 | 149 ◆ |
| **TOPLAM** | **570** | **555** |

> **◆ 03.08.2026 yeniden sayımı (+15, hepsi ARTIŞ — kayıp YOK):** taban
> 28.07'den bu yana güncellenmemişti. Dört pakette artış ölçüldü:
> `test_native_charge_detect` (+5), `test_native_hmi_helpers` (+8),
> `test_native_can_parsing` (+1), `test_native_vcu_logic` (+1). Paket
> SAYISI değişmedi (18) — yani bir paket düşmüş değil, mevcut paketlere
> test eklenmiş. Artışların hangi commit'lerden geldiği bu güncellemede
> commit bazında ayrıştırılmadı; sayılar bugünkü çalışma ağacından
> okundu.

<details>
<summary><b>Tarihsel — 25.07 → 28.07 arası değişimin gerekçeleri († ‡ § ¶)</b></summary>

Aşağıdaki dipnotlar, tablodan kaldırılan **25.07 sütununa** aittir ve
tarihsel kayıt olarak korunmuştur; bugünkü tabloda † ‡ § ¶ işaretleri
ARTIK YOKTUR.

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

</details>

## 2 · `pio test -e native_roles` — **45 test / 3 paket**

`RELAY_ROLES_ASSIGNED=1` varyantı: S1/S2 mod anahtarlaması, flaşör, fan ve far
mantığı YALNIZ bu ortamda derlenir.

| Paket | Test | Önceki taban (28.07) |
|---|---:|---:|
| `test_roles_hmi_mappings` | 3 | 2 ◆ |
| `test_roles_relay_mask` | 10 | 6 ◆ |
| `test_roles_vcu_logic` | 32 | 30 ◆ |
| **TOPLAM** | **45** | **38** |

> **◆ 03.08.2026 yeniden sayımı (+7, hepsi ARTIŞ):** üç paketin üçünde de
> test eklenmiş. `test_roles_relay_mask` artışının (+4) bir kısmı fan
> kanalının NC klemense alınmasıyla gelen polarite/invert-maske
> testleridir (`test_invert_mask_contract`,
> `test_fan_pin_polarity_follows_invert_mask` vb. — bkz. commit `865f8f6`).
> Bu ortamda `RUN_TEST` çağrılarının hiçbiri `#if` arkasında değildir;
> dosyadaki `#if RELAY_CH_FAN_NC_WIRED` blokları test GÖVDELERİ içindeki
> beklenti varyantlarıdır, çağrı sayısını değiştirmez.

> **NOT — "36 rol testi" diyen eski metin:** Bu sayıya atıf yapan
> `BENI_OKU.md` **bu repoda mevcut değildir** (git geçmişinde de hiç
> oluşturulmamış — bkz. aşağıdaki "İLGİLİ DOSYALAR" notu). Rol testi sayısı
> için tek doğruluk kaynağı yukarıdaki tablodur: **45**. Bayrağı açmadan
> önce 36'ya veya 38'e değil, buraya bakın.

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
| Kalan açık konular / teknik kontrol listesi | `ESP_AKS/TEKNIK_KONTROL_PROVASI.md` |
| Protokol sözleşmesi ve test zorunluluğu | `CLAUDE.md` |
| Kanal-klemens eşlemesi | `Documents/RELAY_CHANNEL_TABLE.md` |
| Test altyapısı / yeni test ekleme | `Documents/Test_Guide.md` |

> ⚠️ **`BENI_OKU.md` bu repoda YOKTUR.** Bu dosya birkaç dokümanda
> (eskiden burada da) kaynak olarak gösteriliyordu, ancak
> `git log --all -- "*BENI_OKU*"` **hiçbir kayıt döndürmüyor** — dosya hiç
> oluşturulmamış. Ona yapılan atıflar 03.08.2026'da gerçek muadilleriyle
> değiştirildi; yeni dokümanlarda `BENI_OKU.md`'ye atıf YAPMAYIN.
