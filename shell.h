/*
 * Developed on top of ESP-IDF-v5.3.1
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/projdefs.h"

#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"

#include "freertos/task.h"

#ifndef SHELL_H
#define SHELL_H

#define debugger 0
#define log_tasks 1
#define log_serial 1

#if (log_serial == 1)
#define uart_write_bytes(s_port, buff, length) uart_write(s_port, buff, length)
#define i2c_master_write_to_device(s_port, slave_addr, data, length, ticks_to_wait) i2c_write(s_port, slave_addr, data, length, ticks_to_wait);
#endif

#if (log_tasks == 1)
#define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
#define vTaskDelete(task) delete_task(task)
#endif

/* cringe wrappers, DON'T USE IN YOUR CODE!! */
int uart_write(uart_port_t s_port, void *buff, int length);

esp_err_t i2c_write(i2c_port_t s_port, uint8_t slave_addr, void *buff, int length, TickType_t ticks_to_wait);

void create_task(TaskFunction_t pxTaskCode, const char * const pcName, const configSTACK_DEPTH_TYPE usStackDepth, void * const pvParameters, UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask);

void delete_task(TaskHandle_t task);

/* 
 * Initializes ESP-32 Shell 
 * 
 * Initializes UART to USB bridge at UART_NUM_0 
 * 
 * Creates a task called command_read -> reads commands 
 * through ESP terminal
 * 
 */
void shell_init();

#endif
