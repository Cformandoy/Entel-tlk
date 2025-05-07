#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"


typedef struct {
    unsigned char* data;
    int length;
} Packet;



Packet* extractPackets(unsigned char* buffer, int bufferLen, int* outPacketCount) {
    int i = 0;
    int capacity = 10;
    int count = 0;

    Packet* packets = (Packet*)malloc(sizeof(Packet) * capacity);

    while (i < bufferLen - 4) {  // -4 para poder validar inicio completo
        // Verificar inicio válido
        if (buffer[i] == 0x5A && buffer[i + 1] == 0x79 && buffer[i + 2] == 0x02 && buffer[i + 4] == 0x04) {
            int start = i;
            int end = i;

            while (end < bufferLen) {
                if (buffer[end] == 0x5D) {
                    // Caso A: 0x5D seguido de nuevo paquete válido
                    if ((end + 4) < bufferLen &&
                        buffer[end + 1] == 0x5A &&
                        buffer[end + 2] == 0x79 &&
                        buffer[end + 3] == 0x02 &&
                        buffer[end + 5] == 0x04) {
                        end++;  // incluir el 0x5D
                        break;
                    }

                    // Caso B: 0x5D es el último byte
                    if (end == bufferLen - 1) {
                        end++;  // incluir el 0x5D
                        break;
                    }
                }
                end++;
            }

            if (end <= bufferLen) {
                int packetLen = end - start;

                // Redimensionar si es necesario
                if (count >= capacity) {
                    capacity *= 2;
                    packets = (Packet*)realloc(packets, sizeof(Packet) * capacity);
                }

                packets[count].data = (unsigned char*)malloc(packetLen);
                memcpy(packets[count].data, &buffer[start], packetLen);
                packets[count].length = packetLen;
                count++;

                i = end;
            } else {
                break;  // paquete incompleto
            }
        } else {
            i++;
        }
    }

    *outPacketCount = count;
    return packets;
}


// get string event byte
const char* getEventByteString(unsigned char eventByte) {
	switch (eventByte) {
		case 0x01:
			return "DROWSINESS";
		case 0x02:
			return "DISTRACTION";
		case 0x04:
			return "YAWNING";
		case 0x08:
			return "CELL_PHONE_USE";
		case 0x10:
			return "SMOKING";
		case 0x20:
			return "SEAT_BELT";
		case 0x40:
			return "CAMERA_BLOCKED";
		case 0x80:
			return "MASK";
		default:
			return "UNKNOWN_EVENT";
	}
}

bool PROCESS_SNAPSHOT_EVENT(unsigned char *data, unsigned char eventByte)
{
	ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT");
	ESP_LOGI("MDSM7_RX", "eventByte: %x", eventByte);

	if (data[0] == 0x5A && data[1] == 0x79 && data[2] == 0x02 && data[3] == eventByte && data[4] == 0x04) {
		int actualPicturePackage = data[7];
		int totalPicturePackages = data[8];

		// aqui se procesa el evento

		if(actualPicturePackage != totalPicturePackages) {
			// solicitar siguiente paquete
			ESP_LOGI("MDSM7_RX", "Paquete %d de %d", actualPicturePackage, totalPicturePackages);
			ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT_END");
			return true;
		}

		ESP_LOGI("MDSM7_RX", "Ultimo paquete de imagen");
	}

	ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT_END");
	return false;
}

void REQUEST_SNAPSHOT()
{
	ESP_LOGI("MDSM7_RX", "REQUEST_SNAPSHOT");
	
	// Send the package 0x1A 0x01 0x79 0x5D to TX
	unsigned char ack[4] = {0x1A, 0x01, 0x79, 0x5D};
	ESP_LOGI("MDSM7_RX", "ACK: %x %x %x %x", ack[0], ack[1], ack[2], ack[3]);

	// Send the package to TX
	int txBytes = uart_write_bytes(UART_MDSM7, (const char *)ack, sizeof(ack));
	ESP_LOGI("MDSM7_RX", "Bytes enviados: %d", txBytes);

	ESP_LOGI("MDSM7_RX", "REQUEST_SNAPSHOT_END");
}


