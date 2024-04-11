import socket
import struct

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as client_socket:

    print("made socket")

    # pack a TTL message
    message = bytearray(11)
    struct.pack_into('B', message, 0, 1)
    struct.pack_into('d', message, 1, 420.69)
    struct.pack_into('B', message, 9, 7)
    struct.pack_into('B', message, 10, 1)
    bytes_sent = client_socket.sendto(message, ("127.0.0.1", 12345))

    print(f"sent {bytes_sent} message bytes")
