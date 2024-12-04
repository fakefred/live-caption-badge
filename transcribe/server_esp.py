import os, datetime, sys
import wave
import argparse
import socket
import json
import requests
import queue
import re
from multiprocessing import Process

import wave
import threading

from vosk import Model, KaldiRecognizer

print("###########Start loading Vosk model###########")
model = Model(lang="en-us")
print("###########Finish loading model###########")

from urllib import parse
from http.server import HTTPServer
from http.server import BaseHTTPRequestHandler
from http.server import ThreadingHTTPServer

PORT = 8000

global audio_queues
audio_queues = dict()  # IP address -> queue.Queue

BADGE_IP_ADDRS = ["192.168.227.113", "192.168.227.117"]


def poke_badge(ip: str):
    url = f"http://{ip}/poke"
    try:
        print(f"  -> Poking {ip}")
        requests.get(url)
    except:
        pass


def unpoke_badge(ip: str):
    url = f"http://{ip}/unpoke"
    try:
        print(f"  -> Unpoking {ip}")
        requests.get(url)
    except:
        pass


global buffer, speaking
buffer = queue.Queue()
speaking = dict() # IP address -> bool


def sendTranscriptionResult():
    global buffer
    while True:
        if not buffer.empty():
            words, clientAddress = buffer.get()
            try:
                url = f"http://{clientAddress}:80/transcription"
                r = requests.post(url, data=words)
                print(f"Sent: {words} to {url}, Response: {r.status_code}")
            except Exception as e:
                print(f"Error sending word {words} to {clientAddress}: {e}")
            finally:
                buffer.task_done()


