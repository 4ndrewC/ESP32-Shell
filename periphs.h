#ifndef PERIPHS_H
#define PERIPHS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "driver/uart.h"


#define MAX_READ_BUF_SIZE 255
#define MAX_WRITE_BUF_SIZE 255
#define BUF_SIZE 255
#define LOG_SIZE 10

#define MAX_TASK_P 23 // for shell functions, reserve 24 for functions like list tasks

#define PINS 40

#define MAX_TASKS 100


typedef enum {
    UART,
    I2C,
    SPI
} comm_type;

typedef struct{

    uart_port_t port;
    uint8_t io;
    comm_type comm;
    uint8_t length;
    uint32_t buff[BUF_SIZE/4];

} action_t;

typedef struct {
    char *task_name;
    // uint32_t stack_depth;
    unsigned long priority;
} task_log_t;

#endif
