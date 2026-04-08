// ─────────────────────────────────────────────────────────────────────────────
// alarms.h — Mapeo de Alarmas IoT/Medidor → IDs de Alarma EAAB
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>
#include "uwm.h"

// ── Tabla de Mapeo Confirmada ─────────────────────────────────────────────────
// | Fuente                     | Máscara / Flag              | ID EAAB | Descripción            |
// |----------------------------|-----------------------------|---------|------------------------|
// | Medidor (bit 9)            | alarms & 0x0200             | 1       | Contraflujo            |
// | Medidor (bit 12)           | alarms & 0x1000             | 6       | Sub caudal             |
// | Medidor (bit 11)           | alarms & 0x0800             | 7       | Sobre caudal           |
// | Medidor (bit 13)           | alarms & 0x2000             | 9       | Batería Baja Medidor   |
// | battery_low_flag (IoT)     | vbat_mv < BATTERY_ALARM_TRESH | 10    | Batería Baja Transmisor|
// | !currentMeterData.valid    | comm fail RS485             | 11      | Cable Desconectado     |

#define MAX_ALARM_ENTRIES 12

// Estructura de una entrada de alarma
struct AlarmEntry {
    uint8_t id;     // ID de alarma EAAB (1-12)
    bool    active; // true si la alarma está activa
};

// ─────────────────────────────────────────────────────────────────────────────
// build_alarm_list: Evalúa las condiciones y carga las alarmas activas.
//   list[]      : Array de salida (usar MAX_ALARM_ENTRIES de tamaño)
//   count       : Número de entradas cargadas (salida)
//   data        : Datos del medidor leídos por RS485
//   battery_low : Bandera de batería baja del módulo IoT
// ─────────────────────────────────────────────────────────────────────────────
inline void build_alarm_list(AlarmEntry* list, uint8_t& count,
                              const MeterData& data, bool battery_low) {
    count = 0;

    // ID 1 — Contraflujo: bit 9 de la palabra de alarmas
    list[count++] = { 1,  data.valid && (data.alarms & 0x0200) };

    // ID 6 — Sub caudal: bit 12
    list[count++] = { 6,  data.valid && (data.alarms & 0x1000) };

    // ID 7 — Sobre caudal: bit 11
    list[count++] = { 7,  data.valid && (data.alarms & 0x0800) };

    // ID 9 — Batería Baja Medidor: bit 13
    list[count++] = { 9,  data.valid && (data.alarms & 0x2000) };

    // ID 10 — Batería Baja Transmisor (módulo IoT)
    list[count++] = { 10, battery_low };

    // ID 11 — Cable Medidor Desconectado: fallo RS485
    list[count++] = { 11, !data.valid };
}
