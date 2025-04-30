/*
product:	GWI-201
version:	1.0
date:		12/11/2021
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "soc/uart_struct.h"
#include "string.h"
#include <stdio.h>
#include "esp_log.h"

#include "config.h"
#include "FMS.c"
#include "MOVON_MDAS9.c"
#include "MOVON_MDSM7.c"


// Incluye solo los encabezados necesarios; elimina los innecesarios como "driver/uart.h", "soc/uart_struct.h", "string.h", y <stdio.h> si no los usas.

#define LED_G   GPIO_NUM_13  // Definir los pines GPIO específicos
#define LED_Y   GPIO_NUM_15

void config() {
    // Configurar pines GPIO
    gpio_set_direction(LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_Y, GPIO_MODE_OUTPUT);

    // Configurar niveles iniciales
    gpio_set_level(LED_G, 1);  // Encender LED_G (nivel alto)
    gpio_set_level(LED_Y, 1);  // Encender LED_Y (nivel alto)
}

void init() {
    // Apagar LEDs al inicio
    gpio_set_level(LED_G, 0);  // Apagar LED_G (nivel bajo)
    gpio_set_level(LED_Y, 0);  // Apagar LED_Y (nivel bajo)
}

void app_main() {
    config();  // Configurar pines y niveles
    init();    // Inicializar estados iniciales de los LEDs

    // Inicializar otros módulos
    FMS_init();
    MDAS9_init();
    MDSM7_init();

    // Esperar un tiempo antes de continuar
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Loguear información del dispositivo
    ESP_LOGI("Device", "GWI-202 v1.0");

    // Crear tareas de recepción para MDAS9 y MDSM7
    xTaskCreate(MDAS9_rxTask, "MDAS9_rxTask", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(MDSM7_rxTask, "MDSM7_rxTask", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
