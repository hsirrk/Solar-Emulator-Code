import tkinter as tk
import serial
from time import sleep, monotonic
import threading

STEPPER_UPPER_LIMIT: float = 20.0
STEPPER_LOWER_LIMIT: float = -20.0
SERVO_MAX_DEVIATION: int = 30
STEPPER_INPUT_COOLDOWN_SEC: float = 0.35
SERVO_INPUT_COOLDOWN_SEC: float = 0.35

stepper_curr_angle: float = 0.0
servo_curr_angle: int = 0
servo_home_angle: int = -1  # filled during init from firmware

serial_lock = threading.Lock()
stepper_busy = threading.Lock()

_next_stepper_input_time: float = 0.0
_next_servo_input_time: float = 0.0


def _stepper_input_allowed() -> bool:
    global _next_stepper_input_time
    now = monotonic()
    if now < _next_stepper_input_time:
        remaining = _next_stepper_input_time - now
        stepper_status.config(text=f"Please wait {remaining:.1f}s before next command", fg="orange")
        return False
    _next_stepper_input_time = now + STEPPER_INPUT_COOLDOWN_SEC
    return True


def _servo_input_allowed() -> bool:
    global _next_servo_input_time
    now = monotonic()
    if now < _next_servo_input_time:
        remaining = _next_servo_input_time - now
        servo_status.config(text=f"Please wait {remaining:.1f}s before next command", fg="orange")
        return False
    _next_servo_input_time = now + SERVO_INPUT_COOLDOWN_SEC
    return True

# ===== SERIAL CONNECTION =====
connected = False
try:
    ser = serial.Serial("COM6", 9600, timeout=30)
    sleep(2)  # Wait for Arduino to reset

    # Read initialization messages until READY
    while True:
        line = ser.readline().decode().strip()
        if line:
            print(f"Init: {line}")
            # Extract servo home angle for display
            if line.startswith("SERVO:Home angle="):
                try:
                    servo_home_angle = int(line.split("=")[1])
                except ValueError:
                    pass
        if line == "READY":
            break
        if not line:  # timeout
            print("Timeout waiting for READY — continuing anyway")
            break

    ser.timeout = 5  # Normal operation timeout
    connected = True
    print("Successfully connected to Arduino")
except Exception as e:
    print(f"Arduino not connected: {e}")
    class MockSerial:
        def write(self, data):
            print(f"Mock write: {data}")
        def readline(self):
            return b"STEPPER:0.0\n"
    ser = MockSerial()


def send_command(cmd: str) -> str:
    """Send a command and return the response (thread-safe)."""
    with serial_lock:
        ser.write(f"{cmd}\n".encode())
        response = ser.readline().decode().strip()
        return response

def stepper_move_to_angle(angle: float) -> None:
    global stepper_curr_angle

    if angle > STEPPER_UPPER_LIMIT or angle < STEPPER_LOWER_LIMIT:
        win.after(0, lambda: stepper_status.config(
            text="Error: Angle exceeds limits", fg="red"))
        return

    target = stepper_curr_angle + angle
    if target > STEPPER_UPPER_LIMIT or target < STEPPER_LOWER_LIMIT:
        # Reset to home position
        send_command(f"S{-stepper_curr_angle}")
        stepper_curr_angle = 0
        win.after(0, update_stepper_label)
        win.after(0, lambda: stepper_status.config(
            text="Error: Limit exceeded, reset to home", fg="red"))
        return

    stepper_curr_angle += angle
    win.after(0, update_stepper_label)

    response = send_command(f"S{angle}")
    print(f"Stepper: {response}")
    win.after(0, lambda: stepper_status.config(text="", fg="green"))


def _run_stepper_move(angle: float) -> None:
    if not stepper_busy.acquire(blocking=False):
        win.after(0, lambda: stepper_status.config(text="Stepper is busy, wait for movement", fg="orange"))
        return
    try:
        stepper_move_to_angle(angle)
    finally:
        stepper_busy.release()


