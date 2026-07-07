#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#define LORA_BUFFER_INIT_INDEX 7


static Serial2 serial2;
static const int baudrate = 115200;
static const uint8_t sf = 7;
static const uint8_t bw = 7;
static const uint8_t cr = 1;
static size_t buffer_index;

static void execute_command(const char* cmd);
static void execute_command_fmt(const char* param, int value);
static void flush_buffer();

void config_controller() {
    setCpuFrequencyMhz(80);

    const int pins_to_disable[] = {0, 2, 4, 12, 13, 14, 15, 18, 19, 21, 22, 23, 25, 27, 32};
    for (int pin : pins_to_disable) {
        pinMode(pin, INPUT_PULLDOWN);
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    btStop();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    rtc_gpio_init(WAKEUP_PIN);
    rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_dis(WAKEUP_PIN);
    rtc_gpio_pulldown_en(WAKEUP_PIN);

    Serial.begin(115200);
}

void config_lora_module(uint16_t own) {
    execute_command_fmt("SF=", sf);
    execute_command_fmt("BW=", bw);
    execute_command_fmt("CR=", cr);
    execute_command_fmt("OWN=", own);

    execute_command("CTRL=7");
    execute_command("ECHO=0");

    flush_buffer();
}

void config_serial(void *comm_port, const int uart_tx_pin, const int uart_rx_pin) {
    serial2 = (Serial2)comm_port;
    serial2.begin(baudrate, SERIAL_8N1, uart_rx_pin, uart_tx_pin);
}

void set_dst(uint16_t dst) {
    execute_command_fmt("DST=", dst);
}

void set_gid(uint16_t gid) {
    execute_command_fmt("GID=", gid);
}

void send_preamble(uint32_t time_ms) {
    execute_command_fmt("TXWAVE %u", time_ms);
}

void open_tx_buffer() {
    buffer_index = LORA_BUFFER_INIT_INDEX;
}

void set_tx_byte(uint8_t data) {
    char cmd[32];
    sprintf(cmd, "TXD(%u)=%u", buffer_index, data);
    execute_command(cmd);
    buffer_index++;
}

void set_tx_word(uint16_t data) {
    char cmd[32];
    sprintf(cmd, "TXDW(%u)=%u", buffer_index, data);
    execute_command(cmd);
    buffer_index += 2;
}

void send_packet() {
    execute_command("SEND");
}

int recv_packet(char *raw_buffer, size_t raw_buffer_size,uint32_t timeout_ms) {
    char cmd[32];
    sprintf(cmd, "RECV -%u,$$", timeout_ms);
    execute_command(cmd);

    int index = 0;
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (!time_reached(deadline)) {
        while (uart_is_readable(uart_id)) {
            char c = uart_getc(uart_id);
            if (index < (sizeof(raw_buffer) - 1)) {
                raw_buffer[index++] = c;
            }
        }

        if (index > 0 && raw_buffer[index - 1] == '\r') {
            index--;
            break;
        }
    }

    if (index % 2 != 0) {
        printf("[lora] recv_packet: invalid length=%d\n", index);
        return -1;
    }

    raw_buffer[index] = '\0';
    return index;
}

static void execute_command(const char* cmd) {
    flush_buffer();

    printf("[lora] execute_command: '%s'\n", cmd);
    uart_puts(uart_id, cmd);
    uart_putc(uart_id, '\r');
    sleep_ms(10);
}

static void execute_command_fmt(const char* param, int value) {
    char buf[32];
    sprintf(buf, "%s%d", param, value);
    execute_command(buf);
}

static void flush_buffer() {
    absolute_time_t idle_deadline = make_timeout_time_ms(2);
    absolute_time_t max_deadline = make_timeout_time_ms(20);
    while (!time_reached(max_deadline)) {
        bool has_received = false;
        while (uart_is_readable(uart_id)) {
            uart_getc(uart_id);
            has_received = true;
        }
        if (!has_received && time_reached(idle_deadline)) {
            break;
        }
        if (has_received) {
            idle_deadline = make_timeout_time_ms(2);
        }
    }
}
