#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lora.h"

#define UART_TX_PIN 0
#define UART_RX_PIN 1

struct Children {
    uint16_t lmid;
    char sensor_data[128];
    bool is_collected = false;
};

// プロジェクトごとにIDを設定
Children children[4] = { {1}, {2}, {3}, {4} };

void initialize_raspi();
bool all_children_collected();
int validate_packet(const char *buffer, int received_size);


int main() {
    initialize_raspi();
    lora_serial_begin();
    initialize_lora_module();
    while (!all_children_collected()) { // ここの条件変える。グループごとに送信にする。
        set_gid(0); // 仮置き。すべてのグループに送る必要ある。
        set_dst(65535);
        execute_command("TXWAVE 1200");
        sleep_ms(1200);
        flush_buffer();
        execute_command("RECV -10000,$$");
        char buffer[256];
        int received_size = receive_serial_until_timeout(buffer, sizeof(buffer), 10000);
        if (received_size < 0) {
            continue;
        } else {
            printf("received(%d bytes): %s\n", received_size, buffer);
        }

        if (validate_packet(buffer, received_size) < 0) {
            continue;
        }

        send_ack();
        // ACKの送信
        // サーバへのデータ送信
        // Android端末へのデータ送信
    }

    // サンプルコード
    // stdio_init_all();

    // // Initialise the Wi-Fi chip
    // if (cyw43_arch_init()) {
    //     printf("Wi-Fi init failed\n");
    //     return -1;
    // }

    // // Enable wifi station
    // cyw43_arch_enable_sta_mode();

    // printf("Connecting to Wi-Fi...\n");
    // if (cyw43_arch_wifi_connect_timeout_ms("Your Wi-Fi SSID", "Your Wi-Fi Password", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
    //     printf("failed to connect.\n");
    //     return 1;
    // } else {
    //     printf("Connected.\n");
    //     // Read the ip address in a human readable way
    //     uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    //     printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    // }

    // while (true) {
    //     printf("Hello, world!\n");
    //     sleep_ms(1000);
    // }
}

void initialize_raspi() {
    stdio_init_all();

    sleep_ms(1000);
    printf("UART initialized");
}

bool all_children_collected() {
    for (Children &child : children) {
        if (child.is_collected == false) {
            return false;
        }
    }
    return true;
}

int validate_packet(const char *buffer, int received_size) {
    int parsed_len = received_size / 2;
    uint8_t parsed[128];
    int index = 0;
    bool parse_err = false;
    for (int i = 0; i < received_size; i += 2) {
        unsigned int v;
        if (sscanf(buffer + i, "%2x", &v) != 1) {
            parse_err = true;
            break;
        }
        parsed[index++] = (uint8_t)v;
        if (index >= sizeof(parsed)) {
            parse_err = true;
            break;
        }
    }
    if (parse_err) {
        printf("validate: parse FAIL\n");
        return -1;
    }
    printf("validate: parse OK (parsed_len=%d)\n", parsed_len);

    // ヘッダ検証
    if (parsed[0] != (uint8_t)'T' || parsed[1] != (uint8_t)'G') {
        printf("validate: header FAIL\n");
        return -1;
    }
    printf("validate: header OK\n");

    // データ長検証
    if (parsed[2] != parsed_len - 3) {
        printf("validate: length FAIL (declared=%u expected=%d)\n", (unsigned)parsed[2], (int)(parsed_len - 3));
        return -1;
    }
    printf("validate: length OK (declared=%u)\n", (unsigned)parsed[2]);

    // チェックサム検証
    uint8_t checksum = parsed[parsed_len - 1];
    uint32_t sum = 0;
    for (int i = 0; i < parsed_len - 1; i++) {
        sum += parsed[i];
    }
    if ((sum & 0xFF) != checksum) {
        printf("validate: checksum FAIL (sum=0x%02X chk=0x%02X)\n", (unsigned)(sum & 0xFF), (unsigned)checksum);
        return -1;
    }
    printf("validate: checksum OK\n");
    

    // parsed[3], parsed[4] をリトルエンディアンの uint16_t と解釈して children の lmid と照合
    uint16_t recv_id = (uint16_t)parsed[3] | ((uint16_t)parsed[4] << 8);
    int matched_index = -1;
    for (int i = 0; i < (sizeof(children)/sizeof(Children)); i++) {
        if (children[i].lmid == recv_id) {
            matched_index = i;
            set_dst(matched_index);
            break;
        }
    }
    if (matched_index < 0) {
        printf("validate: id FAIL (recv_id=%u)\n", (unsigned)recv_id);
        return -1;
    }
    printf("validate: id OK (recv_id=%u index=%d)\n", (unsigned)recv_id, matched_index);

    // ペイロードを children[matched_index].sensor_data にコピー（NUL終端）
    for (int i = 0; i < parsed_len - 4; i++) {
        children[matched_index].sensor_data[i] = (char)parsed[i + 3];
    }
    children[matched_index].sensor_data[parsed_len - 4] = '\0';

    children[matched_index].is_collected = true;
    printf("validate: stored payload %d bytes into children[%d].sensor_data\n", parsed_len - 4, matched_index);
    return 0;
}

// 文字列を2文字ずつ16進数に変換する関数
void hex_encode(const uint8_t *data, size_t data_len, char *output, size_t output_size) {
    const char hex_chars[] = "0123456789ABCDEF";
    size_t required_size = data_len * 2 + 1; // 2文字/バイト + NUL終端
    if (output_size < required_size) {
        printf("hex_encode: output buffer too small (required=%zu)\n", required_size);
        return;
    }
    for (size_t i = 0; i < data_len; i++) {
        output[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
        output[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    output[data_len * 2] = '\0';
}

int validate_pakcet(const char *input, size_t input_len) {
    if (input_len < 5) {
        printf("validate_packet: too short input\n");
        return -1;
    }

    if (input[0] != 'T' || input[1] != 'G') {
        printf("validate_packet: invalid header\n");
        return -1;
    }

    if (input[2] != input_len - 3) {
        printf("validate_packet: length mismatch (declared=%u expected=%zu)\n", (unsigned)input[2], input_len - 3);
        return -1;
    }

    int checksum = 0;
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

void format_packet(const char *input, size_t input_len, char *output, size_t output_size) {
    if (output_size < input_len * 2 + 1) {
        printf("format_packet: output buffer too small\n");
        return;
    }

    for (size_t i = 0; i < input_len; i++) {
        sprintf(output + i * 2, "%02X", (unsigned char)input[i]);
    }
    output[input_len * 2] = '\0';