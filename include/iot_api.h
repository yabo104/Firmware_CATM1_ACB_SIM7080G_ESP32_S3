#pragma once
#include <Arduino.h>
#include <TinyGsmClient.h>
#include "iot_config.h"
#include "alarms.h"

// ── Stream con debug AT ─────────────────────────────────────────────────────
class ProxyStream : public Stream {
  private:
    Stream* _stream;
  public:
    ProxyStream(Stream& s) : _stream(&s) {}
    void setStream(Stream& s) { _stream = &s; }
    int    available()    override { return _stream->available(); }
    int    read()         override { return _stream->read(); }
    int    peek()         override { return _stream->peek(); }
    void   flush()        override { _stream->flush(); }
    size_t write(uint8_t c) override { return _stream->write(c); }
    size_t write(const uint8_t* buf, size_t sz) override { return _stream->write(buf, sz); }
};

extern ProxyStream modemProxy;
extern TinyGsm modem;

// ── Funciones de Control de Energía ─────────────────────────────────────────
void encenderModuloSIM();
void apagarModuloSIM();

// ── Funciones de Tiempo ─────────────────────────────────────────────────────
String get_timestamp_7080();       // ISO 8601  — para timestamp del payload
String get_meter_datetime_7080();  // yyyyMMddHHmmss — para meterDateTime en SaveIndex

// ── Funciones de Encriptación ────────────────────────────────────────────────
String aes_256_cbc_encrypt(String plaintext);
String urlEncode(const char* msg);

// ── Funciones de HTTP ────────────────────────────────────────────────────────
String http_get_token_7080(const char* auth_path, const char* user_name, const char* password);
bool   http_post_data_7080(const char* subPath, String payload);
String http_get_data_7080(const char* subPath);

// ── Métodos API Telemetría EAAB ─────────────────────────────────────────────
bool   api_register_meter_serial(String meterSerial);
bool   api_save_index(String meterSerial, String meterIndex, String meterDateTime);
bool   api_save_alarm(String jsonAlarmData);
bool   api_save_batch(String jsonBatchData);
String api_get_indexs_by_meter(String meterSerial);
String api_get_meters_by_provider();
bool   api_encrypt_data_test(String plainData);
uint16_t api_get_battery_voltage();

// ── Envío de Alarmas Activas ─────────────────────────────────────────────────
// Itera el array de alarmas y llama api_save_alarm() por cada una activa.
void send_active_alarms(const AlarmEntry* list, uint8_t count,
                        const String& meterSerial, const String& alarmDateTime);
