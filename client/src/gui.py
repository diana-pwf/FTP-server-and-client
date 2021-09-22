import sys
import socket
import os
from functools import partial
from PyQt5 import QtWidgets
from PyQt5.QtWidgets import QMenu, QTableWidgetItem, QHeaderView, QAbstractItemView, \
    QFileDialog, QLineEdit, QInputDialog, QMessageBox, QPushButton, QProgressBar
from client import Ui_MainWindow
from lib import *
from PyQt5.QtCore import QStringListModel, QThread, pyqtSignal
import time

size = 1024

class MyPyQT_Form(QtWidgets.QMainWindow,Ui_MainWindow):
    def __init__(self):
        super(MyPyQT_Form,self).__init__()
        self.setupUi(self)
        self.user = Client()
        self.listModel = QStringListModel()
        self.responseItems = []
        self.uploadItems = [] # 文件路径，文件名，文件大小，当前进度（int）
        self.downloadItems = [] # 文件名，文件大小，当前进度
        self.lineEdit_password.setEchoMode(QLineEdit.Password)
        self.tableWidget_ServerFile.setColumnCount(4)
        self.tableWidget_ServerFile.setHorizontalHeaderLabels(['Name', 'Type', 'Modify time', 'Size'])
        self.tableWidget_ServerFile.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_ServerFile.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.tableWidget_ServerFile.setContextMenuPolicy(3)  ######允许右键产生子菜单
        self.tableWidget_ServerFile.customContextMenuRequested.connect(self.generateMenu)  ####右键菜单
        self.tableWidget_uploadQueue.setColumnCount(4)
        self.tableWidget_uploadQueue.setHorizontalHeaderLabels(['Name', 'Pause', 'Continue', 'Progress'])
        self.tableWidget_uploadQueue.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_downloadQueue.setColumnCount(4)
        self.tableWidget_downloadQueue.setHorizontalHeaderLabels(['Name', 'Pause', 'Continue', 'Progress'])
        self.tableWidget_downloadQueue.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)

    # 创建右键菜单
    def generateMenu(self, pos):
        row_num = -1
        for i in self.tableWidget_ServerFile.selectionModel().selection().indexes():
            row_num = i.row()
        if row_num < self.tableWidget_ServerFile.rowCount() and row_num != -1:
            menu = QMenu()
            action_open = menu.addAction("open")
            action_download = menu.addAction("download")
            action_delete = menu.addAction("delete")
            action_rename = menu.addAction("rename")
            if self.tableWidget_ServerFile.item(row_num, 1).text() != 'Dir':
                action_open.setEnabled(False)
            if self.tableWidget_ServerFile.item(row_num, 1).text() != 'File':
                action_download.setEnabled(False)
            action = menu.exec_(self.tableWidget_ServerFile.mapToGlobal(pos))
            if action == action_open:
                self.openDirectory(self.tableWidget_ServerFile.item(row_num, 0).text())
            elif action == action_download:
                self.downloadFile(self.tableWidget_ServerFile.item(row_num, 0).text(), self.tableWidget_ServerFile.item(row_num, 3).text())
            elif action == action_delete:
                self.deleteDirectory(self.tableWidget_ServerFile.item(row_num, 0).text())
            elif action == action_rename:
                self.renameFile(self.tableWidget_ServerFile.item(row_num, 0).text())
            else:
                pass

    def updateResponseList(self, str):
        self.responseItems.append(str)
        self.listModel.setStringList(self.responseItems)
        self.listView.setModel(self.listModel)
        self.listView.scrollToBottom()

    def sendDataOrder(self, order):
        if self.user.is_passive:
            self.enterPasvMode()
        else:
            self.enterPortMode()
        self.user.control_sock.sendall((order + '\r\n').encode())
        if not self.user.is_passive:
            sock, addr = self.user.data_sock.accept()
            self.user.data_sock = sock
        self.updateResponseList(self.user.get_control_sock_content())

    def getServerFiles(self):
        self.sendDataOrder('LIST ')
        # 开始处理文件
        ans = self.user.get_data_sock_content().decode()
        filelist = ans.split("\n")
        fileInfoList = []
        for i in range(len(filelist)):
            infoList = filelist[i].split()
            if len(infoList) == 9:
                fileInfo = []                   # Name, TYPE， Size， Name
                fileInfo.append(infoList[-1])
                if infoList[0][0] == '-':
                    fileInfo.append('File')
                elif infoList[0][0] == 'd':
                    fileInfo.append('Dir')
                else:
                    fileInfo.append('other')
                fileInfo.append(infoList[-4] + ' ' + infoList[-3] + ' ' + infoList[-2])
                fileInfo.append(infoList[-5])
                fileInfoList.append(fileInfo)
        self.tableWidget_ServerFile.setRowCount(len(fileInfoList))
        for i in range(len(fileInfoList)):
            for j in range(4):
                item = QTableWidgetItem(fileInfoList[i][j])
                self.tableWidget_ServerFile.setItem(i, j, item)
        self.user.data_sock.close()
        reply_1 = self.user.get_control_sock_content()
        self.updateResponseList(reply_1)

    def uploadFile(self):
        if self.user.control_sock is None:
            pass
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        try:
            filepath = QFileDialog.getOpenFileName()[0]
            filename = filepath.split('/')[-1]
        except:
            return
        self.sendDataOrder('STOR ' + filename)
        filesize = os.path.getsize(filepath)
        find = -1
        for i in range(len(self.uploadItems)):
            if self.uploadItems[i][0] == filepath:
                self.uploadItems[i] = [filepath, filename, filesize, 0, 1]
                find = i
                break
        if find == -1:
            self.uploadItems.append([filepath, filename, filesize, 0, 1])
            find = len(self.uploadItems) - 1
        self.refreshUploadList()
        self.user.isTransportFile = 1
        self.uploadThread = UploadThread(filepath, self.user.data_sock, find, 0)
        self.uploadThread.upload_signal.connect(self.setUploadProgress)
        self.uploadThread.pause_signal.connect(self.pauseUpload)
        self.uploadThread.start()

    def setUploadProgress(self, progress, index):
        self.uploadItems[index][3] = progress
        self.tableWidget_uploadQueue.setRowCount(len(self.uploadItems))
        progressbar = QProgressBar(self)
        progressbar.setValue(int(round((100 * progress) / self.uploadItems[index][2])))
        self.tableWidget_uploadQueue.setCellWidget(index, 3, progressbar)

    def setDownloadProgress(self, progress, index):
        self.downloadItems[index][2] = progress
        self.tableWidget_downloadQueue.setRowCount(len(self.downloadItems))
        progressbar = QProgressBar(self)
        progressbar.setValue(int(round((100 * progress) / self.downloadItems[index][1])))
        self.tableWidget_downloadQueue.setCellWidget(index, 3, progressbar)

    def refreshUploadList(self):
        self.tableWidget_uploadQueue.setRowCount(len(self.uploadItems))
        for i in range(len(self.uploadItems)):
            item_1 = QTableWidgetItem(self.uploadItems[i][1])
            self.tableWidget_uploadQueue.setItem(i, 0, item_1)
            pauseButton = QPushButton('pause')
            pauseButton.clicked.connect(partial(self.pauseUpload, i))
            self.tableWidget_uploadQueue.setCellWidget(i, 1, pauseButton)
            continueButton = QPushButton('continue')
            continueButton.clicked.connect(partial(self.continueUpload, i))
            self.tableWidget_uploadQueue.setCellWidget(i, 2, continueButton)
            if self.uploadItems[i][3] == self.uploadItems[i][2]:
                pauseButton.setEnabled(False)
                continueButton.setEnabled(False)
            elif self.uploadItems[i][-1]:
                continueButton.setEnabled(False)
            else:
                pauseButton.setEnabled(False)
            progressbar = QProgressBar(self)
            progressbar.setValue(int(round((100 * self.uploadItems[i][3]) / self.uploadItems[i][2])))
            self.tableWidget_uploadQueue.setCellWidget(i, 3, progressbar)

    def refreshDownloadList(self):
        self.tableWidget_downloadQueue.setRowCount(len(self.downloadItems))
        for i in range(len(self.downloadItems)):
            item_1 = QTableWidgetItem(self.downloadItems[i][0])
            self.tableWidget_downloadQueue.setItem(i, 0, item_1)
            pauseButton = QPushButton('pause')
            pauseButton.clicked.connect(partial(self.pauseDownload, i))
            self.tableWidget_downloadQueue.setCellWidget(i, 1, pauseButton)
            continueButton = QPushButton('continue')
            continueButton.clicked.connect(partial(self.continueDownload, i))
            self.tableWidget_downloadQueue.setCellWidget(i, 2, continueButton)
            if self.downloadItems[i][1] == self.downloadItems[i][2]:
                pauseButton.setEnabled(False)
                continueButton.setEnabled(False)
            elif self.downloadItems[i][-1]:
                continueButton.setEnabled(False)
            else:
                pauseButton.setEnabled(False)
            progressbar = QProgressBar(self)
            progressbar.setValue(int(round((100 * self.downloadItems[i][2]) / self.downloadItems[i][1])))
            self.tableWidget_downloadQueue.setCellWidget(i, 3, progressbar)

    def pauseUpload(self, index):
        self.user.data_sock.close()
        self.uploadItems[index][-1] = 0
        self.refreshUploadList()
        self.user.isTransportFile = 0
        self.updateResponseList(self.user.get_control_sock_content())
        self.getServerFiles()

    def pauseDownload(self, index):
        self.user.data_sock.close()
        self.downloadItems[index][-1] = 0
        self.refreshDownloadList()
        self.user.isTransportFile = 0
        self.updateResponseList(self.user.get_control_sock_content())
        self.getServerFiles()

    def continueUpload(self, index):
        # 确保在同一个目录下进行断点续传
        filepath = self.uploadItems[index][0]
        filename = self.uploadItems[index][1]
        self.sendDataOrder('APPE ' + filename)
        progress = self.uploadItems[index][3]
        # 只用改变下载状况
        self.uploadItems[index][-1] = 1
        self.refreshUploadList()
        self.user.isTransportFile = 1
        self.uploadThread = UploadThread(filepath, self.user.data_sock, index, progress)
        self.uploadThread.upload_signal.connect(self.setUploadProgress)
        self.uploadThread.pause_signal.connect(self.pauseUpload)
        self.uploadThread.start()

    def continueDownload(self, index):
        # 确保在同一个目录下进行断点续传
        filename = self.downloadItems[index][0]
        filestart = self.downloadItems[index][2]
        self.user.control_sock.sendall(('REST ' + str(filestart) + '\r\n').encode())
        self.updateResponseList(self.user.get_control_sock_content())
        self.sendDataOrder('RETR ' + filename)
        # 只用改变下载状况
        self.downloadItems[index][-1] = 1
        self.refreshDownloadList()
        self.user.isTransportFile = 1
        self.downloadThread = DownloadThread(filename, self.user.data_sock, index, filestart)
        self.downloadThread.download_signal.connect(self.setDownloadProgress)
        self.downloadThread.pause_signal.connect(self.pauseDownload)
        self.downloadThread.start()

    def downloadFile(self,filename, filesize):
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        self.sendDataOrder('RETR ' + filename)
        filesize = int(filesize)
        find = -1
        for i in range(len(self.downloadItems)):
            if self.downloadItems[i][0] == filename:
                self.downloadItems[i] = [filename, filesize, 0, 1]
                find = i
                break
        if find == -1:
            self.downloadItems.append([filename, filesize, 0, 1])
            find = len(self.downloadItems) - 1
        self.refreshDownloadList()
        self.user.isTransportFile = 1
        self.downloadThread = DownloadThread(filename, self.user.data_sock, find, 0)
        self.downloadThread.download_signal.connect(self.setDownloadProgress)
        self.downloadThread.pause_signal.connect(self.pauseDownload)
        self.downloadThread.start()

    def openDirectory(self, str=''):
        if self.user.control_sock is None:
            pass
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        if not str:
            if self.user.currentPath == self.user.rootpath:
                print("无法再返回上一级")
                return
            else:
                index = self.user.currentPath.rfind('/')
                if not index:
                    str = '/'
                else:
                    str = self.user.currentPath[:index]
        self.user.control_sock.sendall(('CWD ' + str + '\r\n').encode())
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        self.getServerPath()
        self.getServerFiles()

    def makeDirectory(self):
        if self.user.control_sock is None:
            pass
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        dirname, ok = QInputDialog.getText(self, 'Input', "please input the new dir name:")
        if dirname != '' and dirname.find(' ') == -1:
            self.user.control_sock.sendall(('MKD ' + dirname + '\r\n').encode())
        else:
            QMessageBox.warning(self, "warning", "the name is invalid")
            return
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        self.getServerFiles()

    def deleteDirectory(self, filename):
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        self.user.control_sock.sendall(('RMD ' + filename + '\r\n').encode(encoding='utf-8'))
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        self.getServerFiles()

    def renameFile(self, oldfilename):
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        self.user.control_sock.sendall(('RNFR ' + oldfilename + '\r\n').encode(encoding='utf-8'))
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        newfilename, ok = QInputDialog.getText(self, 'Input', "please input the new file name:")
        if newfilename != '' and newfilename.find(' ') == -1:
            self.user.control_sock.sendall(('RNTO ' + newfilename + '\r\n').encode())
        else:
            QMessageBox.warning(self, "warning", "the name is invalid")
            return
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        self.getServerFiles()

    def getServerPath(self):
        self.user.control_sock.sendall(('PWD ' + '\r\n').encode(encoding='utf-8'))
        reply = self.user.get_control_sock_content()
        index_l = reply.find('\"')
        index_r = reply.rfind('\"')

        self.user.currentPath = reply[index_l + 1:index_r]
        self.label_path.setText(self.user.currentPath)

    def enterPasvMode(self):
        self.user.data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.user.control_sock.sendall(('PASV' + '\r\n').encode(encoding='utf-8'))
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        data_ip, data_port = self.user.create_pasv_mode(reply)
        self.user.data_sock.connect((data_ip, data_port))

    def enterPortMode(self):
        self.user.data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.user.data_sock.bind(('', 0))
        ip, my_port = self.user.data_sock.getsockname()
        self.user.data_sock.listen(1)
        my_port = int(my_port)
        num1 = int(my_port / 256)
        num2 = my_port - num1 * 256
        self.user.control_sock.sendall(
            ("PORT " + get_host_ip().replace('.', ',') + ',%d,%d' % (num1, num2) + '\r\n').encode())
        reply_1 = self.user.get_control_sock_content()
        self.updateResponseList(reply_1)

    def connect_server(self):
        ip = self.lineEdit_IP.text()
        try:
            port = int(self.lineEdit_port.text())
        except:
            print("端口必须为正整数\n")
            return
        self.user.control_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.user.control_sock.connect((ip, port))  # 建立TCP连接
            reply = self.user.get_control_sock_content()
            self.updateResponseList(reply)
        except:
            QMessageBox.warning(self, "warning", "Connection error.")
            return
        if reply.startswith('2'):
            username = self.lineEdit_username.text()
            password = self.lineEdit_password.text()
            self.pushButton_disconnect.setEnabled(True)
            self.pushButton_connect.setEnabled(False)
            self.user.control_sock.sendall(('USER ' + username + '\r\n').encode(encoding='utf-8'))
            reply = self.user.get_control_sock_content()
            self.updateResponseList(reply)
            if reply.startswith('331'):
                self.user.control_sock.sendall(('PASS ' + password + '\r\n').encode(encoding='utf-8'))
                reply = self.user.get_control_sock_content()
                self.updateResponseList(reply)
                self.type_query()
                self.getServerPath()
                self.user.rootpath = self.user.currentPath
                self.label_path.setText(self.user.currentPath)
                self.getServerFiles()

    def disconnect_server(self):
        self.user.control_sock.sendall(('QUIT ' + '\r\n').encode(encoding='utf-8'))
        reply = self.user.get_control_sock_content()
        self.updateResponseList(reply)
        self.user.control_sock.close()
        self.pushButton_connect.setEnabled(True)
        self.pushButton_disconnect.setEnabled(False)
        self.updateResponseList('')
        self.uploadItems = []
        self.downloadItems = []
        self.refreshUploadList()
        self.refreshDownloadList()
        self.tableWidget_ServerFile.setRowCount(0)

    def syst_query(self):
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        if self.user.control_sock is None:
            pass
        else:
            self.user.control_sock.sendall(('SYST ' + '\r\n').encode(encoding='utf-8'))
            reply = self.user.get_control_sock_content()
            self.updateResponseList(reply)

    def type_query(self):
        if self.user.isTransportFile:
            QMessageBox.warning(self, "warning", "File is Transporting. Cannot do other things.")
            return
        if self.user.control_sock is None:
            pass
        else:
            self.user.control_sock.sendall(('TYPE I' + '\r\n').encode(encoding='utf-8'))
            reply = self.user.get_control_sock_content()
            self.updateResponseList(reply)

    def mode_change(self):
        if self.radioButton_pasv.isChecked():
            self.user.is_passive = 1
        else:
            self.user.is_passive = 0


