#include "uwm.h"
#include "iot_config.h"

// Comandos fijos (Modbus RTU)
static const uint8_t acc_command[8] = {0x00, 0x03, 0x00, 0x00, 0x00, 0x09, 0x84, 0x1D};

bool dgm_init() {
    // Inicializar UART0 (Pins 20, 21 por defecto en C3)
    Serial0.begin(9600, SERIAL_8N1, DGMC_RX, DGMC_TX);
    return true;
}

bool dgm_read(MeterData &data, uint32_t timeout_ms) {
    uint8_t dgrx[24];
    memset(dgrx, 0, sizeof(dgrx));
    data.valid = false;

    // Limpiar buffer de entrada
    while (Serial0.available() > 0) Serial0.read();

    // Transmitir comando
    digitalWrite(RS485_EN, HIGH); // Habilitar TX
    delay(5);
    Serial0.write(acc_command, 8);
    Serial0.flush(); // Esperar a que se envíe todo
    delay(5);
    digitalWrite(RS485_EN, LOW); // Habilitar RX

    // Leer respuesta
    uint32_t startTime = millis();
    uint8_t index = 0;
    while (millis() - startTime < timeout_ms && index < 23) {
        if (Serial0.available() > 0) {
            dgrx[index++] = Serial0.read();
        }
    }

    // Verificar respuesta
    if (index < 23) {
        LOG_SERIAL_PRINTLN("[ UWM ] Error: Timeout o trama incompleta.");
        return false;
    }

    if (!dgm_checkCRC(dgrx, index)) {
        LOG_SERIAL_PRINTLN("[ UWM ] Error: Fallo de CRC.");
        return false;
    }

    // Parsear datos (Basado en el algoritmo del usuario)
    // Factores de conversión
    const float KFLOW    = 0.1f;
    const float KACCFLOW = 0.1f;
    const float KPRESS   = 0.001f;

    data.flowRate    = (float)((uint32_t)dgrx[3]<<24 | (uint32_t)dgrx[4]<<16 | (uint32_t)dgrx[5]<<8 | dgrx[6]) * KFLOW;
    data.accFlow     = (float)((uint32_t)dgrx[7]<<24 | (uint32_t)dgrx[8]<<16 | (uint32_t)dgrx[9]<<8 | dgrx[10]) * KACCFLOW;
    data.accFlowInv  = (float)((uint32_t)dgrx[11]<<24 | (uint32_t)dgrx[12]<<16 | (uint32_t)dgrx[13]<<8 | dgrx[14]) * KACCFLOW;
    data.pressure    = (float)((uint16_t)dgrx[17]<<8 | dgrx[18]) * KPRESS;
    data.information = dgrx[15];
    data.temperature = dgrx[16];
    data.alarms      = (uint16_t)dgrx[19]<<8 | dgrx[20];
    data.valid       = true;

    return true;
}

uint16_t dgm_calculateCRC(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if ((crc & 0x0001) != 0) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool dgm_checkCRC(const uint8_t *rxBuff, uint16_t len) {
    if (len < 3) return false;
    uint16_t crcCalc = dgm_calculateCRC(rxBuff, len - 2);
    uint8_t hRcrc = crcCalc & 0xFF;
    uint8_t lRcrc = crcCalc >> 8;
    
    return (rxBuff[len - 2] == hRcrc && rxBuff[len - 1] == lRcrc);
}
