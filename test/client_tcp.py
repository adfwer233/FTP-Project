import socket
 
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('0.0.0.0', 2333))
 
 
while True:
    data = 'test data'
 
    client.send(data.encode("utf-8"))
    info = client.recv(1024)
