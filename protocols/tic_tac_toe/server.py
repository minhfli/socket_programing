import socket
import threading

HEADER = 64
FORMAT = "utf-8"
PORT = "8765"
HOST = "0.0.0.0"
DISCONNECT_MESSAGE = "!DISCONNECT"


server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, int(PORT)))
server.settimeout(5)


def send(conn, addr, msg: str):
    message = msg.encode(FORMAT)  # convert to bytes
    msg_length = len(message)  # get length of message in bytes
    send_length = str(msg_length).encode(FORMAT)  # convert length to bytes
    send_length += b" " * (HEADER - len(send_length))  # pad the length to HEADER size
    conn.send(send_length)  # send the length of the message
    conn.send(message)  # send the message
    print(f"[{addr}] <<< << {msg}")


def recv(conn, addr):
    msg_length = conn.recv(HEADER).decode(FORMAT)  # receive the length of the message
    if msg_length:
        msg_length = int(msg_length)
        msg = conn.recv(msg_length).decode(FORMAT)  # receive the message
        print(f"[{addr}] len >> {msg_length} bytes")
        print(f"[{addr}] msg >> {msg}")
        return msg
    return None


def handle_client(conn: socket.socket, addr):
    print(f"[NEW CONNECTION] {addr} connected.")
    connected = True

    step = 0
    while connected:

        msg = recv(conn, addr)

        if msg == DISCONNECT_MESSAGE:
            connected = False
            break

        send(conn, addr, "Msg received: " + msg)

    conn.close()


def start():
    server.listen()
    print(f"[LISTENING] Server is listening on {HOST}:{PORT}")
    while True:
        # accept new connection
        try:
            conn, addr = server.accept()  # blocking call
        except socket.timeout:
            continue  # check for Ctrl+C to Terminate
        # create a new thread for the client
        thread = threading.Thread(target=handle_client, args=(conn, addr))
        thread.start()
        print(f"[ACTIVE CONNECTIONS] {threading.active_count() - 1}")


if __name__ == "__main__":
    print("[STARTING] Server is starting...")
    start()
