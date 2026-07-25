#pragma once
#include <cstdint>

// HMI command frame parser'ının SAF fonksiyon hali.
// Donanım veya global durum (static değişken) barındırmaz; state,
// dışarıdan struct olarak geçirilir.

struct HMI_TouchParserState {
    int rxState = 0;
    uint8_t pendingCmd = 0;
};

// Nextion touch parser.
// Girdi:
//   rxByte: UART'tan okunan yeni byte
//   state: parser'ın mevcut durumu (başlangıçta varsayılan 0/0 olmalı)
//   outCommand: eğer başarılı bir çerçeve çözüldüyse komut buraya yazılır
// Çıktı:
//   true: bir komut başarıyla çözüldü ve outCommand'a yazıldı
//   false: çerçeve tamamlanmadı veya geçersiz (checksum mismatch, çöp byte vb.)
bool HMI_parseTouchByte(uint8_t rxByte, HMI_TouchParserState& state, uint8_t& outCommand);
