from flask import Flask, jsonify, send_from_directory
from flask_cors import CORS
import serial
import threading
import json
import time
import os

# --- CONFIGURATION ---
# !!! IMPORTANT: Check Device Manager to find your ESP32's COM port !!!
SERIAL_PORT = 'COM3' 
BAUD_RATE = 115200
# ---------------------

# Get the absolute path to the directory where this script is located
script_dir = os.path.dirname(__file__)

app = Flask(__name__)
CORS(app)

# Global dictionary to store the latest sensor data received from the ESP-NOW bridge
latest_data = {
    "temp": 0, "humid": 0, "aq_val": 0,
    "ozone_val": 0, "aqi": 0, "rssi": 0, "state": 0
}

def read_from_serial():
    """Continuously read JSON packets from the ESP32 receiver via Serial."""
    global latest_data
    while True:
        try:
            with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2) as ser:
                print(f"Successfully connected to {SERIAL_PORT}")
                while True:
                    line = ser.readline().decode('utf-8').strip()
                    if line:
                        try:
                            # Parse the JSON string sent by the ground receiver ESP32
                            data = json.loads(line)
                            latest_data = data
                            print(f"Received data: {latest_data}")
                        except json.JSONDecodeError:
                            print(f"Warning: Could not decode JSON: {line}")
        except serial.SerialException:
            print(f"Error: Could not connect to {SERIAL_PORT}. Retrying in 5 seconds...")
            time.sleep(5)

@app.route('/')
def index():
    """Serves the dashboard UI (dashboard.html)."""
    return send_from_directory(script_dir, 'dashboard.html')

@app.route('/data')
def get_data():
    """API endpoint used by the browser to fetch the latest telemetry."""
    return jsonify(latest_data)

if __name__ == '__main__':
    # Run the serial reading logic in a background thread to prevent blocking the web server
    serial_thread = threading.Thread(target=read_from_serial, daemon=True)
    serial_thread.start()
    
    print("--- Starting AuraSense local web server ---")
    print(f"1. Listening on Serial Port: {SERIAL_PORT}")
    print("2. Open browser and go to http://127.0.0.1:5000")
    
    # Start the Flask app
    app.run(host='0.0.0.0', port=5000)