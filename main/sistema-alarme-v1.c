#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi-station.h"
#include "mqtt-tls.h"

//======VARIAVEIS E CONSTANTES============

#define BTN_WIN1 5 // da janela quarto 1
#define BTN_RESET 6
#define LED_RESET 39
#define ALARM 1
#define OUTPUT_BIT_MASK (1ULL << ALARM) | (1ULL << LED_RESET)
#define INPUT_BIT_MASK (1ULL << BTN_WIN1) | (1ULL << BTN_RESET)

static QueueHandle_t win1_open_evt_queue = NULL, reset_evt_queue;
uint8_t estadoJanQuarto1 = 0, i = 10;
char *msgRecebida = "";
extern char *MENSAGEM_RECEBIDA;
extern char *TOPICO_MSG_RECEBIDA;
//=============FUNÇOES====================
void init_io(void);
static void win1_isr_handler(void *);
static void reset_isr_handler(void *);
static void tocaAlarmeTask(void *);
static void btnResetTask(void *);
void leTempFake();

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
    xTaskCreatePinnedToCore(tocaAlarmeTask, "tocaAlarmeTask", 2048, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(btnResetTask, "btnResetTask", 2048, NULL, 10, NULL, 1);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_WIN1, win1_isr_handler, (void *)BTN_WIN1);
    gpio_isr_handler_add(BTN_RESET, reset_isr_handler, (void *)BTN_RESET);

    wifi_inicializa();
    mqtt_app_start();
    
    while(1){
        leTempFake();
        vTaskDelay(700/portTICK_PERIOD_MS);

        // printf("\t\tTOPICO: %s\r\n", TOPICO_MSG_RECEBIDA);
        // printf("\t\tMENSAGEM: %s\r\n", MENSAGEM_RECEBIDA);
        sprintf(msgRecebida,"%.*s\r",strlen(MENSAGEM_RECEBIDA),MENSAGEM_RECEBIDA);
        //*msgRecebida++='\0';

        //TODO USAR JSON 
        if(strncmp(msgRecebida,"{\"ligado\": \"false\"}", strlen(msgRecebida)) == 0 ){
            estadoJanQuarto1=0;
        }else{
            printf("\t\tCOMPARACAO: %d\n",strncmp(msgRecebida,"{\"ligado\":\"false\"}", strlen(msgRecebida)));
            printf("NAO FUNFOU \n\t\t%s\n",msgRecebida) ;
        }
    }

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

static void btnResetTask(void *arg){
    uint32_t numPino;
    while (1){
        if (xQueueReceive(reset_evt_queue, &numPino, portMAX_DELAY)){
            estadoJanQuarto1 = 0;
        }

        if(!estadoJanQuarto1){
            for(uint8_t twice = 0;twice < 2;twice++){
                gpio_set_level(LED_RESET, 1);
                vTaskDelay(80 / portTICK_PERIOD_MS);
                gpio_set_level(LED_RESET, 0);
                vTaskDelay(80 / portTICK_PERIOD_MS);
            }
        }
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

void leTempFake(){
     char msg[150];
      sprintf(msg,"{\"valor\":\"%d\"}",i);
      enviarMsg("casa/temperatura/quarto1",msg,1,1);
      i/=10;
      i--;
}