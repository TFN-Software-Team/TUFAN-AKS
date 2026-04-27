// Native test build wrapper'ı: lib/RelayManager/RelayManager.cpp'yi yalnızca
// bu test suite'i kapsamında derler. PIO'da RelayManager library lib_ignore
// ile dışlanmış olduğundan diğer suite'lere SPI sembolü sızdırmaz.
#include "RelayManager.cpp"