def stepper_move_custom():
    try:
        if not _stepper_input_allowed():
            return
        angle = float(stepper_entry.get())
        stepper_entry.delete(0, tk.END)
        threading.Thread(target=_run_stepper_move, args=(angle,), daemon=True).start()
        stepper_status.config(text="", fg="green")
    except ValueError:
        stepper_status.config(text="Error: Enter a valid number", fg="red")


def stepper_move_plus5():
    if not _stepper_input_allowed():
        return
    threading.Thread(target=_run_stepper_move, args=(5.0,), daemon=True).start()


def stepper_move_minus5():
    if not _stepper_input_allowed():
        return
    threading.Thread(target=_run_stepper_move, args=(-5.0,), daemon=True).start()


def stepper_home():
    if not _stepper_input_allowed():
        return
    threading.Thread(target=_run_stepper_move, args=(-stepper_curr_angle,), daemon=True).start()

servo_busy = threading.Lock()  # prevent queuing commands during rapid clicks

def _parse_servo_response(response: str) -> None:
    """Parse a SERVO: response and update angle + status labels."""
    global servo_curr_angle
    is_error = "ERROR" in response

    display_text = response
    if response.startswith("SERVO:"):
        val = response[6:]
        # Strip "SERVO:" prefix for display
        # Also strip debug info like " [target=47]" 
        display_text = val.split("[")[0].strip() if "[" in val else val
        # Try to extract numeric angle (before any debug suffix)
        num_str = val.split()[0] if " " in val else val
        try:
            servo_curr_angle = int(num_str)
        except ValueError:
            # On error, servo resets to home (0)
            if is_error:
                servo_curr_angle = 0

    display_text = display_text.replace("ERROR", "Error")

    win.after(0, update_servo_label)
    if is_error:
        win.after(0, lambda t=display_text: servo_status.config(text=t, fg="red"))
    else:
        win.after(0, lambda: servo_status.config(text="", fg="green"))


def servo_forward():
    if not _servo_input_allowed():
        return
    def _run():
        global servo_curr_angle
        if not servo_busy.acquire(blocking=False):
            return  # drop click if another command is in-flight
        try:
            servo_curr_angle += 5
            win.after(0, update_servo_label)
            response = send_command("VF")
            print(f"Servo: {response}")
            _parse_servo_response(response)
        finally:
            servo_busy.release()
    threading.Thread(target=_run, daemon=True).start()


def servo_backward():
    if not _servo_input_allowed():
        return
    def _run():
        global servo_curr_angle
        if not servo_busy.acquire(blocking=False):
            return
        try:
            servo_curr_angle -= 5
            win.after(0, update_servo_label)
            response = send_command("VB")
            print(f"Servo: {response}")
            _parse_servo_response(response)
        finally:
            servo_busy.release()
    threading.Thread(target=_run, daemon=True).start()


def servo_reset():
    if not _servo_input_allowed():
        return
    def _run():
        global servo_curr_angle
        if not servo_busy.acquire(blocking=False):
            return
        try:
            response = send_command("VR")
            print(f"Servo: {response}")
            servo_curr_angle = 0
            win.after(0, update_servo_label)
            win.after(0, lambda: servo_status.config(text="", fg="green"))
        finally:
            servo_busy.release()
    threading.Thread(target=_run, daemon=True).start()


def servo_move_custom():
    try:
        if not _servo_input_allowed():
            return
        angle = float(servo_entry.get())
        servo_entry.delete(0, tk.END)
        def _run():
            if not servo_busy.acquire(blocking=False):
                return
            try:
                response = send_command(f"V{angle}")
                print(f"Servo: {response}")
                _parse_servo_response(response)
            finally:
                servo_busy.release()
        threading.Thread(target=_run, daemon=True).start()
        servo_status.config(text="", fg="green")
    except ValueError:
        servo_status.config(text="Error: Enter a valid number", fg="red")


