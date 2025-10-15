from flask import Flask, request, jsonify, render_template
import pandas as pd
import numpy as np
import joblib
import datetime
import requests
import os

from flask_cors import CORS

app = Flask(__name__)
CORS(app)

# Load your trained model
model = joblib.load("fire_model.pkl")  

# Create a log file if it doesn’t exist
log_file = "fire_logs.csv"
if not os.path.exists(log_file):
    pd.DataFrame(columns=["timestamp", "flame", "smoke", "temperature", "humidity", "alert_intensity"]).to_csv(log_file, index=False)

# Mapping for reverse lookup
intensity_map = {0: "Low", 1: "Medium", 2: "High"}


@app.route('/')
def dashboard():
    """Serve the dashboard HTML page"""
    return render_template("dashboard.html")


@app.route('/predict', methods=['POST'])
def predict():
    """Receive sensor data from ESP8266 and return predicted alert intensity"""
    try:
        data = request.get_json()

        flame = data.get("flame")
        smoke = data.get("smoke")
        temperature = data.get("temperature")
        humidity = data.get("humidity")

        # Ensure all values exist
        if None in [flame, smoke, temperature, humidity]:
            return jsonify({"error": "Missing one or more sensor values"}), 400

        # Convert to 2D numpy array for model
        features = np.array([[flame, smoke, temperature, humidity]])
        prediction = model.predict(features)[0]

        alert_level = intensity_map.get(prediction, "Unknown")
        
        # Log data to CSV for dashboard
        log_entry = pd.DataFrame([{
            "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "flame": flame,
            "smoke": smoke,
            "temperature": temperature,
            "humidity": humidity,
            "alert_intensity": alert_level
        }])
        log_entry.to_csv(log_file, mode='a', header=False, index=False)

        return jsonify({"alert_intensity": alert_level})

    except Exception as e:
        print("Error:", e)
        return jsonify({"error": str(e)}), 500
        

@app.route('/data')
def data():
    """Serve logged data for dashboard visualization"""
    df = pd.read_csv(log_file)
    return df.to_json(orient="records")


        
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)