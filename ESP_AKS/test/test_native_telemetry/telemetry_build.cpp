// Native test build wrapper'ı: lib/Telemetry/Telemetry.cpp'yi yalnızca bu
// test suite'i kapsamında derler.  PIO global olarak Telemetry library'sini
// lib_ignore ile dışlıyor; bu wrapper başka native suite'lere uart sembolü
// sızdırmadan Telemetry.cpp'yi izole biçimde getirir.
#include "Telemetry.cpp"
