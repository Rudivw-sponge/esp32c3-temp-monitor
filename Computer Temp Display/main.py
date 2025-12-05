import WinTmp
import tkinter as tk
from tkinter import ttk
import serial
import serial.tools.list_ports
import threading
import pystray
from PIL import Image, ImageTk
import json
import os
import sys
from pathlib import Path


# Resolve the files inside "assets" folder, both when running normally and when compiled by PyInstaller
def resource_path(relative_path: str) -> str:
    if getattr(sys, "_MEIPASS", None):
        base = Path(sys._MEIPASS)
    else:
        base = Path(__file__).resolve().parent
    return str(base / relative_path)


SETTINGS_FILE = resource_path('assets/settings.json')
ICON_FILE = resource_path('assets/dark-them.ico')


# Load settings from settings.json
def load_settings():
    if os.path.exists(SETTINGS_FILE):
        try:
            with open(SETTINGS_FILE, "r") as f:
                return json.load(f)
        except Exception as e:
            print(f"Loading Json Settings: {e}")
            return {}
    print("No settings file found.")
    return {}


settings = load_settings()


def save_settings(port, interval):
    print(f"SAVE SETTINGS {port} {interval}")
    new_settings = {"port": port, "interval": interval}
    with open(SETTINGS_FILE, "w") as f:
        json.dump(new_settings, f)


# Tkinter Setup and Styling
root = tk.Tk()
root.geometry("300x300")
root.resizable(False, False)
root.title("ESP32C3 LCD Hardware Temp Display")
root.configure(bg="#1e1e1e")

icon_image = ImageTk.PhotoImage(file=ICON_FILE)
root.iconphoto(False, icon_image)

# Tkinter dark theme and style
style = ttk.Style(root)
style.theme_use("clam")
style.configure("TLabel", background="#1e1e1e", foreground="#ffffff", font=("Segoe UI", 10))
style.configure("TButton", background="#333333", foreground="#ffffff", font=("Segoe UI", 10))
style.map("TButton", background=[("active", "#444444")])
style.configure("Dark.TCombobox", fieldbackground="#2b2b2b", background="#2b2b2b",
                foreground="#ffffff", arrowcolor="#ffffff")
style.map("Dark.TCombobox",
          fieldbackground=[("readonly", "#2b2b2b")],
          foreground=[("readonly", "#ffffff")])
root.option_add('*TCombobox*Listbox.background', '#2b2b2b')
root.option_add('*TCombobox*Listbox.foreground', '#ffffff')
root.option_add('*TCombobox*Listbox.selectBackground', '#444444')
root.option_add('*TCombobox*Listbox.selectForeground', '#ffffff')

# Temperatures Labels
label_cpu = tk.Label(root, text="CPU Temp: ", bg="#1e1e1e", fg="#ffffff")
label_gpu = tk.Label(root, text="GPU Temp: ", bg="#1e1e1e", fg="#ffffff")
label_cpu.pack(pady=5)
label_gpu.pack(pady=5)

# Port Selector prefer saved port, otherwise use empty string if none
ports = [port.device for port in serial.tools.list_ports.comports()]
default_port = settings.get("port")
if not default_port:
    default_port = ""
selected_port = tk.StringVar(value=default_port)
port_label = tk.Label(root, text="Select Port:", bg="#1e1e1e", fg="#ffffff")
port_label.pack()
port_combo = ttk.Combobox(root, textvariable=selected_port, values=ports,
                          state="readonly", style="Dark.TCombobox")
port_combo.pack(pady=4)

# Interval Selector
intervals = {"5s": 5000, "10s": 10000, "30s": 30000, "1m": 60000}
default_interval = settings.get("interval", "5s")
selected_interval = tk.StringVar(value=default_interval)

interval_label = tk.Label(root, text="Select Interval:", bg="#1e1e1e", fg="#ffffff")
interval_label.pack()
interval_combo = ttk.Combobox(root, textvariable=selected_interval,
                              values=list(intervals.keys()), state="readonly",
                              style="Dark.TCombobox")
interval_combo.pack(pady=4)

# Serial Connection Status
ser = None
status_label = tk.Label(root, text="Not connected", fg="red", bg="#1e1e1e")
status_label.pack(pady=5)


# Connect to selected serial port, esp32c3 baudrate is 115200 by default
def connect_serial():
    global ser
    try:
        if ser and ser.is_open:
            ser.close()
        ser = serial.Serial(port=selected_port.get(), baudrate=115200, timeout=1)
        status_label.config(text=f"Connected to {selected_port.get()}", fg="lightgreen")
        save_settings(selected_port.get(), selected_interval.get())
        send_interval()
    except Exception as e:
        status_label.config(text=f"Error: {e}", fg="red")


# Update temperatures and send to ESP32C3
# Note WinTmp library works only on Windows and requires admin privileges
def update_temps():
    if ser and ser.is_open:
        cpu_temp = WinTmp.CPU_Temp()
        gpu_temp = WinTmp.GPU_Temp()
        label_cpu.config(text=f"CPU Temp: {cpu_temp}°C")
        label_gpu.config(text=f"GPU Temp: {gpu_temp}°C")
        ser.write(f"{cpu_temp} {gpu_temp}\n".encode())
    root.after(1000, update_temps)


# Set interval on ESP32C3
def send_interval(event=None):
    if ser and ser.is_open:
        interval_ms = intervals[selected_interval.get()]
        ser.write(f"interval={interval_ms}\r\n".encode())
        ser.flush()
        save_settings(selected_port.get(), selected_interval.get())


interval_combo.bind("<<ComboboxSelected>>", send_interval)


# Read serial messages from ESP32C3
def read_serial():
    if ser and ser.is_open and ser.in_waiting:
        try:
            msg = ser.readline().decode(errors="ignore").strip()
            if msg:
                print("ESP32:", msg)
        except Exception as e:
            status_label.config(text=f"Read error: {e}", fg="red")
    root.after(200, read_serial)


# Connect Button
connect_btn = ttk.Button(root, text="Connect", command=connect_serial)
connect_btn.pack(pady=10)


# Tkinter App Minimization to System Tray
def show_window(icon, item):
    root.after(0, root.deiconify)
    # restore from minimized
    root.after(0, root.state, "normal")


def hide_window():
    root.withdraw()


def quit_app(icon, item):
    icon.stop()
    root.quit()


# If window is minimized, hide it completely
def on_minimize(event):
    if root.state() == "iconic":
        hide_window()


# Bind minimize event
root.bind("<Unmap>", on_minimize)


def run_tray():
    try:
        icon = pystray.Icon("ESP32C3",
                            Image.open(ICON_FILE),
                            menu=pystray.Menu(
                                pystray.MenuItem("Show", show_window),
                                pystray.MenuItem("Quit", quit_app)
                            ))
        icon.run()
    except Exception as e:
        print("Tray icon error:", e)


# Start system tray in a separate thread
threading.Thread(target=run_tray, daemon=True).start()
# Hide window on startup if settings file exists with saved port and interval
if os.path.exists(SETTINGS_FILE):
    hide_window()

# Auto connect if settings exist on startup
if default_port and default_interval:
    root.after(500, connect_serial)

update_temps()
read_serial()
root.mainloop()
