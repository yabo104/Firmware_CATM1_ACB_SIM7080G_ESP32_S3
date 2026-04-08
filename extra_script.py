"""
extra_script.py - Patch para bug de framework-arduinoespressif32 v3.20017 en ESP32-C3.

Problema: con ARDUINO_USB_CDC_ON_BOOT=1, HardwareSerial.cpp llama a
Serial.available() en serialEventRun() sin declarar que Serial es HWCDC.

Solución: parchar HardwareSerial.cpp del framework para envolver esa línea
con un guard #if !ARDUINO_USB_CDC_ON_BOOT, igual que el resto del archivo.
"""
Import("env")
import os

PATCH_MARKER  = "// [HWCDC_BOOT_FIX_APPLIED]"
BUGGY_LINE    = "  if(serialEvent && Serial.available()) serialEvent();"
FIXED_BLOCK   = (
    "  " + PATCH_MARKER + "\n"
    "#if !ARDUINO_USB_CDC_ON_BOOT\n"
    "  if(serialEvent && Serial.available()) serialEvent();\n"
    "#endif"
)

# Ubicar HardwareSerial.cpp dentro del paquete del framework instalado
try:
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
except Exception:
    framework_dir = ""

hw_serial = os.path.join(framework_dir, "cores", "esp32", "HardwareSerial.cpp")

if not os.path.isfile(hw_serial):
    print(f"[patch] AVISO: no se encontró HardwareSerial.cpp en: {hw_serial}")
else:
    with open(hw_serial, "r", encoding="utf-8") as f:
        content = f.read()

    if PATCH_MARKER in content:
        print("[patch] HardwareSerial.cpp ya está parcheado — sin cambios.")
    elif BUGGY_LINE in content:
        content = content.replace(BUGGY_LINE, FIXED_BLOCK)
        with open(hw_serial, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"[patch] HardwareSerial.cpp parcheado correctamente en:\n        {hw_serial}")
    else:
        print("[patch] AVISO: línea problemática no encontrada — puede que el framework ya lo corrija.")
