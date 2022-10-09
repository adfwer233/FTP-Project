from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

authorizer = DummyAuthorizer()
authorizer.add_user('adfwer', 123, './rootdir')
authorizer.add_anonymous('./rootdir')

handler = FTPHandler
handler.authorizer = authorizer

server = FTPServer(('127.0.0.1', 5000), handler)
server.serve_forever()