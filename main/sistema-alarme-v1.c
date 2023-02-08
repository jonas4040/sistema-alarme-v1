#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi-station.h"
#include "mqtt-tls.h"
#include "ds18b20.h"
#include "cJSON.h"

//#include "cJSON_Utils.h"

//======VARIAVEIS E CONSTANTES============

#define BTN_WIN1 5 // da janela quarto 1
#define BTN_RESET 6
#define LED_RESET 39
#define ALARM 1
#define SENSOR_TEMPERATURA 18
#define OUTPUT_BIT_MASK (1ULL << ALARM) | (1ULL << LED_RESET)
#define INPUT_BIT_MASK (1ULL << BTN_WIN1) | (1ULL << BTN_RESET)

const char* TAG_JSON="cJSON";

static QueueHandle_t win1_open_evt_queue = NULL, reset_evt_queue;
uint8_t estadoJanQuarto1 = 0, i = 10;/** @param estadoJanQuarto1 == 0 eh fechada a janela **/
extern char *MENSAGEM_RECEBIDA;
extern char *TOPICO_MSG_RECEBIDA;
//=============FUNÇOES====================
void init_io(void);
static void win1_isr_handler(void *);
static void reset_isr_handler(void *);

static void tocaAlarmeTask(void *);
static void btnResetTask(void *);
static void temperaturaTask(void *);
static void mqttTask(void *);

void leTempFake();
float leTemperatura();
uint8_t verificaReset(uint8_t);

//    bool msgRec=true;

void app_main(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_io();

    win1_open_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    reset_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //srand(time(NULL)); // Initialization, should only be called once.

    ds18b20_init(SENSOR_TEMPERATURA);
    wifi_inicializa();
    mqtt_app_start();

    xTaskCreatePinnedToCore(tocaAlarmeTask, "tocaAlarmeTask", 2048, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(btnResetTask, "btnResetTask", 2048, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(temperaturaTask, "temperaturaTask", 4096, NULL, 10, NULL, 1);
    //xTaskCreatePinnedToCore(mqttTask,"mqttTask",6144,NULL,10,NULL,0);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_WIN1, win1_isr_handler, (void *)BTN_WIN1);
    gpio_isr_handler_add(BTN_RESET, reset_isr_handler, (void *)BTN_RESET);
    
    // while(1){
        
    // }

}

void init_io(){
    gpio_config_t pin_config = {};  

    // INPUTS

    pin_config.intr_type = GPIO_INTR_POSEDGE;
    pin_config.mode = GPIO_MODE_INPUT;
    pin_config.pin_bit_mask = INPUT_BIT_MASK;
    pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pin_config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&pin_config);

    // OUTPUTS
    pin_config.intr_type = GPIO_INTR_DISABLE;
    pin_config.mode = GPIO_MODE_OUTPUT;
    pin_config.pin_bit_mask = OUTPUT_BIT_MASK;
    gpio_config(&pin_config);
}

static void win1_isr_handler(void *args){
    uint32_t pino = (uint32_t)args;
    xQueueSendFromISR(win1_open_evt_queue, &pino, NULL);
}

static void reset_isr_handler(void *args){
    uint32_t pino = (uint32_t)args;
    xQueueSendFromISR(reset_evt_queue, &pino, NULL);
}

static void mqttTask(void *arg){
    while(1){
        //msgRec=rand() & 1;S
        // char **jStr = MENSAGEM_RECEBIDA;
        // size_t j = 0;
        // while(*jStr){
        //     //msgJSON = cJSON_Parse(MENSAGEM_RECEBIDA[j]);
        //     if(*(*MENSAGEM_RECEBIDA + j))
        //         printf("MSG RECEBIDA: %.*s\r\n",strlen(*(MENSAGEM_RECEBIDA + j)),*(MENSAGEM_RECEBIDA + j));
        //     else
        //         ESP_LOGE(__func__,"Erro ao mostrar mensagem \a\n");
        //     // ++j;
        //     // ++jStr;
        // }
        //bool alarmeLigado = msgRec;

        cJSON *msgJSON = cJSON_Parse((char *)MENSAGEM_RECEBIDA);
        cJSON *alarmeLigado = cJSON_GetObjectItem(msgJSON,"ligado");
        
        if(cJSON_IsFalse(alarmeLigado)){
            uint32_t numPino;
            xQueueSend(reset_evt_queue, &numPino, portTICK_PERIOD_MS);
        }

        cJSON_Delete(msgJSON);
    }
}

static void btnResetTask(void *arg){
    uint32_t numPino;
    while (1){
        if (xQueueReceive(reset_evt_queue, &numPino, portMAX_DELAY)){
            estadoJanQuarto1 = 0;
        }

        verificaReset(estadoJanQuarto1);
    }
}

static void tocaAlarmeTask(void *arg){
    uint32_t numPino;
    while (1){
        if (xQueueReceive(win1_open_evt_queue, &numPino, portMAX_DELAY)){
            estadoJanQuarto1 = 1;
        }

        // TODO: TOCAR BUZZER OU AUTO-FALANTE
        while (estadoJanQuarto1){
            gpio_set_level(ALARM, 1);
            vTaskDelay(80 / portTICK_PERIOD_MS);
            gpio_set_level(ALARM, 0);
            vTaskDelay(80 / portTICK_PERIOD_MS);
        }
    }
}

static void temperaturaTask(void *args ){
    float temperatura = 25.0;
    char *jsonTempr="", *jsonEstadoAlarme="";
    while(1){
        temperatura = leTemperatura();
        vTaskDelay(700/portTICK_PERIOD_MS);
        
        cJSON *msg = cJSON_CreateObject();
        cJSON *msgT = cJSON_AddNumberToObject(msg,"valor",temperatura);

        jsonTempr = cJSON_Print(msg);
        enviarJSON("casa/quarto1/temperatura",jsonTempr,1,0);
        cJSON_Delete(msg);
    }
}

/**
 * @brief Funcao que faz o reset do alarme
 * @param situacaoComodo estado da porta ou da janela
 * @returns 0 se nao resetou e 1 se foi resetado com sucesso
*/
uint8_t verificaReset(uint8_t situacaoComodo){
    uint8_t ret = 0;
    if(!situacaoComodo){
            for(uint8_t twice = 2;twice>0;twice--){
                gpio_set_level(LED_RESET, 1);
                vTaskDelay(80 / portTICK_PERIOD_MS);
                gpio_set_level(LED_RESET, 0);
                vTaskDelay(80 / portTICK_PERIOD_MS);
            }
        ret=1;
    }
    return ret;
}

/**
 * @brief le temperatura e o estado do alarme
 * @returns temperatura em graus Celsius em float
 * */

float leTemperatura(){
    return ds18b20_get_temp();
}

void leTempFake(){
    // char msgT[150],msgJP[150];
    // sprintf(msgT,"{\"valor\":\"%d\"}",i);
    // sprintf(msgJP,"{\"ligado\":\"%d\"}",true);
    // enviarMsg("casa/quarto1/temperatura",msgT,1,1);
    // enviarMsg("casa/quarto1/alarme",msgJP,1,1);
    i/=10;
    i--;
}