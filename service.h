#ifndef SERVICE_H
#define SERVICE_H

enum SensorDataType {
    INT,
    FLOAT,
    DOUBLE,
    BOOLEAN
};

union SensorDataValue {
    int value_int;
    float value_float;
    double value_double;
    bool value_bool;
};

struct SensorData {
    uint8_t id;
    SensorDataType type;
    SensorDataValue value;
};

struct Packet {
    const uint8_t header[2] = {'T', 'G'};
    uint8_t length;
    uint16_t gid;
    uint16_t lmid;
    uint8_t payload[128];
    uint8_t checksum;
};

extern const uint16_t groups[];

int init_controller();
int init_lora_module(uint16_t lid);
int init_serial(void *serial_instance, const int uart_tx_pin, const int uart_rx_pin);
int prepare_comm(uint16_t gid);
int handle_tx_raw(uint16_t dst, uint16_t gid, uint16_t lid, const uint8_t payload[], size_t payload_len);
int handle_rx(char *buffer, size_t buffer_size, uint32_t timeout_ms);
int enqueue_packet(char *buffer);

#endif // SERVICE_H