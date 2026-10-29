#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "github_ota.h"
#include "esp_https_ota.h"   // Для событий ESP_HTTPS_OTA_EVENT и констант ESP_HTTPS_OTA_START

// Эти три заголовка нужны для NVS и сетевых функций
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_ota_ops.h"


#include "esp_wifi.h"

#define WIFI_SSID      "MyWeb"       // Замените на имя вашей сети
#define WIFI_PASS      "klop90polka??."     // Замените на пароль от Wi-Fi
#define WIFI_MAXIMUM_RETRY  5

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static const char *WIFI_TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
                                                        
        wifi_config_t wifi_config = {}; // Обнуляем структуру

    // Безопасно копируем строки в массивы
    snprintf((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char*)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASS);

    // Задаем тип шифрования
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Применяем настройки
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Ожидание подключения (или неудачи) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    }
}



static const char *TAG = "OTA_APP";

// (Опционально) Обработчик событий OTA для логирования прогресса
static void ota_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != ESP_HTTPS_OTA_EVENT) return;
    switch (id) {
        case ESP_HTTPS_OTA_START:    ESP_LOGI(TAG, "OTA started"); break;
        case ESP_HTTPS_OTA_CONNECTED: ESP_LOGI(TAG, "OTA connected"); break;
        case ESP_HTTPS_OTA_FINISH:   ESP_LOGI(TAG, "OTA finish"); break;
        case ESP_HTTPS_OTA_ABORT:    ESP_LOGW(TAG, "OTA abort"); break;
        default: break;
    }
}

void gitHubOTA()
{

    // 5. (КРИТИЧЕСКИ ВАЖНО) Подтверждение валидности текущей прошивки
    // Отменяет откат, если прошивка загрузилась и работает нормально
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "App marked as valid. Rollback cancelled.");
    }

    // Регистрация обработчика событий OTA (опционально)
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler, NULL));

    // ... (Здесь ваш существующий код подключения к Wi-Fi) ...
    // После того как Wi-Fi подключен и получен IP:

    ESP_LOGI(TAG, "WiFi connected, starting GitHub OTA...");

    // 3. Запуск API конфигурации (позволяет задать owner/repo/token через HTTP или NVS)
    github_config_api_start();

    // 4. Запуск поллера (он будет опрашивать GitHub Releases с интервалом, заданным в Kconfig)
    github_poller_start();

}




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

    // 1. Инициализация NVS (компонент хранит конфигурацию и состояние возобновления в NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Инициализация сетевого интерфейса и цикла событий
    ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());


    // 3. Подключение к Wi-Fi
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta(); // Эта функция ждет подключения

    // 4. Теперь Wi-Fi есть, запуск OTA
    gitHubOTA();

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
            led_strip_set_pixel(led_strip, 0, 0, 0, 50);
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
