#pragma once
// Native test stub for driver/spi_master.h
// Sadece include zincirini sağlar. RelayManager.h `spi_device_handle_t`
// tipine üye olarak ihtiyaç duyar — burada minimal bir alias tanımlanır.
typedef void* spi_device_handle_t;