def apply_stepper_upper_limit():
    global STEPPER_UPPER_LIMIT
    try:
        val = float(stepper_upper_entry.get())
        STEPPER_UPPER_LIMIT = val
        update_stepper_limits_label()
        def _run():
            response = send_command(f"LU{val}")
            print(f"Limit: {response}")
            win.after(0, lambda: stepper_status.config(text=f"Upper limit set to {val}°", fg="green"))
        threading.Thread(target=_run, daemon=True).start()
    except ValueError:
        stepper_status.config(text="Error: Enter a valid number", fg="red")


def apply_stepper_lower_limit():
    global STEPPER_LOWER_LIMIT
    try:
        val = float(stepper_lower_entry.get())
        STEPPER_LOWER_LIMIT = val
        update_stepper_limits_label()
        def _run():
            response = send_command(f"LL{val}")
            print(f"Limit: {response}")
            win.after(0, lambda: stepper_status.config(text=f"Lower limit set to {val}°", fg="green"))
        threading.Thread(target=_run, daemon=True).start()
    except ValueError:
        stepper_status.config(text="Error: Enter a valid number", fg="red")


def apply_servo_limit():
    global SERVO_MAX_DEVIATION
    try:
        val = int(servo_limit_entry.get())
        SERVO_MAX_DEVIATION = val
        update_servo_limits_label()
        def _run():
            response = send_command(f"LS{val}")
            print(f"Limit: {response}")
            win.after(0, lambda: servo_status.config(text=f"Max deviation set to ±{val}°", fg="green"))
        threading.Thread(target=_run, daemon=True).start()
    except ValueError:
        servo_status.config(text="Error: Enter a valid integer", fg="red")

win = tk.Tk()
win.title("Combined Motor Control")
win.minsize(700, 550)

main_frame = tk.Frame(win)
main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
main_frame.columnconfigure(0, weight=1)
main_frame.columnconfigure(1, weight=1)

# ---------- STEPPER PANEL (left) ----------
stepper_frame = tk.LabelFrame(main_frame, text="  Stepper Motor  ",
                              padx=15, pady=10, font=("Arial", 11, "bold"))
stepper_frame.grid(row=0, column=0, padx=10, sticky="nsew")

tk.Label(stepper_frame, text="Enter angle to move:").pack(pady=(5, 2))

stepper_entry = tk.Entry(stepper_frame, width=15, justify="center")
stepper_entry.pack(pady=2)
stepper_entry.bind('<Return>', lambda e: stepper_move_custom())

tk.Button(stepper_frame, text="Move", width=10, command=stepper_move_custom).pack(pady=5)

stepper_btn_frame = tk.Frame(stepper_frame)
stepper_btn_frame.pack(pady=5)
tk.Button(stepper_btn_frame, text="-5°", width=8, command=stepper_move_minus5).grid(row=0, column=0, padx=5)
tk.Button(stepper_btn_frame, text="+5°", width=8, command=stepper_move_plus5).grid(row=0, column=1, padx=5)

tk.Button(stepper_frame, text="Home", width=10, command=stepper_home).pack(pady=5)

stepper_angle_label = tk.Label(stepper_frame,
                               text=f"Current angle: {stepper_curr_angle:.1f}°",
                               font=("Arial", 12))
stepper_angle_label.pack(pady=5)

# --- Stepper Limits (editable) ---
stepper_limits_label = tk.Label(stepper_frame,
                                text=f"Current limits: [{STEPPER_LOWER_LIMIT}°, {STEPPER_UPPER_LIMIT}°]",
                                font=("Arial", 10, "bold"), fg="blue")
stepper_limits_label.pack(pady=(8, 2))

stepper_lim_frame = tk.Frame(stepper_frame)
stepper_lim_frame.pack(pady=2)

tk.Label(stepper_lim_frame, text="Lower:").grid(row=0, column=0, padx=2)
stepper_lower_entry = tk.Entry(stepper_lim_frame, width=7, justify="center")
stepper_lower_entry.grid(row=0, column=1, padx=2)
stepper_lower_entry.insert(0, str(STEPPER_LOWER_LIMIT))
tk.Button(stepper_lim_frame, text="Set", width=4, command=apply_stepper_lower_limit).grid(row=0, column=2, padx=2)

