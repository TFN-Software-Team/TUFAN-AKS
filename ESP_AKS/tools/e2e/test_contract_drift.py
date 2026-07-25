"""Gorev 3: Contract-drift bekcileri.

Bu dosyadaki testler yalnizca OKUR — hicbir firmware/Monitor kaynagini
DEGISTIRMEZ. Amac: sozlesme sabitlerinden biri (TEL_FIELD_COUNT, E22
register hedefleri, one-directionality vb.) gelecekte kaza ile
degistirilirse CI'da ANINDA bir FAIL uretmek.
"""

from __future__ import annotations

import re

import pytest

import contract
from drift_helpers import (
    count_active_code_hits,
    extract_define_int,
    extract_define_raw,
    iter_files,
    read,
    strip_c_comments,
    strip_py_comments,
)


# ===========================================================================
# 3.1 — UKS telemetry.h / telemetry.c
# ===========================================================================


def test_uks_tel_field_count_and_version(uks_telemetry_dir):
    header = read(uks_telemetry_dir / "Core/Inc/telemetry.h")
    assert extract_define_int(header, "TEL_FIELD_COUNT", source="UKS telemetry.h") == contract.TOKEN_COUNT
    assert extract_define_int(header, "TEL_PROTOCOL_VERSION", source="UKS telemetry.h") == contract.VER


def test_uks_current_unit_is_centiamp_not_centima(uks_telemetry_dir, uks_root, aks_root):
    """current alani (TEL 14. alan) birimi centi-Amper (0.01 A)'dir, centi-
    miliamper DEGIL — AKS TEL_bmsCurrentCentiA = 0xE000 byte[0:1] (0.1 A raw)
    x 10 (bkz. AKS Telemetry.h sendStatus birim sozlesmesi yorumu,
    CanParse.cpp). UKS tarafinda telemetry.h/telemetry.c ve iki README'de
    yanlislikla '0.01 mA' yaziyordu — 1000x birim karisikligi kaynagiydi. Bu
    bekci yanlis etiketin (yorum/dokuman/dashboard) geri gelmedigini
    denetler."""
    header = read(uks_telemetry_dir / "Core/Inc/telemetry.h")
    assert "0.01 mA" not in header, (
        "UKS telemetry.h icinde '0.01 mA' hala var — dogru birim centi-"
        "Amper (0.01 A), AKS TEL_bmsCurrentCentiA ile ayni. Bu geri donus "
        "1000x birim karisikligina yol acar."
    )

    dashboard_src = read(uks_telemetry_dir / "Core/Src/telemetry.c")
    assert "%02ld mA" not in dashboard_src, (
        "Telemetry_PrintDashboard hala 'mA' etiketiyle basiyor gorunuyor — "
        "hesaplama (/100, %100) zaten dogru Amper degeri uretiyor, yalnizca "
        "etiket 'A' olmali (mA degil)."
    )

    for label, path in (
        ("UKS kok README.md", uks_root / "README.md"),
        ("UKS-Telemetry/README.md", uks_telemetry_dir / "README.md"),
        ("AKS Documents/UKS_LoRa_Protocol.md",
         aks_root.parent / "Documents/UKS_LoRa_Protocol.md"),
    ):
        text = read(path)
        assert "×0.01 mA" not in text, (
            f"{label} icinde 'current' alani hala ×0.01 mA olarak yaziyor — "
            "dogrusu ×0.01 A (centi-Amper)."
        )


def test_uks_tel_link_timeout_matches_contract(uks_telemetry_dir):
    """contract.TEL_LINK_TIMEOUT_MS, UKS telemetry.h'daki gercek degerden
    SAPMAMALI (bkz. Documents/LoRa_Link_Analysis.md 'UKS-side TEL Timeout
    Margin')."""
    header = read(uks_telemetry_dir / "Core/Inc/telemetry.h")
    uks_value = extract_define_int(header, "TEL_LINK_TIMEOUT_MS", source="UKS telemetry.h")
    assert uks_value == contract.TEL_LINK_TIMEOUT_MS, (
        f"UKS TEL_LINK_TIMEOUT_MS={uks_value} contract.py degerinden "
        f"({contract.TEL_LINK_TIMEOUT_MS}) sapmis — iki tarafi senkronla."
    )


