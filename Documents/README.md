# Documents/ — Doküman Dizini ve Otorite Haritası

> **Bu dosya bir INDEX'tir, içerik taşımaz.**
>
> 03.08.2026'ya kadar bu dosya kök [`README.md`](../README.md)'nin **bayat bir
> kopyasıydı**: kendi CAN kapsam tablosunu, röle durumunu ve katkıcı listesini
> tekrar ediyordu ve bunların hepsi gerçekle çelişecek kadar eskimişti
> (ör. `0xE000`'i "BMS config", `0xE001`'i "BMS live" diye tanımlıyordu — ikisi
> de yanlış; "10 rölenin tamamı tek kontaktör bankıdır" diyordu — bu, rol
> makroları geldiğinden beri geçerli değil). İki README'yi senkron tutma
> zorunluluğunu ortadan kaldırmak için içerik silindi ve yerine bu dizin
> konuldu.
>
> **Projeye genel bakış için: kök [`README.md`](../README.md).**

---

## Otorite Kuralı — hangi konunun TEK DOĞRULUK KAYNAĞI hangi dosya?

Bir konu birden fazla dosyada geçiyorsa, **yalnız aşağıdaki "otorite" dosyada
tam içerik bulunur**; diğer dosyalar 1-2 cümle özet + bu dosyaya link verir,
tabloyu KOPYALAMAZ. (Kural: [`CLAUDE.md`](../CLAUDE.md) §5 — kod davranışı
değişince ilgili `.md` aynı commit'te güncellenir. Tek otorite, tek güncelleme
noktası demektir.)

| Konu | OTORİTE dosya |
| --- | --- |
| CAN frame'leri, byte-byte alan anlamları, doğrulama seviyeleri | [CAN_Message_Table.md](CAN_Message_Table.md) |
| Röle kanal ↔ klemens ↔ yük haritası, bank maskeleri, polarite | [RELAY_CHANNEL_TABLE.md](RELAY_CHANNEL_TABLE.md) |
| Batarya eşikleri (pack vs hücre), hangi eşik neyi tetikler | [Threshold_Ownership.md](Threshold_Ownership.md) |
| Ekran (Nextion) alan sözleşmesi, buton komutları, `.pic` eşlemeleri | [HMI_Field_Map.md](HMI_Field_Map.md) |
| Beklenen test sayıları (sessiz test kaybı bekçisi) | [`../TEST_BASELINE.md`](../TEST_BASELINE.md) |
| AKS↔UKS LoRa protokolü, 19 alanlı `TEL,...` sözleşmesi | [UKS_LoRa_Protocol.md](UKS_LoRa_Protocol.md) |
| Katkı kuralları: dal adlandırma, önek şeması, kod yerleşimi (M5) | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Çalışma kuralları, çapraz-repo sözleşmesi, test zorunluluğu | [`../CLAUDE.md`](../CLAUDE.md) |

---

## Dosyalar

### Aktif — sahada/bench'te kullanılan

| Dosya | Konu |
| --- | --- |
| [BRING_UP_CHECKLIST.md](BRING_UP_CHECKLIST.md) | Kart bring-up: pin bağlantıları, boot logu, sorun giderme, **ekran butonu doğrulaması**, HV testleri. Bring-up için tek checklist. |
| [ACCEPTANCE_TEST.md](ACCEPTANCE_TEST.md) | BMS + Nextion uçtan uca kabul testi prosedürü |
| [CAN_Message_Table.md](CAN_Message_Table.md) | **OTORİTE** — tüm CAN frame'leri, alan alan kanıt/doğrulama durumu |
| [RELAY_CHANNEL_TABLE.md](RELAY_CHANNEL_TABLE.md) | **OTORİTE** — 10 röle kanalı, bank üyelikleri, Faz 1/Faz 2 doğrulama |
| [Threshold_Ownership.md](Threshold_Ownership.md) | **OTORİTE** — `SystemConfig.h` (pack, güvenlik) vs `BmsAlgo.h` (hücre, gösterim) eşikleri |
| [HMI_Field_Map.md](HMI_Field_Map.md) | **OTORİTE** — firmware → Nextion alan eşlemesi, buton komut sözleşmesi |
| [UKS_LoRa_Protocol.md](UKS_LoRa_Protocol.md) | **OTORİTE** — AKS↔UKS telemetri/komut sözleşmesi |
| [Test_Guide.md](Test_Guide.md) | Test altyapısı, testleri çalıştırma, yeni test ekleme rehberi |
| [CONTRIBUTING.md](CONTRIBUTING.md) | **OTORİTE** — dal adlandırma, değişken önekleri, kod yerleşimi (M5) |
| [PlatformIO_init.md](PlatformIO_init.md) | Yeni başlayanlar için PlatformIO kurulum/derleme rehberi |
| [LoRa_Link_Analysis.md](LoRa_Link_Analysis.md) | Bant genişliği bütçesi, link flapping düzeltmesi, TX periyodu gerekçesi |
| [E22_CRYPT_SENKRON.md](E22_CRYPT_SENKRON.md) | G7 heartbeat-injection açığı ve CRYPT anahtarı çapraz-repo senkronu |
| [E22_ZORLA_YAZMA_CHECKLIST.md](E22_ZORLA_YAZMA_CHECKLIST.md) | Yanlış CRYPT ile flash'lanmış eski E22 modüllerini kurtarma |
| [NEXTION_EKRAN_YAPILACAKLAR.md](NEXTION_EKRAN_YAPILACAKLAR.md) | Nextion Editor'de **elle** yapılacak ekran işleri (`.HMI` ikili format) |

### Açık iş / karar bekleyen

| Dosya | Durum |
| --- | --- |
| [MOTOR_ENTEGRASYON_NOTU.md](MOTOR_ENTEGRASYON_NOTU.md) | ⏳ Motor sürücüsü henüz araçta yok (`MOTOR_DRIVER_PRESENT=0`); entegrasyon günü yapılacaklar listesi |
| [VEHICLE_PARAMS_TEYIT.md](VEHICLE_PARAMS_TEYIT.md) | Madde 1–4 ✅ tamamlandı; **yalnız madde 5 (saha hız provası) açık** |
| [TORQUE_ALAN_KARAR_NOTU.md](TORQUE_ALAN_KARAR_NOTU.md) | ✅ Ara karar verildi (2026-07-13): alan mevcut haliyle kalıyor, kalıcı çözüm motor entegrasyonuna ertelendi |

### Tarihsel kayıt — çözülmüş, aksiyon gerektirmez

| Dosya | Sonuç |
| --- | --- |
| [CELL_VOLTAGE_INVESTIGATION.md](CELL_VOLTAGE_INVESTIGATION.md) | ✅ ÇÖZÜLDÜ — 24 hücre voltajı `0xE015–0xE020`'de bulundu ve entegre edildi |
| [BENCH_E22_TEYIT.md](BENCH_E22_TEYIT.md) | ✅ DOĞRULANDI (2026-07-15 bench dump) — prosedür yeni modül provizyonu için korunuyor |

> Ayrıca `ESP_AKS/UYUM_NOTU.md` **E32 dönemine ait tarihsel bir kayıttır**,
> güncel E22 değerleri için kullanılmamalıdır.
