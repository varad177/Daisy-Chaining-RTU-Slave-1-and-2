#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ModbusRTU.h>

// ───────── CONFIG ─────────
const uint8_t SLAVE_ID = 2;
const uint32_t BAUD = 9600;
const uint8_t PIN_RS485_DE = 4;

#define TEMP_PIN 14
const uint16_t REG_TEMPERATURE = 4;

// ───────── OBJECTS ─────────
ModbusRTU mb;
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

float g_temperature = 25.0;

// Float helper
union FloatRegs { float f; uint16_t w[2]; };

void writeFloat(uint16_t startReg, float value) {
    FloatRegs d;
    d.f = value;
    mb.Hreg(startReg, d.w[1]);
    mb.Hreg(startReg + 1, d.w[0]);
}

void setup() {

    Serial.begin(115200);
    sensors.begin();

    Serial2.begin(BAUD, SERIAL_8N1, 16, 17);

    pinMode(PIN_RS485_DE, OUTPUT);
    digitalWrite(PIN_RS485_DE, LOW);

    mb.begin(&Serial2, PIN_RS485_DE);
    mb.slave(SLAVE_ID);

    // Only temperature registers
    mb.addHreg(REG_TEMPERATURE);
    mb.addHreg(REG_TEMPERATURE + 1);

    mb.onRequest([](Modbus::FunctionCode fc, Modbus::RequestData data)
        -> Modbus::ResultCode {

        Serial.print("Slave 2 Request | FC: ");
        Serial.println((uint8_t)fc);

        return Modbus::EX_SUCCESS;
    });
}

void loop() {

    mb.task();   // Always first

    static unsigned long lastTemp = 0;

    if (millis() - lastTemp > 1000) {

        sensors.requestTemperatures();
        float t = sensors.getTempCByIndex(0);

        if (t != DEVICE_DISCONNECTED_C)
            g_temperature = t;

        writeFloat(REG_TEMPERATURE, g_temperature);

        lastTemp = millis();
    }
}   