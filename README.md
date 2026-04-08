# Firmware CAT-M1 ACB SIM7080G (ESP32-S3)

Este repositorio contiene el firmware para el dispositivo de telemetría e IoT basado en el microcontrolador de alto rendimiento **ESP32-S3-WROOM-1-N16R8** (16MB Flash, 8MB PSRAM Octal) y el módem **SIM7080G** (conectividad CAT-M1).

## Funcionalidades Funcionales
1. **Conectividad M2M:** Inicialización y conexión robusta a la red celular CAT-M1 mediante APN usando TinyGSM.
2. **Sincronización NTP:** Obtención de la hora de red directamente desde el módem celular para generar timestamps (ISO 8601 y formato API `yyyyMMddHHmmss`).
3. **Lectura Modbus RS485:** Interrogación y decodificación de tramas de un medidor digital de agua, capturando acumulados, flujo, presión, temperatura y bits de error físicos.
4. **Manejo de Alarmas Completo:** Captura y mapeo de banderas de alerta interna (falta de comunicación, batería sub-umbral de 3.0V) junto con alarmas físicas del medidor digital (fuga, flujo inverso, batería baja interna del medidor, fraude magnético) mediante tablas cruzadas de 12 IDs requeridas por EAAB.
5. **Seguridad End-to-End (AES):** Encriptación AES-256-CBC local del payload en formato HEX antes de emitir a la API externa.
6. **Integración API REST EAAB:**
   - **Autenticación (JWT):** Peticiones HTTP POST para obtener tokens temporales.
   - **RegisterMeterSerial:** Registro de inicio e inventario del serial del medidor físico.
   - **SaveIndex:** Transmisión de índices de volumen al servidor en la nube.
   - **SaveAlarm:** Envío unitario de las alarmas activas usando endpoints seguros.
7. **Control de Bajo Consumo (Deep Sleep):** Gestión en el que se deshabilitan radios integrados (WiFi/BT) y control discreto del elevador DC/DC de 12V, aislando pines físicos para entrar en rutinas prolongadas de repetición prolongando la batería de manera óptima.

## Tabla de Mapeo de Pines Implementada
| Función Genérica | Pin ESP32-S3 | Descripción |
| :--- | :--- | :--- |
| `BOOT_PIN` | GPIO 0 | Pin de Boot al subir el firmware. |
| `HALL_PIN` | GPIO 1 | Retorno desde sensor de efecto hall magnético para despertar de Deep Sleep. |
| `BT_EN_PIN` | GPIO 3 | Habilita un modo de configuración para alistamiento por Bluetooth. |
| `BOOST_EN_PIN` | GPIO 4 | Activa el convertidor DC-DC (3.6v a 12v) que da soporte al medidor en los momentos de lectura por RS485. |
| `LED_PIN` | GPIO 5 | Diodo LED para pruebas lógicas (Activo en bajo lógico = 0). Apagado totalmente en modo Deep Sleep. |
| `DGMC_TX` | GPIO 10 | Trasmisión física en diagrama de interconexión UART 2 (Hacia medidor RS485). |
| `DGMC_RX` | GPIO 11 | Recepción física en diagrama de interconexión UART 2 (Desde RS485). |
| `RS485_EN` | GPIO 12 | Habilitación conjunta de los pines control transceptor para transmitir en el MAX3485. |
| `CAT_KEY` | GPIO 15 | Inyección de pulsos para la rutina de encendido/despertar del módulo celular Simcom. |
| `CAT_EN` | GPIO 16 | Control de potencia y suiche principal de suministro desde el regulador hacia el módulo celular. |
| `ESPC_TX` | GPIO 17 | Tx en UART 1 (Comandos AT hacia el SIM7080G). |
| `ESPC_RX` | GPIO 18 | Rx en UART 1 (Comandos AT desde el módulo celular SIM7080G). |
| `USB D-` | GPIO 19 | Pin exclusivo del bus Native USB D-. |
| `USB D+` | GPIO 20 | Pin exclusivo del bus Native USB D+. |

## Dependencias
- [TinyGSM](https://github.com/vshymanskyy/TinyGSM.git) (Manejo del módem serial SIM7080G)
- [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) (Peticiones HTTPS seguras sobre TinyGSM)
- [StreamDebugger](https://github.com/vshymanskyy/StreamDebugger) (Para depuración opcional del tráfico AT)

## Placa Objetivo
- ESP32-S3-WROOM-1-N16R8 Series.
- Entorno de compilación programado para levantar el monitor y carga usando directamente subpuerto Nativo USB CDC (COM).

## Notas de Desarrollo
- El firmware expone una flexibilidad en logs con `app_debug_enabled` y `at_debug_enabled` configurables.
- Se configuran explícitamente y bloquean los pines catalogados como `GPIO libres` según revisión HW (`Pines 2, 6, 7, 8, 9, 13, y 14`) dejándolos aislados y seteados en estado `INPUT_PULLDOWN` justo antes de entrar a hibernar garantizando el menor drenaje de picos estáticos posible.
