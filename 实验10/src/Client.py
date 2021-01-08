#!/usr/bin/python
# Lab10
import json
import os
import socket
import struct
import threading

HELP_INFO = "Usage:\n upload [FILE_PATH]\t upload file to server.\n download [FILE_ID]\t download file from server\n " \
            "info\t get file list from server\n quit\t Say bye bye.\n "

HOST = "127.0.0.1"
PORT = 10001
BUFF_SIZE = 1024

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


def send_file(file_path: str):
    s.send(b'1')  # 表明是upload行为
    file_name = file_path.split("\\")[-1].split("/")[-1]
    filesize_bytes = os.path.getsize(file_path)
    dirc = {
        'filename': file_name,
        'filesize_bytes': filesize_bytes,
    }
    head_info = json.dumps(dirc)  # 文件头 json序列化
    head_info_len = struct.pack('i', len(head_info))  # 将文件头长度作为二进制格式
    s.send(head_info_len)
    s.send(head_info.encode('utf-8'))
    print("[INFO] Sending file {} size: {}.".format(file_name, filesize_bytes))
    with open(file_path, 'rb') as f:
        data = f.read()
        s.sendall(data)
    print("[INFO] Sending finished.")


def show_file():
    s.send(b'2')
    file_struct = s.recv(4)  # 获取列表长度
    file_list_length = struct.unpack('i', file_struct)[0]  # 解析为int
    data = s.recv(file_list_length)  # 接受文件头 之后进行json反序列化
    file_list_info = json.loads(data.decode('utf-8'))
    print("\t\t\t\t\t\t----File Info----")
    print("{:^5}{:^30}{:^8}{:^35}".format("ID", "FILE_NAME", "FILE_SIZE", "UPLOADER_ADDRESS", ))
    for file in file_list_info:
        print("{:^5}{:^30}{:^8}{:^35}".
              format(file["id"], file["filename"], file["size"], file["address"], ))


def download_file(file_id: int):
    s.send(b'3')
    file_id_sent = struct.pack('i', file_id)
    s.send(file_id_sent)
    flag = s.recv(1)
    if flag == b'0':
        head_struct = s.recv(4)  # 获取长度
        head_len = struct.unpack('i', head_struct)[0]  # 解析为int
        data = s.recv(head_len)  # 接受文件头 之后进行json反序列化
        head_dir = json.loads(data.decode('utf-8'))
        file_size = head_dir['filesize_bytes']
        filename = head_dir['filename']
        recv_len = 0
        recv_message = b''
        print("[INFO]Start downloading file {}, size: {}.".format(filename, file_size))
        f = open("./" + filename, 'wb')
        while recv_len < file_size:  # 分1024字节进行接受
            percent = recv_len / file_size
            if file_size - recv_len > BUFF_SIZE:

                recv_message = s.recv(BUFF_SIZE)
                f.write(recv_message)
                recv_len += len(recv_message)
            else:
                recv_message = s.recv(file_size - recv_len)
                recv_len += len(recv_message)
                f.write(recv_message)
    else:
        print("[ERROR]Can't find this file.")


if __name__ == '__main__':
    print("Lab10 Socket -Client-")
    try:
        s.connect((HOST, PORT))
    except socket.error:
        print("Connect Error.")
        input()
        quit(-1)
    print("Connect Success.")
    print("Type 'help' to get usage")
    while True:
        try:
            commands = input(">").strip()
            if commands == "help":
                print("%s" % HELP_INFO)
            elif commands[:6] == "upload":
                path = ""
                try:
                    path = commands.split(" ")[1]
                except IndexError:
                    print("[ERROR]Can't Find Path")
                    continue
                if not os.path.isfile(path):
                    print("[ERROR]Can't Find Path")
                    continue
                # threading.Thread(target=send_file, args=(path,))
                send_file(path)
            elif commands == "ls":
                show_file()
            elif commands[:8] == "download":
                file_id = ""
                try:
                    file_id = commands.split(" ")[1]
                except IndexError:
                    print("[ERROR]Please input ID")
                    continue
                # threading.Thread(target=send_file, args=(path,))
                download_file(int(file_id))
            elif commands == "quit" or commands == "q":
                print("Bye~")
                s.close()
                exit(0)
        except ConnectionResetError:
            print("[ERROR] Connect lost. Maybe server has been closed.")
            s.close()
            exit(-1)
        except Exception:
            print("[ERROR] Unknown Error.")
            s.close()
            exit(-2)
