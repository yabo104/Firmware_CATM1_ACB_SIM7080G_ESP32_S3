// ─────────────────────────────────────────────────────────────────────────────
// iot_api.cpp  –  SIM7080G CAT-M1 HTTP Client
// Usa TinyGSM + ArduinoHttpClient para HTTPS con headers Authorization Bearer
// ─────────────────────────────────────────────────────────────────────────────
#include "iot_api.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include <base64.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// ─────────────────────────────────────────────────────────────────────────────
// Control de Energía SIM7080G
// ─────────────────────────────────────────────────────────────────────────────
void encenderModuloSIM() {
    Serial.println("\n=== [BOOT] Encendiendo módulo SIM7080G ===");
    pinMode(CAT_EN,     OUTPUT);
    pinMode(PWRKEY_PIN, OUTPUT);

    digitalWrite(CAT_EN, LOW);
    digitalWrite(PWRKEY_PIN, LOW);
    delay(1000);

    digitalWrite(CAT_EN, HIGH);
    delay(100);

    // Pulso en PWRKEY para encender el SIM7080G
    digitalWrite(PWRKEY_PIN, HIGH);
    delay(1000);
    digitalWrite(PWRKEY_PIN, LOW);
    delay(2000);
    digitalWrite(PWRKEY_PIN, HIGH);

    Serial.println("[STATE] Alimentación activada, esperando arranque...");
    delay(8000);
    Serial.println("[STATE] Módulo debería estar listo.");
}

void apagarModuloSIM() {
    Serial.println("\n=== [POWER DOWN] Apagando módulo SIM7080G ===");
    // Pulso de apagado PWRKEY
    digitalWrite(PWRKEY_PIN, LOW);
    delay(2600);
    digitalWrite(PWRKEY_PIN, HIGH);
    delay(3000);
    digitalWrite(CAT_EN, LOW);
    Serial.println("[STATE] Módulo apagado correctamente.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Encriptación AES-256-CBC con Padding PKCS7
// ─────────────────────────────────────────────────────────────────────────────
String aes_256_cbc_encrypt(String plaintext) {
    Serial.println("[ AES ] Texto plano: " + plaintext);
    mbedtls_aes_context aes;

    size_t dlen = 0;
    unsigned char key[32];
    mbedtls_base64_decode(key, 32, &dlen, (const unsigned char*)http_encrypt_key, strlen(http_encrypt_key));

    unsigned char iv[16];
    mbedtls_base64_decode(iv, 16, &dlen, (const unsigned char*)http_encrypt_iv, strlen(http_encrypt_iv));

    int len = plaintext.length();
    int n_blocks  = (len / 16) + 1;
    int n_padding = (n_blocks * 16) - len;
    size_t input_len = n_blocks * 16;

    unsigned char* input  = (unsigned char*)malloc(input_len);
    unsigned char* output = (unsigned char*)malloc(input_len);
    memcpy(input, plaintext.c_str(), len);
    for (int i = 0; i < n_padding; i++) input[len + i] = (unsigned char)n_padding;

    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 256);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, input_len, iv, input, output);
    mbedtls_aes_free(&aes);

    String encoded = base64::encode(output, input_len);
    Serial.println("[ AES ] Base64 Cipher: " + encoded);

    free(input);
    free(output);
    return encoded;
}

// ─────────────────────────────────────────────────────────────────────────────
// URL Encode
// ─────────────────────────────────────────────────────────────────────────────
String urlEncode(const char* msg) {
    const char* hex = "0123456789ABCDEF";
    String encodedMsg = "";
    while (*msg != '\0') {
        if (('a' <= *msg && *msg <= 'z') ||
            ('A' <= *msg && *msg <= 'Z') ||
            ('0' <= *msg && *msg <= '9') ||
            *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~') {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Timestamp ISO 8601 desde el RTC del módem
// ─────────────────────────────────────────────────────────────────────────────
String get_timestamp_7080() {
    int year, month, day, hour, minute, second;
    float timezone;
    if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone)) {
        char buf[40];
        int tz_h = (int)timezone;
        int tz_m = abs((int)((timezone - tz_h) * 60));
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%+03d:%02d",
                 year, month, day, hour, minute, second, tz_h, tz_m);
        return String(buf);
    }
    return "2025-02-03T09:29:55-05:00"; // Fallback
}

