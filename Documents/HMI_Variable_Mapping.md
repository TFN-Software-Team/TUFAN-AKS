# EV Dashboard (Nextion HMI) ↔ Firmware Değişken Eşleştirme Tablosu

Bu tablo, `EV_Dashboard_v01.HMI` dosyasındaki Nextion bileşen (obje) adlarını,
GitHub'daki firmware kodundaki (`ESP_AKS/lib/DisplayHMI/DisplayHMI.cpp` ve
`Documents/CAN_Message_Table.md`) değişkenlerle eşleştirir.

> **Amaç:** Dashboard'daki obje adlarını firmware koduna göre standartlaştırmak.
> Nextion editöründe obje adını değiştirmek için: objeyi seç → **objname** alanını düzenle.
> Firmware bir değere `<objname>.val=...` komutuyla yazar; bu yüzden objname ile koddaki
> ad **birebir** aynı olmalıdır.

---

## 1. Firmware'in Şu An Yazdığı Değişkenler (DisplayHMI.cpp)

`updateScreen(uint16_t HMI_speed, uint8_t HMI_battery)` fonksiyonu yalnızca 2 değer gönderir:

| Firmware komutu | Dashboard objname | Tip | Anlamı | Veri kaynağı (firmware) | Durum |
|-----------------|-------------------|-----|--------|--------------------------|-------|
| `speed.val=%u`  | `speed`           | number | Hız | `MotorStatus.rpm` (CAN 0x200, RPM_H/L) | ✅ İsim eşleşiyor |
| `bat.val=%u`    | `bat`             | number | Batarya % (SOC) | Şimdilik sabit `100` (BMS kapsam dışı) | ✅ İsim eşleşiyor |

Dokunma komutu (firmware → okuma, `readTouchCommand`): `1`=START, `2`=RESET, `3`=EMERGENCY_STOP.
Bunlar dashboard butonlarının `printh` ile gönderdiği bayt değerleridir.

> **Sonuç:** `speed` ve `bat` objeleri zaten firmware ile aynı isimde. Bunlar için
> rename gerekmez — sadece dashboard buton baytlarının 1/2/3 gönderdiğini doğrulayın.

---

## 2. Dashboard'da Var Olan Ama Firmware'in Henüz Yazmadığı Değer Objeleri

Bu objeler dashboard'da mevcut; firmware şu an bunlara veri göndermiyor.
Eklenince koddaki önerilen objname ile yazılmalı.

| Dashboard objname (mevcut) | Tip | Anlamı | Karşılık gelen firmware verisi | Önerilen kod adı |
|----------------------------|-----|--------|--------------------------------|------------------|
| `temp`     | number | Sıcaklık (°C) | Motor over-temp / sıcaklık (CAN 0x200, henüz alan yok) | `temp` (aynı kalsın) |
| `torque`   | number | Tork | Torque feedback (CAN 0x200, TORQUE_FB_H/L) | `torque` (aynı kalsın) |
| `tAkim`    | text | Akım (A) | Faz akımı (CAN'de henüz tanımlı değil) | `tAkim` / `cur` |
| `VOLTAJ` (etiket) | text | DC bus voltajı (V) | DC bus voltage (CAN'de henüz tanımlı değil) | `volt` |

---

## 3. Batarya Gösterge Grubu (görsel/etiket — firmware yazmaz, dahili)

| Dashboard objname | Tip | Anlamı |
|-------------------|-----|--------|
| `tBat`        | text    | "BATARYA" etiketi |
| `tBatLevel`   | text    | Batarya seviye yazısı |
| `tBatPercent` | text    | Batarya yüzde yazısı |
| `pBatActive`  | picture | Batarya aktif ikonu |
| `pBat80_100`  | picture | Batarya doluluk görseli (80–100%) |
| `pSpeedometer`| picture | Hız göstergesi (speedometer) görseli |
| `z0`          | gauge   | Hız ibresi (gauge) |
| `pAmper`      | picture | Amper ikonu |
| `pTermometer` | picture | Termometre ikonu |
| `tIzo` / `pIzoActive` | text/picture | İzolasyon (yalıtım) durumu |
| `va0`, `va1`  | variable | Animasyon/yardımcı sayaç |
| `tm0`         | timer    | Zamanlayıcı |

---

## 4. Durum Çubuğu (System State) Objeleri

Bu durum değişkenleri (`va*State`) firmware'deki `VcuLogic::VcuState` ve
motor hata bayraklarıyla eşleştirilebilir. Şu an firmware bunları yazmıyor.

| Dashboard objname | Tip | Değerler | Anlamı | Olası firmware kaynağı |
|-------------------|-----|----------|--------|------------------------|
| `vaMsState` / `tMS` / `tMSLabel`     | var/text | 0=PASIF, 1=AKTIF | Motor Sürücü (MS) durumu | Motor error flags (CAN 0x200, ERROR_FLAGS) |
| `vaYsState` / `tYS` / `tYSLabel`     | var/text | 0=PASIF, 1=AKTIF | Yalıtım Sistemi (YS) durumu | İzolasyon izleme |
| `vaBmsState` / `tBMS` / `tBMSLabel`  | var/text | 0=PASIF, 1=AKTIF | BMS durumu | CAN 0x300 (şu an **kapsam dışı / ignore**) |
| `vaTeleState` / `tTeleState` / `tTeleLabel` | var/text | 0=YOK, 1=AKTIF, 2=PASIF, 3=HATA | Telemetri durumu | `Telemetry` modülü |
| `vaFarState` / `pFar`                | var/picture | 0–3 | Far (headlight) durumu | RelayManager |
| `vaClockDemo` / `tClock`             | var/text | 0–4 (demo) | Saat | (UI demo) |
| `pTele`                              | picture | — | Telemetri ikonu | — |

---

## Özet

- **Zaten eşleşen (rename gerekmez):** `speed`, `bat`
- **İsmi koda uygun, firmware desteği eklenebilir:** `temp`, `torque`
- **Firmware'de henüz karşılığı yok:** akım, voltaj, durum çubuğu objeleri (`va*State`),
  far, telemetri, saat — bunlar dashboard'da var ama `DisplayHMI.cpp` bunlara veri göndermiyor.

> **Not:** Firmware şu an dashboard'a sadece `speed.val` ve `bat.val` gönderiyor.
> Diğer göstergeleri canlandırmak için `DisplayHMI::updateScreen` genişletilmeli
> (ör. `temp.val`, `torque.val`, `vaMsState.val` vb.). İstenirse bu genişletmeyi yapabilirim.
