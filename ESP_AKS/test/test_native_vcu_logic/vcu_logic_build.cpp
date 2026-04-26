// Native test build wrapper'ı: src/VcuLogic.cpp'yi bu test suite'i için
// derler. PIO'nun build_src_filter'ı tüm test'lere global uygulandığından,
// VcuLogic.cpp'yi yalnızca state machine testleri kapsamında derlemek için
// bu satıcı (vendor) include'unu kullanıyoruz. Production build'de bu dosya
// derlenmez (test/ ağacı dışında).
#include "VcuLogic.cpp"
