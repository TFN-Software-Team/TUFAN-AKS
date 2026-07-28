# Nextion Ekran Projesi — Yapılacaklar (Elle, Nextion Editor'de)

Bu doküman, `EV_Dashboard_v01__14_.HMI` ekran projesinde **elle** yapılması
gereken işleri adım adım listeler.

> **`.HMI` dosyası tescilli ikili formattır.** Programatik olarak
> düzenlenemez — buradaki her adım Nextion Editor'de insan eliyle yapılır,
> ardından proje yeniden derlenip (`Compile`) ekrana yüklenir (`Upload` veya
> SD kart). Bu doküman bittiğinde firmware tarafında **hiçbir** değişiklik
> gerekmez; aşağıdaki işler tamamlanana kadar ilgili alanlar ekranda
> görünmez (komutlar `bkcmd=0` altında sessizce yutulur).

İlgili sözleşme: [HMI_Field_Map.md](HMI_Field_Map.md).

---

## Özet — Neden Bu Liste Var

Firmware ile ekran projesi arasında dört uyumsuzluk tespit edildi:

| # | Sorun | Sonuç |
| --- | --- | --- |
| 0 | **Butonların gönderdiği komut baytları dokümanda YANLIŞTI** (`2/3/4` sırası kodla ters) | **Eski tabloya göre çizilen E-STOP butonu firmware'e RESET gönderir.** Ayrıca `STOP` (kontrollü durdurma, komut `6`) hiç belgelenmemişti → ekranda DUR butonu yok |
| 1 | Firmware `contactor.txt` ve `warn.val` gönderiyor, ekranda bu isimde obje **yok** | Komutlar sessizce yutuluyor — kontaktör durumu ve BMS uyarı seviyesi ekranda **hiç** görünmüyor |
| 2 | Ekranda `chg`/`chgtxt`/`tm0` var ama `chg.val`'ı yazan kod **yoktu** | Alan kalıcı 0'da → ekran her koşulda "Bosta" yazıyordu. **Firmware tarafı bu iş kapsamında yapıldı**; ekran tarafında yalnız §4 kaldı |
| 3 | `far` objesi yok (`pFar` var, ismi tutmuyor) ve touch event'inde **yerel durum** tutuyor | Nextion brown-out'undan sonra far ikonu gerçek durumdan sapıyor |

---

## 0. Butonlar — komut çerçeveleri (ÖNCE BUNU YAPIN)

**Sayfa:** `pageMain`

Bu bölüm listenin **en kritik** maddesidir: yanlış bayt = yanlış eylem.
Firmware komutları 3 baytlık `0x5A <CMD> <~CMD>` çerçevesi olarak bekler
(header, komut, bit-tersi checksum). Tam sözleşme:
[HMI_Field_Map.md](HMI_Field_Map.md) → "Command Inputs".

### 0.1 — Beş buton, objname ve tam kod

Her buton için **Touch Release Event** sekmesine (Touch Press **değil**) tek
satır yazılır:

| Buton | `objname` | Touch Release Event kodu | Komut |
| --- | --- | --- | --- |
| START | `bStart` | `printh 5A 01 FE` | `1` = `HMI_CMD_START` |
| DRIVE (sürüşe izin) | `bDrive` | `printh 5A 02 FD` | `2` = `HMI_CMD_DRIVE_ENABLE` |
| RESET | `bReset` | `printh 5A 03 FC` | `3` = `HMI_CMD_RESET` |
| **E-STOP (acil)** | `bEstop` | `printh 5A 04 FB` | `4` = `HMI_CMD_EMERGENCY_STOP` |
| STOP / DUR (kontrollü) | `bStop` | `printh 5A 06 F9` | `6` = `HMI_CMD_STOP` |

