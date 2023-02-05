#include "mqtt_client.h"
#include "esp_event.h"

/**
 * @brief FUNCAO PARA ENVIAR MENSAGEM E PUBLICAR EM UM TOPICO DEFINIDO
 * @param topico topico
 * @param msg dado a ser enviado em string ou JSON
 * @param qos qualidade do serviço: 0,1 ou 2
 * @param retain se salva (1) ou nao (0) o dado a ser enviado
*/
void enviarMsg( char *, char *, uint8_t, uint8_t);

/**
 * @brief FUNCAO PARA RECEBER MENSAGEM EM UM TOPICO DEFINIDO
 * @param topico topico
 * @param qos qualidade do serviço: 0,1 ou 2
*/
char* receberMsg(char *, uint8_t);

void mqtt_event_handler(void *, esp_event_base_t , int32_t , void *);
void mqtt_callback(esp_mqtt_event_handle_t event);
void mqtt_app_start(void);
void testaMem(void);
size_t min(int,int);