def test_uks_tel_link_timeout_has_enough_margin_over_tx_period(uks_telemetry_dir, aks_root):
    """Invariant: TEL_LINK_TIMEOUT_MS >= 3 x LORA_TX_PERIOD_MS.

    Bkz. Documents/LoRa_Link_Analysis.md 'UKS-side TEL Timeout Margin': bu
    oran, UKS'in ardisik 3 atlanmis (AUX mesgul / TX ring dolu ertelemesi)
    TX tik'ini FALSE LINK,DOWN vermeden tolere etmesini garanti eder (4.
    ardisik atlanmis tik gerekir tetiklenmesi icin). Biri LORA_TX_PERIOD_MS'i
    (AKS SystemConfig.h) tek tarafli artirirsa ve UKS TEL_LINK_TIMEOUT_MS
    buna gore yeniden hesaplanmazsa, bu marj sessizce erir — bu bekci CI'da
    ANINDA yakalar."""
    header = read(uks_telemetry_dir / "Core/Inc/telemetry.h")
    uks_timeout = extract_define_int(header, "TEL_LINK_TIMEOUT_MS", source="UKS telemetry.h")

    sys_config = read(aks_root / "include/SystemConfig.h")
    lora_tx_period = extract_define_int(sys_config, "LORA_TX_PERIOD_MS", source="AKS SystemConfig.h")

    assert uks_timeout >= 3 * lora_tx_period, (
        f"UKS TEL_LINK_TIMEOUT_MS ({uks_timeout} ms) artik en az 3x AKS "
        f"LORA_TX_PERIOD_MS'i ({lora_tx_period} ms) karsilamiyor "
        "— ardisik tik atlama toleransi (bkz. Documents/LoRa_Link_Analysis.md "
        "'UKS-side TEL Timeout Margin' tablosu) 3'un altina dusmus olabilir. "
        "LORA_TX_PERIOD_MS degistiyse TEL_LINK_TIMEOUT_MS'i (UKS telemetry.h) "
        "AYNI COMMIT SETINDE yeniden kalibre edin ve analiz tablosunu guncelleyin."
    )


def test_uks_telemetry_c_has_parse_u32(uks_telemetry_dir):
    src = read(uks_telemetry_dir / "Core/Src/telemetry.c")
    assert re.search(r"\bstatic\s+int\s+Parse_U32\s*\(", src), (
        "UKS telemetry.c icinde Parse_U32 bulunamadi — ts_ms/seq'in uint32 "
        "(sarma-guvenli) parse edilmesi 1918430 duzeltmesiyle Parse_U32'ye "
        "bagliydi; bu fonksiyon kaldirilir/yeniden adlandirilirsa 24.8 gunluk "
        "sarma hatasi geri gelir."
    )
    # ts_ms (f[17]) ve seq (f[2]) fiilen Parse_U32 ile parse ediliyor mu?
    assert re.search(r"Parse_U32\(f\[2\]", src), "seq (f[2]) Parse_U32 ile parse edilmiyor"
    assert re.search(r"Parse_U32\(f\[17\]", src), "ts_ms (f[17]) Parse_U32 ile parse edilmiyor"


# ===========================================================================
# 3.2 — ESP_AKS SystemConfig.h / TelemetrySanitize.h / Telemetry.h
# ===========================================================================


def test_aks_lora_protocol_version(aks_root):
    text = read(aks_root / "include/SystemConfig.h")
    assert extract_define_int(text, "LORA_PROTOCOL_VERSION", source="AKS SystemConfig.h") == contract.VER


def test_aks_tel_spd_x10_max(aks_root):
    text = read(aks_root / "lib/Telemetry/Telemetry.h")
    assert extract_define_int(text, "TEL_SPD_X10_MAX", source="AKS Telemetry.h") == contract.TEL_SPD_X10_MAX


def test_aks_telemetry_sanitize_exists_with_system_state_rule(aks_root):
    path = aks_root / "lib/Telemetry/TelemetrySanitize.h"
    assert path.is_file(), "AKS TelemetrySanitize.h eksik"
    text = read(path)
    assert "sanitizeSystemState" in text
    assert "sanitizeSoc" in text
    assert "sanitizeCurrent" in text
    # sysState disi -> 2 (BOSTA) kurali kaynakta hala mevcut mu?
    # AKS-17: TEL_bmsSystemState gercek bir kaynaktan gelmiyor; 0 -> 2
    # raporlanir (onceki davranis 0 -> 4/FAULT'tu, yaniltici gosterime yol aciyordu).
    # Y33 (24.07.2026): fallback 2 KALDI, ama alan artik sabit 2 DEGIL — gercek
    # deger akimdan turetiliyor (bkz. test_aks_sysstate_derived_from_current).
    # 1..4 araligi UKS Decode_Line kapisidir (Parse_Int(f[12], 1, 4)) ve
    # DARALTILAMAZ: aralik disi bir deger TUM frame'i reddettirir.
    m = re.search(
        r"sanitizeSystemState[^{]*\{[^}]*\}", strip_c_comments(text), re.DOTALL
    )
    assert m, "sanitizeSystemState govdesi bulunamadi"
    assert re.search(r">=\s*1U?\s*&&.*<=\s*4U?", m.group(0)) or re.search(
        r"1U?\s*<=.*<=\s*4U?", m.group(0)
    ), "sanitizeSystemState govdesinde 1..4 aralik kontrolu bulunamadi"
    assert re.search(r":\s*2U?\s*;", m.group(0)), (
        "sanitizeSystemState govdesinde IDLE(2) donus degeri bulunamadi"
    )


