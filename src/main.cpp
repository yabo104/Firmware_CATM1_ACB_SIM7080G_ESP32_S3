#include <Arduino.h>
#include <TinyGsmClient.h>
#include <StreamDebugger.h>
#include <WiFi.h>          // Para deshabilitar WiFi
#include "iot_api.h"
#include "iot_config.h"
#include "uwm.h"
#include "alarms.h"
#include "esp_sleep.h"
#include "esp_bt.h"        // Para deshabilitar controlador BLE
#include "driver/gpio.h"   // Para configurar pines libres

// ── Objetos Globales ────────────────────────────────────────────────────────
ProxyStream   modemProxy(Serial1);
StreamDebugger* debugger = nullptr;
TinyGsm modem(modemProxy);
MeterData currentMeterData;

// ── Prototipos de funciones locales ─────────────────────────────────────────
void power_init();
void setup_io();
void read_meter_data();
void go_to_deep_sleep();
void print_wakeup_reason();

// ────────────────────────────────────────────────────────────────────────────
// ── Variables de Alarma ──────────────────────────────────────────────────────
bool battery_low_flag     = false;
bool meter_comm_fail_flag = false;
uint16_t vbat_mv          = 0;

void setup() {
    // 1. Inicializar Serial Debug (USB Nativo)
    Serial.begin(115200);
    delay(3000); 
    Serial.println("\n=== ESP32-S3 SIM7080G — MONITOR INICIADO ===");
    
    print_wakeup_reason();

    // 2. Deshabilitar radios no utilizados (WiFi/BT)
    power_init();

    // 3. Configurar Pines de Hardware y UARTs
    setup_io();
    dgm_init();

    // 4. Lectura del Medidor RS485 (Protocolo Propio)
    read_meter_data();
    // Liberar UART RS485 para ahorrar energía
    Serial0.end();
    delay(500);

    // 4. UART al SIM7080G (ESPC_TX=8, ESPC_RX=10)
    Serial1.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);

    // 5. Encender módulo SIM7080G
    encenderModuloSIM();
    delay(2000);

    // Habilitar debug AT si está configurado
    if (at_debug_enabled) {
        debugger = new StreamDebugger(Serial1, Serial);
        modemProxy.setStream(*debugger);
    }

    // 6. Inicializar módem y conectar
    LOG_SERIAL_PRINT("Waiting for modem ... ");
    if (!modem.init()) {
        LOG_SERIAL_PRINTLN("ERROR — reiniciando.");
        delay(5000);
        ESP.restart();
    }
    LOG_SERIAL_PRINTLN("OK");

    modem.setNetworkMode(38); // LTE only
    modem.setPreferredMode(1); // CAT-M

    LOG_SERIAL_PRINT("Waiting for network ... ");
    if (!modem.waitForNetwork(120000L)) {
        LOG_SERIAL_PRINTLN("timeout");
    } else {
        LOG_SERIAL_PRINTLN("done");

        LOG_SERIAL_PRINT("Connecting to APN ... ");
        if (!modem.gprsConnect(apn, "", "")) {
            LOG_SERIAL_PRINTLN("fail");
        } else {
            LOG_SERIAL_PRINTLN("connected");

            // 7. Sincronizar Tiempo NTP (Necesario para SSL)
            LOG_SERIAL_PRINT("Syncing NTP Time ... ");
            if (modem.NTPServerSync("pool.ntp.org", -20)) { // -20 cuartos = -5h Colombia
                LOG_SERIAL_PRINTLN("OK");
            } else {
                LOG_SERIAL_PRINTLN("fail");
            }

            // 8. Monitoreo de Batería
            vbat_mv = api_get_battery_voltage();
            if (vbat_mv > 0 && vbat_mv < BATTERY_ALARM_TRESH) {
                battery_low_flag = true;
                LOG_SERIAL_PRINTLN("[ ALARM ] ¡Batería Baja detectada!");
            }
            LOG_SERIAL_PRINTF("[ INFO ] Battery :  [mV] %d\n", vbat_mv);

            // 9. Flujo HTTPS (Get Token + Save Index)
            // Reintento en caso de fallo SSL transitorio (error -3)
            for (uint8_t attempt = 1; attempt <= 2 && http_token.length() == 0; attempt++) {
                LOG_SERIAL_PRINTF("[ HTTPS ] Obteniendo token (intento %d/2)...\n", attempt);
                http_get_token_7080(http_auth_path, http_user_name, http_password);
                if (http_token.length() == 0 && attempt < 2) {
                    LOG_SERIAL_PRINTLN("[ HTTPS ] Reintentando en 3 segundos...");
                    delay(3000);
                }
            }

            if (http_token.length() > 0) {
                // 8.1 Registrar Serial del Medidor
                LOG_SERIAL_PRINTLN("[ HTTPS ] Verificando/Registrando Serial...");
                api_register_meter_serial(meterSerialTest);

                String dtApi = get_meter_datetime_7080();

                // 8.2 Guardar Lectura — solo si la comunicación RS485 fue exitosa
                if (currentMeterData.valid) {
                    uint32_t accFlowInt = (uint32_t)currentMeterData.accFlow;
                    String indexToSend = String(accFlowInt);
                    LOG_SERIAL_PRINTF("[ INFO ] Enviando lectura real: %s m3 (entero)\n", indexToSend.c_str());
                    LOG_SERIAL_PRINTLN("[ HTTPS ] Enviando lectura...");
                    api_save_index(meterSerialTest, indexToSend, dtApi);
                } else {
                    LOG_SERIAL_PRINTLN("[ INFO ] Comunicación RS485 fallida — omitiendo SaveIndex.");
                }

                // 8.3 Evaluar y enviar alarmas activas (siempre, incluso sin lectura)
                LOG_SERIAL_PRINTLN("[ HTTPS ] Evaluando alarmas...");
                AlarmEntry alarmList[MAX_ALARM_ENTRIES];
                uint8_t alarmCount = 0;
                build_alarm_list(alarmList, alarmCount, currentMeterData, battery_low_flag);
                send_active_alarms(alarmList, alarmCount, meterSerialTest, dtApi);
            }
            modem.gprsDisconnect();
        }
    }

    // 8. Apagar y dormir
    apagarModuloSIM();
    go_to_deep_sleep();
}

