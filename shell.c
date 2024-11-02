#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

#define MAX_READ_BUF_SIZE 256
#define MAX_WRITE_BUF_SIZE 256
#define BUF_SIZE 256 // number of bytes
#define LOG_SIZE 10


#define PINS 40

#define PIN_INDEX pin_index[pin]
#define PORT_INDEX port_index[port]


int task_hp = 0;
int log_index = 0;

typedef enum{
    IO_INT,
    IO_SHORT,
    IO_CHAR,
    IO_FLOAT,
    IO_U32,
    IO_U16,
    IO_U8
} io_type;

typedef enum {
    UART,
    I2C,
    SPI
} comm_type;


typedef struct{

    uart_port_t port;
    uint8_t io;
    io_type dtype;
    comm_type comm;
    uint16_t length;
    uint32_t buff[BUF_SIZE/4];

} action_t;

uint8_t pinstate[PINS+1];

uint8_t pin_index[PINS+1];

action_t port_actions[LOG_SIZE];

SemaphoreHandle_t write_logs_semaphore;

/* storing pin data */
// void store(uint32_t compressed[], uint32_t data[], int length, io_type type){
//     int div = 1;
//     if(type==IO_CHAR || type==IO_U8) div = 4;
//     else if(type==IO_SHORT || type==IO_U16) div = 2;
//     for(int i = 0; i<length/div; i++){
//         compressed[i] = data[i];
//     }
// }

void store(uint32_t compressed[], void *input, int length){
    uint32_t *data = (uint32_t *)input;
    int div = 4/sizeof(input[0]);
    for(int i = 0; i<length/div; i++){
        compressed[i] = data[i];
    }
}

/* unpacking pin data */
void load(uint32_t compressed[], uint32_t data[], int length, io_type type){
    int div = 1;
    if(type==IO_CHAR) div = 4;
    else if(type==IO_SHORT) div = 2;
    for(int i = 0; i<length/div; i++){
        data[i] = compressed[i];
    } 
}

/* helper for stripping left and right spaces from command reads */
void strip(char *str) {
    int start = 0;
    int end = strlen(str) - 1;

    while (isspace((unsigned char)str[start])) start++;

    while (end >= start && isspace((unsigned char)str[end])) end--;
    int i, j = 0;
    for (i = start; i <= end; i++) str[j++] = str[i];
    str[j] = '\0';
}


/* update logs on computer */
void write_logs(void *pvParameter){
    for(int i = 0; i<LOG_SIZE; i++){
        char port[1];
        switch(port_actions[i].port){
            case UART_NUM_0: 
                port[0] = '0'; break;
            case UART_NUM_1: 
                port[0] = '1'; break;
            case UART_NUM_2: 
                port[0] = '2'; break;
            case UART_NUM_MAX: 
                port[0] = '3'; break;
        }

        char io[1];

        io[0] = port_actions[i].io==1 ? '1' : '0';

        char *dtype = (char *)malloc(5*sizeof(char));
        switch(port_actions[i].dtype){
            case IO_INT: 
                dtype = "int"; break;
            case IO_SHORT: 
                dtype = "short"; break;
            case IO_CHAR: 
                dtype = "char"; break;
            case IO_FLOAT: 
                dtype = "float"; break;
            case IO_U32: 
                dtype = "u32"; break;
            case IO_U16: 
                dtype = "u16"; break;
            case IO_U8: 
                dtype = "u8"; break;
        }

        char *comm = (char *)malloc(4*sizeof(char));
        comm = port_actions[i].comm == UART ? "uart" : (port_actions[i].comm == I2C ? "i2c" : "spi");
        
        char length[6];
        snprintf(length, 6, "%d", port_actions[i].length);
        
        char *data = (char *)malloc(port_actions[i].length*sizeof(char));
        data = (char *)port_actions[i].buff;

        strcat(data, port);
        strcat(data, io);
        strcat(data, dtype);
        strcat(data, comm);
        strcat(data, length);
        char header[BUF_SIZE+15] = "!send ";
        strcat(header, data);
        uart_write_bytes(UART_NUM_0, (const uint8_t *)header, BUF_SIZE+15);
    }
    xSemaphoreGive(write_logs_semaphore);
    vTaskDelete(NULL);
}


void port_write(uart_port_t s_port, void *buff, int length, comm_type commtype, io_type iotype){

    // this forces write_logs() to finish before resuming current task
    write_logs_semaphore = xSemaphoreCreateBinary();

    if(commtype==UART){
        uint32_t *data = buff;

        /* write to computer logs if temp log is filled up */
        if(log_index==LOG_SIZE){
            xTaskCreate(&write_logs, "log the logs", STACK_SIZE, NULL, task_hp+1, NULL);

            xQueueSemaphoreTake(write_logs_semaphore, portMAX_DELAY); // wait for temp logs to be sent first         
            log_index = 0;

            for(int i = 0; i<LOG_SIZE; i++) memset(port_actions[i].buff, 0, BUF_SIZE/4);
        }
        

        port_actions[log_index].port = s_port;
        port_actions[log_index].io = 1;
        port_actions[log_index].dtype = iotype;
        port_actions[log_index].comm = UART;
        
        // this is to make sure the data sent fits perfectly into multiples of 32-bits
        int mod = length%4;
        port_actions[log_index].length = length+mod;

        store(port_actions[log_index].buff, data, length);

        log_index++;

        uart_write_bytes(s_port, (const uint8_t *)buff, length);
    }
}

/* tracks pin direction per enable/disable */
int get_pin_dir(int pin){
    int enabled = -1;
    if (pin < 32){
        enabled = (REG_READ(GPIO_ENABLE_REG) & (1 << pin)) != 0;
        pinstate[pin] = enabled;
    }else if (pin < 40){
        enabled = (REG_READ(GPIO_ENABLE1_REG) & (1 << (pin - 32))) != 0;
        pinstate[pin] = enabled;
    }
    return enabled;
}

/* pin state command */
void pindir(char *command){
    if(memcmp(command, "pindir -a", 9)==0){
        for(int i = 0; i<PINS; i++){
            char *state = pinstate[i]==1 ? "output" : "input";
            printf("pin %d: %s\n", i, state);
        }
        return;
    }

    while (*command && !isdigit((unsigned char)*command)) command++;
    int pin = strtol(command, NULL, 10);
    int ret = get_pin_dir(pin);
    char *state = ret ? "output" : "input";
    printf("%s\n", state);
}



void command_read(void *pvParameter){
    while(1){
        char buff[MAX_READ_BUF_SIZE];
        memset(buff, 0, MAX_READ_BUF_SIZE);
        int ret = 0;
        ret = uart_read_bytes(UART_NUM_0, (uint8_t*)buff, MAX_READ_BUF_SIZE, pdMS_TO_TICKS(500));
        if(ret) printf("uart read success: %s\n", buff);

        /*freaky command parsing*/
        strip(buff);
        if(memcmp(buff, "uart", 4)==0){
            printf("nah\n");
        }
        else if(memcmp(buff, "pindir", 6)==0){
            pindir(buff);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}