def test_aks_single_soc_source(aks_root):
    """Y8 (24.07.2026): ekranda ve telemetride TEK SoC gosterilir —
    BMS'in KENDI raporladigi TEL_bmsSocHundredths (0xE000 byte[4:5],
    DOGRULANDI). Uretici hesabi, ortalama gerilimden lineer tahminden
    guvenilirdir (LiFePO4'un duz OCV bolgesinde tahmin KABADIR).

    Sistemde SoC ureten UC yer var:
      1. TEL_bmsSocHundredths   -> GOSTERILEN (telemetri alan 15 + Nextion)
      2. TEL_bmsSoc2Hundredths  -> parse edilir, GOSTERILMEZ
      3. BmsComputed::socPercent -> AKS'in tahmini, YEDEK, TUKETILMEZ

    Bu bekci, 2 veya 3'un sessizce bir gosterim yoluna baglanmasini yakalar;
    aksi halde ekranda IKI FARKLI yuzde gorunur ve operator hangisine
    guvenecegini bilemez (Y8'in kapatmak istedigi durum).
    """
    tel = strip_c_comments(read(aks_root / "lib/Telemetry/Telemetry.cpp"))
    assert "TEL_bmsSocHundredths" in tel, (
        "Telemetry.cpp artik TEL_bmsSocHundredths gondermiyor — telemetrideki "
        "SoC kaynagi degismis olabilir (Y8 tek-kaynak kurali)."
    )
    assert "TEL_bmsSoc2Hundredths" not in tel, (
        "Telemetry.cpp SoC 2'yi (TEL_bmsSoc2Hundredths) hat formatina yazmis "
        "gorunuyor — bu alan bilincli olarak GOSTERILMEZ (Y8). Iki farkli SoC "
        "yayinlamak operatoru yaniltir; ayrica alan sayisi degisirse UKS "
        "Decode_Line TUM frame'i reddeder."
    )

    main_cpp = strip_c_comments(read(aks_root / "src/main.cpp"))
    assert "HMI_batteryDisplayValue" in main_cpp and "TEL_bmsSocHundredths" in main_cpp, (
        "main.cpp'deki Nextion batarya gostergesi artik TEL_bmsSocHundredths'ten "
        "beslenmiyor gorunuyor (Y8 tek-kaynak kurali)."
    )
    assert "socPercent" not in main_cpp, (
        "main.cpp BmsComputed::socPercent'i (AKS'in ortalamadan lineer tahmini) "
        "bir gosterim yoluna baglamis gorunuyor. Gosterilen SoC yalnizca BMS'in "
        "kendi degeri olmalidir; bu alan YEDEK/tani amaclidir (bkz. BmsComputed.h). "
        "Bilincli olarak degistirildiyse VehicleData.h'deki tek-kaynak kuralini ve "
        "bu bekciyi AYNI COMMIT'TE guncelleyin."
    )


