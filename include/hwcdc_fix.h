// hwcdc_fix.h
// Workaround para bug en framework-arduinoespressif32 v3.20017:
// HardwareSerial.cpp referencia 'Serial' pero no incluye HWCDC.h cuando
// ARDUINO_USB_CDC_ON_BOOT=1 en ESP32-C3.
// Este archivo se inyecta via -include en build_flags para que TODOS los
// archivos (incluidos los del framework) vean la declaración de HWCDC Serial.
#pragma once
#ifdef __cplusplus
  #if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    #include "HWCDC.h"
  #endif
#endif
