import serial

ed = '6f70706169'
op = '10ff'
uart_message_op = '334455'
uart_message_ed = '66778899'


def read_from_uart(port='COM4', baudrate=115200, timeout=0.01):
    message = ""
    while True:
        try:
            # Open the serial port
            with serial.Serial(port, baudrate, timeout=timeout) as ser:
                print(f"Reading from {port} at {baudrate} baud...")
                
                while True:
                    temp = ""
                    if ser.in_waiting > 0:
                        data = ser.read(ser.in_waiting or 1)
                        temp = "".join(f"{byte:02x}" for byte in data)
                        message+=temp
                        if uart_message_ed in message:
                            print("do string parsing here")
                            parse_start = message.index(uart_message_op)
                            parse_end = message.index(uart_message_ed)
                            
                            piece = message[parse_start+6:parse_end]
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
                            for i in arr: print("each", i)

                            read_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'r')
                            output = read_logs.read()
                            output+=sentence
                            read_logs.close()
                            serial_logs = open('D:\Andrew\Programming\esp32\cli\serial_logs.out', 'w')
                            serial_logs.write(output)
                            serial_logs.close()

                            message = message[parse_end+8:]
                        
                        
                        print("Received: ", temp)
        except serial.SerialException as e:
            
            print(f"Error: {e}")

if __name__ == "__main__":
    read_from_uart()
