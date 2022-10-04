import socket

tcp_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

address = ('', 2333)

tcp_server_socket.bind(address)

tcp_server_socket.listen(128)

while True:
    client_socket, clientAddr = tcp_server_socket.accept()
    while True:
        recv_data = client_socket.recv(1024)
        print('recv:', recv_data.decode('utf-8'))
        
    client_socket.close()

tcp_server_socket.close()