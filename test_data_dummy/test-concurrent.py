from datetime import datetime, timedelta
import paho.mqtt.client as mqtt
import json
import random
import time
import concurrent.futures
import pytz

from dotenv import load_dotenv
import os
import asyncio

load_dotenv()

MAX_SENSOR = os.getenv("MAX_SENSOR")
DELAY = os.getenv("DELAY")

# Define MQTT broker host and port
broker_host = "34.123.176.43"
broker_port = 1883

# Define callback functions for MQTT events
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")  
    else:
        print(f"Failed to connect, return code {rc}")

def on_publish(client, userdata, mid):
    global message_count
    message_count += 1
    print("Message published")
    print("Total messages sent:", message_count)

# Create MQTT client instance
client = mqtt.Client()

# Set up callback functions
client.on_connect = on_connect
client.on_publish = on_publish

# Connect to MQTT broker
client.connect(broker_host, broker_port)

# Start background thread for MQTT client
client.loop_start()

# Configuration for the 5 specific sensors
specific_sensors = [
    {"latitude": -6.3643444, "longitude": 106.8290695, "sensor_id": "sensor1"},
    {"latitude": -6.3653083, "longitude": 106.8246415, "sensor_id": "sensor2"},
    {"latitude": -6.3651944, "longitude": 106.8311499, "sensor_id": "sensor3"},
    {"latitude": -6.367975, "longitude": 106.830208, "sensor_id": "sensor4"},
    {"latitude": -6.3705264, "longitude": 106.8268372, "sensor_id": "sensor5"},
    {"latitude": -6.3607737, "longitude": 106.8265036, "sensor_id": "sensor6"}
]

# Generate fixed random latitude and longitude for the 45 random sensors
random_sensors_config = []
max_sensor = int(MAX_SENSOR)

for sensor_id in range(7, max_sensor + 1):
    random_latitude = round(random.uniform(-90, 90), 6)
    random_longitude = round(random.uniform(-180, 180), 6)
    sensor_id = str(round(random.uniform(1, max_sensor)))
    # Initialize timestamp to current time
    jakarta_timezone = pytz.timezone('Asia/Jakarta')
    timestamp = datetime.now(jakarta_timezone)
    random_sensors_config.append({
        "latitude": random_latitude, 
        "longitude": random_longitude, 
        "timestamp": timestamp.strftime("%Y-%m-%d %H:%M:%S"),
        "current_time": timestamp.strftime("%Y-%m-%d %H:%M:%S"),
        "sensor_id": sensor_id
        })

# Function to generate data for a sensor
def generate_sensor_data(sensor_config):
    temperature = round(random.uniform(-20, 40), 2)
    humidity = round(random.uniform(30, 60), 2)
    pressure = round(random.uniform(5, 15), 2)

    jakarta_timezone = pytz.timezone('Asia/Jakarta')
    current_time = datetime.now(jakarta_timezone)

    data = {
        "timestamp": current_time.strftime("%Y-%m-%d %H:%M:%S"),
        "latitude": sensor_config["latitude"],
        "longitude": sensor_config["longitude"],
        "temperature": temperature,
        "humidity": humidity,
        "pressure": pressure,
        "current_time": current_time.strftime("%Y-%m-%d %H:%M:%S"),
        "sensor_id": sensor_config["sensor_id"]
    }

    return data

# Function to continuously publish data from a sensor
def publish_sensor_data(sensor_config):
    while True:
        data = generate_sensor_data(sensor_config)
        json_string = json.dumps(data).encode('ascii')
        client.publish("sensor_mqtt", json_string)
        time.sleep(int(DELAY))  # Adjust sleep time as needed for the desired publishing rate

async def stop_after_1_minutes():
    await asyncio.sleep(60)  # 5 minutes = 300 seconds
    print("Stopping execution after 5 minutes")
    client.loop_stop()  # Stop MQTT client loop
    # Optionally, you can also disconnect the MQTT client if needed
    client.disconnect()

# Create a thread pool executor for concurrent publishing
with concurrent.futures.ThreadPoolExecutor(max_workers=2000) as executor:
    message_count = 0
    # # Submit tasks for the 5 specific sensors
    # for sensor_config in specific_sensors:
    #     executor.submit(publish_sensor_data, sensor_config)
        
    # # Submit tasks for the 45 random sensors
    # for sensor_config in random_sensors_config:
    #     executor.submit(publish_sensor_data, sensor_config)

    # executor.map(publish_sensor_data, specific_sensors)
    executor.map(publish_sensor_data, specific_sensors + random_sensors_config)
    # asyncio.run(stop_after_1_minutes())
# The program will keep running until manually stopped.
