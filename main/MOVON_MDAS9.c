#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h"

uint8_t MDAS9_last_data[37];
uint8_t MDSM7_last_data[46];

QueueHandle_t uart_queue;

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
	uart_driver_install(UART_MDAS9, RX_BUF_SIZE * 2, 0, 20, &uart_queue, 0);
	// uart_driver_install(UART_MDAS9, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

typedef struct
{
	unsigned char *data;
	int length;
} Packet;

Packet *extractPackets(unsigned char *buffer, int bufferLen, int *outPacketCount)
{
	int i = 0;
	int capacity = 10;
	int count = 0;

	Packet *packets = (Packet *)malloc(sizeof(Packet) * capacity);

	while (i < bufferLen - 4)
	{ // -4 para poder validar inicio completo
		// Verificar inicio válido
		if (buffer[i] == 0x5A && buffer[i + 1] == 0x79 && buffer[i + 2] == 0x02 && buffer[i + 4] == 0x04)
		{
			int start = i;
			int end = i;

			while (end < bufferLen)
			{
				if (buffer[end] == 0x5D)
				{
					// Caso A: 0x5D seguido de nuevo paquete válido
					if ((end + 4) < bufferLen &&
						buffer[end + 1] == 0x5A &&
						buffer[end + 2] == 0x79 &&
						buffer[end + 3] == 0x02 &&
						buffer[end + 5] == 0x04)
					{
						end++; // incluir el 0x5D
						break;
					}

					// Caso B: 0x5D es el último byte
					if (end == bufferLen - 1)
					{
						end++; // incluir el 0x5D
						break;
					}
				}
				end++;
			}

			if (end <= bufferLen)
			{
				int packetLen = end - start;

				// Redimensionar si es necesario
				if (count >= capacity)
				{
					capacity *= 2;
					packets = (Packet *)realloc(packets, sizeof(Packet) * capacity);
				}

				packets[count].data = (unsigned char *)malloc(packetLen);
				memcpy(packets[count].data, &buffer[start], packetLen);
				packets[count].length = packetLen;
				count++;

				i = end;
			}
			else
			{
				break; // paquete incompleto
			}
		}
		else
		{
			i++;
		}
	}

	*outPacketCount = count;
	return packets;
}

