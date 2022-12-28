#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"

//======VARIAVEIS E CONSTANTES============

#define BTN_WIN1 5
#define BTN_RESET 6
#define LED_RESET 39
#define ALARM 1
#define OUTPUT_BIT_MASK (1ULL << ALARM)  | ( 1ULL << LED_RESET)
#define INPUT_BIT_MASK  (1ULL << BTN_WIN1) | (1ULL << BTN_RESET)

static QueueHandle_t click_evt_queue = NULL;
uint8_t estadoJanQuarto1 = 0;

//=============FUNÇOES====================
void init_io(void);
static void isr_handler(void *);
static void tocaAlarmeTask(void *);
static void isJanelaAberta(void *);

void app_main(){
    init_io();

    click_evt_queue = xQueueCreate(10,sizeof(uint32_t));
    xTaskCreate(tocaAlarmeTask,"tocaAlarmeTask",2048,NULL,10,NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BTN_WIN1,isr_handler,(void *)BTN_WIN1);
}

void init_io(){
    gpio_config_t pin_config={};

    //INPUTS

    pin_config.intr_type=GPIO_INTR_POSEDGE;
    pin_config.mode= GPIO_MODE_INPUT;
    pin_config.pin_bit_mask=INPUT_BIT_MASK;
    pin_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pin_config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&pin_config);

    //OUTPUTS
    pin_config.intr_type=GPIO_INTR_DISABLE;
    pin_config.mode =  GPIO_MODE_OUTPUT;
    pin_config.pin_bit_mask = OUTPUT_BIT_MASK;
    gpio_config(&pin_config);
}

static void isr_handler(void *args){
    uint32_t pino = (uint32_t) args;
    xQueueSendFromISR(click_evt_queue,&pino,NULL);
}

static void isJanelaAberta(void *arg){

}

static void tocaAlarmeTask(void *arg){
    uint32_t numPino,qtdClicks=0;
    while(1){
        if(xQueueReceive(click_evt_queue,&numPino,portMAX_DELAY)){
            estadoJanQuarto1 = 1;
        }

        //TODO: TOCAR BUZZER OU AUTO-FALANTE
        while(estadoJanQuarto1){
            gpio_set_level(ALARM,1);
            vTaskDelay(80/portTICK_PERIOD_MS);
            gpio_set_level(ALARM,0);
            vTaskDelay(80/portTICK_PERIOD_MS);
        }
    }
}