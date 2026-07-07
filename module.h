#ifndef MODULE_H
#define MODULE_H

void config_controller();
void config_lora_module(uint16_t own);
void config_serial(void *comm_port, const int uart_tx_pin, const int uart_rx_pin);
void set_dst(uint16_t dst);
void set_gid(uint16_t gid);
void send_preamble(uint32_t time_ms);
void open_tx_buffer();
void set_tx_byte(uint8_t data);
void set_tx_word(uint16_t data);
void send_packet();
int recv_packet(char *raw_buffer, size_t raw_buffer_size,uint32_t timeout_ms);

#endif // MODULE_H