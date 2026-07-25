# TUFAN-AKS Release Bilgisi

## Yarışa Giden Sürüm
- **Commit:** [DOLDURULACAK]
- **Tag:** [DOLDURULACAK — `git tag v1.0-teknofest` ekip kararıyla atılacak]
- **Dal:** main
- **Tarih:** [DOLDURULACAK]

## Derleme
```bash
cd ESP_AKS
pio run -e esp32dev
```

## Flash
```bash
pio run -e esp32dev --target upload
```

## Doğrulama
Boot logunda `TUFAN-AKS <version> (<hash>)` satırı görünmelidir.