class UploadThread(QThread):
    upload_signal = pyqtSignal(int, int)
    pause_signal = pyqtSignal(int)

    def __init__(self, filepath, data_sock, index, start_point):
        super(UploadThread, self).__init__()
        self.filepath = filepath
        self.data_sock = data_sock
        self.index = index
        self.start_point = start_point

    def run(self):
        with open(self.filepath, 'rb') as f:
            f.seek(self.start_point)
            content = f.read()
        f.close()
        index_l = 0
        while index_l < len(content):
            index_r = min(index_l + size, len(content))
            try:
                self.data_sock.sendall(content[index_l:index_r])
            except:
                self.exit(0)
                return
            index_l = index_r
            self.upload_signal.emit(index_l + self.start_point, self.index)
            time.sleep(0.001)
            if index_r == len(content):
                break
        self.pause_signal.emit(self.index)
        self.exit(0)


class DownloadThread(QThread):
    download_signal = pyqtSignal(int, int)
    pause_signal = pyqtSignal(int)

    def __init__(self, filename, data_sock, index, start_point):
        super(DownloadThread, self).__init__()
        self.filename = filename
        self.data_sock = data_sock
        self.index = index
        self.start_point = start_point

    def run(self):
        filecontent = b''
        try:
            data = self.data_sock.recv(size)
        except:
            self.exit(0)
            return
        while data:
            filecontent += data
            self.download_signal.emit(len(filecontent) + self.start_point, self.index)
            time.sleep(0.001)
            try:
                data = self.data_sock.recv(size)
            except:
                self.exit(0)
                return
        with open(self.filename, 'wb') as f:
            f.write(filecontent)
        f.close()
        self.pause_signal.emit(self.index)
        self.exit(0)



if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    my_pyqt_form = MyPyQT_Form()
    my_pyqt_form.show()
    sys.exit(app.exec_())