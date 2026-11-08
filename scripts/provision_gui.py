#!/usr/bin/env python3
"""Local masked Wi-Fi dialog. Credentials go directly to ESP32 over USB."""
import argparse
import json
from pathlib import Path
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', required=True)
parser.add_argument('--result', type=Path)
parser.add_argument('--lang', choices=['en', 'zh-CN'], default='en', help='Dialog language (default: en)')
args = parser.parse_args()

def text(en, zh):
    return zh if args.lang == 'zh-CN' else en

root = tk.Tk()
root.title(text('FrostBridge · ESP32 Wi-Fi setup', 'FrostBridge · ESP32 配网'))
root.geometry('480x285')
root.resizable(False, False)
root.attributes('-topmost', True)
frame = ttk.Frame(root, padding=20)
frame.pack(fill='both', expand=True)
ttk.Label(frame, text=text('Enter 2.4GHz Wi-Fi. The password is sent to ESP32 over USB only.', '输入 2.4GHz Wi-Fi；密码仅经本机 USB 发给 ESP32。'), wraplength=435).grid(row=0, column=0, columnspan=2, pady=(0, 15), sticky='w')
ttk.Label(frame, text=text('Wi-Fi SSID', 'Wi-Fi 名称')).grid(row=1, column=0, sticky='w', pady=5)
ssid_entry = ttk.Entry(frame, width=35)
ssid_entry.grid(row=1, column=1, pady=5)
ttk.Label(frame, text=text('Wi-Fi password', 'Wi-Fi 密码')).grid(row=2, column=0, sticky='w', pady=5)
password_entry = ttk.Entry(frame, width=35, show='•')
password_entry.grid(row=2, column=1, pady=5)
status = tk.StringVar(value=text('Credentials are not saved to project files or logs.', '凭据不会写入聊天、项目文件或日志。'))
ttk.Label(frame, textvariable=status, wraplength=435).grid(row=3, column=0, columnspan=2, sticky='w', pady=12)
events = queue.Queue()
busy = False


def configure(ssid, password):
    try:
        with serial.Serial(args.port, 115200, timeout=1) as port:
            port.dtr = False
            port.rts = False
            time.sleep(4)
            port.reset_input_buffer()
            port.write(('WIFI\t' + ssid + '\t' + password + '\n').encode())
            password = ''
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                line = port.readline().decode(errors='replace').strip()
                if line == 'M0_CONFIG_SAVED':
                    events.put(('saved', None))
                    return
                if line == 'M0_CONFIG_REJECTED':
                    events.put(('error', text('Device rejected configuration. Check the input lengths.', '设备拒绝配置，请检查输入长度。')))
                    return
            events.put(('error', text('No device acknowledgement. Check the port and firmware.', '未收到设备确认，请检查串口与固件。')))
    except serial.SerialException:
        events.put(('error', text('Cannot open the port. Close other serial monitors and retry.', '无法访问串口，请关闭其他串口监视器后重试。')))
    except Exception:
        events.put(('error', text('Setup failed. Retry; credentials were not saved locally.', '配置失败，请重试。凭据未写入本地文件。')))


def submit():
    global busy
    ssid = ssid_entry.get()
    password = password_entry.get()
    if not (1 <= len(ssid.encode()) <= 32 and 8 <= len(password.encode()) <= 63):
        status.set(text('SSID: 1–32 bytes; password: 8–63 bytes.', '名称需要 1–32 字节，密码需要 8–63 字节。'))
        return
    if any(c in ssid + password for c in '\t\r\n\0'):
        status.set(text('SSID/password cannot contain tabs, line breaks or NUL.', '名称和密码不能包含制表符或换行。'))
        return
    busy = True
    save_button.config(state='disabled')
    password_entry.delete(0, 'end')
    ssid_entry.config(state='disabled')
    status.set(text('Saving to ESP32 over USB. Please wait…', '正在通过 USB 保存到 ESP32，请稍候…'))
    threading.Thread(target=configure, args=(ssid, password), daemon=True).start()


def finish():
    if args.result:
        args.result.parent.mkdir(parents=True, exist_ok=True)
        args.result.write_text(json.dumps({'configuration_saved': True}) + '\n')
    print('M0_GUI_CONFIG_SAVED', flush=True)
    root.destroy()


def poll():
    global busy
    try:
        kind, message = events.get_nowait()
        busy = False
        if kind == 'saved':
            status.set(text('Saved to ESP32; restarting. This window will close shortly.', '已保存到 ESP32，设备正在重启。此窗口即将关闭。'))
            root.after(2000, finish)
            return
        status.set(message)
        save_button.config(state='normal')
        ssid_entry.config(state='normal')
    except queue.Empty:
        pass
    root.after(100, poll)


save_button = ttk.Button(frame, text=text('Save to ESP32', '配置到 ESP32'), command=submit)
save_button.grid(row=4, column=1, sticky='e')
root.protocol('WM_DELETE_WINDOW', lambda: None if busy else root.destroy())
root.bind('<Return>', lambda event: submit() if not busy else None)
root.after(100, poll)
root.after(300, lambda: root.attributes('-topmost', False))
ssid_entry.focus_set()
print('M0_GUI_DIALOG_READY', flush=True)
root.mainloop()
