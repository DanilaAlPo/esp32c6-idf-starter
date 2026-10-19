#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_app_desc.h"

// Встроенный светодиод на ESP32-C6 DevKit обычно подключён к GPIO8
#define BLINK_GPIO 8

// Дескриптор светодиодной ленты
static led_strip_handle_t led_strip;

// Инициализация светодиода WS2812
void configure_led(void)
{
    // Конфигурация светодиодной ленты
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // На плате только один светодиод
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .timings = {}, // <--- Добавьте эту строку для инициализации поля timings
        .flags = {
            .invert_out = false,
        }
    };

    // Конфигурация бэкенда RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }
    };

    // Создание устройства светодиодной ленты
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Очистка (выключение) светодиода
    led_strip_clear(led_strip);
}

extern "C" void app_main(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    printf("Firmware Version: %s\n", app_desc->version);
    // Отключение буферизации stdout для удобства наблюдения за логами
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== ESP32-C6 LED Strip Test ===\n");

    // Инициализация светодиода
    configure_led();

    int led_state = 0;

    while (1) {
        if (led_state) {
            // Включение: установка красного цвета (R=50, G=0, B=0)
            // Значение 50 не максимальное, чтобы не было слишком ярко
            led_strip_set_pixel(led_strip, 0, 50, 0, 0);
            led_strip_refresh(led_strip);
            printf("LED: ON\n");
        } else {
            // Выключение
            led_strip_clear(led_strip);
            printf("LED: OFF\n");
        }

        // Переключение состояния
        led_state = !led_state;
        
        // Задержка 1 секунда
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
