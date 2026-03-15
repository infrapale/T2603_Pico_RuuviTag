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

    if (data[0] != 0x99 || data[1] != 0x04) return false;
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
static btstack_packet_callback_registration_t hci_cb;

void handle_adv(uint8_t *packet) {
    uint8_t addr[6];
    gap_event_advertising_report_get_address(packet, addr);

    const uint8_t *data = gap_event_advertising_report_get_data(packet);
    uint8_t len = gap_event_advertising_report_get_data_length(packet);

    // Debug: print every advertisement
    Serial.print("ADV from ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.print("  len=");
    Serial.println(len);

    // Look for Ruuvi manufacturer ID
    for (int i = 0; i < len - 2; i++) {
        if (data[i] == 0x99 && data[i+1] == 0x04) {
            RuuviData rd;
            if (parseRuuviFormat5(&data[i], len - i, rd)) {

                Serial.println("=== RuuviTag Found ===");

                Serial.printf("Temp: %.2f C\n", rd.temperature);
                Serial.printf("Hum: %.2f %%\n", rd.humidity);
                Serial.printf("Pressure: %.2f hPa\n", rd.pressure);
                Serial.printf("Accel: %.3f %.3f %.3f g\n", rd.accelX, rd.accelY, rd.accelZ);
                Serial.printf("Battery: %.3f V\n", rd.batteryVoltage);
                Serial.printf("TX Power: %d dBm\n", rd.txPower);
                Serial.printf("Movement: %u\n", rd.movementCounter);
                Serial.printf("Sequence: %u\n", rd.sequence);
                Serial.println("======================\n");
            }
        }
    }
}

void packet_handler(uint8_t type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (type != HCI_EVENT_PACKET) return;

    uint8_t event = hci_event_packet_get_type(packet);

    if (event == GAP_EVENT_ADVERTISING_REPORT) {
        handle_adv(packet);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("Initializing BLE...");

    // BLE and Wi-Fi cannot run together
    //cyw43_arch_disable_wifi();

    hci_cb.callback = &packet_handler;
    hci_add_event_handler(&hci_cb);

    hci_power_control(HCI_POWER_ON);

    // REQUIRED: start scanning
    gap_set_scan_parameters(0, 0x30, 0x30);
    gap_start_scan();

    Serial.println("Scanning for RuuviTags...");
}

void loop() {
    // BTstack runs in background
}