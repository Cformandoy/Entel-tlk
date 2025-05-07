#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h"


typedef struct {
    unsigned char* data;
    int length;
} Packete;

void MDSM7_init()
{
	const uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
	uart_param_config(UART_MDSM7, &uart_config);
	uart_set_pin(UART_MDSM7, UART_MDSM7_TXD_PIN, UART_MDSM7_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install(UART_MDSM7, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

Packete* extractPackets2(unsigned char* buffer, int bufferLen, int* outPacketCount) {
    int i = 0;
    int capacity = 10;
    int count = 0;

    Packete* packets = (Packete*)malloc(sizeof(Packete) * capacity);

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
                    packets = (Packete*)realloc(packets, sizeof(Packete) * capacity);
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
const char* getEventByteString2(unsigned char eventByte) {
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


bool PROCESS_SNAPSHOT_EVENT2(unsigned char *data, unsigned char eventByte)
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


void REQUEST_SNAPSHOT2()
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


static void MDSM7_rxTask() 
{
	ESP_LOGI("MDSM7_RX", "Init RX Task");
	uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);

	gpio_set_level(LED_G, 1); // Encender LED_G (nivel alto)
	
	while (1) {
		const int rxBytes = uart_read_bytes(UART_MDSM7, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);

		ESP_LOGI("MDSM7_RX", "Bytes recibidos: %d", rxBytes);
		// MODIFICACION DESDE ACA
		// Verificar si el frame es del protocolo lite
		if (data[0] == 0x5B && data[1] == 0x79 && data[2] == 0x42 && data[4] == 0x5F){
			// se utiliza la data eliminando las primeras 5 posiciones
			unsigned char* eventData = &data[5];
			int eventDataLen = rxBytes - 5;

			unsigned char eventByte = eventData[3];
			const char* eventName = getEventByteString2(eventByte);

			// Verificar si el evento es uno de los validos
			if(strcmp(eventName, "UNKNOWN_EVENT") != 0) {
				ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

				int packetCount = 0;
				Packete* packets = extractPackets2(eventData, eventDataLen, &packetCount);

				ESP_LOGI("MDSM7_RX", "Total paquetes: %d", packetCount);
				for (int i = 0; i < packetCount; i++) {
					ESP_LOGI("MDSM7_RX", "Paquete %d (len %d): ", i + 1, packets[i].length);

					bool processResult = PROCESS_SNAPSHOT_EVENT2(packets[i].data, eventByte);
					ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT: %d", processResult);
					// Si el paquete no es el ultimo, se solicita el siguiente
					if (processResult) {
						// Se solicita el siguiente paquete
						ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
						REQUEST_SNAPSHOT2();
					}
					free(packets[i].data);
				}

				free(packets);


			} else {
				ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
			}

		}else if (data[0] == 0x5A && data[1] == 0x79 && data[2] == 0x02 && data[4] == 0x04) {
			// Se recibe un paquete de imagen
			ESP_LOGI("MDSM7_RX", "Paquete de imagen recibido");
			// se utiliza la data eliminando las primeras 5 posiciones
			unsigned char* eventData = data;
			//int eventDataLen = rxBytes;

			unsigned char eventByte = eventData[3];
			const char* eventName = getEventByteString2(eventByte);
			
			if(strcmp(eventName, "UNKNOWN_EVENT") != 0) {
				ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

				bool processResult = PROCESS_SNAPSHOT_EVENT2(eventData, eventByte);
				ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT: %d", processResult);
				// Si el paquete no es el ultimo, se solicita el siguiente
				if (processResult) {
					// Se solicita el siguiente paquete
					ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
					REQUEST_SNAPSHOT2();
				}

				free(eventData);
			} else {
				ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
			}
		} else
		{
			ESP_LOGI("MDSM7_RX", "Paquete no valido");
		}
		// HASTA ACA
	}

	gpio_set_level(LED_G, 0);
	free(data);
}




