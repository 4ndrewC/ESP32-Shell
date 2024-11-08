#include "periphs.h"

int log_index;
uint8_t pinstate[PINS+1];
action_t port_actions[LOG_SIZE];

SemaphoreHandle_t write_logs_semaphore;

/* storing pin data */
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

int get_number(char *command){
    while (*command && !isdigit((unsigned char)*command)) command++;
    int pin = strtol(command, NULL, 10);
    return pin;
}


/* update logs on computer */
void write_serial_logs(void *pvParameter){

    for(int i = 0; i<log_index; i++){
        char *port = (char *)malloc(sizeof(char));
        switch(port_actions[i].port){
            case UART_NUM_0: 
                port = "0"; break;
            case UART_NUM_1: 
                port = "1"; break;
            case UART_NUM_2: 
                port = "2"; break;
            case UART_NUM_MAX: 
                port = "3"; break;
        }

        char *io = (char *)malloc(sizeof(char));
        io = port_actions[i].io == 1 ? "1" : "0";

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
        
        char *length = (char *)malloc(BUF_SIZE*sizeof(char));
        snprintf(length, BUF_SIZE, "%d", port_actions[i].length);
        
        char *data = (char *)malloc(port_actions[i].length*sizeof(char));
        data = (char *)port_actions[i].buff;

        strcat(length, port);
        strcat(length, io);
        strcat(length, dtype);
        strcat(length, comm);
        strcat(length, data);
        char header[BUF_SIZE+15] = "!serial ";
        strcat(header, length);
        uart_write_bytes(UART_NUM_0, (const uint8_t *)header, BUF_SIZE+15);
    }
    xSemaphoreGive(write_logs_semaphore);
    vTaskDelete(NULL);
}


/* clear temp logs */
void serial_logs_clear(){
    for(int i = 0; i<LOG_SIZE; i++) memset(port_actions[i].buff, 0, BUF_SIZE/4);
    log_index = 0;
}

// for specific port logs, computer side will parse the data
void port_logs(){
    #undef xTaskCreate
    xTaskCreate(&write_serial_logs, "log the serials", 1024, NULL, MAX_TASK_P, NULL);
    xQueueSemaphoreTake(write_logs_semaphore, portMAX_DELAY); // wait for temp logs to be sent first
    serial_logs_clear();
    #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
}


void uart_write(uart_port_t s_port, void *buff, int length, io_type iotype){

    uint32_t *data = buff;

    /* write to computer logs if temp log is filled up */
    if(log_index==LOG_SIZE) port_logs();
    

    port_actions[log_index].port = s_port;
    port_actions[log_index].io = 0;
    port_actions[log_index].dtype = iotype;
    port_actions[log_index].comm = UART;
    
    // this is to make sure the data sent fits perfectly into multiples of 32-bits
    int mod = length%4;
    port_actions[log_index].length = length+mod;

    store(port_actions[log_index].buff, data, length);

    log_index++;

    uart_write_bytes(s_port, (const uint8_t *)buff, length);
}

/* ------------ UNTESTED --------------*/
void uart_read(uart_port_t s_port, void *buff, int length, io_type iotype, TickType_t ticks_to_wait){

    int ret = uart_read_bytes(s_port, buff, length, ticks_to_wait);
    if(ret==ESP_FAIL) printf("uart failed to read\n");

    uint32_t *data = buff;

    if(log_index==LOG_SIZE) port_logs();

    port_actions[log_index].port = s_port;
    port_actions[log_index].io = 1;
    port_actions[log_index].dtype = iotype;
    port_actions[log_index].comm = UART;

    int mod = length%4;
    port_actions[log_index].length = length+mod;

    store(port_actions[log_index].buff, data, length);

    log_index++;
}

// ------------ WORK ON THIS -------------------

/* 
to do:
- uart write instead of just esp log info
*/

int task_index;
task_log_t task_log[MAX_TASKS];
uint8_t next_task[MAX_TASKS];
uint8_t task_active[MAX_TASKS];

/* trying something cringe */
#define log_tasks 1

#ifdef log_tasks
#define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
#define vTaskDelete(task) delete_task(task)
#endif

int get_next_task(){
    for(int i = 0; i<MAX_TASKS; i++){
        if(!task_active[i]) return i;
    }
    return 0;
}

void create_task(TaskFunction_t pxTaskCode, const char * const pcName, const configSTACK_DEPTH_TYPE usStackDepth, void * const pvParameters, UBaseType_t uxPriority, TaskHandle_t * const pxCreatedTask){
    if(task_index>=MAX_TASKS){
        ESP_LOGI("TAG", "Max task limit reached");
        return;
    }
    #undef xTaskCreate
    xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
    // log tasks here
    ESP_LOGI("TAG", "Task %s created through pseudo function\n", pcName);
    int next = get_next_task(); // get next available task index
    task_log[next].task_name = pcName;
    // task_log[next].stack_depth = usStackDepth;
    task_log[next].priority = uxPriority;
    task_active[next] = 1;
    if(next>task_index) task_index = next;
    #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
}

void delete_task(TaskHandle_t task){
    #undef vTaskDelete
    ESP_LOGI("TAG", "Task deleted through pseudo function\n");
    // unlog task
    TaskHandle_t cur_task = task==NULL? xTaskGetCurrentTaskHandle() : task;
    char *name = pcTaskGetName(cur_task);
    UBaseType_t priority = uxTaskPriorityGet(cur_task);
    // search for the one with that name and priority
    for(int i = 0; i<MAX_TASKS; i++){
        if(strcmp(name, task_log[i].task_name)==0 && priority==task_log[i].priority){
            task_active[i] = 0;
            break;
        }
    }
    vTaskDelete(task);
    #define vTaskDelete(task) delete_task(task)
}

/* show all tasks */
void task_list(void *pvParameter){
    ESP_LOGI("TAG", "cringe\n");
    
    // send all active tasks
    for(int i = 0; i<MAX_TASKS; i++){
        if(!task_active[i]) continue;
        ESP_LOGI("TAG", "%s", task_log[i].task_name);
    }
    #undef vTaskDelete
    vTaskDelete(NULL);
    #define vTaskDelete(task) delete_task(task)

    xSemaphoreGive(write_logs_semaphore); // are you sure about this
}

// ---------------------------------------------

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
    if(strlen(command)==9 && memcmp(command, "pindir -a", 9)==0){
        for(int i = 0; i<PINS; i++){
            char *state = pinstate[i]==1 ? "output" : "input";
            printf("pin %d: %s\n", i, state);
        }
        return;
    }

    int pin = get_number(command);
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
        if(ret==ESP_FAIL) printf("uart failed to read\n");
        // uart_read(UART_NUM_0, buff, MAX_READ_BUF_SIZE, IO_CHAR, pdMS_TO_TICKS(500));
        /*freaky command parsing*/
        strip(buff);
        if(memcmp(buff, "uart", 4)==0){
            printf("nah\n");
        }
        else if(memcmp(buff, "pindir", 6)==0){
            pindir(buff);
        }
        else if(memcmp(buff, "port logs", 9)==0){
            port_logs();
        }
        else if(memcmp(buff, "task list", 9)==0){
            #undef xTaskCreate
            xTaskCreate(task_list, "task_list", 2048, NULL, MAX_TASK_P+1, NULL);
            #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void shell_init(){
    write_logs_semaphore = xSemaphoreCreateBinary();
    task_index = 0;
    log_index = 0;

    #undef xTaskCreate
    xTaskCreate(&command_read, "command_read", 2048, NULL, 5, NULL);
    #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
}
