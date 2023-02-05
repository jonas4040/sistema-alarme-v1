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

char **MENSAGEM_RECEBIDA;
char *TOPICO_MSG_RECEBIDA="";

#define BROKER_URL CONFIG_BROKER_URI
#define BROKER_USR CONFIG_BROKER_USERNAME
#define BROKER_PASSWD CONFIG_BROKER_PASSWD
#define BUFFER_SIZE 4096

uint8_t buffer[BUFFER_SIZE];

#if CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t ca_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END CERTIFICATE-----";
#else
extern const uint8_t ca_pem_start[]   asm("_binary_ca_pem_start");
#endif
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

void testaMem();
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
    char *msg_data = "";

    int msg_id = esp_mqtt_client_subscribe(client,topico,qos);
    ESP_LOGI(TAG, "Dado recebido com o seguinte id e mensagem =%d \n\t\t%s", msg_id,msg_data);
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
        esp_mqtt_client_subscribe(client, "casa/quarto1/#", 1);
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
        mqtt_callback(event);//TODO apagar

        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        // printf("DADO MENSAGEM = %.*s\r\n", event->data_len, event->data);
        //TODO APAGAR // MENSAGEM_RECEBIDA = event->data;
        // TOPICO_MSG_RECEBIDA=event->topic;
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

void mqtt_callback(esp_mqtt_event_handle_t event) {
    int msg_len = event->data_len;
    if (msg_len > BUFFER_SIZE) {
        ESP_LOGE(TAG, "MENSAGEM GRANDE DEMAIS, IGNORANDO ...");
        return;
    }
    memcpy(buffer, event->data, min(msg_len, BUFFER_SIZE));
    buffer[min(msg_len, BUFFER_SIZE)] = '\0';

    //PERCORRE AS MENSAGENS
    //char **iStr = MENSAGEM_RECEBIDA;
    size_t i = 0;
    //while(*iStr){
        sprintf(MENSAGEM_RECEBIDA[i],"%s \r",event->data);
        //sprintf(TOPICO_MSG_RECEBIDA,"%s\r",event->topic);
        ++i;
        //++iStr;
    //}
        
    
    memset(buffer, 0, BUFFER_SIZE);
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

    ESP_LOGI(TAG, "[APP] Free HEAP memory: %d bytes", (int)esp_get_free_heap_size());
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

}

size_t min(int num1,int num2){
    return num1 < num2 ? num1 : num2;
}

void testaMem(){
    printf("\t\tTESTANDO ...\n");
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", (int)esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
}