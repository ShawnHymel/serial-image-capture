import tkinter as tk
import threading
import time
import base64
import io

# Install with `python -m pip install Pillow pyserial`
from PIL import Image, ImageTk
import serial


# Settings
serial_port = "COM26"
baud_rate = 230400

#-------------------------------------------------------------------------------
# Classes

# GUI class
class GUI:
    
    # Constructor
    def __init__(self, root):
        self.root = root
        
        # Create the main container
        self.frame_main = tk.Frame(self.root)
        self.frame_main.pack(fill=tk.BOTH, expand=True)

        # Allow middle cell of grid to grow when window is resized
        self.frame_main.columnconfigure(1, weight=1)
        self.frame_main.rowconfigure(0, weight=1)
        
        # TkInter variables
        self.var_fps = tk.StringVar()
        self.var_fps.set("FPS: ")
        

        # Create control widgets
        self.frame_control = tk.Frame(self.frame_main)
        self.label_fps = tk.Label(  self.frame_control, 
                                    textvariable=self.var_fps)
        self.button_save = tk.Button(   self.frame_control, 
                                        text="Save Image", 
                                        padx=5)

        # Create canvas
        self.canvas = tk.Canvas(self.frame_main, width=500, height=500)

        # Lay out widgets on control frame
        self.frame_control.grid(row=0, column=0, padx=5, pady=5, sticky=tk.NW)
        self.label_fps.pack(anchor=tk.W)
        self.button_save.pack()

        # Lay out canvas on main frame grid
        self.canvas.grid(row=0, column=1, padx=5, pady=5, sticky=tk.NW)

        # Place focus on save button by default
        self.button_save.focus()
        
        # Start image Rx thread
        self.rx_task = ImageRxTask(self)
        self.rx_task.daemon = True
        self.rx_task.start()
        
    # Update FPS information
    def update_fps(self, fps):
        self.var_fps.set("FPS: {}".format(fps))
        
    # Update image in canvas
    def update_image(self, img_bytes):
    
        # Convert bytes to image object
        img_stream = io.BytesIO(img_bytes)
        img = Image.open(img_stream)
        img_w, img_h = img.size

        # Convert to TkInter image (class member to avoid garbage collection)
        self.tk_img = ImageTk.PhotoImage(img)

        #Show image on canvas
        self.canvas.create_image(0, 0, anchor="nw", image=self.tk_img)
        self.canvas.config(width=img_w, height=img_h)
        
# Background thread to read image data and send to GUI
class ImageRxTask(threading.Thread):

    # Receiver state machine constants
    RX_STRING = 0
    RX_JPEG = 1

    # Constructor
    def __init__(self, parent):
        self.gui = parent
        super().__init__()
        
    # Main part of thread
    def run(self):
    
        # Rx state machine state
        rx_mode = self.RX_STRING
        
        # Where we store the base64 encoded image message
        rx_buf = b''
    
        # Try to open serial port
        try:
            ser = serial.Serial(serial_port, baud_rate, timeout=0)
            print(ser.name)
        except serial.SerialException as e:
            print("Error:", e)
            exit()
            
        # Forever loop
        while True:
        
            # Read bytes to be read
            if ser.in_waiting > 0:
                while(ser.in_waiting):
        
                    # Read those bytes
                    rx_buf = rx_buf + ser.read()
                
                    # Look for start of JPEG base64 header
                    if rx_mode == self.RX_STRING:
                        if rx_buf == b'/9j/':
                            rx_mode = self.RX_JPEG
                    
                    # Look for newline ('\n')
                    if rx_buf[-1] == 10:
                    
                        # If we're not recording anything, print it
                        if rx_mode == self.RX_STRING:
                            try:
                                print("Recv:", rx_buf.decode("utf-8").strip())
                            except:
                                pass
                    
                        # If we're recording the JPEG image data, display it
                        elif rx_mode == self.RX_JPEG:
                            rx_mode = self.RX_STRING
                            
                            # Remove \r\n at the end
                            rx_buf = rx_buf[:-2]
                            
                            # Attempt to decode image and display in GUI
                            try:
                                img_dec = base64.b64decode(rx_buf)
                                self.gui.update_image(img_dec)
                            except:
                                pass
                        
                        # Clear buffer
                        rx_buf = b''
            
            # Sleep the thread for a bit if there are no bytes to be read
            else:
                time.sleep(0.001)

#-------------------------------------------------------------------------------
# Main

if __name__ == "__main__":
    root = tk.Tk()
    root.title("Serial Image Capture")
    main_ui = GUI(root)
    root.mainloop()
    