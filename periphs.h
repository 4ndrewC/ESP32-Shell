#ifndef PERIPHS_H
#define PERIPHS_H

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
#include "freertos/projdefs.h"

#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

#include "freertos/task.h"


#define MAX_READ_BUF_SIZE 256
#define MAX_WRITE_BUF_SIZE 256
#define BUF_SIZE 256
#define LOG_SIZE 10

#define MAX_TASK_P 23 // for shell functions, reserve 24 for functions like list tasks

#define PINS 40

#define MAX_TASKS 100

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

typedef struct {
    char *task_name;
    // uint32_t stack_depth;
    unsigned long priority;
} task_log_t;

#endif
