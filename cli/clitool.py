import sys
import serial
import threading

# global code
ser = serial.Serial(port="COM4", baudrate=115200, timeout=0.01)

ed = '6f70706169'
op = '10ff'

serial_op = '334455'
serial_ed = '66778899'

uart_op = '736b6962696469'
uart_ed = '7369676d61'

serial_output = ""
cmd_semaphore = 0

newline = 'ff'

stop = threading.Event()
kill = 0

comms = ['UART', 'I2C', 'SPI']
io = ['Output', 'Input']

def conv_bytes(b):
    return bytes.fromhex(b).decode('ascii')

def generate_table(columns, content):
    n = len(columns)
    max_size = [len(columns[x]) for x in range(n)]
    size_sum = 0
    col = '|'
    line = '|'
    for r in range(len(content)):
        for i in range(len(content[r])):
            # print(content[r])
            max_size[i] = max(max_size[i], len(content[r][i]))
    for i in range(n):
        size_sum += max_size[i]
        label = columns[i]
        temp = max_size[i]-len(label)
        col += label+' '*temp + '|'
        line += '-' * max_size[i] + '|'
    print(col)
    print(line)
    for r in range(len(content)):
        temp_col = '|'
        for i in range(n):
            label = content[r][i]
            temp = max_size[i]-len(label)
            temp_col += label+' '*temp + '|'
        print(temp_col)
        print(line)

def generate_serial_logs(content):
    ret = ''
    for i in range(len(content)):
        line = '['
        line += comms[int(content[i][3])] + ' '
        line += 'port ' + str(int(content[i][1])) + ' '
        line += io[int(content[i][2])] + '] '
        line += content[i][4] + '\n'
        ret += line
    return ret


def uart_read(port='COM4', baudrate=115200, timeout=0.01):
    global cmd_semaphore
    global serial_output
    global kill
    message = ""
    logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'w')
    logs.write("")
    logs.close()
    while kill!=1:
        try:
            temp = ""
            debug = 0
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting or 1)
                temp = "".join(f"{byte:02x}" for byte in data)
                message+=temp
                if serial_ed in message:
                    # print("do string parsing here")
                    parse_start = 0
                    debug=1
                    if serial_op in message: parse_start = message.index(serial_op)
                    debug = 2
                    parse_end = message.index(serial_ed)
                    debug = 3
                    serial_output = message[parse_start+6:parse_end]
                    # print("made it here")
                    debug = 4
                    message = message[parse_end+8:]
                    debug = 5
                    get_serial_logs()
                    # print("made it past")
                    debug = 6
                    # print("made it to the end")
                    cmd_semaphore = 1
                    debug = 7
                elif uart_ed in message:
                    print("run regular function")
                    parse_start = 0
                    debug = 8
                    if uart_op in message: parse_start = message.index(uart_op)
                    debug = 9
                    parse_end = message.index(uart_ed)
                    debug = 10
                    serial_output = message[parse_start+14:parse_end]
                    debug = 11
                    # print(message)
                    # print(serial_output)
                    message = message[parse_end+10:]
                    debug = 12
                    cmd_semaphore = 1       
                    debug = 13

                    # print("Received: ", temp)
        except serial.SerialException as e:
            print(message)
            print(debug)
            print(f"Error: {e}")

# ------------ COMMANDS ---------------

def get_serial_logs():
    global serial_output
    if len(serial_output)==0: return
    # print("piece", piece)   
    serial_output = serial_output.replace('a5', '')
    outer = serial_output.split(newline)
    res = [[] for x in range(len(outer)-1)]
    for i in range(len(outer)-1):
        # outer[i] = conv_bytes(outer[i])
        try:
            if len(res)<=1: return
            res[i] = outer[i].split('7c')
            res[i][0] = int(res[i][0], 16)
            res[i][1] = int(res[i][1], 16)
            res[i][2] = int(res[i][2], 16)
            res[i][3] = int(res[i][3], 16)
            # res[i][4] = conv_bytes(res[i][4])[:res[i][0]]
        except:
            print(serial_output)
            print('----------------')
            print(res)
        # print(res[i])
    # print(serial_output)
    sentence = generate_serial_logs(res)
    # for i in arr: print("each", i)
    read_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'r')
    output = read_logs.read()
    output+=sentence
    read_logs.close()
    serial_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'w')
    serial_logs.write(output)
    serial_logs.close()
    # serial_output = ""

def port_logs(command):
    global serial_output
    if '-a' in command:
        # display all
        read_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'r')
        output = read_logs.read()
        print(output)
    return
        

def task_list():
    global serial_output
    print("running task list")
    columns = ["Task", "Stack Size", "Priority"]
    display = serial_output
    rows = display.split(newline)
    content = [[] for x in range(len(rows))]
    for i in range(len(rows)):
        content[i] = rows[i].split('7c')
        for j in range(len(content[i])):
            content[i][j] = bytes.fromhex(content[i][j]).decode('ascii')

    content = content[:len(content)-1]
    generate_table(columns, content)
    # serial_output = ""

def ipconfig():
    global serial_output
    print("running ipconfig")
    display = serial_output
    # print(display)
    # print('------------')
    res = display.replace('a5', '').split('ff')
    # print(res)
    for i in res:
        print(conv_bytes(i))
    # serial_output = ""

command_list = ['port logs', 'task list', 'ipconfig']

def interface():
    global cmd_semaphore
    global kill
    print("Welcome to the ESP-Shell! Type 'exit' to quit.")
    while True:
        try:
            command = input("ESP-Shell> ") 
            try:
                if command.lower() == 'quit':
                    print("Quitting Shell")
                    kill = 1
                    break

                else:
                    # send command through uart
                    cmd_semaphore = 0
                    ser.write(command.encode('utf-8'))
                    found = 0
                    for i in command_list:
                        if i in command or command in i:
                            found = 1
                            break
                    if found: print("valid command found")
                    else: print("no valid command found")
                    # if len(serial_output)==0: cmd_semaphore = 1
                    while cmd_semaphore!=1 and found==1:
                        pass
                    found = 0
                    if cmd_semaphore==1:
                        if 'port logs' in command.lower():
                            port_logs(command)
                        elif 'task list' in command.lower():
                            task_list()
                        elif 'ipconfig' in command.lower():
                            ipconfig()

                        cmd_semaphore = 0
            except KeyboardInterrupt:
                print("Quitting Shell")
                kill = 1
                return
        except KeyboardInterrupt:
            print("Quitting Shell")
            stop.set()
            kill = 1
            return
            

write_thread = threading.Thread(target=interface)
read_thread = threading.Thread(target=uart_read)

write_thread.start()
read_thread.start()

stop.set()

write_thread.join()
read_thread.join()
