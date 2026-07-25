// native_roles link stub'i — uart_write_bytes.
//
// NEDEN GEREKLI: bu suite VcuLogic.h'yi dahil eder; PlatformIO LDF zinciri
// ayni basligi kullanan HMIHelpers kutuphanesini de (HMIMappings.h ->
// VcuLogic.h) programa baglar. HMIMappings.cpp/HMIHelpers.cpp gercek
// `uart_write_bytes`i cagirir; native ortamda ESP-IDF UART surucusu YOKTUR,
// bu yuzden sembol tanimsiz kalir ve suite LINK EDEMEZ (ERRORED).
//
// Ayni desen native ortaminda zaten kullaniliyor:
//   test/test_native_hmi/test_touch_parser.cpp  (satir ici tanim)
//   test/test_native_hmi_helpers/fake_uart.cpp  (TX yakalayan surum)
//
// Bu suite UART TX'i DOGRULAMAZ (rol/kontaktor mantigini test eder), bu yuzden
// yakalama yapmayan, yalnizca basariyi taklit eden en yalin surum yeterlidir.
#include "driver/uart.h"

extern "C" int uart_write_bytes(uart_port_t /*uart_num*/, const void* /*src*/,
                                size_t size) {
    return static_cast<int>(size);
}
