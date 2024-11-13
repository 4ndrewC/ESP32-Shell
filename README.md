# ESP32-Shell
A shell to interact with the current state of the hardware on the ESP32 microcontroller using FreeRTOS on the ESP-IDF framework

**Features**
- Logging serial read/write history
- Get pin direction 
- Logging tasks
  -  List all currently running tasks



# How to use

When building using ESP-IDF, include "shell.c" as one of the idf component registers
e.g
```make
idf_component_register(SRCS "your_file.c" "path/to/shell.c" INCLUDE_DIRS ".")
```

include "shell.h" in your C file