def test_aks_sysstate_derived_from_current(aks_root):
    """Y33 (24.07.2026): BMS'in SAGLIK durumunu yayinladigi bir CAN ID'ye
    ULASILAMADI ve aranmayacak. sysState alani (TEL 12) artik DOGRULANMIS
    akimdan (0xE000 byte[0:1]) turetilen CALISMA MODUNU tasir:
    1=Desarj, 2=Bosta, 3=Sarj — FAULT(4) URETILMEZ.

    Bu bekci iki seyi korur:
      1) SYSSTATE_DERIVE_FROM_CURRENT varsayilani 1 kalmali. 0'a donerse alan
         sessizce sabit 2'ye duser ve operator ekraninda hicbir bilgi tasimaz
         (AKS-17 sonrasi yasanan durum) — testler yine yesil gecerdi.
      2) applyIfEnabled'daki BAYAT VERI korumasi durmali. TEL_bmsDataValid
         kontrolu kaldirilirsa, BMS verisi kesildiginde son gorulen (bayat)
         akimdan "SARJ EDILIYOR" gibi YANLIS bir mod raporlanir.
    """
    cfg = read(aks_root / "include/SystemConfig.h")
    assert (
        extract_define_int(cfg, "SYSSTATE_DERIVE_FROM_CURRENT", source="AKS SystemConfig.h")
        == 1
    ), (
        "SYSSTATE_DERIVE_FROM_CURRENT varsayilani 1 DEGIL — sysState alani "
        "akimdan turetilmiyor demektir; alan sabit 2 (Bosta) gonderir ve UKS/"
        "Monitor ekraninda batarya calisma modu GORUNMEZ (Y33 gerilemesi). "
        "Bayrak bilincli olarak kapatildiysa bu bekciyi AYNI COMMIT'TE guncelleyin."
    )

    derive = strip_c_comments(read(aks_root / "lib/Telemetry/SysStateDerive.h"))
    m = re.search(r"applyIfEnabled[^{]*\{.*?\n\}", derive, re.DOTALL)
    assert m, "SysStateDerive::applyIfEnabled govdesi bulunamadi"
    assert "TEL_bmsDataValid" in m.group(0), (
        "applyIfEnabled artik TEL_bmsDataValid'i kontrol etmiyor — BMS verisi "
        "bayatladiginda son gorulen akimdan yanlis bir calisma modu (orn. "
        "'SARJ') raporlanir. Bayat veride notr 2 (Bosta) yazilmalidir; gercek "
        "'veri yok' bilgisi TEL alan 16 (bmsValid) uzerinden gider."
    )


def test_aks_p6_offline_buffer_constants(aks_root):
    sys_config = read(aks_root / "include/SystemConfig.h")
    assert extract_define_int(sys_config, "OFFLINE_SAMPLE_PERIOD_MS", source="AKS SystemConfig.h") == contract.OFFLINE_SAMPLE_PERIOD_MS
    assert extract_define_int(sys_config, "REPLAY_BURST_PER_TICK", source="AKS SystemConfig.h") == contract.REPLAY_BURST_PER_TICK
    assert extract_define_int(sys_config, "BOOT_LINK_GRACE_MS", source="AKS SystemConfig.h") == contract.BOOT_LINK_GRACE_MS
    assert extract_define_int(sys_config, "LORA_TX_PERIOD_MS", source="AKS SystemConfig.h") == contract.LORA_TX_PERIOD_MS
    assert extract_define_int(sys_config, "LINK_TIMEOUT_MS", source="AKS SystemConfig.h") == contract.LINK_TIMEOUT_MS

    ob_header = read(aks_root / "lib/OfflineBuffer/OfflineBuffer.h")
    assert extract_define_int(ob_header, "OB_CAPACITY", source="AKS OfflineBuffer.h") == contract.OB_CAPACITY

def test_aks_telemetry_cpp_format_string_matches_contract(aks_root):
    src = read(aks_root / "lib/Telemetry/Telemetry.cpp")
    m = re.search(r'"TEL,([^"]+)\\r\\n"', src)
    assert m, "Telemetry.cpp icinde TEL format stringi bulunamadi"
    fmt = m.group(1)
    
    specifiers = fmt.split(",")
    
    assert len(specifiers) == len(contract.FIELD_ORDER), (
        f"Telemetry.cpp icindeki format specifier sayisi ({len(specifiers)}) ile "
        f"contract.py'deki FIELD_ORDER sayisi ({len(contract.FIELD_ORDER)}) eslesmiyor."
    )


# ===========================================================================
# 3.3 — E22 register sozlesmesi (UKS e22_regs.h <-> AKS E22Regs.h)
# ===========================================================================

_E22_FIELDS = ["ADDH", "ADDL", "NETID", "REG0", "REG1", "REG2", "REG3", "CRYPT_H", "CRYPT_L"]


