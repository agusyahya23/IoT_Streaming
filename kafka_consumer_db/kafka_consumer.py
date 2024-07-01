import csv
import json
from aiokafka import AIOKafkaConsumer
from pydantic import BaseModel
from databases import Database
import pytz
from sqlalchemy import create_engine, Column, Integer, String, MetaData, Table, TIMESTAMP, Float
from dotenv import load_dotenv
import os
from datetime import datetime
import asyncio

load_dotenv()

DB_HOST = os.getenv("DB_HOST")
DB_PORT = os.getenv("DB_PORT")
DB_USER = os.getenv("DB_USER")
DB_PASSWORD = os.getenv("DB_PASSWORD")
DB_NAME = os.getenv("DB_NAME")
CLOUD_SQL_CONNECTION_NAME = os.getenv("CLOUD_SQL_CONNECTION_NAME")
BOOTSTRAP_SERVER = os.getenv("BOOTSTRAP_SERVER")
KAFKA_TOPIC = os.getenv("KAFKA_TOPIC")
TARGET_TABLE = os.getenv("TARGET_TABLE")
GROUP_ID = os.getenv("GROUP_ID")
AUTO_OFFSET_RESET = os.getenv("AUTO_OFFSET_RESET")
FILE = os.getenv("FILE")

LOG_FILE = FILE

DATABASE_URL = f"postgresql+psycopg2://{DB_USER}:{DB_PASSWORD}@{DB_HOST}:{DB_PORT}/{DB_NAME}"

class SensorData(BaseModel):
    timestamp: datetime
    longitude: float
    latitude: float
    temperature: float
    humidity: float
    pressure: float
    sensor_id: str

# Database setup
database = Database(DATABASE_URL)

metadata = MetaData()

# Define PostgreSQL table
sensors_table = Table(
    TARGET_TABLE,
    metadata,
    Column("id", Integer, primary_key=True, index=True),
    Column("timestamp", TIMESTAMP),
    Column("longitude", Float),
    Column("latitude", Float),
    Column("temperature", Float),
    Column("humidity", Float),
    Column("pressure", Float),
    Column("sensor_id", String),
)

# Create the table
engine = create_engine(DATABASE_URL)

metadata.create_all(engine)

async def insert_sensor_data(data: SensorData):
    timestamp = datetime.strptime(data['timestamp'], "%Y-%m-%d %H:%M:%S")
    query = sensors_table.insert().values(
        timestamp=timestamp,
        longitude=data['longitude'],
        latitude=data['latitude'],
        temperature=data['temperature'],
        humidity=data['humidity'],
        pressure=data['pressure'],
        sensor_id=data['sensor_id']
    )
    try:
        await database.execute(query)
        print("Data inserted successfully:", data)
    except Exception as e:
        print("Error inserting data:", e)

async def consume_and_insert():
    consumer = AIOKafkaConsumer(
        KAFKA_TOPIC,
        bootstrap_servers=BOOTSTRAP_SERVER,
        group_id=GROUP_ID,
        auto_offset_reset=AUTO_OFFSET_RESET,
        value_deserializer=lambda m: json.loads(m.decode('ascii')),
        loop=asyncio.get_running_loop()
    )
    await consumer.start()
    try:
        async for message in consumer:
            try:
                data = message.value
                await insert_sensor_data(data)
                print("Received message:", data)
            except Exception as e:
                print("Error processing Kafka message: %s", e)
    finally:
        await consumer.stop()

async def main():
    try:
        await database.connect()
        await consume_and_insert()
    finally:
        await database.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
