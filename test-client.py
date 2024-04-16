import socket
import struct
import time

import zmq

# Connect to the UDP Events plugin at this address and port.
udp_destination = ("127.0.0.1", 12345)

# Bind this address and port to receive ACK messages from the UDP Events plugin.
udp_local_bind = ("127.0.0.1", 12344)

# Connect to the Network Events plugin, using ZMQ, at this address and port.
zmq_destination = ("127.0.0.1", 5556)

# Send both "real" and "soft" TTL events on this line number.
sync_line_number = 3

# Send other TTL events on this line number.
# These will be inserted into an existing Open Ephys data stream with high-precision timestamps.
extra_line_number = 0

# What time did this script start running?
start_time = time.time()


# Take local timestamps relative to when the script started running.
def up_time():
    return time.time() - start_time


# Connect a ZMQ socket to the Network Events plugin.
with zmq.Context() as zmq_context:
    with zmq_context.socket(zmq.REQ) as zmq_socket:
        zmq_socket.connect('tcp://%s:%d' % (zmq_destination))
        print("Connected ZMQ socket.")

        # Connect a plain UDP socket to the UDP Events plugin.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:

            udp_socket.bind(udp_local_bind)
            print("Bound UDP socket.")

            # Send 10 groups of messages over about 10 seconds.
            for index in range(10):

                time.sleep(0.75)

                # Start with a pair of TTL messages on the sync line, as if starting a trial.
                # These allow us to insert soft messages with local timestamps into an Open Ephys data stream with high precision timestamps.

                # First, a "real" TTL event sent to the Network Events plugin.
                # In real life this event should come from an acquisition card.
                # The Open Ephys UI presents 1-based line numbers 1-256.
                # In the code (ours and Open Ephys!) we use 0-based line numbers 0-255.
                # These fit naturally into a uint8.
                # However, the Open Ephys Network Events plugin seems to expect 1-based line numbers,
                # And subtracts 1 from what it receives in TTL messages ¯\_(ツ)_/¯
                zmq_sync_message = f"TTL Line={sync_line_number + 1} State={index % 2}"
                zmq_socket.send_string(zmq_sync_message)
                print("ZMQ reply: " + zmq_socket.recv_string())

                # Pack a UDP Events TTL message.
                udp_sync_message = bytearray(11)
                struct.pack_into('B', udp_sync_message, 0, 1)
                struct.pack_into('d', udp_sync_message, 1, up_time())
                struct.pack_into('B', udp_sync_message, 9, sync_line_number)
                struct.pack_into('B', udp_sync_message, 10, index % 2)

                bytes_sent = udp_socket.sendto(udp_sync_message, udp_destination)
                print("UDP reply: " + str(struct.unpack('d', udp_socket.recvfrom(256)[0])[0]))

                # Other messages, like trial data.

                time.sleep(0.25)

                # Pack a UDP Events text message.
                text_bytes = b"He who laughs last laughs ... never gonna laugh him again."
                udp_text_message = bytearray(11 + len(text_bytes))
                struct.pack_into('B', udp_text_message, 0, 2)
                struct.pack_into('d', udp_text_message, 1, up_time())
                struct.pack_into('H', udp_text_message, 9, socket.htons(len(text_bytes)))
                udp_text_message[11:11+len(text_bytes)] = text_bytes

                bytes_sent = udp_socket.sendto(udp_text_message, udp_destination)
                print("UDP reply: " + str(struct.unpack('d', udp_socket.recvfrom(256)[0])[0]))

                # Pack more UDP Events TTL message in quick succession.
                # These should get recorded with sub-"block" precision...
                # ...Yes!
                udp_extra_on = bytearray(11)
                struct.pack_into('B', udp_extra_on, 0, 1)
                struct.pack_into('d', udp_extra_on, 1, up_time())
                struct.pack_into('B', udp_extra_on, 9, extra_line_number)
                struct.pack_into('B', udp_extra_on, 10, 1)
                bytes_sent = udp_socket.sendto(udp_extra_on, udp_destination)

                udp_extra_off = bytearray(11)
                struct.pack_into('B', udp_extra_off, 0, 1)
                struct.pack_into('d', udp_extra_off, 1, up_time())
                struct.pack_into('B', udp_extra_off, 9, extra_line_number)
                struct.pack_into('B', udp_extra_off, 10, 0)
                bytes_sent = udp_socket.sendto(udp_extra_off, udp_destination)

                # Replies can stack up in the socket queue until we read them.
                print("UDP reply: " + str(struct.unpack('d', udp_socket.recvfrom(256)[0])[0]))
                print("UDP reply: " + str(struct.unpack('d', udp_socket.recvfrom(256)[0])[0]))