def test_e22_register_targets_match_across_repos(uks_telemetry_dir, aks_root):
    uks_text = read(uks_telemetry_dir / "Core/Inc/e22_regs.h")
    aks_text = read(aks_root / "include/E22Regs.h")

    mismatches = []
    for field in _E22_FIELDS:
        uks_val = extract_define_int(uks_text, f"E22_VAL_{field}", source="UKS e22_regs.h")
        aks_val = extract_define_int(aks_text, f"E22_CFG_{field}", source="AKS E22Regs.h")
        if uks_val != aks_val:
            mismatches.append((field, uks_val, aks_val))

    assert not mismatches, (
        "E22 register hedef degerleri UKS/AKS arasinda UYUSMUYOR (cift "
        f"commit senkronizasyonu bozulmus olabilir): {mismatches}"
    )

    # Sozlesme sabitleriyle (contract.py) de birebir tutarli olmali.
    expected = {
        "ADDH": contract.E22_ADDH, "ADDL": contract.E22_ADDL,
        "NETID": contract.E22_NETID, "REG0": contract.E22_REG0,
        "REG1": contract.E22_REG1, "REG2": contract.E22_REG2,
        "REG3": contract.E22_REG3, "CRYPT_H": contract.E22_CRYPT_H,
        "CRYPT_L": contract.E22_CRYPT_L,
    }
    for field in _E22_FIELDS:
        uks_val = extract_define_int(uks_text, f"E22_VAL_{field}", source="UKS e22_regs.h")
        assert uks_val == expected[field], (
            f"contract.py E22_{field} degeri kaynaktan sapmis: "
            f"contract={expected[field]} kaynak={uks_val}"
        )


def test_e22_config_mode_pin_levels_match(uks_telemetry_dir, aks_root):
    uks_text = read(uks_telemetry_dir / "Core/Inc/e22_regs.h")
    aks_text = read(aks_root / "include/SystemConfig.h")

    uks_m0 = extract_define_int(uks_text, "E22_MODE_CONFIG_M0", source="UKS e22_regs.h")
    uks_m1 = extract_define_int(uks_text, "E22_MODE_CONFIG_M1", source="UKS e22_regs.h")
    aks_m0 = extract_define_int(aks_text, "LORA_MODE_CONFIG_M0_LEVEL", source="AKS SystemConfig.h")
    aks_m1 = extract_define_int(aks_text, "LORA_MODE_CONFIG_M1_LEVEL", source="AKS SystemConfig.h")

    assert (uks_m0, uks_m1) == (aks_m0, aks_m1) == (
        contract.E22_MODE_CONFIG_M0, contract.E22_MODE_CONFIG_M1
    )


def test_e22_dump_format_string_matches_across_repos(uks_telemetry_dir, aks_root):
    uks_lora_c = read(uks_telemetry_dir / "Core/Src/lora.c")
    aks_main_cpp = read(aks_root / "src/main.cpp")

    needle = contract.E22_DUMP_LINE_FORMAT
    assert needle in uks_lora_c, f"UKS lora.c icinde dump formati bulunamadi: {needle!r}"
    assert needle in aks_main_cpp, f"AKS main.cpp icinde dump formati bulunamadi: {needle!r}"


def test_e32_leftover_constants_are_gone(uks_telemetry_dir, aks_root):
    """E32->E22 gecisinden (1803e1f) sonra hicbir E32_CFG_* sabiti kalmamali."""
    uks_files = iter_files(uks_telemetry_dir, ["Core", "test"], (".c", ".h"))
    aks_files = iter_files(aks_root, ["include", "lib", "src", "test"], (".c", ".h", ".cpp"))

    pattern = re.compile(r"\bE32_CFG_\w+")
    for label, files in (("UKS", uks_files), ("AKS", aks_files)):
        hits = count_active_code_hits(files, pattern)
        assert not hits, f"{label} reposunda E32_CFG_* leftover bulundu: {hits}"


# ===========================================================================
# 3.4 — Tek yonluluk (9.2.a) bekcisi
# ===========================================================================


def test_no_removed_command_bytes_in_active_production_code(uks_telemetry_dir, aks_root):
    """0xA1-0xA4 (eski komut byte'lari) PRODUCTION kaynaklarda (yorumlar
    HARIC) SIFIR gecmeli. Test dosyalari kasitli olarak KAPSAM DISI
    birakilir — cunku AKS'in kendi test/test_native_lora_rx_handler
    testi bu byte'lari, TAM DA UNKNOWN olarak siniflandigini KANITLAMAK
    icin literal kullanir (yani varligi kaldirilmayi degil, kaldirilmis
    OLDUGUNU dogrular)."""
    pattern = re.compile(r"\b0[xX][Aa][1-4]\b")

    uks_files = iter_files(uks_telemetry_dir, ["Core"], (".c", ".h"))
    aks_files = iter_files(aks_root, ["include", "lib", "src"], (".c", ".h", ".cpp"))

    for label, files in (("UKS", uks_files), ("AKS", aks_files)):
        hits = count_active_code_hits(files, pattern)
        assert not hits, (
            f"{label} PRODUCTION kodunda kaldirilmis komut byte'i (0xA1-0xA4) "
            f"bulundu — 9.2.a tek-yonluluk ihlali olabilir: {hits}"
        )


