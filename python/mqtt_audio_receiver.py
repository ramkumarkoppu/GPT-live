import paho.mqtt.client as mqtt

broker = "127.0.0.1"   # local mosquitto on this Pi; ESP32 connects to 192.168.1.123
port = 1883 # Standard MQTT port
topic = "xvf3800/audio.wav" # Topic to Subscribe to receive audio
output_file = "output.wav"
audio_data = bytearray()

def on_message(client, userdata, msg):
    global audio_data
    print("🔊 Received audio message")
    audio_data.extend(msg.payload)
    with open(output_file, "wb") as f:
        f.write(audio_data)
    print(f"✅ WAV file saved as: {output_file}")

client = mqtt.Client()
client.on_message = on_message

print(f"Connecting to MQTT broker {broker}...")
client.connect(broker, port, 60)
client.subscribe(topic)
print(f"📡 Subscribed to: {topic}")
client.loop_forever()
