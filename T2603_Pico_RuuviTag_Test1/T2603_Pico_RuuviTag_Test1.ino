#include <Arduino.h>
#include <btstack.h>

// -----------------------------
// RuuviTag Format 5 Parser
// -----------------------------
struct RuuviData {
    float temperature;
    float humidity;
    float pressure;
    float accelX;
    float accelY;
    float accelZ;
    float batteryVoltage;
    int8_t txPower;
    uint8_t movementCounter;
    uint16_t sequence;
};

bool parseRuuviFormat5(const uint8_t *data, uint8_t len, RuuviData &out) {
    if (len < 24) return false;

    // Must start with company ID 0x0499 (little endian: 0x99 0x04)
    if (data[0] != 0x99 || data[1] != 0x04) return false;

    // Format must be 5
    if (data[2] != 0x05) return false;

    int16_t tempRaw = (data[3] << 8) | data[4];
    uint16_t humRaw = (data[5] << 8) | data[6];
    uint16_t presRaw = (data[7] << 8) | data[8];
    int16_t axRaw = (data[9] << 8) | data[10];
    int16_t ayRaw = (data[11] << 8) | data[12];
    int16_t azRaw = (data[13] << 8) | data[14];
    uint16_t powerInfo = (data[15] << 8) | data[16];

    out.temperature = tempRaw * 0.005f;
    out.humidity = humRaw * 0.0025f;
    out.pressure = (presRaw + 50000) / 100.0f;

    out.accelX = axRaw / 1000.0f;
    out.accelY = ayRaw / 1000.0f;
    out.accelZ = azRaw / 1000.0f;

    out.batteryVoltage = ((powerInfo >> 5) + 1600) / 1000.0f;
    out.txPower = (powerInfo & 0x1F) * 2 - 40;

    out.movementCounter = data[17];
    out.sequence = (data[18] << 8) | data[19];

    return true;
}

// -----------------------------
// BLE Scanner
// -----------------------------
static btstack_packet_callback_registration_t hci_event_callback_registration;

void handle_adv_report(uint8_t *packet) {
    uint8_t addr[6];
    gap_event_advertising_report_get_address(packet, addr);

    const uint8_t *data = gap_event_advertising_report_get_data(packet);
    uint8_t data_len = gap_event_advertising_report_get_data_length(packet);

    // Look for RuuviTag manufacturer data
    for (int i = 0; i < data_len - 2; i++) {
      Serial.printf("%0X- ",data[i]);
    }  
    Serial.println();


    for (int i = 0; i < data_len - 2; i++) {
        if (data[i] == 0x99 && data[i+1] == 0x04) {
            RuuviData rd;
            if (parseRuuviFormat5(&data[i], data_len - i, rd)) {

                Serial.print("RuuviTag ");
                for (int j = 0; j < 6; j++) {
                    Serial.printf("%02X", addr[j]);
                    if (j < 5) Serial.print(":");
                }
                Serial.println();

                Serial.printf("  Temp: %.2f C\n", rd.temperature);
                Serial.printf("  Hum: %.2f %%\n", rd.humidity);
                Serial.printf("  Pressure: %.2f hPa\n", rd.pressure);
                Serial.printf("  Accel: %.3f %.3f %.3f g\n", rd.accelX, rd.accelY, rd.accelZ);
                Serial.printf("  Battery: %.3f V\n", rd.batteryVoltage);
                Serial.printf("  TX Power: %d dBm\n", rd.txPower);
                Serial.printf("  Movement: %u\n", rd.movementCounter);
                Serial.printf("  Sequence: %u\n\n", rd.sequence);
            }
        }
    }
}

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);

    if (event == GAP_EVENT_ADVERTISING_REPORT) {
        handle_adv_report(packet);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starting BLE scan for RuuviTags...");
    cyw43_arch_disable_wifi();

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hci_power_control(HCI_POWER_ON);
}

void loop() {
    // BTstack runs in background
}