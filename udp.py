import socket
import struct
import time

destination = ("127.0.0.1", 12345)

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client_socket:

    print("made socket")

    for index in range(10):

        time.sleep(1)

        # Pack a TTL message.
        ttl_message = bytearray(11)
        struct.pack_into('B', ttl_message, 0, 1)
        struct.pack_into('d', ttl_message, 1, time.time())
        struct.pack_into('B', ttl_message, 9, 7)
        struct.pack_into('B', ttl_message, 10, index % 2)

        bytes_sent = client_socket.sendto(ttl_message, destination)
        print(f"TTL: sent {bytes_sent} message bytes")

        # Pack a text message.
        text_bytes = b"He who laughs last laughs ... never gonna laugh him again."
        text_message = bytearray(11 + len(text_bytes))
        struct.pack_into('B', text_message, 0, 2)
        struct.pack_into('d', text_message, 1, time.time())
        text_message[9:9+len(text_bytes)] = text_bytes

        bytes_sent = client_socket.sendto(text_message, destination)
        print(f"Text: sent {bytes_sent} message bytes")
