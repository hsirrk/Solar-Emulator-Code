# Solar-Emulator-Code

A combined **Python Tkinter GUI** and **Arduino firmware** for controlling a stepper motor and a servo motor in a unified interface. Designed for hardware experimentation, prototyping, education, or lab setups where you need safe, responsive, and intuitive motor control.

The GUI provides real-time angle feedback, editable safety limits, ±5° quick buttons, custom angle input, and homing. The Arduino sketch handles precise movement, homing sequences (using a limit switch for the stepper and a button/trigger for the servo), and enforces software limits to protect the hardware.

## ✨ Features

### Stepper Motor Control
- Relative angle movements (custom or ±5° steps)
- Software upper/lower limits (default ±20°, fully editable from GUI)
- Automatic homing to limit switch
- Input cooldown to prevent command flooding
- Visual status feedback and current angle display

### Servo Motor Control
- Precise deviation control from a learned home position (±5° quick buttons or custom)
- Automatic homing sequence using a physical trigger/button with debounce
- Editable maximum deviation limit (default ±30°)
- Smart movement with overshoot/approach logic for better repeatability
- Automatic return-to-home on limit violation

### General
- Thread-safe serial communication (no freezing GUI)
- Mock mode when Arduino is not connected (great for development/testing)
- Real-time status messages and error handling
- Clean, responsive two-panel GUI layout

## 🛠️ Hardware Requirements

- Arduino board (Uno, Mega, etc.)
- Stepper motor + driver (with DIR and STEP pins)
- Limit switch for stepper homing
- Standard hobby servo (PWM)
- Push-button or optical trigger for servo homing
- USB connection to PC

**Pinout** is defined in the Arduino sketch (easy to modify).

## 📋 How to Use

1. **Upload the Arduino sketch** (`firmware.ino`) to your board.
2. **Update the COM port** in the Python script (`main.py` or whatever you name it) — currently set to `"COM6"`.
3. **Install Python dependencies**:
   ```bash
   pip install pyserial tkinter
