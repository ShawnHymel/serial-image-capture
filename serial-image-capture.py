import tkinter as tk
import threading
import time
import base64
import io

# Install with `python -m pip install Pillow pyserial`
from PIL import Image, ImageTk
import serial
import serial.tools.list_ports

# Settings
init_baud = 230400          
max_refresh = 10        # Milliseconds

# EIML constants for header
# |     SOF     |  format  |   width   |   height  |
# | xFF xA0 XFF | [1 byte] | [4 bytes] | [4 bytes] |
EIML_HEADER_SIZE = 12
EIML_SOF_SIZE = 3
EIML_FORMAT_SIZE = 1
EIML_WIDTH_SIZE = 4
EIML_HEIGHT_SIZE = 4
EIML_SOF_B64 = b'/6D/'
EIML_RESERVED = 0
EIML_GRAYSCALE = 1
EIML_RGB888 = 2

#-------------------------------------------------------------------------------
# Classes

class GUI:
    """Main GUI class
    
    Controls the window used to visualize the received images. Buttons allow for
    connecting to a device and saving images.
    
    Note that the refresh_ method(s) are called in an independent thread.
    If another thread calls update_ method(s), data is passed safely between
    threads using a mutex.
    """
    
    def __init__(self, root):
        """Constructor"""
    
        self.root = root
        
        # Start image Rx thread
        self.rx_task = ImageRxTask(self)
        self.rx_task.daemon = True
        self.rx_task.start()
        
        # Create the main container
        self.frame_main = tk.Frame(self.root)
        self.frame_main.pack(fill=tk.BOTH, expand=True)

        # Allow middle cell of grid to grow when window is resized
        self.frame_main.columnconfigure(1, weight=1)
        self.frame_main.rowconfigure(0, weight=1)
        
        # TkInter variables
        self.var_port = tk.StringVar()
        self.var_baud = tk.IntVar()
        self.var_baud.set(init_baud)
        self.var_fps = tk.StringVar()
        self.var_fps.set("FPS: ")
        
        # Create control widgets
        self.frame_control = tk.Frame(self.frame_main)
        self.frame_connection = tk.Frame(self.frame_control)
        self.label_port = tk.Label( self.frame_connection,
                                    text="Port:")
        self.entry_port = tk.Entry( self.frame_connection,
                                    textvariable=self.var_port)
        self.label_baud = tk.Label( self.frame_connection,
                                    text="Baud:")
        self.entry_baud = tk.Entry( self.frame_connection,
                                    textvariable=self.var_baud)
        self.button_connect = tk.Button(    self.frame_control,
                                            text="Connect",
                                            padx=5,
                                            command=self.on_connect_clicked)
        self.label_fps = tk.Label(  self.frame_control, 
                                    textvariable=self.var_fps)
        self.button_save = tk.Button(   self.frame_control, 
                                        text="Save Image", 
                                        padx=5,
                                        command=self.on_save_clicked)

        # Create canvas
        self.canvas = tk.Canvas(self.frame_main, width=500, height=500)

        # Lay out widgets on control frame
        self.frame_control.grid(row=0, column=0, padx=5, pady=5, sticky=tk.NW)
        self.frame_connection.pack()
        self.label_port.grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.entry_port.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        self.label_baud.grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.entry_baud.grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        self.button_connect.pack()
        self.label_fps.pack(anchor=tk.W)
        self.button_save.pack()

        # Lay out canvas on main frame grid
        self.canvas.grid(row=0, column=1, padx=5, pady=5, sticky=tk.NW)

        # Place focus on port entry button by default
        self.entry_port.focus_set()
        
        # Start refresh loop
        self.img_mutex = threading.Lock()
        self.img_mutex.acquire()
        self.timestamp = time.monotonic()
        self.canvas.after(max_refresh, self.refresh_image)
        
    def __del__(self):
        """Desctructor: make sure we close that serial port!"""
        self.rx_task.close()
        
    def on_connect_clicked(self):
        """Attempt to connect to the given serial port"""
        
        # Check to make sure baud rate is an integer
        try:
            baud_rate = int(self.var_baud.get())
        except:
            print("ERROR: baud rate must be an integer")
            return
        
        # Attempt to connect to device
        self.rx_task.connect(self.var_port.get(), baud_rate)
        
    def on_save_clicked(self):
        """Save current image to the disk drive
        
        %%%TODO: this needs to be implemented
        """
        print("ERROR: the save functionality is not implemented yet")
        self.button_save.focus_set()

    def refresh_image(self):
        """Update canvas periodically
        
        Updates only happen if there is a new image to be saved. To make this
        function thread-safe, it checkes if a mutex/lock is available first.
        """
    
        # If new image is ready, update the canvas
        if self.img_mutex.acquire(blocking=False):
        
            # If we're interrupted, just fail gracefully
            try:
        
                # Convert to TkInter image: class member to avoid garbage collection
                img_w, img_h = self.img.size
                self.tk_img = ImageTk.PhotoImage(self.img)

                #Show image on canvas
                self.canvas.create_image(0, 0, anchor="nw", image=self.tk_img)
                self.canvas.config(width=img_w, height=img_h)
                
                # Update FPS
                self.fps = 1 / (time.monotonic() - self.timestamp)
                self.timestamp = time.monotonic()
                self.var_fps.set("FPS: {:.1f}".format(self.fps))
            
            except:
                pass
        
        self.canvas.after(max_refresh, self.refresh_image)

    def update_image(self, img):
        """Method to update the image in the cavas
        
        This will release a lock to notify the other thread that new image data
        is ready.
        """
    
        # Save image to class member
        self.img = img
        
        # Release lock to notify other thread that it can update the canvas
        self.img_mutex.release()
        
