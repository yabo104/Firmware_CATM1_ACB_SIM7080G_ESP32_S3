#include "iot_config.h"

bool app_debug_enabled = true;   // Logs de la aplicación
bool at_debug_enabled  = false;  // Tráfico AT puro — desactivado (comunicación verificada)

// ── Variables de Configuración Globales ─────────────────────────────────────

// Servidor y Conexión
const char* server_name = "wsp.acueducto.com.co";
const int   server_port = 443;
const bool  use_ssl     = true;
const char* apn         = "alai";

// Credenciales HTTP
const char* http_user_name = "cofemetrex";
const char* http_password  = "jUr#BT3^2Pu#lHsAl$qU";

// Rutas de API
const char* http_auth_path      = "miatelemetryapitest/api/Auth/GetToken";
const char* http_test_post_path = "miatelemetryapitest/api/EncryptDataTest/EncryptData";

// Llaves de Encriptación AES-256-CBC
const char* http_encrypt_key = "snHZjFmUkvD4BfmvdsyPcb3TptXWQvvpGWGSasv3Y3g=";
const char* http_encrypt_iv  = "sAxi0e7BvpE9oR6WdfOlew==";

// Variables dinámicas
String http_token = "";

// Variables de prueba para API
String meterSerialTest   = "serial1320001";
String meterIndexTest    = "010";        // Lectura del medidor (entero, sin decimales) — modificar para pruebas
String meterDateTimeTest = "";           // vacío = usa timestamp del módem (yyyyMMddHHmmss)

void init_config() {
    // Reservado para carga desde NVS / SPIFFS
}