// ─────────────────────────────────────────────────────────────────────────────
// Timestamp formato yyyyMMddHHmmss — requerido por SaveIndex y SaveAlarm
// Ejemplo: "20260317161730"
// ─────────────────────────────────────────────────────────────────────────────
String get_meter_datetime_7080() {
    int year, month, day, hour, minute, second;
    float timezone;
    if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &timezone)) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d",
                 year, month, day, hour, minute, second);
        return String(buf);
    }
    return "20250203092955"; // Fallback
}

// ─────────────────────────────────────────────────────────────────────────────
// Payload encriptado para la API
// ─────────────────────────────────────────────────────────────────────────────
static String build_telemetry_payload(String rawData) {
    String timestamp   = get_timestamp_7080();
    String encData     = aes_256_cbc_encrypt(rawData);
    String payload = "{";
    payload += "\"timestamp\":\"" + timestamp + "\",";
    payload += "\"showErrorDetails\":true,";
    payload += "\"data\":\"" + encData + "\"";
    payload += "}";
    return payload;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: realizar petición HTTPS (Token o POST de datos)
// ArduinoHttpClient con TinyGsm soporta HTTPS nativo y addHeader ilimitado
// ─────────────────────────────────────────────────────────────────────────────
static bool http_do_request(const char* method, const char* fullPath,
                             const char* contentType, const String& body,
                             String& responseBody, int& httpCode) {
    httpCode = -1;
    responseBody = "";

    // Cliente SSL seguro sobre el modem SIM7080G
    TinyGsmClientSecure sslClient(modem);
    HttpClient http(sslClient, server_name, server_port);
    http.setHttpResponseTimeout(30000); // 30 s de espera para respuesta

    LOG_SERIAL_PRINTF("[ HTTP ] %s https://%s%s\n", method, server_name, fullPath);

    int ret;
    if (strcmp(method, "POST") == 0) {
        http.beginRequest();
        http.post(fullPath);
        http.sendHeader("Content-Type", contentType);
        http.sendHeader("Content-Length", body.length());
        if (http_token.length() > 0) {
            http.sendHeader("Authorization", ("Bearer " + http_token).c_str());
            LOG_SERIAL_PRINTF("[ HTTP ] Authorization: Bearer %s...\n", http_token.substring(0, 20).c_str());
        }
        http.sendHeader("Accept", "*/*");
        http.sendHeader("Connection", "close");
        http.endRequest();
        http.print(body);
    } else {
        http.beginRequest();
        http.get(fullPath);
        if (http_token.length() > 0) {
            http.sendHeader("Authorization", ("Bearer " + http_token).c_str());
        }
        http.sendHeader("Accept", "*/*");
        http.sendHeader("Connection", "close");
        http.endRequest();
    }

    ret = http.responseStatusCode();
    httpCode = ret;

    if (ret > 0) {
        http.skipResponseHeaders();
        responseBody = http.responseBody();
        LOG_SERIAL_PRINTF("[ HTTP ] Status: %d | Body: %s\n", httpCode, responseBody.c_str());
    } else {
        LOG_SERIAL_PRINTF("[ HTTP ] Error HTTP: %d\n", ret);
    }

    http.stop();
    return (httpCode >= 200 && httpCode < 300);
}

// ─────────────────────────────────────────────────────────────────────────────
// Obtener Token HTTP
// ─────────────────────────────────────────────────────────────────────────────
String http_get_token_7080(const char* auth_path, const char* user_name, const char* password) {
    LOG_SERIAL_PRINTLN("[ HTTP ] Obteniendo Token...");

    String body = "name=" + urlEncode(user_name) + "&key=" + urlEncode(password);
    String fullPath = "/";
    fullPath += auth_path;

    String responseBody;
    int httpCode;
    http_do_request("POST", fullPath.c_str(), "application/x-www-form-urlencoded", body, responseBody, httpCode);

    LOG_SERIAL_PRINTF("[ HTTP ] Token Response (%d): %s\n", httpCode, responseBody.c_str());

    if (httpCode >= 200 && httpCode < 300) {
        // Extraer access_token del JSON
        int idx = responseBody.indexOf("\"access_token\":\"");
        if (idx == -1) idx = responseBody.indexOf("\"accessToken\":\"");
        if (idx == -1) idx = responseBody.indexOf("\"token\":\"");

        if (idx != -1) {
            int start = responseBody.indexOf(":", idx) + 2;
            int end   = responseBody.indexOf("\"", start);
            http_token = responseBody.substring(start, end);
            LOG_SERIAL_PRINTLN("[ HTTP ] Token guardado correctamente.");
        } else if (!responseBody.startsWith("{")) {
            http_token = responseBody;
            http_token.trim();
        }
    }

    return "{\"status\":\"Token\",\"code\":" + String(httpCode) + "}";
}

// ─────────────────────────────────────────────────────────────────────────────
// POST genérico con Authorization Bearer (funciona porque ArduinoHttpClient
// no tiene límite de tamaño de header como AT+HTTPPARA)
// ─────────────────────────────────────────────────────────────────────────────
bool http_post_data_7080(const char* subPath, String payload) {
    LOG_SERIAL_PRINTF("[ HTTP ] Enviando POST a PATH: %s\n", subPath);

    String fullPath = "/";
    fullPath += subPath;

    String responseBody;
    int httpCode;
    bool ok = http_do_request("POST", fullPath.c_str(), "application/json", payload, responseBody, httpCode);

    LOG_SERIAL_PRINTF("[ HTTP ] Resultado FINAL (%d): %s\n", httpCode, responseBody.c_str());

    if (httpCode == 401) {
        http_token = "";
        LOG_SERIAL_PRINTLN("[ HTTP ] Token rechazado (401) - Limpiando para refrescar.");
    }

    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET genérico
// ─────────────────────────────────────────────────────────────────────────────
String http_get_data_7080(const char* subPath) {
    LOG_SERIAL_PRINTF("[ HTTP ] GET %s\n", subPath);

    String fullPath = "/";
    fullPath += subPath;

    String responseBody;
    int httpCode;
    http_do_request("GET", fullPath.c_str(), "", "", responseBody, httpCode);

    String result = "{";
    result += "\"status\":\"Consulta\",";
    result += "\"code\":" + String(httpCode) + ",";
    result += "\"response\":" + (responseBody.startsWith("{") ? responseBody : "\"" + responseBody + "\"");
    result += "}";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// API: RegisterMeterSerial
// ─────────────────────────────────────────────────────────────────────────────
bool api_register_meter_serial(String meterSerial) {
    String rawData  = "{\"meterSerial\":\"" + meterSerial + "\"}";
    String payload  = build_telemetry_payload(rawData);
    return http_post_data_7080("miatelemetryapitest/api/MiaTelemetryMeter/RegisterMeterSerial", payload);
}

// ─────────────────────────────────────────────────────────────────────────────
// API: SaveIndex
// ─────────────────────────────────────────────────────────────────────────────
bool api_save_index(String meterSerial, String meterIndex, String meterDateTime) {
    String rawData = "{";
    rawData += "\"meterSerial\":\"" + meterSerial + "\",";
    rawData += "\"meterIndex\":\"" + meterIndex + "\",";
    rawData += "\"meterDateTime\":\"" + meterDateTime + "\"";
    rawData += "}";
    String payload = build_telemetry_payload(rawData);
    return http_post_data_7080("miatelemetryapitest/api/MiaTelemetryIndex/SaveIndex", payload);
}

// ─────────────────────────────────────────────────────────────────────────────
// API: SaveAlarm
// ─────────────────────────────────────────────────────────────────────────────
bool api_save_alarm(String jsonAlarmData) {
    String payload = build_telemetry_payload(jsonAlarmData);
    return http_post_data_7080("miatelemetryapitest/api/MiaTelemetryAlarm/SaveAlarm", payload);
}

// ─────────────────────────────────────────────────────────────────────────────
// API: SaveBatch
// ─────────────────────────────────────────────────────────────────────────────
bool api_save_batch(String jsonBatchData) {
    String payload = build_telemetry_payload(jsonBatchData);
    return http_post_data_7080("miatelemetryapitest/api/MiaTelemetryBatch/SaveBatch", payload);
}

// ─────────────────────────────────────────────────────────────────────────────
// API: GetIndexsByMeter
// ─────────────────────────────────────────────────────────────────────────────
String api_get_indexs_by_meter(String meterSerial) {
    String path = "miatelemetryapitest/api/MiaTelemetryReport/GetIndexsByMeter?meterSerial=" + meterSerial;
    return http_get_data_7080(path.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// API: GetMetersByProvider
// ─────────────────────────────────────────────────────────────────────────────
String api_get_meters_by_provider() {
    return http_get_data_7080("miatelemetryapitest/api/MiaTelemetryReport/GetMetersByProvider");
}

// ─────────────────────────────────────────────────────────────────────────────
// API: EncryptDataTest
// ─────────────────────────────────────────────────────────────────────────────
bool api_encrypt_data_test(String plainData) {
    String payload = build_telemetry_payload(plainData);
    return http_post_data_7080("miatelemetryapitest/api/EncryptDataTest/EncryptData", payload);
}

// ─────────────────────────────────────────────────────────────────────────────
// Leer Voltaje de Batería (+CBC)
// ─────────────────────────────────────────────────────────────────────────────
extern ProxyStream modemProxy; // Declarado en main.cpp

uint16_t api_get_battery_voltage() {
    modem.sendAT("+CBC");
    if (modem.waitResponse(1000, "+CBC: ") != 1) {
        return 0;
    }

    // Respuesta esperada: <bcs>,<bcl>,<voltage>
    // Ejemplo: +CBC: 0,100,4200
    // parseInt() salta automáticamente separadores no numéricos como comas
    int bcs = modemProxy.parseInt(); 
    int bcl = modemProxy.parseInt();
    int mv  = modemProxy.parseInt();
    
    LOG_SERIAL_PRINTF("[ MODEM ] Voltaje Batería: %d mV (Carga: %d%%)\n", mv, bcl);
    
    modem.waitResponse(); // Consume el OK final
    return (uint16_t)mv;
}

// ─────────────────────────────────────────────────────────────────────────────
// Enviar Alarmas Activas
// Itera el array de AlarmEntry y hace POST por cada alarma activa.
// ─────────────────────────────────────────────────────────────────────────────
void send_active_alarms(const AlarmEntry* list, uint8_t count,
                        const String& meterSerial, const String& alarmDateTime) {
    uint8_t sent = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (!list[i].active) continue;

        // Construir JSON con los campos que espera api_save_alarm
        String rawData = "{";
        rawData += "\"meterSerial\":\"" + meterSerial + "\",";
        rawData += "\"alarmId\":"  + String(list[i].id) + ",";
        rawData += "\"meterDateTime\":\"" + alarmDateTime + "\"";
        rawData += "}";

        LOG_SERIAL_PRINTF("[ ALARM ] Enviando alarma ID %d...\n", list[i].id);
        bool ok = api_save_alarm(rawData);
        LOG_SERIAL_PRINTF("[ ALARM ] Alarma ID %d: %s\n", list[i].id, ok ? "OK" : "FAIL");
        sent++;
    }
    if (sent == 0) {
        LOG_SERIAL_PRINTLN("[ ALARM ] Sin alarmas activas.");
    }
}
