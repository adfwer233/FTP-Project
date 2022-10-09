import socket
 
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('0.0.0.0', 2333))
 
data = 'test data'

hello = client.recv(1024)
print(hello)

while True:
    client.send(data.encode("utf-8"))
    info = client.recv(1024)
    print(info)
    break