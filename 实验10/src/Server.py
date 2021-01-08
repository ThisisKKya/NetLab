#!/usr/bin/python
# Lab10
import json
import os
import socket
import struct
import threading

HOST = "127.0.0.1"
PORT = 10001
BUFF_SIZE = 1024

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

file_list = []
last_id = 0


def handle_single_connect(_connect, _address):
    print("[Thread: {}] Start handle address{}.".format(threading.get_ident(), _address))
    while True:
        try:
            if not _connect:
                print("[Thread: {}] Address{} connection break.".format(threading.get_ident(), _address))
                break

            command = _connect.recv(1)
            if command == b'1':
                receive_file(_connect, _address)
            if command == b'2':
                show_file(_connect, _address)
            if command == b'3':
                _file_id_byte = _connect.recv(4)  # 获取id
                _file_id = struct.unpack('i', _file_id_byte)[0]  # 解析为int
                send_file(_connect, _address, _file_id)
        except ConnectionResetError:
            print("[Thread: {}] Address{} connection break.".format(threading.get_ident(), _address))
            break


def receive_file(_connect, _address):
    global file_list, last_id

    head_struct = _connect.recv(4)  # 获取长度
    head_len = struct.unpack('i', head_struct)[0]  # 解析为int
    data = _connect.recv(head_len)  # 接受文件头 之后进行json反序列化

    head_dir = json.loads(data.decode('utf-8'))
    file_size = head_dir['filesize_bytes']
    filename = head_dir['filename']

    recv_len = 0
    recv_message = b''
    file_list.append(
        {
            "id": str(last_id),
            "filename": filename,
            "size": file_size,
            "address": _address[0] + ":" + str(address[1])
        }
    )
    f = open("./pan/" + str(last_id) + filename, 'wb')
    last_id = last_id + 1
    while recv_len < file_size:  # 分1024字节进行接受
        percent = recv_len / file_size
        if file_size - recv_len > BUFF_SIZE:

            recv_message = _connect.recv(BUFF_SIZE)
            f.write(recv_message)
            recv_len += len(recv_message)
        else:
            recv_message = _connect.recv(file_size - recv_len)
            recv_len += len(recv_message)
            f.write(recv_message)

    print("[Thread: {}] Address{} upload file: {}, size: {}.".format(threading.get_ident(), _address, filename,
                                                                     file_size))


def show_file(_connect, _address):
    # 将文件目录序列化之后发送 先是4字节的目录长度 然后发送目录
    file_info = json.dumps(file_list)
    file_info_len = struct.pack('i', len(file_info))
    _connect.send(file_info_len)
    _connect.send(file_info.encode('utf-8'))
    print("[Thread: {}] Address{} pull file list.".format(threading.get_ident(), _address))


def send_file(_connect, _address, file_id):
    file_name = ""
    file_size = 0
    for file in file_list:
        if str(file_id) == str(file["id"]):
            file_name = file["filename"]
            file_size = file["size"]
            break
    if file_name == "":
        _connect.send(b'1')  # 没找到
        print("[Thread: {}] Address{} try to download with id:{} But file not found.".format(threading.get_ident(),
                                                                                             _address, file_id))
    else:
        _connect.send(b'0')  # 找到惹
        file_path = "./pan/" + str(file_id) + file_name
        filesize_bytes = os.path.getsize(file_path)
        dirc = {
            'filename': file_name,
            'filesize_bytes': filesize_bytes,
        }
        head_info = json.dumps(dirc)  # 文件头 json序列化
        head_info_len = struct.pack('i', len(head_info))  # 将文件头长度作为二进制格式
        _connect.send(head_info_len)
        _connect.send(head_info.encode('utf-8'))
        print("[Thread: {}] Address{} download file: {}, size: {}.".format(threading.get_ident(), _address, file_name,
                                                                           file_size))
        with open(file_path, 'rb') as f:
            data = f.read()
            _connect.sendall(data)


if __name__ == '__main__':
    print("Lab10 Socket -Server-")
    s.bind((HOST, PORT))
    s.listen(5)
    while True:
        connect, address = s.accept()
        new_thread = threading.Thread(target=handle_single_connect, args=(connect, address,))  # 用独立的线程管理每一个conn
        new_thread.setDaemon(True)  # 设置守护进程
        new_thread.start()
