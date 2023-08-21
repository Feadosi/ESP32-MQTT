#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

static const char *TAG = "MQTT_EXAMPLE";
//int flag12 = -1, flag14 = -1;

//#define GPIO_OUTPUT_PIN_SEL  (1ULL<<CONFIG_BLINK_GPIO)
//#define GPIO_INPUT_PIN_SEL  ((1ULL<<CONFIG_REED_SWITCH_1) | (1ULL<<CONFIG_REED_SWITCH_2))
//#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;

#define toilet1 12
#define toilet2 14
char topic[] = "EncataRestroom";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        		msg_id = esp_mqtt_client_subscribe(client, topic, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_publish(client, topic, "Hi from ESP32", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        //msg_id = esp_mqtt_client_unsubscribe(client, "my_topic");
        //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    return client;
}

/*Обработчик прерываний на оба входа*/
/*
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}
*/
/*Задача для отслеживания состояния пинов и прерываний*/
/*
static void task1(void* arg)
{
    uint32_t io_num;

    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if(io_num == 12)
            {
                if(gpio_get_level(io_num) == 0) flag12 = 1;
                if(gpio_get_level(io_num) == 1) flag12 = 0;
            }
            if(io_num == 14)
            {
                if(gpio_get_level(io_num) == 0) flag14 = 1;
                if(gpio_get_level(io_num) == 1) flag14 = 0;
            }
        }
    }
}
*/

void app_main(void)
{
/*---------------------------Настроим обработку герконов-----------------------------------------------*/
     /*Настроим поля для пинов, работающих на вход*/
    /*
     gpio_config_t io_conf = {};
     io_conf.intr_type = GPIO_INTR_POSEDGE;
     io_conf.mode = GPIO_MODE_INPUT;
     io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
     io_conf.pull_up_en = 1;
     gpio_config(&io_conf);
     */

     /*Настроим тип прерываний пинов:нарастающий и спадающий(оба)*/
     //gpio_set_intr_type(CONFIG_REED_SWITCH_1, GPIO_INTR_ANYEDGE);
     //gpio_set_intr_type(CONFIG_REED_SWITCH_2, GPIO_INTR_ANYEDGE);

     /*Очередь для отправки состояния пинов*/
     //gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

     /*Задача для отслеживания состояния пинов и прерываний*/
     //xTaskCreate(task1, "task1", 4096, NULL, 10, NULL);

     /*Включим прерывания*/
     //gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

     /*Объявим обработчик прерываний для каждой ножки*/
     //gpio_isr_handler_add(CONFIG_REED_SWITCH_1, gpio_isr_handler, (void*) CONFIG_REED_SWITCH_1);
     //gpio_isr_handler_add(CONFIG_REED_SWITCH_2, gpio_isr_handler, (void*) CONFIG_REED_SWITCH_2);

/*---------------------------Настроим MQTT-----------------------------------------------*/
     ESP_ERROR_CHECK(nvs_flash_init());
     ESP_ERROR_CHECK(esp_netif_init());
     ESP_ERROR_CHECK(esp_event_loop_create_default());

     ESP_ERROR_CHECK(example_connect());                   /*Подключение к WIFI*/
     esp_mqtt_client_handle_t client = mqtt_app_start();  /*Запуск MQTT*/

     esp_mqtt_client_publish(client, topic, "Op1", 0, 1, 0);
     vTaskDelay(500 / portTICK_RATE_MS);
     esp_mqtt_client_publish(client, topic, "Op1", 0, 1, 0);
     while (1) {

         if(gpio_get_level(toilet1) == 0)
         {
             esp_mqtt_client_publish(client, topic, "Op1", 0, 1, 0);
         }

         if(gpio_get_level(toilet1) == 1)
         {
             esp_mqtt_client_publish(client, topic, "Cl1", 0, 1, 0);
         }

         if(gpio_get_level(toilet2) == 0)
         {
             esp_mqtt_client_publish(client, topic, "Op2", 0, 1, 0);
         }

         if(gpio_get_level(toilet2) == 1)
         {
             esp_mqtt_client_publish(client, topic, "Cl2", 0, 1, 0);
         }

         vTaskDelay(500 / portTICK_RATE_MS);
     }
}


