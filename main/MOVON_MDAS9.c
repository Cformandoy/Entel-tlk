#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h"

uint8_t MDAS9_last_data[37];
uint8_t MDSM7_last_data[46];

void MDAS9_init()
{
	const uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
	uart_param_config(UART_MDAS9, &uart_config);
	uart_set_pin(UART_MDAS9, UART_MDAS9_TXD_PIN, UART_MDAS9_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install(UART_MDAS9, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}



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
		case 0x00:
			FMS_sendData("MDSM7_RX", "[EVENTO: DROWSINESS]");
			return "DROWSINESS";
		case 0x01:
			FMS_sendData("MDSM7_RX", "[EVENTO: DISTRACTION]");
			return "DISTRACTION";
		case 0x02:
			FMS_sendData("MDSM7_RX", "[EVENTO: YAWNING]");
			return "YAWNING";
		case 0x03:
			FMS_sendData("MDSM7_RX", "[EVENTO: CELL_PHONE_USE]");
			return "CELL_PHONE_USE";
		case 0x04:
			FMS_sendData("MDSM7_RX", "[EVENTO: SMOKING]");
			return "SMOKING";
		case 0x05:
			FMS_sendData("MDSM7_RX", "[EVENTO: SEAT_BELT]");
			return "CAMERA_BLOCKED";
		default:
			FMS_sendData("MDSM7_RX", "[EVENTO: UNKNOWN_EVENT]");
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

		FMS_sendData("MDSM7_RX", "[PROCESS_SNAPSHOT_EVENT]");

		// aqui se procesa el evento

		if(actualPicturePackage != totalPicturePackages) {
			// solicitar siguiente paquete
			ESP_LOGI("MDSM7_RX", "Paquete %d de %d", actualPicturePackage, totalPicturePackages);
			FMS_sendData("MDSM7_RX", "[PROCESS_SNAPSHOT_EVENT - Aqui se procesa el evento]");
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
	FMS_sendData("MDSM7_RX", "[REQUEST_SNAPSHOT - ACK]");
	// Crear un buffer para la representación en texto
	char ackString[12];  // Suficiente para "1A 01 79 5D\0"

	// Convertir los valores del arreglo a formato hexadecimal
	snprintf(ackString, sizeof(ackString), "%02X %02X %02X %02X", ack[0], ack[1], ack[2], ack[3]);

	FMS_sendData("MDSM7_RX", ackString);

	// Enviar el ACK al UART
    int txBytes = uart_write_bytes(UART_MDAS9, (const char *)ack, sizeof(ack));
    char txBytesString[50];
    snprintf(txBytesString, sizeof(txBytesString), "Bytes enviados: %d", txBytes);
	FMS_sendData("MDSM7_RX", ["REQUEST_SNAPSHOT - Bytes enviados]");
    FMS_sendData("MDSM7_RX", txBytesString);

	if (txBytes != sizeof(ack)) {
        FMS_sendData("MDSM7_RX", "[Error: No se enviaron todos los bytes del ACK]");
        return;
    }

	// Leer respuesta del dispositivo


	uint8_t *dataResponse = (uint8_t *)malloc(RX_BUF_SIZE + 1);


	int rxBytesResponse = uart_read_bytes(UART_MDAS9, dataResponse, sizeof(dataResponse), 1000 / portTICK_PERIOD_MS);

	if (rxBytesResponse > 0) {
        FMS_sendData("MDSM7_RX", "[Respuesta recibida]");
        char responseString[256];  // Buffer para la respuesta en formato hexadecimal
        int offset = 0;

        for (int i = 0; i < rxBytesResponse; i++) {
            offset += snprintf(responseString + offset, sizeof(responseString) - offset, "%02X ", dataResponse[i]);
            if (offset >= sizeof(responseString)) {
                break;  // Evitar desbordamiento del buffer
            }
        }

        FMS_sendData("MDSM7_RX", responseString);
    } else {
        FMS_sendData("MDSM7_RX", "[No se recibió respuesta]");
    }


	ESP_LOGI("MDSM7_RX", "REQUEST_SNAPSHOT_END");
}





static void MDAS9_rxTask()
{
	ESP_LOGI("MDAS_9", "Init RX Task");
	uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);

	while (1)
	{
		const int rxBytes = uart_read_bytes(UART_MDAS9, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
		// ESP_LOGI("APP", "%s", (const char *)data );
		// ESP_LOGI( "GO_RX", "%d", rxBytes );
		// MDAS9


		// MDSM7 LITE
		if (rxBytes == 5)
		{
			// ESP_LOGI("MDSM7_RX", "L:%d DATA: %x %x %x ALARM: %x STATUS: %x", rxBytes, data[0], data[1], data[2], data[8], data[29]);

			// valid Data frame
			if (data[0] == 0x5b && data[1] == 0x79 && data[2] == 0x42 && data[4] == 0x5f)
			{	
				FMS_sendData( "MDSM7_RX", "[1- LITE ]");

				gpio_set_level(LED_G, 1);

				unsigned char eventByte = data[3];
				const char* eventName = getEventByteString(eventByte);
				ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

				// send eventName to FMS
				FMS_sendData("MDSM7_RX", "[2 - EVENTO]");
				gpio_set_level(LED_G, 0);
			}else{
				FMS_sendData( "MDSM7_RX", "[paquete no valido]" );
			}
		}
		// NUEVO MDSM7 con dato y imagen
		else if (data[0] == 0x5B && data[1] == 0x79 && data[2] == 0x42 && data[4] == 0x5F && rxBytes > 5){
			gpio_set_level(LED_G, 1);

			FMS_sendData( "MDSM7_RX", "[1- DATO Y IMAGEN ]");
			// se utiliza la data eliminando las primeras 5 posiciones
			unsigned char* eventData = &data[5];
			int eventDataLen = rxBytes - 5;

			unsigned char eventByte = eventData[3];
			const char* eventName = getEventByteString(eventByte);

			// Verificar si el evento es uno de los validos
			if(strcmp(eventName, "UNKNOWN_EVENT") != 0) {
				ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

				int packetCount = 0;
				Packet* packets = extractPackets(eventData, eventDataLen, &packetCount);

				ESP_LOGI("MDSM7_RX", "Total paquetes: %d", packetCount);
				FMS_sendData("MDSM7_RX", "[3 - Total paquetes]");
				for (int i = 0; i < packetCount; i++) {
					ESP_LOGI("MDSM7_RX", "Paquete %d (len %d): ", i + 1, packets[i].length);

					bool processResult = PROCESS_SNAPSHOT_EVENT(packets[i].data, eventByte);
					ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT: %d", processResult);
					// Si el paquete no es el ultimo, se solicita el siguiente
					if (processResult) {
						// Se solicita el siguiente paquete
						FMS_sendData("MDSM7_RX", "[4 - Se solicita el siguiente paquete]");
						ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
						REQUEST_SNAPSHOT();
					}
					free(packets[i].data);
				}

				free(packets);


			} else {
				ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
			}
			gpio_set_level(LED_G, 0);
		}
		else if (data[0] == 0x5A && data[1] == 0x79 && data[2] == 0x02 && data[4] == 0x04){
		{		
				gpio_set_level(LED_G, 1);

				FMS_sendData( "MDSM7_RX", "[1- IMAGEN ]");


				unsigned char* eventData = data;
				int eventDataLen = rxBytes;
				ESP_LOGI("MDSM7_RX", "eventDataLen: %d", eventDataLen);
	
				unsigned char eventByte = eventData[3];
				const char* eventName = getEventByteString(eventByte);

				if(strcmp(eventName, "UNKNOWN_EVENT") != 0) {
					ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);
	
					bool processResult = PROCESS_SNAPSHOT_EVENT(eventData, eventByte);
					ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT");
					// Si el paquete no es el ultimo, se solicita el siguiente
					if (processResult) {
						// Se solicita el siguiente paquete
						ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
						FMS_sendData("MDSM7_RX", "[4 - Se solicita el siguiente paquete]");
						REQUEST_SNAPSHOT();
					}
	
					free(eventData);
				} else {
					ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
				}
				gpio_set_level(LED_G, 0);
			}
		} else {
			ESP_LOGI("MDSM7_RX", "Paquete no valido");
			FMS_sendData( "MDSM7_RX", "[paquete no valido]" );
		}


	}
	free(data);
}
