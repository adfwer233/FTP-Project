import re

server_response = '227 Entering passive mode (127,0,0,1,135,81).'


match_res = re.search(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)', server_response)
h1, h2, h3, h4, p1, p2 = match_res.groups()
print(h1, h2, h3, h4, p1 ,p2)