class Handler(BaseHTTPRequestHandler):
    text = ""
    previous_stream = ""
    textBuffer = []

    def _set_headers(self, length):
        self.send_response(200)
        if length > 0:
            self.send_header("Content-length", str(length))
        self.end_headers()

    def _get_chunk_size(self):
        data = self.rfile.read(2)
        while data[-2:] != b"\r\n":
            data += self.rfile.read(1)
        return int(data[:-2], 16)

    def _get_chunk_data(self, chunk_size):
        data = self.rfile.read(chunk_size)
        self.rfile.read(2)
        return data

    def _write_wav(self, data, rates, bits, ch):
        t = datetime.datetime.now(datetime.UTC)
        time = t.strftime("%Y%m%dT%H%M%SZ")
        filename = str.format("{}_{}_{}_{}.wav", time, rates, bits, ch)

        wavfile = wave.open(filename, "wb")
        wavfile.setparams((ch, int(bits / 8), rates, 0, "NONE", "NONE"))
        wavfile.writeframesraw(bytearray(data))
        wavfile.close()
        return filename

    def do_POST(self):
        global audio_queues, buffer, speaking

        urlparts = parse.urlparse(self.path)
        request_file_path = urlparts.path.strip("/")
        total_bytes = 0
        sample_rates = 0
        bits = 0
        channel = 0
        if (
            request_file_path == "audio"
            and self.headers.get("Transfer-Encoding", "").lower() == "chunked"
        ):
            ip = self.client_address[0]
            print(f"POST /audio from {ip}")

            speaking[ip] = True

            for badge_ip in BADGE_IP_ADDRS:
                if badge_ip != ip:
                    p = Process(target=poke_badge, args=(badge_ip,))
                    p.start()

            rec = KaldiRecognizer(model, 16000)
            rec.SetWords(True)
            rec.SetPartialWords(True)
            data = []
            sample_rates = self.headers.get("x-audio-sample-rates", "").lower()
            bits = self.headers.get("x-audio-bits", "").lower()
            channel = self.headers.get("x-audio-channel", "").lower()
            sample_rates = self.headers.get("x-audio-sample-rates", "").lower()

            print(
                "Audio information, sample rates: {}, bits: {}, channel(s): {}".format(
                    sample_rates, bits, channel
                )
            )
            # https://stackoverflow.com/questions/24500752/how-can-i-read-exactly-one-response-chunk-with-pythons-http-client
            while True:
                chunk_size = self._get_chunk_size()  # 4096
                total_bytes += chunk_size
                print(f"<-   {ip}: RX {total_bytes} bytes total")
                if chunk_size == 0:
                    break
                else:
                    chunk_data = self._get_chunk_data(chunk_size)
                    data += chunk_data
                    for peer_ip, peer_queue in audio_queues.items():
                        if peer_ip != ip:
                            peer_queue.put(chunk_data)

                    if rec.AcceptWaveform(chunk_data):
                        result = json.loads(rec.Result())["text"]
                        print("Result:", result)
                        if result:
                            self.text = result
                            temp = result.split()
                            new_elements = temp[len(self.textBuffer) :]
                            self.textBuffer.extend(new_elements)
                            if new_elements:
                                buffer.put((" ".join(new_elements), self.client_address[0]))
                            for word in self.textBuffer:
                                print(word)
                            self.textBuffer = []
                            self.previous_stream = ""
                    else:
                        partialResult = json.loads(rec.PartialResult())["partial"]
                        if partialResult.startswith(self.previous_stream):
                            new_part = partialResult[
                                len(self.previous_stream) :
                            ].strip()
                            if new_part:
                                self.textBuffer.extend(new_part.split())
                                buffer.put((new_part, self.client_address[0]))
                        else:
                            self.textBuffer = partialResult.split()
                            buffer.put((partialResult, self.client_address[0]))
                        self.previous_stream = partialResult

            print("____________")
            self.send_response(200)
            self.send_header("Content-type", "text/html;charset=utf-8")
            self.end_headers()
            #  self.wfile.write(partialResult.encode("utf-8"))
            self._write_wav(data, 16000, 16, 1)
            speaking[ip] = False

    def do_GET(self):
        urlparts = parse.urlparse(self.path)
        request_file_path = urlparts.path.strip("/")

        if request_file_path == "audio":
            global audio_queues

            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()

            ip = self.client_address[0]
            audio_queues[ip] = queue.Queue()

            total_bytes = 0

            while True:
                try:
                    chunk = audio_queues[ip].get(timeout=1) # block max 1 second
                except queue.Empty:
                    peer_is_speaking = False
                    for peer_ip, peer_speak in speaking.items():
                        if peer_ip != ip and peer_speak:
                            peer_is_speaking = True

                    if peer_is_speaking:
                        # audio stream is alive
                        continue
                    else:
                        # close connection
                        p = Process(target=unpoke_badge, args=(ip,))
                        p.start()
                        break

                total_bytes += len(chunk)
                print(
                    f"  -> {ip}: TX {total_bytes} bytes total, queue length: {audio_queues[ip].qsize()}"
                )
                size = f"{len(chunk):X}".encode("utf-8")
                buf = size + b"\r\n" + chunk + b"\r\n"
                try:
                    self.wfile.write(buf)
                except ConnectionResetError:
                    del audio_queues[ip]
                    return

            self.wfile.write(b"0\r\n\r\n")
            del audio_queues[ip]
        elif request_file_path == "pair":
            if not urlparts.query.startswith("with="):
                self.send_response(400)

            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()


def get_host_ip():
    # https://www.cnblogs.com/z-x-y/p/9529930.html
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip


parser = argparse.ArgumentParser(
    description="HTTP Server save pipeline_raw_http example voice data to wav file"
)
parser.add_argument("--ip", "-i", nargs="?", type=str)
parser.add_argument("--port", "-p", nargs="?", type=int)
args = parser.parse_args()
if not args.ip:
    args.ip = get_host_ip()
if not args.port:
    args.port = PORT

transcriptionThread = threading.Thread(target=sendTranscriptionResult, daemon=True)
transcriptionThread.start()

httpd = ThreadingHTTPServer((args.ip, args.port), Handler)

print("Serving HTTP on {} port {}".format(args.ip, args.port))
httpd.serve_forever()
