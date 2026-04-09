#pragma once
#include <Arduino.h>

// ── Control de Debug Global ──────────────────────────────────────────────────
extern bool app_debug_enabled;
extern bool at_debug_enabled;

#define LOG_SERIAL_PRINTF(...) if (app_debug_enabled) Serial.printf(__VA_ARGS__)
#define LOG_SERIAL_PRINTLN(...) if (app_debug_enabled) Serial.println(__VA_ARGS__)
#define LOG_SERIAL_PRINT(...) if (app_debug_enabled) Serial.print(__VA_ARGS__)

// ── Variables de Configuración Globales ─────────────────────────────────────

// Servidor y Conexión
extern const char* server_name;
extern const int   server_port;
extern const bool  use_ssl;
extern const char* apn;

// Credenciales HTTP
extern const char* http_user_name;
extern const char* http_password;

// Rutas de API
extern const char* http_auth_path;
extern const char* http_test_post_path;

// Llaves de Encriptación AES-256-CBC
extern const char* http_encrypt_key;
extern const char* http_encrypt_iv;

// Variables dinámicas
extern String http_token;

// Variables de prueba para API
extern String meterSerialTest;
extern String meterIndexTest;     // Lectura del medidor (ej. "000012345.678")
extern String meterDateTimeTest;  // Fecha/hora del medidor en ISO 8601 ("" = timestamp actual)

// ── Configuración de Hardware (Pines) para ESP32-S3 + SIM7080G ──────────────
// UART al módulo SIM7080G (UART1)
#define PIN_TX      17  // ESPC_TX
#define PIN_RX      18  // ESPC_RX

// Control de energía del módulo SIM7080G
#define CAT_EN      15  // CAT_EN
#define PWRKEY_PIN  16  // CAT_KEY

// RS485 / Medidor Digital (UART2)
#define DGMC_TX     10
#define DGMC_RX     12
#define RS485_EN    11

// Periféricos y Sensores
#define HALL_PIN        1   // Sensor de Efecto Hall (Wake-up)
#define BT_EN_PIN       3   // Habilitación modo configuración
#define BOOST_EN_PIN    4   // Habilitación elevador 3.6V a 12V
#define LED_PIN         5   // LED de test, 0=ON, 1=OFF
#define BOOT_PIN        0   // Pin de Boot (Strapping)

// ── Funciones de Configuración ──────────────────────────────────────────────
void init_config();

// ── Parámetros de Alarma ─────────────────────────────────────────────────────
#define BATTERY_ALARM_TRESH 3000           // Umbral de alarma de batería en mV (3.0V)
