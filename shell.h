/*
 * Developed on top of ESP-IDF-v5.3.1
 */

#ifndef SHELL_H
#define SHELL_H

#define debugger 0
#define log_tasks 1
#define log_serial 1

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
