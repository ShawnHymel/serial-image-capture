import tkinter as tk
import threading
import time

# Install with `python -m pip install Pillow`
from PIL import Image, ImageTk

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
        self.counter = tk.StringVar()
        self.counter.set("FPS: ")

        # Create control widgets
        self.frame_control = tk.Frame(self.frame_main)
        self.label_fps = tk.Label(self.frame_control, textvariable=self.counter)
        self.button_save = tk.Button(self.frame_control, text="Save Image", padx=5)

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

        # Open image, get size
        img = Image.open("Patern_test.jpg")
        img_w, img_h = img.size
        print(img_w, img_h)

        # Convert to TkInter image (make class member to avoid garbage collection)
        self.tk_img = ImageTk.PhotoImage(img)

        #Show image on canvas
        self.canvas.create_image(0, 0, anchor="nw", image=self.tk_img)
        self.canvas.config(width=img_w, height=img_h)
        
        # Start image Rx thread
        self.rx_task = ImageRxTask(self)
        self.rx_task.daemon = True
        self.rx_task.start()
        
    def update_counter(self, num):
        self.counter.set("FPS: {}".format(num))
        
# Background thread to read image data and send to GUI
class ImageRxTask(threading.Thread):

    # Constructor
    def __init__(self, parent):
        self.gui = parent
        super().__init__()
        
    # Constructor
    def run(self):
        cnt = 0
        while True:
            print("blah")
            cnt = cnt + 1
            self.gui.update_counter(cnt)
            time.sleep(1.0)

# Main
if __name__ == "__main__":
    root = tk.Tk()
    root.title("Serial Image Capture")
    main_ui = GUI(root)
    root.mainloop()
    