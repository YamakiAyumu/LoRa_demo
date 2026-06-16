#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include <stddef.h>

extern uart_inst_t *uart_id;
extern uint baudrate;

struct Packet {
    const uint8_t header[2] = {'T', 'G'};
    uint8_t length;    // ペイロードの長さ
    uint8_t payload[128]; // ペイロード（最大128バイト）
    uint8_t checksum;  // チェックサム
};

// LoRa module interface
void lora_serial_begin();
int initialize_lora_module();
void flush_buffer();
void execute_command(const char *cmd);
void execute_command_fmt(const char *param, int value);
int receive_serial_until_timeout(char *buffer, size_t buffer_size, uint32_t timeout_ms);
int send_ack();
void set_dst(int dst);
void set_gid(int gid);

#endif // LORA_H