# Contributing

> **Katkı kuralları için TEK DOĞRULUK KAYNAĞI bu dosyadır.** 03.08.2026'ya
> kadar `ESP_AKS/CONTRIBUTING.md` adında ikinci bir dosya vardı (kod yerleşim
> / M5 konvansiyonu); aynı isimde iki farklı içerik kafa karışıklığı
> yarattığı için o dosyanın içeriği aşağıya **"Kod Yerleşim Konvansiyonu
> (M5)"** bölümü olarak taşındı.

## Kod Yerleşim Konvansiyonu (M5)

Tek kural: **saf (donanımsız) ve native test edilen her modül `lib/<Modül>/`
altında yaşar** — header'ı ve varsa `.cpp`'si aynı pakette. `include/` yalnızca
**proje-geneli config header'ları** içindir (ör. `SystemConfig.h`, `E22Regs.h`,
`VehicleParams.h`); test edilen modül header'ları oraya konmaz.

Böylece "modül nerede?" sorusunun tek yanıtı olur: PlatformIO LDF paketi
otomatik keşfeder, native testler `test/test_native_<modül>/` altında ilgili
`lib/` paketini derler. Örnek: `LinkMonitor` ve `LoraRxHandler` saf/test edilen
LoRa yardımcılarıdır → `lib/LoraLink/` içindedirler (`include/`'ta değil).

## Branch Naming

All feature and fix branches must follow this format:

`AKS-filenamehere`

Examples:

- `AKS-canmanager-bms-parser`
- `AKS-vculogic-reset-interlock`
- `AKS-readme-update`

Use short, lowercase, hyphen-separated suffixes after the `AKS-` prefix.

## Variable and Function Naming

Project-local identifiers must use a module prefix in uppercase, followed by an underscore and a descriptive name:

`PREFIX_descriptiveName`

Examples:

- `CAN_motorRpm`
- `LO_uartConfig`
- `HMI_currentSpeed`
- `TEL_payloadLength`

Prefix guide:

- `CAN_` for CAN / motor / BMS parsing helpers
- `LO_` for LoRa / uplink / radio helpers
- `HMI_` for display and touch helpers
- `TEL_` for telemetry payload and queue data
- `VCU_` for state-machine-local helpers if added later
- `REL_` for relay-related locals if added later

## General Rules

- ⚠️ **AÇIK İŞ — dil kuralı ekip kararı bekliyor.** Bu satır eskiden koşulsuz
  "Use English for code, comments, logs, and documentation" diyordu, ancak
  **fiili durum bu değildir**: `include/SystemConfig.h`, `src/main.cpp` ve
  `lib/VcuLogic/` içindeki yorumların büyük kısmı, `Documents/` altındaki
  dokümanların neredeyse tamamı ve commit mesajları **Türkçedir**. Yani kural
  yazıldığı haliyle repoda uygulanmıyor. Gözlenen fiili konvansiyon:
  - **Türkçe:** dokümanlar (`.md`), kod yorumları, commit mesajları.
  - **İngilizce:** tanımlayıcılar (değişken/fonksiyon/tip adları), log
    etiketleri (`TAG`) ve aşağıdaki önek şeması.
  - **ASCII-only:** seri log metinleri (ESP-IDF konsolunda Türkçe karakterler
    bozulduğu için kodda `dogrulandi`, `basarili` gibi aksansız yazım
    kullanılıyor).

  Ekip ya kuralı fiili duruma göre güncellemeli ya da kodu/dokümanı kurala
  taşımalıdır; bu doküman güncellemesinde **karar VERİLMEDİ**, yalnız çelişki
  kayda geçirildi.
- Use `vTaskDelay()` inside FreeRTOS tasks. Do not use `delay()`.
- Prefer ESP-IDF log macros such as `ESP_LOGI`, `ESP_LOGW`, and `ESP_LOGE` for state transitions and failure paths.
- Keep documentation synchronized with code whenever CAN IDs, queue payloads, or control commands change.
