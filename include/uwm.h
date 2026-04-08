#ifndef UWM_H
#define UWM_H

#include <Arduino.h>

// Estructura para almacenar los datos del medidor
struct MeterData {
    float flowRate;
    float accFlow;
    float accFlowInv;
    float pressure;
    uint8_t information;
    uint8_t temperature;
    uint16_t alarms;
    bool valid;
};

// Funciones principales
bool dgm_init();
bool dgm_read(MeterData &data, uint32_t timeout_ms = 3000);

// Funciones de utilidad (internas)
uint16_t dgm_calculateCRC(const uint8_t *data, uint16_t len);
bool dgm_checkCRC(const uint8_t *rxBuff, uint16_t len);

#endif
