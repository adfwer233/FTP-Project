import enum
from http import client, server
import socket
import threading
import random
import re
import time

class FTPClient():

    def __init__(self, ip: str, port: int) -> None:
        self.ip = ip
        self.port = port

        self.control_connection = None
        self.data_connection_listen = None
        self.data_connection_conn = None

        self.login_status = False

    def recv_msg(self, socket_conn):
        buffer: str = ''
        while True:
            tmp = socket_conn.recv(1024).decode()
            if len(tmp) == 0:
                continue
            buffer += tmp
            if '\n' in buffer:
                break
        return buffer

    def data_connection_process(self):
        self.data_connection_conn, conn_addr = self.data_connection_listen.accept()
        print("data port connection accept")
        recv_data = self.recv_msg(self.data_connection_conn)
        if len(recv_data) > 0:
            print("data_conn > ",recv_data)

    def pasv_data_connection(self):
        server_msg = self.recv_msg(self.data_connection_conn)
        print("pasv data conn > ", server_msg)

    def cmd_sender(self, cmd: str):
        cmd = cmd + '\r\n'

        command_verb = cmd.split(' ')[0]

        if command_verb.lower() == 'port':
            self.cmd_port(cmd)
        else:
            self.control_connection.send(cmd.encode('utf-8'))

    def server_msg_handler(self):
        print("waiting")
        server_response = self.recv_msg(self.control_connection)
        status_code = server_response.split(' ')[0]
        if status_code == '227':
            pattern = re.compile('\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\)')
            match_res = re.search(pattern, server_response.strip())
            h1, h2, h3, h4, p1, p2 = match_res.groups()
            self.data_connection_conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.data_connection_conn.connect((f'{h1}.{h2}.{h3}.{h4}', int(p1) * 256 + int(p2)))

            data_connection_listening_thread = threading.Thread(target=self.pasv_data_connection, daemon=True)
            data_connection_listening_thread.start()
        print(server_response.strip())

    def cmd_port(self, cmd: str):
        cmd = cmd.strip()
        verb, params = cmd.split(' ')
        try:
            h1, h2, h3, h4, p1, p2 = params.split(',')
            print(f'{h1}.{h2}.{h3}.{h4}', int(p1) * 256 + int(p2))
            self.control_connection.send((cmd + '\r\n').encode('utf-8'))

            self.data_connection_listen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.data_connection_listen.bind((f'{h1}.{h2}.{h3}.{h4}', int(p1) * 256 + int(p2)))
            self.data_connection_listen.listen(128)

            data_connection_listening_thread = threading.Thread(target=self.data_connection_process, daemon=True)
            data_connection_listening_thread.start()
        except:
            print('Client: Invalid params of PORT')
        

    def connect_server(self):
        self.control_connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.control_connection.connect((self.ip, self.port))

        hello_msg = self.control_connection.recv(1024).decode()
        print(hello_msg)

    def message_loop(self):
        while(1):
            input_cmd = input()
            self.cmd_sender(input_cmd)
            self.server_msg_handler()



if __name__ == '__main__':
    client = FTPClient('127.0.0.1', 2333)

    client.connect_server()

    client.message_loop()

