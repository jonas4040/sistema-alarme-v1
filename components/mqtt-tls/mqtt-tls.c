/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>

#include "mqtt-tls.h"

static const char *TAG = "MQTTS_CNX";
esp_mqtt_client_handle_t client;

#define BROKER_URL CONFIG_BROKER_URI
#define BROKER_USR CONFIG_BROKER_USERNAME
#define BROKER_PASSWD CONFIG_BROKER_PASSWD

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t ca_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t ca_pem_start[]   asm("_binary_ca_pem_start");
#endif
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

/**
 * @brief FUNCAO PARA ENVIAR MENSAGEM E PUBLICAR EM UM TOPICO DEFINIDO
 * @param topico topico
 * @param msg dado a ser enviado em string ou JSON
 * @param qos qualidade do serviço: 0,1 ou 2
 * @param retain se salva (1) ou nao (0) o dado a ser enviado
*/
void enviarMsg(char *topico, char *msg, uint8_t qos, uint8_t retain){
    int msg_id = esp_mqtt_client_publish(client, topico, msg, 0, qos, retain);
    ESP_LOGI(TAG, "Dado enviado com o seguinte id de mensagem =%d", msg_id);
}

/**
 * @brief FUNCAO PARA RECEBER MENSAGEM EM UM TOPICO DEFINIDO
 * @param topico topico
 * @param qos qualidade do serviço: 0,1 ou 2
 * @return msg_data: mensagem recebida
*/
char* receberMsg(char *topico, uint8_t qos){
    char *msg_data = NULL;

    // int msg_id = esp_mqtt_client_subscribe(client,topico,qos);

    // uint8_t *msg_buf = client->mqtt_state.in_buffer;
    // size_t msg_read_len = client->mqtt_state.in_buffer_read_len;
    // size_t msg_data_len = msg_read_len;

    // msg_data = mqtt_get_publish_data(msg_buf, &msg_data_len);
    // ESP_LOGI(TAG, "Dado recebido com o seguinte id e mensagem =%d \n\t\t%s", msg_id,msg_data);
    return msg_data;
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
 void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado ! ");
        esp_mqtt_client_subscribe(client, "camara", 0);
        ESP_LOGI(TAG,"Subscreveu no topico!");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Desconectado !");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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
        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            ESP_LOGI(TAG, "Enviando . . . ");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

 void mqtt_app_start(void){
    //testaMem();//verbose function

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = BROKER_URL,
            .verification.certificate = (const char *)ca_pem_start,
            .verification.skip_cert_common_name_check = true
        },
        .credentials = {
            .username = BROKER_USR,
            .authentication.password= BROKER_PASSWD
        }

    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", (int)esp_get_free_heap_size());
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}