> **`objname`'ler firmware için serbesttir.** Firmware butonların adını
> **okumaz** — sözleşme yalnız yukarıdaki bayt çerçevesidir (§0.3'e bkz.).
> Yukarıdaki isimler ekran projesinin kendi düzeni için önerilir; farklı bir
> isimlendirme kullanıyorsanız **kodu değil, yalnız bu tabloyu** güncelleyin.
> Öte yandan `contactor` / `warn` / `far` / `chg` gibi **gösterge** objelerinin
> isimleri sözleşmedir ve harfi harfine tutmalıdır.

### 0.2 — Adımlar

1. Nextion Editor'de `pageMain` sayfasını aç.
2. Her buton için Toolbox → **Button** bileşenini sayfaya sürükle.
3. Attribute panelinde `objname` alanını tablodaki değere ayarla.
4. **Touch Release Event** sekmesini aç, tablodaki `printh` satırını **tek satır
   olarak** yaz. Argümanlar hex ve boşlukla ayrılmış olmalı (`printh 5A 04 FB`),
   `0x` öneki **yazılmaz**.
5. **Touch Press Event** sekmesini **boş bırak** (çift komut göndermeyin).
6. **Send Component ID** kutusunun **işaretsiz** olduğunu doğrula (§0.4).
7. E-STOP butonunu görsel olarak ayır: kırmızı zemin, en büyük dokunma alanı,
   diğer butonlardan uzağa yerleştir.

### 0.3 — Checksum nasıl hesaplanır (yeni komut eklerseniz)

Üçüncü bayt komutun **bit tersidir** (`~CMD`), yani `0xFF - CMD`:

```
CMD=0x01 → ~CMD=0xFE      CMD=0x04 → ~CMD=0xFB
CMD=0x02 → ~CMD=0xFD      CMD=0x05 → ~CMD=0xFA
CMD=0x03 → ~CMD=0xFC      CMD=0x06 → ~CMD=0xF9
```

Checksum tutmazsa firmware çerçeveyi **sessizce atar** — buton hiçbir şey
yapmamış gibi görünür. Ekranda hata mesajı çıkmaz.

### 0.4 — ⚠️ "Send Component ID" İŞARETLENMEMELİ

Butonun attribute panelindeki **Send Component ID** kutusu (Touch Press /
Touch Release) **işaretsiz** olmalıdır. İşaretliyse ekran, `printh` çıktısına
**ek olarak** 7 baytlık bir `0x65 <page> <cid> <event> 0xFF 0xFF 0xFF`
çerçevesi yollar.

Firmware bu çerçeveyi kullanmaz (`HMI_parseTouchByte` yalnız `0x5A` ile
başlayan çerçeveleri çözer; `0x00`/`0xFF` baytları parser durumunu resetler,
dolayısıyla **yanlış komut üretemez**) — ama her dokunuşta **gereksiz RX
trafiği** doğurur ve ekran-canlı zaman damgasını gerçek komut trafiği olmadan
besler. Tek kaynak `printh` olsun.

### 0.5 — ⚠️ STOP (`6`) ile E-STOP (`4`) AYRI butonlardır

Bu ikisi birbirinin yerine geçmez:

- **E-STOP (`printh 5A 04 FB`)** — acil. Her durumda çalışır, kontaktörleri
  ANINDA açar, olay kuyruğunu bypass eder, `EMERGENCY_STOP` durumuna geçer.
  Çıkmak için RESET interlock'u gerekir (arıza kaydı düşer).
- **STOP / DUR (`printh 5A 06 F9`)** — normal. Yalnız `READY` ve `DRIVE`'da
  anlamlıdır, güvenli kapanış sırasını izler (önce sıfır tork, `20 ms` sonra
  kontaktör), `IDLE`'a döner, **arıza kaydı bırakmaz**. Bu yüzden durum
  göstergesinin `IDLE`'a düşmesi bir VCU tick'i (~20 ms) gecikebilir — normaldir
  (bkz. `Documents/MOTOR_ENTEGRASYON_NOTU.md` §7).

DUR butonu olmadan sürücünün normal durmak için tek yolu E-STOP'a basmaktır —
aşırı tepki, gereksiz arıza kaydı ve RESET zorunluluğu demektir.

> Ekrandaki E-STOP butonu **fiziksel acil durdurma butonunun yerini tutmaz**;
> o ayrı bir donanım yoludur.

### 0.6 — Far butonu VARSA KALDIRIN, yenisini EKLEMEYİN

**Karar (28.07.2026):** farın resmî kontrol yolu **fiziksel düğmedir**
(`HEADLIGHT_SWITCH_PIN`, şartname B2 9.19.c); **ekran farı yalnız GÖSTERİR**.

Firmware tarafında `case 5` dalı **silindi** — `0x5A 05 FA` çerçevesi artık
`default` dalına düşüyor, yalnız `"Ignored/Unknown HMI command received: 5"`
WARN'ı basıyor ve **far rölesine dokunmuyor**.

Ekran tarafında yapılacak:

1. `pageMain` (ve diğer sayfalar) taranıp far/ışık amaçlı **buton varsa
   SİLİNİR**.
2. Herhangi bir event içinde `printh 5A 05 FA` satırı varsa **silinir**
   (Editor'de `printh 5A 05` diye arayın).
3. Yeni far butonu **eklenmez**.

> **Neden silmek gerekiyor:** buton kalırsa basıldığında hiçbir şey olmaz ama
> ekranda duruyor olması sürücüye "far ekrandan kontrol edilebiliyor"
> izlenimi verir — **ölü kontrol**. Yarış sırasında farı açmaya çalışan
> sürücünün ekrana basıp beklemesi, fiziksel düğmeye gitmemesi demektir.

`far` ekranda yalnız **gösterge** (Picture) olarak durur ve touch event'i boş
kalır (§3).

---

## 1. `contactor` — Text bileşeni (YENİ)

**Sayfa:** `pageMain`

| Özellik | Değer |
| --- | --- |
| Tip | **Text** |
| `objname` | `contactor` — **tam olarak böyle, büyük/küçük harf duyarlı** |
| `txt_maxl` | **≥ 8** (firmware `"CLOSED"` / `"OPEN"` gönderir; en uzunu 6 karakter, marj bırakıldı) |
| `txt` (varsayılan) | `--` |
| Touch event (Press/Release) | **BOŞ BIRAKILACAK** |

### Adımlar

1. Nextion Editor'de `pageMain` sayfasını aç.
2. Toolbox → **Text** bileşenini sayfaya sürükle.
3. Attribute panelinde `objname` alanını `contactor` yap.
4. `txt_maxl` değerini `8` (veya üstü) yap.
5. `txt` varsayılanını `--` yap.
6. **Touch Press Event / Touch Release Event sekmelerini boş bırak.**

### Neden varsayılan `--`, `"OPEN"` değil?

Boot anında ve Nextion brown-out'undan sonra kontaktörün gerçek durumu
**bilinmiyor**. `"OPEN"` yazmak "kontaktörler açık" diye bir bilgi **iddia
etmektir** — o an doğru olmayabilir. `"CLOSED"` yazmak ise açıkça tehlikeli.
`--` "veri yok" demektir ve dürüst olan tek varsayılandır. Firmware ilk
telemetri döngüsünde (≤ 100 ms) gerçek değeri basar; round-robin resync de
en geç 6.5 sn içinde onarır.

### Anlamı (operatör için)

- `CLOSED` = maskedeki **TÜM** kanallar kapalı.
- `OPEN` = kanallardan **en az biri** açık (biri bile açıksa `OPEN`).

> **Bu bir KOMUT durumudur, geri besleme DEĞİLDİR.** `RelayManager`'ın
> bildiği şey "ben bu kanala kapan komutu verdim"dir. Kontaktörün yardımcı
> kontağından gelen **bağımsız bir geri besleme okunmuyor** — yani kontaktör
> fiziksel olarak yapışmış veya açılmamış olsa da ekranda `CLOSED` yazar.
> Fiziksel doğrulama gerekiyorsa yardımcı kontak girişi ayrı bir iş kalemidir.

---

## 2. `warn` — Number bileşeni (YENİ)

**Sayfa:** BMS paneli (`pageBms`) — `cellmax` / `cellmin` ile **aynı grup**.

| Özellik | Değer |
| --- | --- |
| Tip | **Number** |
| `objname` | `warn` — **tam olarak böyle** |
| `val` (varsayılan) | **`3`** (NO_DATA) |

### Adımlar

1. `pageBms` sayfasını aç.
2. Toolbox → **Number** bileşenini `cellmax`/`cellmin` grubunun yanına sürükle.
3. `objname` = `warn`.
4. `val` varsayılanını **`3`** yap (0 DEĞİL — aşağıya bakınız).
5. Görsel gösterim için: ya `warn`'a bağlı bir Text bileşeni ekle, ya da
   mevcut bir timer içinde aşağıdaki eşlemeyi kur.

### Eşleme tablosu

| `warn.val` | Gösterim | Renk | Anlamı |
| --- | --- | --- | --- |
| `0` | `OK` | Yeşil | Nominal |
| `1` | `UYARI` | Sarı | Eşiğe yaklaşıldı |
| `2` | `KRİTİK` | Kırmızı | Kritik eşik **VEYA** BMS verisi yok/bayat |
| `3` | `--` | Nötr gri | Hücre verisi henüz tam değil — uyarı **hesaplanamıyor** |

### ⚠️ `3` "kritiğin bir üstü" DEĞİLDİR

`3` = **VERİ YOK**. Kırmızı **yakılmamalıdır**, alarm **çalmamalıdır**. Nötr
gri/soluk gösterim doğrudur. `BmsComputed.h` bunu zaten şart koşuyor: 24
hücrenin tamamı okunmadan uyarı seviyesi hesaplanamaz ve o durumda uydurma
bir seviye üretmek yerine "bilmiyorum" denir.

Varsayılanın `3` olması bu yüzden kritik: `0` olsaydı boot ile ilk BMS
telemetrisi arasında ekran **"OK" yalanı** söylerdi.

### ⚠️ `2` İKİ farklı durumu temsil ediyor (bilinen sınırlama)

`2` hem **gerçek kritik eşik aşımını** hem de `isValid=false` →
`makeSafeInvalid()` durumunu (BMS verisi geçersiz) gösterir. Operatör bu
ikisini `warn` alanına bakarak **ayırt edemez** — ayrım için `valid` alanına
(`VALID` / `INVALID` / `TIMEOUT`) da bakmak gerekir:

- `warn=2` **ve** `valid=VALID` → gerçek kritik eşik aşımı.
- `warn=2` **ve** `valid=INVALID`/`TIMEOUT` → veri geçersiz, güvenli tarafa
  düşülmüş.

Bu belirsizliğin ileride ayrı bir seviyeyle çözülmesi önerilir (bkz.
[HMI_Field_Map.md](HMI_Field_Map.md) "Bilinen Sınırlamalar").

---

## 3. `far` — Picture bileşeni (İSİM VE EVENT DÜZELTMESİ)

**Mevcut durum:** Ekranda `pFar` adlı bir bileşen var. **İki sorun:**

1. **İsim tutmuyor.** Firmware `far.pic=<ID>` gönderiyor; `pFar` bu komutu
   almıyor.
2. **Touch event'inde `vaFarState.val++` ile YEREL far durumu tutuyor.**

İkinci sorun sözleşmenin doğrudan ihlalidir. [HMI_Field_Map.md](HMI_Field_Map.md)
"`far` (Picture) — headlight status indicator contract" bölümü şunu söylüyor:

> **Ekran farı KONTROL ETMEZ ve HİÇBİR yerel far durumu TUTMAZ.** Durumun tek
> sahibi ESP'dir (`VcuLogic::isHeadlightOn`, fiziksel düğmeden sürülür).

Far artık **fiziksel bir düğmeyle** kontrol ediliyor (`HEADLIGHT_SWITCH_PIN`,
şartname B2 9.19.c). Ekran yerel durum tutarsa, Nextion brown-out reset'inden
sonra (bu sistemde **gözlenmiş** bir olaydır) ikon gerçek far durumundan
sapar ve kalıcı olarak yanlış gösterir.

### Yapılacak

| Özellik | Değer |
| --- | --- |
| Tip | **Picture** |
| `objname` | `far` (mevcut `pFar` **yeniden adlandırılacak**) |
| Touch Press/Release Event | **TAMAMEN BOŞALTILACAK** (`vaFarState.val++` satırı **silinecek**) |

### Adımlar

1. `pFar` bileşenini seç, `objname` alanını `far` yap.
2. Touch Press Event sekmesini aç, **içindeki tüm kodu sil** (özellikle
   `vaFarState.val++`).
3. Touch Release Event sekmesini de kontrol et, boşalt.
4. `vaFarState` değişkeni başka hiçbir yerde kullanılmıyorsa **silinebilir**
   (önce Editor'de arama yapın).
5. Resource ID'leri not edin: far AÇIK ve far KAPALI resimlerinin ID'leri.

> **Firmware tarafına geri bildirim gerekiyor:** `SystemConfig.h` içindeki
> `HMI_PIC_HEADLIGHT_ON` / `HMI_PIC_HEADLIGHT_OFF` şu an **placeholder**
> (`1` / `0`) ve **CONFIG** olarak işaretli. Ekran projesindeki gerçek
> resource ID'leri bu iki makroya yazılmalıdır. ID'ler eşleşmezse yanlış
> resim gösterilir.

---

## 4. `chg` — Varsayılan değer + `tm0` timer'ına 3. durum

**Mevcut durum:** `chg` (gizli Number) ve `chgtxt` (Text) objeleri **zaten
var**; `tm0` timer'ı `chg.val`'ı metne çeviriyor. Eksik olan iki şey:

1. `chg.val`'ı yazan kod yoktu → **firmware tarafı bu iş kapsamında yapıldı**
   (`hmi_chargeState`, `lib/HMIHelpers/ChargeState.h`). Ekranda değişiklik
   gerekmiyor.
2. `tm0` **3. durumu (NO_DATA) tanımıyor** ve `chg` varsayılanı `0`.

### 4.1 — `chg` varsayılan `val` değerini `3` yap

**Şu an `0`** (= "Bosta"). `3` yapılmalı.

Sebep §2'dekiyle aynı: boot ile ilk telemetri arasında ekran **"Bosta"
yalanı** söyler. Araç o anda gerçekten şarjda olabilir. `3` = "henüz
bilmiyorum".

### 4.2 — `tm0` timer'ına `chg.val==3` dalını ekle

Mevcut blok 0/1/2'yi tanıyor. Şu satırlar **eklenmeli**:

```
if(chg.val==3)
{
  chgtxt.txt="--"
}
```

Eklendikten sonra tam eşleme:

| `chg.val` | `chgtxt.txt` | Anlamı |
| --- | --- | --- |
| `0` | `Bosta` | Akım ölü bant içinde, şarj yok |
| `1` | `Sarj Oluyor` | `TEL_chargerActive` |
| `2` | `Desarj` | Akım ≤ −1.0 A |
| `3` | `--` | **BMS verisi yok/bayat — durum BİLİNMİYOR** |

> Bu sayısal sözleşme `lib/HMIHelpers/ChargeState.h` içindeki enum ile
> **birebir** aynı olmalıdır; native test
> (`test_chg_enum_values_match_nextion_contract`) firmware tarafını kilitler,
> ekran tarafını **kilitleyemez** — bu tabloyu değiştirirseniz header'ı da
> değiştirin.

---

## 5. `motorErr` — Varsayılan metin düzeltmesi

**Mevcut varsayılan:** `"Herhangi bir hata bulunamadi..."`

Bu, reset anında bir an için **"hata yok" yalanı** gösteriyor — o anda hata
durumu bilinmiyor.

**Yapılacak:** varsayılan `txt` değerini `--` yap.

> **NOT:** Bu iş kapsamında `motorErr` gönderimi firmware tarafında **devre
> dışı bırakıldı** (`MOTOR_DRIVER_PRESENT=0`; `0x200` frame'ini hall-effect
> hız sensörü üretiyor ve `data[7]=0x00` gönderiyor, yani alan yapısal olarak
> hep `0x00` basıyordu). Alan artık **beslenmeyecek**, dolayısıyla varsayılan
> ne ise ekranda o kalacak — bu yüzden varsayılanın dürüst (`--`) olması
> önemli. Bileşeni **silmeyin**; motor sürücüsü entegre edildiğinde geri
> açılacak.

---

## Kontrol Listesi

Nextion Editor'de — **butonlar (§0)**:

- [ ] `pageMain` → START butonu (`bStart`), Touch Release = `printh 5A 01 FE`
- [ ] `pageMain` → DRIVE butonu (`bDrive`), Touch Release = `printh 5A 02 FD`
- [ ] `pageMain` → RESET butonu (`bReset`), Touch Release = `printh 5A 03 FC`
- [ ] `pageMain` → **E-STOP** butonu (`bEstop`), Touch Release = `printh 5A 04 FB`
- [ ] `pageMain` → STOP/DUR butonu (`bStop`), Touch Release = `printh 5A 06 F9`
- [ ] Beş butonun da **Touch Press** event'i **boş** (komut yalnız Release'de)
- [ ] Beş butonda da **Send Component ID işaretsiz** (Press ve Release)
- [ ] E-STOP butonu görsel olarak ayrıştı (kırmızı, büyük, diğerlerinden uzak)
- [ ] Far/ışık butonu **varsa silindi**, yenisi eklenmedi (§0.6 — far yalnız gösterge)
- [ ] Projede `printh 5A 05` araması **sonuç vermiyor** (ölü far komutu kalmadı)

Nextion Editor'de — göstergeler:

- [ ] `pageMain` → `contactor` Text eklendi (`txt_maxl≥8`, `txt="--"`, event boş)
- [ ] `pageBms` → `warn` Number eklendi (`val=3`)
- [ ] `warn` için 0/1/2/3 → metin+renk eşlemesi kuruldu (`3` = nötr gri, kırmızı DEĞİL)
- [ ] `pFar` → `far` olarak yeniden adlandırıldı
- [ ] `far` touch event'i tamamen boşaltıldı (`vaFarState.val++` silindi)
- [ ] `far` resource ID'leri not edildi → firmware ekibine iletildi
- [ ] `chg` varsayılan `val` = `3` yapıldı
- [ ] `tm0` timer'ına `chg.val==3 → chgtxt.txt="--"` dalı eklendi
- [ ] `motorErr` varsayılan `txt` = `--` yapıldı
- [ ] Proje **Compile** edildi
- [ ] Ekrana **Upload** edildi (USB-TTL veya SD kart)

Firmware ekibine geri bildirim:

- [ ] `HMI_PIC_HEADLIGHT_ON` / `HMI_PIC_HEADLIGHT_OFF` gerçek resource ID'leri
      (`include/SystemConfig.h`, şu an placeholder `1`/`0`)

Ekranda doğrulama (araç üzerinde):

- [ ] **Her buton ESP log'unda DOĞRU komutu üretiyor** (araç tekerlekleri
      yerden kesik / kontaktör devre dışıyken, `idf.py monitor` ile):
      START → `"HMI command: START request"`,
      DRIVE → `"DRIVE_ENABLE request"`,
      RESET → `"RESET request"`,
      **E-STOP → `"EMERGENCY_STOP request"`** (`RESET` görürseniz butonun
      `printh` satırı yanlış — §0.1'e dönün),
      DUR → `"STOP (kontrollu durdurma) request"`
- [ ] Hiçbir butonda `"Ignored/Unknown HMI command received"` WARN'ı çıkmıyor
- [ ] `contactor` START öncesi `OPEN`, START sonrası `CLOSED` gösteriyor
- [ ] `warn` BMS bağlıyken `0`/`1`/`2`, BMS sökülüyken `3` (gri) gösteriyor
- [ ] `chg` şarj kablosu takılıyken "Sarj Oluyor", sürüşte "Desarj",
      kontak açık dururken "Bosta", BMS sökülüyken `--` gösteriyor
- [ ] Far düğmesine basınca ikon değişiyor; ekranı resetleyince ikon
      **≤ 6.5 sn içinde** gerçek duruma dönüyor (yerel durum tutulmadığının kanıtı)
