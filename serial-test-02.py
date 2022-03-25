import io

from PIL import Image
import serial
import base64

serial_port = "COM26"
baud_rate = 115200

rx_mode = 0

# Try to open serial port
try:
    ser = serial.Serial(serial_port, baud_rate, timeout=0)
    print(ser.name)
except serial.SerialException as e:
    print("Error:", e)
    exit()

# Where we store the base64 encoded image message
img_enc = b''

while(1):
    while ser.in_waiting:
    
        img_enc = img_enc + ser.read()
    
        # Look for start of JPEG base64 header
        if rx_mode == 0:
            if img_enc == b'/9j/':
                rx_mode = 1
        
        # Look for newline ('\n')
        if img_enc[-1] == 10:
        
            # If we're recording the image data, display it
            if rx_mode == 1:
                #print(img_enc)
                rx_mode = 0
                
                # Remove \r\n at the end
                img_enc = img_enc[:-2]
                
                # Decode image
                img_dec = base64.b64decode(img_enc)
                
                # Display image
                img_stream = io.BytesIO(img_dec)
                img = Image.open(img_stream)
                img.show()
            
            # Clear buffer
            img_enc = b''
        