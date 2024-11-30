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


def uart_read(port='COM4', baudrate=115200, timeout=0.01):
    global cmd_semaphore
    global serial_output
    message = ""
    logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'w')
    logs.write("")
    logs.close()
    while True:
        try:
            temp = ""
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting or 1)
                temp = "".join(f"{byte:02x}" for byte in data)
                message+=temp
                if serial_ed in message:
                    print("do string parsing here")
                    parse_start = message.index(serial_op)
                    parse_end = message.index(serial_ed)
                    serial_output = message[parse_start+6:parse_end]
                    # print("made it here")
                    get_serial_logs()
                    # print("made it past")
                    message = message[parse_end+8:]
                    # print("made it to the end")
                    cmd_semaphore = 1
                elif uart_ed in message:
                    print("run regular function")
                    parse_start = message.index(uart_op)
                    parse_end = message.index(uart_ed)
                    serial_output = message[parse_start+14:parse_end]
                    message = message[parse_end+14:]
                    cmd_semaphore = 1       

                    # print("Received: ", temp)
        except serial.SerialException as e:
            
            print(f"Error: {e}")

# ------------ COMMANDS ---------------

def get_serial_logs():
    piece = serial_output
    if len(serial_output)==0: return
    print("piece", piece)
    sentence = ""
    arr = []
   
    for i in range(len(piece)-4):
        each = ""
        if piece[i:i+4]==op:
            j = i
            while j<len(piece)-10:
                if piece[j:j+10]==ed: break
                each+=piece[j]
                j+=1
            arr.append(each)
            sentence+=each+'\n'
    # for i in arr: print("each", i)

    read_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'r')
    output = read_logs.read()
    output+=sentence
    read_logs.close()
    serial_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'w')
    serial_logs.write(output)
    serial_logs.close()

def port_logs(command):
    if '-a' in command:
        # display all
        print("cringe")

    return
        

def task_list():
    global serial_output
    print("running task list")
    display = serial_output
    print(display)

command_list = ['port logs', 'task list']

def interface():
    global cmd_semaphore
    print("Welcome to the ESP-Shell! Type 'exit' to quit.")
    while True:
        try:
            command = input("ESP-Shell> ") 
            if command.lower() == 'quit':
                print("Quitting Shell")
                break

            else:
                # send command through uart
                ser.write(command.encode('utf-8'))
                found = 0
                for i in command_list:
                    if i in command or command in i:
                        found = 1
                        break
                while cmd_semaphore!=1 and found==1:
                    pass
                if cmd_semaphore==1:
                    if 'port logs' in command.lower():
                        # print("nice gay")
                        port_logs(command)
                    elif 'task list' in command.lower():
                        task_list()

                    cmd_semaphore = 0
        except KeyboardInterrupt:
            print("Quitting")
            return
            

write_thread = threading.Thread(target=interface)
read_thread = threading.Thread(target=uart_read)

write_thread.start()
read_thread.start()

write_thread.join()
read_thread.join()
