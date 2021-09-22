from urllib.request import urlopen
import re

size = 1024


def get_host_ip():
    return urlopen('http://ip.42.pl/raw').read().decode()


class Client:
    def __init__(self):
        self.control_sock = None
        self.data_sock = None
        self.buffer_size = 1024
        self.is_passive = 1
        self.currentPath = '/'
        self.rootpath = '/'
        self.isTransportFile = False

    def create_pasv_mode(self, reply):
        pattern = r"\d{1,3},\d{1,3},\d{1,3},\d{1,3},\d{1,3},\d{1,3}"
        array = re.search(pattern, reply).group().split(',')
        data_ip = '.'.join(array[:-2])
        data_port = int(array[-2]) * 256 + int(array[-1])
        return data_ip, data_port

    def get_control_sock_content(self):
        content = ''
        tmp = ''
        while True:
            data = self.control_sock.recv(size).decode()
            content += data
            tmp += data
            if tmp.endswith('\r\n'):
                lines = tmp.split('\r\n')[:-1]
                if lines[-1][3] == ' ':
                    break
                else:
                    tmp = ''
        return content

    def get_data_sock_content(self):
        content = b''
        data = self.data_sock.recv(size)
        while data:
            content += data
            data = self.data_sock.recv(size)
        return content