def test_uks_active_code_has_no_button_estop_symbols(uks_telemetry_dir):
    """UKS'te RF komut kanali + E-STOP butonu kaldirildi (d8b321f). UKS
    PRODUCTION kodunda buton/estop sembolu SIFIR olmali. (AKS tarafinda
    fiziksel kontaktor/HMI E-STOP MANTIGI KASITLI OLARAK KALIR — bu bekci
    yalnizca UKS icindir, spesifikasyonla birebir.)"""
    pattern = re.compile(r"(?i)estop|e_stop|emergency|\bbutton\b|\bbtn\b")
    uks_files = iter_files(uks_telemetry_dir, ["Core"], (".c", ".h"))
    hits = count_active_code_hits(uks_files, pattern)
    assert not hits, f"UKS production kodunda buton/estop sembolu bulundu: {hits}"


def test_uks_sends_heartbeat_0xb0(uks_telemetry_dir):
    lora_h = read(uks_telemetry_dir / "Core/Inc/lora.h")
    main_c = read(uks_telemetry_dir / "Core/Src/main.c")

    hb_value = extract_define_int(lora_h, "LORA_HEARTBEAT_BYTE", source="UKS lora.h")
    assert hb_value == contract.LORA_HEARTBEAT_BYTE

    # main.c fiilen Lora_SendHeartbeatFast(&lora_ctx, LORA_HEARTBEAT_BYTE)
    # cagirip 0xB0 gonderiyor mu? (UKS-05: Lora_Send yerine
    # Lora_SendHeartbeatFast kullanilir — RX interrupt'siz, TX-safe path.)
    assert re.search(r"LORA_HEARTBEAT_BYTE", main_c), (
        "UKS main.c LORA_HEARTBEAT_BYTE'i kullanmiyor gorunuyor — 0xB0 "
        "gonderimi POZITIF dogrulanamadi"
    )
    assert re.search(r"Lora_SendHeartbeatFast\s*\(\s*&lora_ctx\s*,\s*LORA_HEARTBEAT_BYTE", main_c), (
        "UKS main.c icinde heartbeat byte'inin Lora_SendHeartbeatFast ile TX edildigi "
        "kod bulunamadi"
    )


def test_aks_handles_0xb0_via_lora_rx_handler(aks_root):
    """AKS'te 0xB0 isleme (LoraRxHandler) POZITIF dogrulanir: UKS_HEARTBEAT_BYTE
    == 0xB0 ve lora_classify_rx_byte bu degeri HEARTBEAT olarak siniflandiriyor.

    main.cpp bu siniflandirmayi DOGRUDAN cagirabilir, ya da (M1 refactor,
    bkz. 25c3746) UplinkScheduler::onRxByte uzerinden DOLAYLI cagirabilir.
    Dolayli durumda zincirin gercekten bagli oldugunu (bos bir kabuk
    olmadigini) dogrulamak icin UplinkScheduler.cpp'nin onRxByte govdesinde
    lora_classify_rx_byte'i fiilen cagirdigi ayrica denetlenir."""
    sys_config = read(aks_root / "include/SystemConfig.h")
    hb_value = extract_define_int(sys_config, "UKS_HEARTBEAT_BYTE", source="AKS SystemConfig.h")
    assert hb_value == contract.LORA_HEARTBEAT_BYTE

    handler = read(aks_root / "lib/LoraLink/LoraRxHandler.h")
    assert "lora_classify_rx_byte" in handler
    m = re.search(
        r"lora_classify_rx_byte\s*\([^)]*\)\s*\{.*?\}", strip_c_comments(handler), re.DOTALL
    )
    assert m, "lora_classify_rx_byte govdesi bulunamadi"
    assert re.search(r"UKS_HEARTBEAT_BYTE", m.group(0)) and re.search(
        r"HEARTBEAT", m.group(0)
    ), "lora_classify_rx_byte, UKS_HEARTBEAT_BYTE'i HEARTBEAT'e eslemiyor"

    main_cpp = read(aks_root / "src/main.cpp")
    calls_directly = re.search(r"lora_classify_rx_byte\s*\(", main_cpp)
    calls_via_scheduler = re.search(r"\.onRxByte\s*\(", main_cpp)
    assert calls_directly or calls_via_scheduler, (
        "AKS main.cpp RX dongusu lora_classify_rx_byte'i (dogrudan ya da "
        "UplinkScheduler::onRxByte uzerinden) CAGIRMIYOR — 0xB0 isleme yolu "
        "POZITIF dogrulanamadi"
    )

    if not calls_directly:
        scheduler_src = read(aks_root / "lib/LoraLink/UplinkScheduler.cpp")
        sm = re.search(
            r"UplinkScheduler::onRxByte\s*\([^)]*\)\s*\{.*?\n\}",
            strip_c_comments(scheduler_src), re.DOTALL,
        )
        assert sm, "UplinkScheduler::onRxByte govdesi bulunamadi"
        assert re.search(r"lora_classify_rx_byte\s*\(", sm.group(0)), (
            "UplinkScheduler::onRxByte lora_classify_rx_byte'i CAGIRMIYOR — "
            "main.cpp'nin cagirdigi onRxByte 0xB0'i fiilen siniflandirmiyor "
            "olabilir (zincir kopuk)"
        )