// get string event byte
const char *getEventByteString(unsigned char eventByte)
{
	switch (eventByte)
	{
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

	if (data[0] == 0x5A && data[1] == 0x79 && data[2] == 0x02 && data[3] == eventByte && data[4] == 0x04)
	{
		int actualPicturePackage = data[7];
		int totalPicturePackages = data[8];

		FMS_sendData("MDSM7_RX", "[PROCESS_SNAPSHOT_EVENT]");

		// aqui se procesa el evento

		if (actualPicturePackage != totalPicturePackages)
		{
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

bool REQUEST_SNAPSHOT()
{
	ESP_LOGI("MDSM7_RX", "REQUEST_SNAPSHOT_START");

	// Definir ACK según protocolo Movon
	const uint8_t ack[4] = {0x1A, 0x01, 0x79, 0x5D};

	FMS_sendData("MDSM7_RX", "[Enviando ACK]");

	// Enviar por UART
	int txBytes = uart_write_bytes(UART_MDAS9, (const char *)ack, sizeof(ack));
	if (txBytes == sizeof(ack))
	{
		FMS_sendData("MDSM7_RX", "[ACK enviado correctamente]");
	}
	else
	{
		FMS_sendData("MDSM7_RX", "[ACK parcial o fallido]");
	}
	FMS_sendData("MDSM7_RX", "[REQUEST_SNAPSHOT_END]");

	return true;
}

bool procesar_evento(unsigned char *data, int len)
{

	// MDSM7 LITE
	if (len == 5)
	{
		// ESP_LOGI("MDSM7_RX", "L:%d DATA: %x %x %x ALARM: %x STATUS: %x", len, data[0], data[1], data[2], data[8], data[29]);

		// valid Data frame
		if (data[0] == 0x5b && data[1] == 0x79 && data[2] == 0x42 && data[4] == 0x5f)
		{
			FMS_sendData("MDSM7_RX", "[1- LITE ]");

			gpio_set_level(LED_G, 1);

			unsigned char eventByte = data[3];
			const char *eventName = getEventByteString(eventByte);
			ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);
			ESP_LOGI("MDSM7_RX", "EVENTO: %s", eventName);
			// send eventName to FMS
			FMS_sendData("MDSM7_RX", "[2 - EVENTO]");
			gpio_set_level(LED_G, 0);
		}
		else
		{
			ESP_LOGI("MDSM7_RX", "Paquete no valido");
		}
	}
	// NUEVO MDSM7 con dato y imagen
	else if (data[0] == 0x5B && data[1] == 0x79 && data[2] == 0x42 && data[4] == 0x5F && len > 5)
	{
		gpio_set_level(LED_G, 1);

		FMS_sendData("MDSM7_RX", "[1- DATO Y IMAGEN ]");
		// se utiliza la data eliminando las primeras 5 posiciones
		unsigned char *eventData = &data[5];
		int eventDataLen = len - 5;

		unsigned char eventByte = eventData[3];
		const char *eventName = getEventByteString(eventByte);

		// Verificar si el evento es uno de los validos
		if (strcmp(eventName, "UNKNOWN_EVENT") != 0)
		{
			ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

			int packetCount = 0;
			Packet *packets = extractPackets(eventData, eventDataLen, &packetCount);

			ESP_LOGI("MDSM7_RX", "Total paquetes: %d", packetCount);
			FMS_sendData("MDSM7_RX", "[3 - Total paquetes]");
			for (int i = 0; i < packetCount; i++)
			{
				ESP_LOGI("MDSM7_RX", "Paquete %d (len %d): ", i + 1, packets[i].length);

				bool processResult = PROCESS_SNAPSHOT_EVENT(packets[i].data, eventByte);
				ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT: %d", processResult);
				// Si el paquete no es el ultimo, se solicita el siguiente
				if (processResult)
				{
					// Se solicita el siguiente paquete
					FMS_sendData("MDSM7_RX", "[4 - Se solicita el siguiente paquete]");
					ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
					REQUEST_SNAPSHOT();
				}
				free(packets[i].data);
			}

			free(packets);
		}
		else
		{
			ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
		}
		gpio_set_level(LED_G, 0);
	}
	else if (data[0] == 0x5A && data[1] == 0x79 && data[2] == 0x02 && data[4] == 0x04)
	{
		{
			gpio_set_level(LED_G, 1);

			FMS_sendData("MDSM7_RX", "[1- IMAGEN ]");

			unsigned char *eventData = data;
			int eventDataLen = len;
			ESP_LOGI("MDSM7_RX", "eventDataLen: %d", eventDataLen);

			unsigned char eventByte = eventData[3];
			const char *eventName = getEventByteString(eventByte);

			if (strcmp(eventName, "UNKNOWN_EVENT") != 0)
			{
				ESP_LOGI("MDSM7_RX", "EVENTO: %02X", eventByte);

				bool processResult = PROCESS_SNAPSHOT_EVENT(eventData, eventByte);
				ESP_LOGI("MDSM7_RX", "PROCESS_SNAPSHOT_EVENT");
				// Si el paquete no es el ultimo, se solicita el siguiente
				if (processResult)
				{
					// Se solicita el siguiente paquete
					ESP_LOGI("MDSM7_RX", "Solicitando siguiente paquete");
					FMS_sendData("MDSM7_RX", "[4 - Se solicita el siguiente paquete]");
					bool request_response = REQUEST_SNAPSHOT();
					if (request_response)
					{
						FMS_sendData("MDSM7_RX", "[5 - Solicitud de imagen enviada]");
						ESP_LOGI("MDSM7_RX", "Solicitud de imagen enviada");
					}
					else
					{
						FMS_sendData("MDSM7_RX", "[Error al solicitar la imagen]");
						ESP_LOGI("MDSM7_RX", "Error al solicitar la imagen");
					}
				}
			}
			else
			{
				ESP_LOGI("MDSM7_RX", "EVENTO DESCONOCIDO: %02X", eventByte);
			}
			gpio_set_level(LED_G, 0);
		}
	}
	else if (data[0] == 0x1A && data[2] == 0x79 && data[3] == 0x5D)
	{
		// i need send data to FMS
		FMS_sendData("MDSM7_RX", "[ACK]");
		if (data[1] == 0x00)
		{
			FMS_sendData("MDSM7_RX", "[ACK - 0]");
		}
		if (data[1] == 0x01)
		{
			FMS_sendData("MDSM7_RX", "[ACK - 1]");
		}
		if (data[1] == 0x02)
		{
			FMS_sendData("MDSM7_RX", "[ACK - 2]");
		}
		if (data[1] == 0x03)
		{
			FMS_sendData("MDSM7_RX", "[ACK - 3]");
		}
		if (data[1] == 0x04)
		{
			FMS_sendData("MDSM7_RX", "[ACK - 4]");
		}
		ESP_LOGI("MDSM7_RX", "ACK");
	}
	else
	{
		ESP_LOGI("MDSM7_RX", "Paquete no valido");
		// FMS_sendData( "MDSM7_RX", "[NV]" );
	}

	return true;
}

static void MDAS9_rxTask()
{

	ESP_LOGI("MDAS_9", "Init RX Task");

	uart_event_t event;
	uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);

	FMS_sendData("MDSM7_RX", "[Init RX Task]");

	while (1)
	{
		FMS_sendData("MDSM7_RX", "[Esperando evento]");

		if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY))
		{
			FMS_sendData("MDSM7_RX", "[Evento recibido]");
			switch (event.type)
			{
			case UART_DATA:
			{
				FMS_sendData("MDSM7_RX", "[Recibiendo datos]");
				int len = uart_read_bytes(UART_MDAS9, data, RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
				char logBuffer[64];
				snprintf(logBuffer, sizeof(logBuffer), "[uart_read_bytes len: %d]", len);
				FMS_sendData("MDSM7_RX", logBuffer);
				if (len > 0)
				{
					FMS_sendData("MDSM7_RX", "[Datos recibidos]");

					// Procesar el evento
					if (procesar_evento(data, len))
					{
						FMS_sendData("MDSM7_RX", "[Evento procesado]");
						ESP_LOGI("MDAS_9", "Evento procesado");
					}
					else
					{
						FMS_sendData("MDSM7_RX", "[Error al procesar el evento]");
						ESP_LOGI("MDAS_9", "Error al procesar el evento");
					}
				}
				// Se recibe un paquete de datos
				break;
			}
			default:
			{
				char buffer[64];
				snprintf(buffer, sizeof(buffer), "[Evento no UART_DATA: %d]", event.type);
				FMS_sendData("MDSM7_RX", buffer);
				break;
			}
			}
		}
		
	}
	gpio_set_level(LED_G, 0); // Apagar LED_G (nivel bajo)
	free(data);
}
