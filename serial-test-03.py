import tkinter as tk
from PIL import Image, ImageTk

# Create window
root = tk.Tk()
root.title("Serial Image Capture")

# Create the main container
frame_main = tk.Frame(root)
frame_main.pack(fill=tk.BOTH, expand=True)

# Allow middle cell of grid to grow when window is resized
frame_main.columnconfigure(1, weight=1)
frame_main.rowconfigure(0, weight=1)

# Create control widgets
frame_control = tk.Frame(frame_main)
label_fps = tk.Label(frame_control, text="FPS: ")
button_save = tk.Button(frame_control, text="Save Image", padx=5)

# Create canvas
canvas = tk.Canvas(frame_main, width=500, height=500)

# Lay out widgets on control frame
frame_control.grid(row=0, column=0, padx=5, pady=5, sticky=tk.NW)
label_fps.pack(anchor=tk.W)
button_save.pack()

# Lay out canvas on main frame grid
canvas.grid(row=0, column=1, padx=5, pady=5, sticky=tk.NW)

# Place focus on save button by default
button_save.focus()

# Open image, get size
img = Image.open("Patern_test.jpg")
img_w, img_h = img.size
print(img_w, img_h)

# Convert to TkInter image
tk_img = ImageTk.PhotoImage(img)

#Show image on canvas
canvas.create_image(0, 0, anchor="nw", image=tk_img)
canvas.config(width=img_w, height=img_h)

# Do main
root.mainloop()