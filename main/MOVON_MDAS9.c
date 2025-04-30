#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "driver/gpio.h"

uint8_t MDAS9_last_data[ 37 ];
uint8_t MDSM7_last_data[ 46 ];

void MDAS9_init() {
   const uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config( UART_MDAS9, &uart_config);
	uart_set_pin( UART_MDAS9, UART_MDAS9_TXD_PIN, UART_MDAS9_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	// We won't use a buffer for sending data.
	uart_driver_install( UART_MDAS9, RX_BUF_SIZE * 2, 0, 0, NULL, 0 );
}

static void MDAS9_rxTask() {
   ESP_LOGI( "MDAS_9", "Init RX Task" );
	uint8_t* data = (uint8_t*) malloc( RX_BUF_SIZE+1 );

	while (1) {
		const int rxBytes = uart_read_bytes( UART_MDAS9, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
		// ESP_LOGI("APP", "%s", (const char *)data );
		// ESP_LOGI( "GO_RX", "%d", rxBytes );
      // MDAS9
      if(rxBytes == 37 ) {
			// ESP_LOGI("MDAS_9", "L:%d DATA: %x %x REC: %x STATUS: %x", rxBytes, data[0], data[1], data[22], data[23]);

         // valid Data frame
			if( data[ 0 ] == 0x5b && data[ 1 ] == 0x49 && data[ 36 ] == 0x5d ) {
            gpio_set_level( LED_G, 1 );
				// Left LDW event
				if( data[ 10 ] == 0x02 && MDAS9_last_data[ 10 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[MOVON][LDW_L]");
					FMS_sendData( "MDAS_9", "[MOVON][LDW_L]");
            }
				// Right LDW event
				if( data[ 11 ] == 0x02 && MDAS9_last_data[ 11 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[MOVON][LDW_R]");
               FMS_sendData( "MDAS_9", "[MOVON][LDW_R]");
            }
				// SDA event
				if( data[ 17 ] == 0x02 && MDAS9_last_data[ 17 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[SDA]");
               FMS_sendData( "MDAS_9", "[MOVON][SDA]");
            }
				// FCW event
				if( data[ 20 ] == 0x02 && MDAS9_last_data[ 20 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[MOVON][FCW]");
               FMS_sendData( "MDAS_9", "[MOVON][FCW]");
            }
				// PCW event
				if( data[ 21 ] == 0x02 && MDAS9_last_data[ 21 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[MOVON][PCW]");
               FMS_sendData( "MDAS_9", "[MOVON][PCW]");
            }
            	// LOW_VISIBILITY event
				if( data[ 23 ] == 0x01 && MDAS9_last_data[ 23 ] != 0x01 ){
               ESP_LOGI("MDAS_9", "[MOVON][LOW_VISIBILITY]");
               FMS_sendData( "MDAS_9", "[MOVON][LOW_VISIBILITY]");
            }
				// CAMERA_BLOCKED event
				if( data[ 23 ] == 0x02 && MDAS9_last_data[ 23 ] != 0x02 ){
               ESP_LOGI("MDAS_9", "[MOVON][CAMERA_BLOCKED]");
               FMS_sendData( "MDAS_9", "[MOVON][CAMERA_BLOCKED]");

            }
				// SLR_recognize event
if (
    (data[33] == 0x1E ||
     data[33] == 0x28 ||
     data[33] == 0x32 ||
     data[33] == 0x3C ||
     data[33] == 0x46 ||    
     data[33] == 0x50 ||
     data[33] == 0x5A ||
     data[33] == 0x64 ||
     data[33] == 0x6E ||
     data[33] == 0x78
    ) && 
    (MDAS9_last_data[33] != 0x1E &&
     MDAS9_last_data[33] != 0x28 &&
     MDAS9_last_data[33] != 0x32 &&
     MDAS9_last_data[33] != 0x3C &&
     MDAS9_last_data[33] != 0x46 &&
     MDAS9_last_data[33] != 0x50 &&
     MDAS9_last_data[33] != 0x5A &&
     MDAS9_last_data[33] != 0x64 &&
     MDAS9_last_data[33] != 0x6E &&
     MDAS9_last_data[33] != 0x78
    )
) 
{
    ESP_LOGI("MDAS_9", "[MOVON] [SLR]");
    FMS_sendData("MDAS_9", "[MOVON] [SLR]");
}
				// copy data to MDAS9_last_data
				for (size_t i = 0; i < 37; i++) {
					MDAS9_last_data[ i ] = data[ i ];
				}
            gpio_set_level( LED_G, 0 );
			}
      }

      // MDSM7 LITE
      if( rxBytes == 5 ) {
			//ESP_LOGI("MDSM7_RX", "L:%d DATA: %x %x %x ALARM: %x STATUS: %x", rxBytes, data[0], data[1], data[2], data[8], data[29]);

         // valid Data frame
			if( data[ 0 ] == 0x5b && data[ 1 ] == 0x79 && data[ 2 ] == 0x42 && data[ 4 ] == 0x5f ){
            gpio_set_level( LED_G, 1 );

				if( (data[ 3 ] & 0x01 ) == 0x01 && (MDSM7_last_data[ 3 ] & 0x01 ) != 0x01 ){
					ESP_LOGI("MDSM7_RX", "[DROWSINESS]");
					FMS_sendData( "MDSM7_RX", "[DROWSINESS]");
				}

				if( (data[ 3 ] & 0x02 ) == 0x02 && (MDSM7_last_data[ 3 ] & 0x02 ) != 0x02 ){
					ESP_LOGI("MDSM7_RX", "[DISTRACTION]");
					FMS_sendData( "MDSM7_RX", "[DISTRACTION]");
				}

				if( (data[ 3 ] & 0x04 ) == 0x04 && (MDSM7_last_data[ 3 ] & 0x04 ) != 0x04 ){
					ESP_LOGI("MDSM7_RX", "[YAWNING]");
               FMS_sendData( "MDSM7_RX", "[YAWNING]");
				}

				if( (data[ 3 ] & 0x08 ) == 0x08 && (MDSM7_last_data[ 3 ] & 0x08 ) != 0x08 ){
					ESP_LOGI("MDSM7_RX", "[CELL_PHONE]");
               FMS_sendData( "MDSM7_RX", "[CELL_PHONE]");
				}

				if( (data[ 3 ] & 0x10 ) == 0x10 && (MDSM7_last_data[ 3 ] & 0x10 ) != 0x10 ){
					ESP_LOGI("MDSM7_RX", "[SMOKING]");
               FMS_sendData( "MDSM7_RX", "[SMOKING]");
				}

				if( (data[ 3 ] & 0x20 ) == 0x20 && (MDSM7_last_data[ 3 ] & 0x20 ) != 0x20 ){
               ESP_LOGI("MDSM7_RX", "[CAMERA_FAIL]");
               FMS_sendData( "MDSM7_RX", "[CAMERA_FAIL_MDSM]");
				}

            if( (data[ 3 ] & 0x40 ) == 0x40 && (MDSM7_last_data[ 3 ] & 0x40 ) != 0x40 ){
               ESP_LOGI("MDSM7_RX", "[CAMERA_BLOCKED]");
               FMS_sendData("MDSM7_RX", "[CAMERA_BLOCKED_MDSM]");
				}

			   for( size_t i = 0; i < 5; i++ ) {
					MDSM7_last_data[ i ] = data[ i ];
				}

            gpio_set_level( LED_G, 0 );
			}
      }

      // MDSM7 STANDARD
		if( rxBytes >= 46 && rxBytes < 92 ) {
			//ESP_LOGI("MDSM7_RX", "L:%d DATA: %x %x %x ALARM: %x STATUS: %x", rxBytes, data[0], data[1], data[2], data[8], data[29]);

         // valid Data frame
			if( data[ 0 ] == 0x5b && data[ 1 ] == 0x79 && data[ 45 ] == 0x5f ){
            gpio_set_level( LED_G, 1 );

				if( (data[ 8 ] & 0x01 ) == 0x01 && (MDSM7_last_data[ 8 ] & 0x01 ) != 0x01 ){
					ESP_LOGI("MDSM7_RX", "[DROWSINESS]");
					FMS_sendData( "MDSM7_RX", "[DROWSINESS]");
				}

				if( (data[ 8 ] & 0x02 ) == 0x02 && (MDSM7_last_data[ 8 ] & 0x02 ) != 0x02 ){
					ESP_LOGI("MDSM7_RX", "[DISTRACTION]");
					FMS_sendData( "MDSM7_RX", "[DISTRACTION]");
				}

				if( (data[ 8 ] & 0x04 ) == 0x04 && (MDSM7_last_data[ 8 ] & 0x04 ) != 0x04 ){
					ESP_LOGI("MDSM7_RX", "[YAWNING]");
               FMS_sendData( "MDSM7_RX", "[YAWNING]");
				}

				if( (data[ 8 ] & 0x08 ) == 0x08 && (MDSM7_last_data[ 8 ] & 0x08 ) != 0x08 ){
					ESP_LOGI("MDSM7_RX", "[CELL_PHONE]");
               FMS_sendData( "MDSM7_RX", "[CELL_PHONE]");
				}

				if( (data[ 8 ] & 0x10 ) == 0x10 && (MDSM7_last_data[ 8 ] & 0x10 ) != 0x10 ){
					ESP_LOGI("MDSM7_RX", "[SMOKING]");
               FMS_sendData( "MDSM7_RX", "[SMOKING]");
				}

				if( (data[ 8 ] & 0x20 ) == 0x20 && (MDSM7_last_data[ 8 ] & 0x20 ) != 0x20 ){
					ESP_LOGI("MDSM7_RX", "[NO_DRIVER]");
               FMS_sendData( "MDSM7_RX", "[NO_DRIVER]");
				}

				if( (data[ 29 ] & 0x01 ) == 0x01 && (MDSM7_last_data[ 29 ] & 0x01 ) != 0x01 ){
					ESP_LOGI("MDSM7_RX", "[CAMERA_FAIL_MDSM]");
               FMS_sendData( "MDSM7_RX", "[CAMERA_FAIL_MDSM]");
				}

				if( (data[ 29 ] & 0x10 ) == 0x10 && (MDSM7_last_data[ 29 ] & 0x10 ) != 0x10 ){
					ESP_LOGI("MDSM7_RX", "[CAMERA_BLOCKED_MDSM]");
               FMS_sendData( "MDSM7_RX", "[CAMERA_BLOCKED_MDSM]");
				}

			   for( size_t i = 0; i < 46; i++ ) {
					MDSM7_last_data[ i ] = data[ i ];
				}
            gpio_set_level( LED_G, 0 );
			}
      }
	}
	free( data );
}
