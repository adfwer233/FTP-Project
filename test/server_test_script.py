from ftplib import FTP
ftp = FTP()
print(ftp.connect('127.0.0.1', 2333))
ftp.login()