## HV Testleri
- `TUFAN_ALLOW_HV_TEST` bayrağı olmadan gömülü röle testleri derlenmez.
- Araç montajlıyken `pio test -e esp32dev` çalıştırıldığında test_embedded_smoke otomatik olarak atlanır.
- HV testlerini bilinçli olarak çalıştırmak için: `pio test -e esp32dev -- -D TUFAN_ALLOW_HV_TEST=1`
- [ ] Diag firmware yüklendiyse normal firmware geri yüklendi mi?
