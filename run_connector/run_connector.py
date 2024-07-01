import http.client
import json

def post_json_to_connector():
    # Define the JSON configuration as a dictionary
    json_data = {
        "name": "mqtt-source",
        "config": {
            "connector.class": "io.confluent.connect.mqtt.MqttSourceConnector",
            "tasks.max": 1,
            "mqtt.server.uri": "tcp://34.123.176.43:1883",
            "mqtt.topics": "sensor_mqtt",
            "kafka.topic": "sensor_data",
            "value.converter": "org.apache.kafka.connect.converters.ByteArrayConverter",
            "confluent.topic.bootstrap.servers": "34.134.239.203:9092",
            "confluent.topic.replication.factor": 1
        }
    }

    # Convert the dictionary to a JSON string
    json_data_str = json.dumps(json_data)

    # Define the server and path
    server = "34.28.84.184"
    port = 8083
    path = "/connectors"

    # Create a connection to the server
    conn = http.client.HTTPConnection(server, port)

    # Define the headers
    headers = {
        "Content-Type": "application/json"
    }

    # Send the POST request
    conn.request("POST", path, body=json_data_str, headers=headers)

    # Get the response
    response = conn.getresponse()

    # Print the response status and body
    print("Status Code:", response.status)
    print("Response Body:", response.read().decode())

    # Close the connection
    conn.close()

# Run the function
post_json_to_connector()