void loop() {
    // No se usa
}

// ── Implementaciones Locales ────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// power_init: Deshabilita radios y configura pines libres para bajo consumo
// ─────────────────────────────────────────────────────────────────────────────
void power_init() {
    // 1. Deshabilitar WiFi (no se usa en este proyecto)
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    // 2. Deshabilitar controlador BT/BLE
    esp_bt_controller_disable();
    esp_bt_mem_release(ESP_BT_MODE_BTDM);

    // 3. Configurar pines libres (sin función) como INPUT + PULLDOWN
    // para evitar corrientes de fuga por pines flotantes.
    // Pines libres en ESP32-S3 según la tabla.
    const gpio_num_t free_pins[] = { GPIO_NUM_2, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_13, GPIO_NUM_14 };
    for (gpio_num_t pin : free_pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    }

    LOG_SERIAL_PRINTLN("[ POWER ] WiFi/BT deshabilitados. Pines libres configurados.");
}

void setup_io() {
    LOG_SERIAL_PRINTLN("[ INIT ] Configurando GPIOs...");
    
    pinMode(BOOST_EN_PIN, OUTPUT);
    pinMode(RS485_EN,     OUTPUT);
    pinMode(BT_EN_PIN,    OUTPUT);
    pinMode(LED_PIN,      OUTPUT);
    pinMode(HALL_PIN,     INPUT_PULLUP);

    digitalWrite(BOOST_EN_PIN, LOW); // Apagado por defecto
    digitalWrite(RS485_EN,     LOW); // Recepción por defecto
    digitalWrite(BT_EN_PIN,    LOW);
    digitalWrite(LED_PIN,      HIGH); // LED apagado con 1 lógico
}

void read_meter_data() {
    LOG_SERIAL_PRINTLN("[ RS485 ] Iniciando etapa de lectura...");
    
    // 1. Activar el elevador de voltaje (12V)
    digitalWrite(BOOST_EN_PIN, HIGH);
    delay(1000); // 1 segundo para estabilizar fuente y medidor

    // 2. Realizar sondeo (polling)
    if (dgm_read(currentMeterData, 3000)) {
        LOG_SERIAL_PRINTLN("[ RS485 ] Lectura EXITOSA:");
        LOG_SERIAL_PRINTF("  > Flujo Inst:  %.2f m3/h\n", currentMeterData.flowRate);
        LOG_SERIAL_PRINTF("  > Acumulado:   %.2f m3\n",   currentMeterData.accFlow);
        LOG_SERIAL_PRINTF("  > Presion:     %.3f Mpa\n",  currentMeterData.pressure);
        LOG_SERIAL_PRINTF("  > Temperatura: %d C\n",      currentMeterData.temperature);
        LOG_SERIAL_PRINTF("  > Alarmas:     0x%04X\n",    currentMeterData.alarms);
        meter_comm_fail_flag = false;
    } else {
        LOG_SERIAL_PRINTLN("[ RS485 ] FALLO en la comunicación con el medidor.");
        meter_comm_fail_flag = true;
    }

    // 3. Apagar elevador para ahorrar energía
    digitalWrite(BOOST_EN_PIN, LOW);
}

void go_to_deep_sleep() {
    LOG_SERIAL_PRINTLN("[ SLEEP ] Entrando en Deep Sleep (12 horas)...");
    
    // Aislar pines libres para reducir consumo en deep sleep
    const gpio_num_t free_pins[] = { GPIO_NUM_2, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_13, GPIO_NUM_14 };
    for (gpio_num_t pin : free_pins) {
        gpio_hold_dis(pin);
    }
    esp_sleep_config_gpio_isolate();  // Aisla todos los GPIO no-RTC durante el sleep

    uint64_t sleep_time_us = 1ULL * 3600ULL * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleep_time_us);

    // Wakeup por Hall Sensor (GPIO 1)
    esp_deep_sleep_enable_gpio_wakeup(1 << HALL_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

    Serial.flush();
    esp_deep_sleep_start();
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_GPIO : LOG_SERIAL_PRINTLN("[ WAKEUP ] Hall Sensor Detectado"); break;
        case ESP_SLEEP_WAKEUP_TIMER: LOG_SERIAL_PRINTLN("[ WAKEUP ] Timer Cíclico"); break;
        default : LOG_SERIAL_PRINTLN("[ WAKEUP ] Inicio en frío / Reset"); break;
    }
}
