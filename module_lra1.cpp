#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "lora.h"

uint baudrate = 115200;
uart_inst_t* uart_id = uart0;

const int sf = 7;
const int bw = 7;
const int cr = 1;

void lora_serial_begin() {
    uart_init(uart_id, baudrate);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    uart_set_format(uart_id, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart_id, false, false);
}

int initialize_lora_module() {
    execute_command("OWN=0");
    execute_command("SF=7");
    execute_command("BW=7");
    execute_command("CR=1");
    execute_command("CTRL=7");
    execute_command("ECHO=0");

    flush_buffer();

    return 0;
}

void flush_buffer() {
    absolute_time_t idle_deadline = make_timeout_time_ms(10);
    absolute_time_t max_deadline = make_timeout_time_ms(100);
    while (!time_reached(max_deadline)) {
        bool any = false;
        while (uart_is_readable(uart_id)) {
            uart_getc(uart_id);
            any = true;
        }
        if (!any && time_reached(idle_deadline)) {
            break;
        }
        if (any) {
            idle_deadline = make_timeout_time_ms(10);
        }
    }
}

void execute_command(const char* cmd) {
    printf("[lora] execute_command: '%s'\n", cmd);
    uart_puts(uart_id, cmd);
    uart_putc(uart_id, '\r');
    sleep_ms(10);
}

void execute_command_fmt(const char* param, int value) {
    char buf[32];
    sprintf(buf, "%s%d", param, value);
    execute_command(buf);
}

int receive_serial_until_timeout(char *buffer, size_t buffer_size, uint32_t timeout_ms) {
    if (buffer == NULL || buffer_size == 0) {
        printf("[lora] receive_serial_until_timeout: invalid buffer or size\n");
        return -1;
    }

    memset(buffer, 0, buffer_size);
    int index = 0;
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    printf("[lora] receive_serial_until_timeout: start timeout=%u buffer_size=%zu\n", timeout_ms, buffer_size);

    while (!time_reached(deadline)) {
        while (uart_is_readable(uart_id)) {
            char c = (char)uart_getc(uart_id);
            if (index < (buffer_size - 1)) {
                buffer[index++] = c;
                unsigned char uc = (unsigned char)c;
                if (uc >= 32 && uc <= 126) {
                    printf("[lora] read[%d]=0x%02x '%c'\n", index - 1, uc, c);
                } else {
                    printf("[lora] read[%d]=0x%02x\n", index - 1, uc);
                }
            } else {
                printf("[lora] buffer full, dropping byte 0x%02x\n", (unsigned char)c);
            }
        }

        if (index > 0 && buffer[index - 1] == '\r') {
            printf("[lora] received CR, end of packet\n");
            // strip the CR from the returned data length
            index--;
            break;
        }
    }

    if ((index % 2) != 0 || index < 20) {
        printf("[lora] receive_serial_until_timeout: invalid length=%d\n", index);
        return -1;
    }

    buffer[index] = '\0';
    printf("[lora] receive_serial_until_timeout: success len=%d\n", index);
    return index;
}

int send_ack() {
    execute_command("TXD(7)=1");
    execute_command("TXD(8)=6");
    for (int i = 0; i < 3; i++) {
        execute_command("SEND");
        sleep_ms(100);
    }

    return 0;
}

void set_dst(int dst) {
    execute_command_fmt("DST=", dst);
}

void set_gid(int gid) {
    execute_command_fmt("GID=", gid);
}