# ===========================================================================
# 3.5 — Monitor sozlesmesi
# ===========================================================================


def test_monitor_header_matches_spec_byte_for_byte(csv_logger_module):
    assert csv_logger_module.HEADER == contract.MONITOR_HEADER


def test_aks_vehicle_params_confirmed_flag_exists(aks_root):
    text = read(aks_root / "include/VehicleParams.h")
    assert re.search(r"#define\s+VEHICLE_PARAMS_CONFIRMED\b", text), (
        "AKS VehicleParams.h icinde VEHICLE_PARAMS_CONFIRMED bayragi yok "
        "(9.2.c.i teyitsiz-build korumasi kaldirilmis olabilir)"
    )


def test_monitor_config_confirmed_flag_exists(monitor_root):
    text = read(monitor_root / "config.py")
    text_no_comments = strip_py_comments(text)
    assert re.search(r"^\s*CONFIG_CONFIRMED\s*=", text_no_comments, re.MULTILINE), (
        "Monitor config.py icinde CONFIG_CONFIRMED bayragi yok"
    )


# ===========================================================================
# 3.6 — AÇIK İŞ izleyicisi: Lithium Balance c-BMS alan kapsamı (9.2.c.ii)
#
# 2026-07-03 main-merge'inde BMS vendörü Solion SK -> Lithium Balance
# c-BMS'e gecti (donanim teyitli). Su an SADECE packV (CAN ID 0xE000)
# cozuldu; digger 7 ID (E001-E005, E032, E033) stub — TelemetryData'ya
# hicbir alan yazmiyor (bkz. TEKNIK_KONTROL_PROVASI.md "AÇIK İŞ" maddesi).
#
# Bu test BİLEREK xfail(strict=True): alan kapsamı eksikliğini izler.
# Ekip stub'lardan birine gerçek parse ekleyip TelemetryData'ya yazmaya
# baslarsa bu test XPASS eder ve strict=True nedeniyle SUITE KIRILIR —
# bu, TEKNIK_KONTROL_PROVASI.md'yi ve boot-log uyarisini guncellemeyi
# unutmamak icin bilincli bir hatirlatma mekanizmasidir.
# ===========================================================================

_LB_STUB_IDS = ["E001", "E002", "E003", "E004", "E005", "E006"]
# E001 hâlâ listede çünkü sysState kısmı (byte[0:5] dışındaki)
# parse edilmiyor — xfail yine geçerli ama liste küçüldü.
# E015-E020 stub olmaktan çıktı → bu liste'den çıkar.

