#include "driver/uart.h"
#include "soc/uart_struct.h"

uint8_t state[ 37 ];

void FMS_init() {
   state[ 0 ] = 0; // no new data
	const uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config( UART_FMS, &uart_config);
	uart_set_pin( UART_FMS, UART_FMS_TXD_PIN, UART_FMS_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install( UART_FMS, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

int FMS_sendData(const char* logName, const char* data) {
	ESP_LOGI(logName , "[1]");

	const int len = strlen(data);
	ESP_LOGI(logName , "[2]");

	const int txBytes = uart_write_bytes(UART_FMS, data, len);
	ESP_LOGI(logName , "[3]");
	return txBytes;
}