tk.Label(stepper_lim_frame, text="Upper:").grid(row=1, column=0, padx=2, pady=2)
stepper_upper_entry = tk.Entry(stepper_lim_frame, width=7, justify="center")
stepper_upper_entry.grid(row=1, column=1, padx=2, pady=2)
stepper_upper_entry.insert(0, str(STEPPER_UPPER_LIMIT))
tk.Button(stepper_lim_frame, text="Set", width=4, command=apply_stepper_upper_limit).grid(row=1, column=2, padx=2, pady=2)

stepper_status = tk.Label(stepper_frame, text="", fg="green", wraplength=200)
stepper_status.pack(pady=5)

# ---------- SERVO PANEL (right) ----------
servo_frame = tk.LabelFrame(main_frame, text="  Servo Motor  ",
                            padx=15, pady=10, font=("Arial", 11, "bold"))
servo_frame.grid(row=0, column=1, padx=10, sticky="nsew")

tk.Label(servo_frame, text="Enter angle to move:").pack(pady=(5, 2))

servo_entry = tk.Entry(servo_frame, width=15, justify="center")
servo_entry.pack(pady=2)
servo_entry.bind('<Return>', lambda e: servo_move_custom())

tk.Button(servo_frame, text="Move", width=10, command=servo_move_custom).pack(pady=5)

servo_btn_frame = tk.Frame(servo_frame)
servo_btn_frame.pack(pady=5)
tk.Button(servo_btn_frame, text="-5°", width=8, command=servo_backward).grid(row=0, column=0, padx=5)
tk.Button(servo_btn_frame, text="+5°", width=8, command=servo_forward).grid(row=0, column=1, padx=5)

tk.Button(servo_frame, text="Home", width=10, command=servo_reset).pack(pady=5)

servo_angle_label = tk.Label(servo_frame,
                             text=f"Current angle: {servo_curr_angle}°",
                             font=("Arial", 12))
servo_angle_label.pack(pady=5)
servo_home_info = tk.Label(servo_frame,
                           text=f"Home position: {servo_home_angle}\u00b0" if servo_home_angle >= 0 else "Home position: unknown",
                           font=("Arial", 9), fg="gray")
servo_home_info.pack(pady=(0, 5))
# --- Servo Limits (editable) ---
servo_limits_label = tk.Label(servo_frame,
                              text=f"Current limit: ±{SERVO_MAX_DEVIATION}°",
                              font=("Arial", 10, "bold"), fg="blue")
servo_limits_label.pack(pady=(8, 2))

servo_lim_frame = tk.Frame(servo_frame)
servo_lim_frame.pack(pady=2)

tk.Label(servo_lim_frame, text="Max ±:").grid(row=0, column=0, padx=2)
servo_limit_entry = tk.Entry(servo_lim_frame, width=7, justify="center")
servo_limit_entry.grid(row=0, column=1, padx=2)
servo_limit_entry.insert(0, str(SERVO_MAX_DEVIATION))
tk.Button(servo_lim_frame, text="Set", width=4, command=apply_servo_limit).grid(row=0, column=2, padx=2)

servo_status = tk.Label(servo_frame, text="", fg="green", wraplength=200)
servo_status.pack(pady=5)

status_text = f"Connected: COM3" if connected else "Not connected (mock mode)"
status_bar = tk.Label(win, text=status_text, bd=1, relief=tk.SUNKEN, anchor=tk.W)
status_bar.pack(side=tk.BOTTOM, fill=tk.X)

def update_stepper_label():
    stepper_angle_label.config(text=f"Current angle: {stepper_curr_angle:.1f}°")


def update_servo_label():
    servo_angle_label.config(text=f"Current angle: {servo_curr_angle}°")


def update_stepper_limits_label():
    stepper_limits_label.config(text=f"Current limits: [{STEPPER_LOWER_LIMIT}°, {STEPPER_UPPER_LIMIT}°]")


def update_servo_limits_label():
    servo_limits_label.config(text=f"Current limit: ±{SERVO_MAX_DEVIATION}°")


win.mainloop()