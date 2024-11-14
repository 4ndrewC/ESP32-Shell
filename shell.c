#include "periphs.h"
#include "shell.h"

int log_index;
uint8_t pinstate[PINS+1];
action_t port_actions[LOG_SIZE];

SemaphoreHandle_t write_logs_semaphore;

/* storing serial data */
void store(uint32_t compressed[], void *input, int length){
    uint32_t *data = (uint32_t *)input;
    int div = 4/sizeof(input[0]);
    for(int i = 0; i<length/div; i++){
        compressed[i] = data[i];
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

/* 2 bytes for length, 1 byte for port, 1 byte for io,
   4 bytes for comm, 256 bytes for data*/
void write_serial_logs(void *pvParameter){

    for(int i = 0; i<log_index; i++){
        uint8_t uart_buff[BUF_SIZE + 15];

        uint8_t port;
        switch(port_actions[i].port){
            case UART_NUM_0: 
                port = 0; break;
            case UART_NUM_1: 
                port = 1; break;
            case UART_NUM_2: 
                port = 2; break;
            case UART_NUM_MAX: 
                port = 3; break;
            default:
                port = 5; break;
        }

        uint8_t io = port_actions[i].io;

        char *comm = (char *)malloc(4*sizeof(char));
        comm = port_actions[i].comm == UART ? "uart" : (port_actions[i].comm == I2C ? "i2c" : "spi");
        
        uint8_t length = port_actions[i].length;
        
        uint8_t *data = (uint8_t *)malloc(port_actions[i].length*sizeof(uint8_t));
        data = (uint8_t *)port_actions[i].buff;
        
        uart_buff[0] = length;
        uart_buff[1] = port;
        uart_buff[2] = io;
        int index = 3;
        while(*comm!='\0'){
            uart_buff[index] = *comm;
            index++;
            comm++;
        } 
        if(index==6) uart_buff[index] = '0';
        index = 7;
        for(int i = 0; i<BUF_SIZE; i++){
            uart_buff[index] = data[i];
            index++; 
        }
        #undef uart_write_bytes
        uart_write_bytes(UART_NUM_0, uart_buff, sizeof(uart_buff));
        #define uart_write_bytes(s_port, buff, length) uart_write(s_port, buff, length)
    }
    xSemaphoreGive(write_logs_semaphore);
    #undef vTaskDelete
    vTaskDelete(NULL);
    #define vTaskDelete(task) delete_task(task)

}

#if (debugger == 1)
    void write_serial_logs_debugger(void *pvParameter){

        xSemaphoreGive(write_logs_semaphore);
        vTaskDelete(NULL);
    }
#endif

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

#if (debugger == 1)
    void port_logs_debugger(){
        #undef xTaskCreate
        xTaskCreate(&write_serial_logs_debugger, "debug the serials", 1024, NULL, MAX_TASK_P, NULL);
        xQueueSemaphoreTake(write_logs_semaphore, portMAX_DELAY); // wait for temp logs to be sent first
        serial_logs_clear();
        #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
    }
#endif

// ----------------- SERIAL LOGGING -----------------

int uart_write(uart_port_t s_port, void *buff, int length){
    
    uint32_t *data = buff;

    /* write to computer logs if temp log is filled up */
    if(log_index==LOG_SIZE) port_logs();
    

    port_actions[log_index].port = s_port;
    port_actions[log_index].io = 0;
    port_actions[log_index].comm = UART;
    
    // this is to make sure the data sent fits perfectly into multiples of 32-bits
    int mod = length%4;
    port_actions[log_index].length = length+(4-mod);

    store(port_actions[log_index].buff, data, port_actions[log_index].length);

    log_index++;

    #undef uart_write_bytes
    int ret = uart_write_bytes(s_port, (const uint8_t *)buff, length);
    #define uart_write_bytes(s_port, buff, length) uart_write(s_port, buff, length)
    
    return ret;
}

/* wrapper for i2c write */
esp_err_t i2c_write(i2c_port_t s_port, uint8_t slave_addr, void *buff, int length, TickType_t ticks_to_wait){

    uint32_t *data = buff;

    /* write to computer logs if temp log is filled up */
    if(log_index==LOG_SIZE) port_logs();
    

    port_actions[log_index].port = s_port;
    port_actions[log_index].io = 0;
    port_actions[log_index].comm = I2C;
    
    // this is to make sure the data sent fits perfectly into multiples of 32-bits
    int mod = length%4;
    port_actions[log_index].length = length+(4-mod);
    

    store(port_actions[log_index].buff, data, port_actions[log_index].length);

    log_index++;

    #undef i2c_master_write_to_device
    esp_err_t ret = i2c_master_write_to_device(s_port, slave_addr, data, sizeof(data), ticks_to_wait);
    #define i2c_master_write_to_device(s_port, slave_addr, data, length, ticks_to_wait) i2c_write(s_port, slave_addr, data, length, ticks_to_wait);

    return ret;
}

/* ------------ UNTESTED --------------*/
int uart_read(uart_port_t s_port, void *buff, int length, TickType_t ticks_to_wait){

    int ret = uart_read_bytes(s_port, buff, length, ticks_to_wait);
    if(ret==ESP_FAIL) printf("uart failed to read\n");

    uint32_t *data = buff;

    if(log_index==LOG_SIZE) port_logs();

    port_actions[log_index].port = s_port;
    port_actions[log_index].io = 1;
    port_actions[log_index].comm = UART;

    int mod = length%4;
    port_actions[log_index].length = length+(4-mod);

    store(port_actions[log_index].buff, data, port_actions[log_index].length);

    log_index++;

    return ret;
}

// ---------- TASK LOGGING -----------
/* 
to do:
- uart write instead of just esp log info
*/

int task_index;
task_log_t task_log[MAX_TASKS];
uint8_t next_task[MAX_TASKS];
uint8_t task_active[MAX_TASKS];


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
    // log tasks here
    ESP_LOGI("TAG", "Task %s created through pseudo function\n", pcName);
    int next = get_next_task(); // get next available task index
    task_log[next].task_name = pcName;
    // task_log[next].stack_depth = usStackDepth;
    task_log[next].priority = uxPriority;
    task_active[next] = 1;
    if(next>task_index) task_index = next;

    #undef xTaskCreate
    xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
    #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
}

void delete_task(TaskHandle_t task){

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

    #undef vTaskDelete
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

void init_uart(){
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, MAX_READ_BUF_SIZE*2, 0, 0, NULL, 0);
}

void shell_init(){
    write_logs_semaphore = xSemaphoreCreateBinary();
    task_index = 0;
    log_index = 0;
    init_uart();
    #undef xTaskCreate
    xTaskCreate(&command_read, "command_read", 2048, NULL, 5, NULL);
    #define xTaskCreate(task, name, stack_size, parameters, priority, handle) create_task(task, name, stack_size, parameters, priority, handle)
}
