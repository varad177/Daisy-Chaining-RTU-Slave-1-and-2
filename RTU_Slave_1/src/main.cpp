#include <Arduino.h>
#include <ModbusRTU.h>

#define SLAVE_ID       1
#define RS485_DE_PIN   4
#define RTU_BAUD       9600

#define REG_VOLTAGE    0   // Holding register start address

#define ROT_CLK  18
#define ROT_DT   19

ModbusRTU mb;

// Encoder variables
volatile int encoderCount = 0;
volatile int lastEncoded = 0;

float g_voltage = 24.0f;   // Initial voltage

// ───────── Encoder ISR ─────────
void IRAM_ATTR readEncoder() {
    int MSB = digitalRead(ROT_CLK);
    int LSB = digitalRead(ROT_DT);

    int encoded = (MSB << 1) | LSB;
    int sum = (lastEncoded << 2) | encoded;

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
        encoderCount++;
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
        encoderCount--;

    lastEncoded = encoded;
}

// Float → 2 Registers (Big Endian)
union FloatRegs {
    float f;
    uint16_t w[2];
};

void writeFloat(uint16_t reg, float value) {
    FloatRegs d;
    d.f = value;

    // Big-endian format
    mb.Hreg(reg,     d.w[1]);
    mb.Hreg(reg + 1, d.w[0]);
}

void setup() {

    Serial.begin(115200);

    // Encoder setup
    pinMode(ROT_CLK, INPUT_PULLUP);
    pinMode(ROT_DT,  INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ROT_CLK), readEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ROT_DT),  readEncoder, CHANGE);

    // RS485 setup (TX=17, RX=16)
    Serial2.begin(RTU_BAUD, SERIAL_8N1, 16, 17);

    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);

    mb.begin(&Serial2, RS485_DE_PIN);
    mb.slave(SLAVE_ID);

    // Add 2 registers for float voltage
    mb.addHreg(REG_VOLTAGE);
    mb.addHreg(REG_VOLTAGE + 1);
     mb.onRequest([](Modbus::FunctionCode fc, Modbus::RequestData data)
        -> Modbus::ResultCode {

        Serial.print("Slave 1 Request | FC: ");
        Serial.println((uint8_t)fc);

        return Modbus::EX_SUCCESS;
    });
}

void loop() {

    mb.task();   // Always first

    static int lastCount = 0;

    // Encoder voltage adjustment
    int diff = encoderCount - lastCount;
    if (diff != 0) {
        g_voltage += diff * 0.1f;
        lastCount = encoderCount;
    }

    g_voltage = constrain(g_voltage, 15.0f, 30.0f);

    // Update Modbus registers
    writeFloat(REG_VOLTAGE, g_voltage);
}