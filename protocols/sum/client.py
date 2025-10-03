import socket
import time
import os

HEADER = 64
FORMAT = "utf-8"
PORT = "8765"
HOST = "127.0.0.1"

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, int(PORT)))


def send(msg: str):
    message = msg.encode(FORMAT)  # convert to bytes
    msg_length = len(message)  # get length of message in bytes
    send_length = str(msg_length).encode(FORMAT)  # convert length to bytes
    send_length += b" " * (HEADER - len(send_length))  # pad the length to HEADER size
    client.send(send_length)  # send the length of the message
    client.send(message)  # send the message


def recv():
    msg_length = client.recv(HEADER).decode(FORMAT)  # receive the length of the message
    if msg_length:
        msg_length = int(msg_length)
        msg = client.recv(msg_length).decode(FORMAT)  # receive the message
        print(f"[SERVER] len >> {msg_length} bytes")
        print(f"[SERVER] msg >> {msg}")
        return msg
    return None


def disconnect():
    send("!DISCONNECT")
    client.close()


if __name__ == "__main__":
    send("Hello")
    msg = recv()
    msg = msg.split(" ")
    a, b = int(msg[0]), int(msg[1])
    send(f"{a+b}")
    recv()
    disconnect()