class ImageRxTask(threading.Thread):
    """Background thread to read image data and send to GUI"""

    # Receiver state machine constants
    RX_STRING = 0
    RX_JPEG = 1
    RX_EIML = 2

    def __init__(self, parent):
        """Constructor"""

        self.gui = parent
        super().__init__()
        
        # Create serial port
        self.ser = serial.Serial()
        
        # List ports
        print("Available serial ports:")
        available_ports = serial.tools.list_ports.comports()
        for port, desc, hwid in sorted(available_ports):
            print("  {} : {} [{}]".format(port, desc, hwid))
            
    def __del__(self):
        """Desctructor"""
        self.close()
            
    def connect(self, port, baud_rate):
        """Connect to the given serial port"""
        
        # Try closing the port first (just in case)
        try:
            self.ser.close()
        except Exception as e:
            print("ERROR:", e)
        
        # Update port settings
        self.ser.port=port
        self.ser.baudrate=baud_rate
        
        # Say that we're trying here
        print("Connecting to {} at a baud rate of {}".format(port, baud_rate))    
        # Try to open a connection
        try:
            self.ser.open()
        except Exception as e:
            print("ERROR:", e)
            
    def close(self):
        """Close serial port"""
        self.ser.close()
        
    def run(self):
        """Main part of the thread"""
    
        # Rx state machine state
        rx_mode = self.RX_STRING
        
        # Where we store the base64 encoded image message
        rx_buf = b''
            
        # Forever loop
        while True:
                
            # Read bytes if there are some waiting
            try:
                if self.ser.in_waiting > 0:
                    while(self.ser.in_waiting):

                        # Read those bytes
                        rx_buf = rx_buf + self.ser.read()
                    
                        # Look for start of JPEG or EIML header
                        if rx_mode == self.RX_STRING:
                            if rx_buf == b'/9j/':
                                rx_mode = self.RX_JPEG
                            if rx_buf == EIML_SOF_B64:
                                rx_mode = self.RX_EIML
                        
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
                                    img_stream = io.BytesIO(img_dec)
                                    img = Image.open(img_stream)
                                    self.gui.update_image(img)
                                except:
                                    pass
                                    
                            # If we're recording the raw image data, display it
                            elif rx_mode == self.RX_EIML:
                                rx_mode = self.RX_STRING
                                
                                # Remove \r\n at the end
                                rx_buf = rx_buf[:-2]
                                
                                # Attempt to decode image and display in GUI
                                try:
                                    # Decode message
                                    msg_dec = base64.b64decode(rx_buf)
                                    
                                    # print(msg_dec[3])
                                    # print(int.from_bytes(msg_dec[4:8], 'little'))
                                    # print(int.from_bytes(msg_dec[8:12], 'little'))
                                    
                                    # Extract info from header
                                    idx = EIML_SOF_SIZE
                                    format = msg_dec[idx]
                                    idx += EIML_FORMAT_SIZE                                       
                                    width = int.from_bytes(msg_dec[idx:(idx + EIML_WIDTH_SIZE)], 
                                                                                        'little')
                                    idx += EIML_WIDTH_SIZE
                                    height = int.from_bytes(msg_dec[idx:(idx + EIML_HEIGHT_SIZE)],
                                                                                        'little')
                                    idx += EIML_HEIGHT_SIZE
                                    
                                    # Create image and update GUI
                                    if format == EIML_RGB888:
                                        img = Image.frombytes(  'RGB', 
                                                                (width, height), 
                                                                msg_dec[idx:], 
                                                                'raw')
                                    self.gui.update_image(img)
                                    
                                except:
                                    print(idx)
                                    pass
                            
                            # Clear buffer
                            rx_buf = b''
                
                # Sleep the thread for a bit if there are no bytes to be read
                else:
                    time.sleep(0.001)
            except:
                pass

#-------------------------------------------------------------------------------
# Main

if __name__ == "__main__":
    root = tk.Tk()
    root.title("Serial Image Capture")
    main_ui = GUI(root)
    root.mainloop()
    