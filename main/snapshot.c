#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define UART_MDSM7 UART_NUM_0
#define UART_FMS   UART_NUM_2
#define LED_G      13
#define RX_BUF_SIZE 128

static const char* TAG = "SNAPSHOT";

// ACK a enviar: 0x1A 0x01 0x79 0x5D
static const uint8_t ack_packet[4] = {0x1A, 0x01, 0x79, 0x5D};

static uint8_t* image_buffer = NULL;
static size_t image_size = 0;
static size_t expected_packages = 0;
static size_t received_packages = 0;

static void send_ack() {
    uart_write_bytes(UART_MDSM7, (const char*)ack_packet, sizeof(ack_packet));
    ESP_LOGI(TAG, "ACK enviado");
}

static void send_image_to_fms(uint8_t* img, size_t len) {
    uart_write_bytes(UART_FMS, (const char*)img, len);
    ESP_LOGI(TAG, "Imagen enviada al GPS (%d bytes)", len);
}

static void process_snapshot_event(uint8_t event_code) {
    const char* msg = NULL;
    switch (event_code) {
        case 0x00: msg = "[DROWSINESS]"; break;
        case 0x01: msg = "[DISTRACTION]"; break;
        case 0x02: msg = "[YAWNING]"; break;
        case 0x03: msg = "[CELL_PHONE]"; break;
        case 0x04: msg = "[SMOKING]"; break;
        case 0x05: msg = "[CAMERA_BLOCKED_MDSM]"; break;
        case 0x06: msg = "[NO_DRIVER]"; break;
        case 0x07: msg = "[G_SENSOR]"; break;
        case 0x10: msg = "[SNAPSHOT_REQUESTED]"; break;
        case 0x11: msg = "[NOT_LISTED_EVENT]"; break;
        default: msg = "[UNKNOWN_EVENT]"; break;
    }
    uart_write_bytes(UART_FMS, msg, strlen(msg));
    ESP_LOGI(TAG, "Evento: %s", msg);
}

void handle_snapshot_packet(const uint8_t* data, size_t len) {
    if (len < 20 || data[0] != 0x5A || data[1] != 0x79 || data[2] != 0x02 || data[len - 1] != 0x5D) return;

    gpio_set_level(LED_G, 1);

    uint8_t event_code = data[3];
    uint8_t package_index = data[7];
    expected_packages = data[9];

    uint16_t payload_size = (data[5] << 8) | data[6];
    const uint8_t* payload = &data[10];

    if (package_index == 1) {
        if (image_buffer) free(image_buffer);
        image_size = expected_packages * payload_size;
        image_buffer = (uint8_t*)malloc(image_size);
        memset(image_buffer, 0, image_size);
        received_packages = 0;
        process_snapshot_event(event_code);
    }

    if (image_buffer && package_index <= expected_packages) {
        memcpy(image_buffer + (package_index - 1) * payload_size, payload, payload_size);
        received_packages++;
        ESP_LOGI(TAG, "Paquete %d/%d recibido", package_index, expected_packages);

        if (received_packages == expected_packages) {
            ESP_LOGI(TAG, "Todos los paquetes recibidos. Enviando imagen...");
            send_image_to_fms(image_buffer, image_size);
            free(image_buffer);
            image_buffer = NULL;
        }
    }

    send_ack();
    gpio_set_level(LED_G, 0);
}
