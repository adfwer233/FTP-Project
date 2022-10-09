from ftplib import FTP

ftp = FTP('127.0.0.1')
tmp = ftp.connect('127.0.0.1', 5000)