@pytest.mark.xfail(
    strict=True,
    reason=(
        "9.2.c.ii AÇIK İŞ: Lithium Balance c-BMS'in bazi CAN ID'leri (E001-E006)"
        " henuz TelemetryData'ya tam alan yazmiyor (packV E000 ve"
        " 24 hücre E015-E020 cozuldu). Biri gercek parse kazanirsa bu test XPASS eder —"
        " TEKNIK_KONTROL_PROVASI.md 'AÇIK İŞ' maddesini ve boot-log"
        " uyarisini guncelleyip bu testi kaldirin/genisletin."
    ),
)
def test_lb_bms_field_coverage_is_tracked(aks_root):
    can_parse_cpp = read(aks_root / "lib/CanParse/CanParse.cpp")
    cleaned = strip_c_comments(can_parse_cpp)

    still_stubbed = []
    for can_id in _LB_STUB_IDS:
        m = re.search(
            rf"bool\s+parseLbBms{can_id}\s*\([^)]*\)\s*\{{(.*?)\n\}}",
            cleaned, re.DOTALL,
        )
        assert m, f"parseLbBms{can_id} fonksiyonu CanParse.cpp'de bulunamadi"
        body = m.group(1)
        # Stub imzasi: (void)out; ve govde icinde out. alan atamasi YOK.
        if "(void)out;" in body and not re.search(r"\bout\.\w+\s*=", body):
            still_stubbed.append(can_id)

    # Hedef durum (henuz ULASILMADI): 7 ID'nin TAMAMI gercek parse kazanmis
    # olmali (still_stubbed bos). Bu satir BUGUN FAIL eder (xfail bunu
    # bekliyor); biri parse eklerse still_stubbed kuculur/bosalir, assert
    # PASS eder, xfail(strict=True) bunu XPASS'e cevirip suite'i kirar.
    assert still_stubbed == [], (
        f"Hala stub olan LB BMS ID'leri: {still_stubbed} — hedef: tumu "
        "gercek parse kazanmis olmali (bos liste)."
    )


# ===========================================================================
# 3.7 — aks_loop_sim.py outage sim post_live_ms bekcisi
# ===========================================================================


def test_aks_loop_sim_post_live_ms_is_derived_from_contract(aks_root):
    """417a665, REPLAY_BURST_PER_TICK'i 3'ten 1'e dusurdukten sonra
    aks_loop_sim.run_outage_simulation'in sabit post_live_ms=6000 varsayilani
    bayatlamis, 60 paketlik buffer'in yarisi replay edilmeden kalmisti (bkz.
    test_outage_simulation.py'nin FAIL etmesi). Duzeltme: post_live_ms=None
    varsayilan olsun, deger contract sabitlerinden turetilsin. Bu bekci,
    fonksiyonun tekrar sabit bir sayiya donmedigini kontrol eder — aksi
    halde REPLAY_BURST_PER_TICK bir daha degisirse ayni sozlesme kaymasi
    sessizce geri gelir."""
    src = strip_py_comments(read(aks_root / "tools/e2e/aks_loop_sim.py"))

    assert re.search(r"post_live_ms\s*:\s*int\s*\|\s*None\s*=\s*None", src), (
        "run_outage_simulation'in post_live_ms parametresi artik "
        "'int | None = None' imzasini tasimiyor gorunuyor (sabit bir sayiya "
        "geri donmus olabilir) — bu, REPLAY_BURST_PER_TICK degistiginde "
        "post_live_ms'in yeniden bayatlayip 417a665'teki gibi buffer'in "
        "yarisinin sessizce replay edilmeden kalmasina karsi korur; eger "
        "parametre bilincli olarak yeniden adlandirildiysa/imzasi degistiyse "
        "bu regex'i de AYNI COMMIT'TE yeni imzaya gore guncelleyin"
    )
    assert re.search(r"post_live_ms\s+is\s+None", src), (
        "post_live_ms icin 'None ise dinamik hesapla' dali bulunamadi — "
        "bu dal olmadan fonksiyon cagirani post_live_ms'i hep sabit/varsayilan "
        "degerle calistirir ve REPLAY_BURST_PER_TICK degistiginde outage "
        "testi sessizce yetersiz sureyle FAIL eder (417a665 kaymasi); eger "
        "hesaplama farkli bir kosul ifadesiyle (orn. 'if not post_live_ms') "
        "yeniden yazildiysa bu regex'i yeni ifadeye gore guncelleyin"
    )
    assert "contract.OFFLINE_SAMPLE_PERIOD_MS" in src and "contract.REPLAY_BURST_PER_TICK" in src, (
        "post_live_ms hesaplamasi artik contract.OFFLINE_SAMPLE_PERIOD_MS / "
        "contract.REPLAY_BURST_PER_TICK sabitlerine dayanmiyor gorunuyor — bu "
        "sabitler yerine hard-code sayilar kullanilirsa REPLAY_BURST_PER_TICK "
        "gelecekte tekrar degistiginde post_live_ms otomatik uyum saglamaz ve "
        "417a665'teki sozlesme kaymasi baska bir sekilde geri gelir; eger "
        "hesaplama mesru bir sekilde contract'tan farkli/ek sabitler "
        "kullanacak sekilde genisletildiyse bu test'i de o sabitleri "
        "kontrol edecek sekilde guncelleyin"
    )
