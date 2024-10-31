#!/usr/bin/env python3

# prerequisites: as described in https://alphacephei.com/vosk/install and also python module `sounddevice` (simply run command `pip install sounddevice`)
# Example usage using Dutch (nl) recognition model: `python test_microphone.py -m nl`
# For more help run: `python test_microphone.py -h`

import queue
import sys
import sounddevice as sd
import requests

SERVER_URL = "http://localhost:8080/stream"

q = queue.Queue()

def callback(indata, frames, time, status):
    """This is called (from a separate thread) for each audio block."""
    if status:
        print(status, file=sys.stderr)
    q.put(bytes(indata))

try:
    dump_fn = None
    device_info = sd.query_devices(None, "input")
    with sd.RawInputStream(samplerate=int(device_info["default_samplerate"]), blocksize = 8000, device=None,
            dtype="int16", channels=1, callback=callback):
        print("#" * 80)
        print("Press Ctrl+C to stop the recording")
        print("#" * 80)

        while True:
            data = q.get()
            
            try:
                response = requests.post(SERVER_URL, data = data, headers={'Content-Type': 'application/octet-stream'})
                print("Server response:", response.text)
            except requests.exceptions.RequestException as e:
                print(f"Error sending data: {e}")
except KeyboardInterrupt:
    print("\nDone")
