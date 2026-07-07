#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "service.h"
#include "module.h"

#define RX_BUFFER_SIZE 128

static Packet rx_queue[RX_BUFFER_SIZE];
static int rx_queue_head = 0;
static int rx_queue_tail = 0;
static int rx_queue_count = 0;
static uart_inst_t* uart_id;
uint16_t current_received_from;

static int hex_decode(const char *input, size_t input_len, char *output, size_t output_size);
static int validate_packet(const char *input, size_t input_len);
static int calc_checksum(Packet *packet);

int init_controller() {
    config_controller();

    return 0;
}

int init_lora_module(uint16_t lid) {
    config_lora_module(lid);

    return 0;
}

int init_serial(void *comm_port, const int uart_tx_pin, const int uart_rx_pin) {
    config_serial(comm_port, uart_tx_pin, uart_rx_pin);

    return 0;
}

int prepare_comm(uint16_t gid) {
    send_preamble(1200);
    sleep_ms(1200);

    return 0;
}

int handle_tx_raw(uint16_t dst, uint16_t gid, uint16_t lid, const uint8_t payload[], size_t payload_len) {
    if (dst == 0) {
        set_dst(current_received_from);
    } else {
        set_dst(dst);
    }

    set_gid(gid);

    Packet packet;
    packet.length = payload_len;
    packet.gid = gid;
    packet.lmid = lid;
    for (size_t i = 0; i < payload_len; i++) {
        packet.payload[i] = payload[i];
    }
    packet.checksum = calc_checksum(&packet);

    open_tx_buffer();
    set_tx_byte(packet.length + 6);
    set_tx_byte(packet.header[0]);
    set_tx_byte(packet.header[1]);
    set_tx_word(packet.gid);
    set_tx_word(packet.lmid);
    for (size_t i = 0; i < payload_len; i++) {
        set_tx_byte(packet.payload[i]);
    }
    set_tx_byte(packet.checksum);
    send_packet();

    return 0;
}

int handle_rx(char *buffer, size_t buffer_size, uint32_t timeout_ms) {
    char raw_buffer[256];
    int raw_buffer_len = recv_packet(raw_buffer, sizeof(raw_buffer), timeout_ms);
    if (raw_buffer_len < 0) {
        return -1;
    }

    int decoded_len = hex_decode(raw_buffer, raw_buffer_len, buffer, buffer_size);
    if (decoded_len < 0) {
        return -1;
    }

    if (validate_packet(buffer, decoded_len) < 0) {
        return -1;
    }

    return decoded_len;
}

int enqueue_packet(char *buffer) {
    if (rx_queue_count >= RX_BUFFER_SIZE) {
        printf("enqueue_packet: queue is full\n");
        return -1;
    }

    Packet *packet = &rx_queue[rx_queue_tail];
    packet->length = buffer[2];
    packet->gid = (uint16_t)((uint8_t)buffer[3] | ((uint8_t)buffer[4] << 8));
    packet->lmid = (uint16_t)((uint8_t)buffer[5] | ((uint8_t)buffer[6] << 8));
    for (size_t i = 0; i < packet->length; i++) {
        packet->payload[i] = (uint8_t)buffer[7 + i];
    }
    packet->checksum = (uint8_t)buffer[7 + packet->length];

    rx_queue_tail = (rx_queue_tail + 1) % RX_BUFFER_SIZE;
    rx_queue_count++;

    return 0;
}

static int hex_decode(const char *input, size_t input_len, char *output, size_t output_size) {
    int required_size = input_len / 2;
    for (int i = 0; i < required_size; i++) {
        unsigned int value;
        if (sscanf(&input[i * 2], "%2x", &value) != 1) {
            printf("hex_decode: parse failed at index=%d\n", i);
            return -1;
        }
        output[i] = (char)value;
    }

    return required_size;
}

static int validate_packet(const char *input, size_t input_len) {
    if (input_len < 8) {
        printf("validate_packet: too short input\n");
        return -1;
    }

    if (input[0] != 'T' || input[1] != 'G') {
        printf("validate_packet: invalid header\n");
        return -1;
    }

    if (input[2] != input_len - 8) {
        printf("validate_packet: length mismatch (declared=%u expected=%zu)\n", (unsigned)input[2], input_len - 8);
        return -1;
    }

    uint8_t checksum = 0;
    for (size_t i = 0; i < input_len - 1; i++) {
        checksum += (unsigned char)input[i];
    }
    if ((checksum & 0xFF) != (unsigned char)input[input_len - 1]) {
        printf("validate_packet: checksum mismatch (calculated=%u received=%u)\n", (unsigned)(checksum & 0xFF), (unsigned)(unsigned char)input[input_len - 1]);
        return -1;
    }

    printf("validate_packet: packet is valid\n");
    return 0;
}

static int calc_checksum(Packet *packet) {
    uint8_t checksum = 0;
    checksum += packet->header[0];
    checksum += packet->header[1];
    checksum += packet->length;
    checksum += (packet->gid & 0xFF);
    checksum += ((packet->gid >> 8) & 0xFF);
    checksum += (packet->lmid & 0xFF);
    checksum += ((packet->lmid >> 8) & 0xFF);
    for (size_t i = 0; i < packet->length; i++) {
        checksum += packet->payload[i];
    }

    return checksum;
}
