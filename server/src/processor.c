#include "processor.h"

extern char root_path[ROOT_PATH_SIZE];
extern char host_ip[16];

char* numberToString(int num, char* str)
{
    sprintf(str, "%d", num);
    return str;
}

char* getHostIp()
{
    int listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, "8.8.8.8", &(addr.sin_addr.s_addr));
    connect(listenfd, (struct sockaddr *)&(addr), sizeof(addr));

    socklen_t n = sizeof addr;
    getsockname(listenfd, (struct sockaddr *)&addr, &n);
    inet_ntop(AF_INET, &(addr.sin_addr), host_ip, INET_ADDRSTRLEN);
    return host_ip;
}

// 向连接中写入sentence的全部内容
void writeBuffer(int connfd, char* sentence)
{
    int p = 0;
    int readLen = strlen(sentence);
    while (p < readLen) {
        int n = write(connfd, sentence + p, readLen - p);
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return;
        } else {
            p += n;
        }
    }
}

// 读取连接中的全部内容到sentence
void readBuffer(int connfd, char* sentence)
{
    int p = 0;
    while (1) {
        int n = read(connfd, sentence + p, SIZE - 1 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            return;
        }
        else if (n == 0) {
            break;
        }
        else {
            p += n;
            if (sentence[p - 1] == '\n') {
                break;
            }
        }
    }
    //socket接收到的字符串并不会添加'\0'
    sentence[p] = '\0';
}

void prepareDataConnection(struct ClientPthread* pthread_data)
{
    if (pthread_data->is_passive)
    {
        //等待client的连接 -- 阻塞函数
        if ((pthread_data->data_connfd[1] = accept(pthread_data->data_connfd[0], NULL, NULL)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
        }
    }
    else
    {
        //连接上目标主机（将socket和目标主机连接）-- 阻塞函数
        if (connect(pthread_data->data_connfd[1], (struct sockaddr*)(pthread_data->data_addr), sizeof(*(pthread_data->data_addr))) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            //return 1;
        }
    }
}

int processor(struct ClientPthread* pthread_data, char* order, char* argument)
{
    if(pthread_data->userStatus == no_log)
    {
        if (!strcmp(order, "USER"))
        {
            userHandler(pthread_data, argument);
        }
        else{
            writeBuffer(pthread_data->connfd, "530 You aren't logged in.\r\n");
        }
    }
    else if(pthread_data->userStatus == has_name)
        {
            if(!strcmp(order, "PASS"))
            {
                passHandler(pthread_data, argument);
            }
            else
            {
                writeBuffer(pthread_data->connfd, "530 You aren't logged in.\r\n");
            }
        }
    else
    {
        if (!strcmp(order, "USER"))
        {
            writeBuffer(pthread_data->connfd, "331 Already logged in.\r\n");
        }
        else if(!strcmp(order, "PASS"))
        {
            writeBuffer(pthread_data->connfd, "230 Already logged in.\r\n");
        }
        else if(!strcmp(order, "PASV"))
        {
            pasvHandler(pthread_data);
        }
        else if(!strcmp(order, "PORT"))
        {
            portHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "RETR"))
        {
            retrHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "STOR"))
        {
            storHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "LIST"))
        {
            listHandler(pthread_data);
        }
        else if(!strcmp(order, "RNFR"))
        {
            rnfrHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "RNTO"))
        {
            rntoHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "MKD"))
        {
            mkdHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "RMD"))
        {
            rmdHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "PWD"))
        {
            pwdHandler(pthread_data);
        }
        else if(!strcmp(order, "CWD"))
        {
            cwdHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "SYST"))
        {
            systHandler(pthread_data->connfd);
        }
        else if(!strcmp(order, "TYPE"))
        {
            typeHandler(pthread_data->connfd, argument);
        }
        else if(!strcmp(order, "QUIT"))
        {
            quitHandler(pthread_data->connfd);
            return -1;
        }
        else if(!strcmp(order, "ABOR"))
        {
            aborHandler(pthread_data->connfd);
            return -1;
        }
        else if(!strcmp(order, "APPE"))
        {
            appeHandler(pthread_data, argument);
        }
        else if(!strcmp(order, "REST"))
        {
            restHandler(pthread_data, argument);
        }
        else
        {
            writeBuffer(pthread_data->connfd, "500 Unknown command.\r\n");
        }
    }
    return 0;
}

void userHandler(struct ClientPthread* pthread_data, char* argument)
{
    if(!strcmp(argument, "anonymous"))
    {
        writeBuffer(pthread_data->connfd, "331 Guest login ok, send your complete e-mail address as password.\r\n");
        pthread_data->userStatus = has_name;
    }
    else
    {
        writeBuffer(pthread_data->connfd, "530 This FTP server is anonymous only.\r\n");
    }
}

void passHandler(struct ClientPthread* pthread_data, char* argument)
{
    writeBuffer(pthread_data->connfd, "230 Guest login ok, access restrictions apply.\r\n");
    pthread_data->userStatus = logged;
}

void pasvHandler(struct ClientPthread* pthread_data)
{
    // 清除之前已有的数据连接
    if(pthread_data->data_connfd[0] != -1)
    {
        if(pthread_data->data_connfd[1] != -1)
        {
            close(pthread_data->data_connfd[1]);
            pthread_data->data_connfd[1] = -1;
        }
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[0] = -1;
    }

    //创建socket
    if ((pthread_data->data_connfd[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        writeBuffer(pthread_data->connfd, "425 Fail to enter pasv mode.\r\n");
        return;
    }

    //设置本机的ip和port
    strcpy(host_ip, getHostIp());
    srand(time(0));
    int data_port = (rand() % (65535 - 20000 + 1)) + 20000;
    int h5 = data_port / 256;
    int h6 = data_port % 256;
    memset(pthread_data->data_addr, 0, sizeof(*(pthread_data->data_addr)));
    pthread_data->data_addr->sin_family = AF_INET;
    pthread_data->data_addr->sin_port = htons(data_port);
    pthread_data->data_addr->sin_addr.s_addr = htonl(INADDR_ANY);	//监听"0.0.0.0"

    //将本机的ip和port与socket绑定
    if (bind(pthread_data->data_connfd[0], (struct sockaddr*)(pthread_data->data_addr), sizeof(*(pthread_data->data_addr))) == -1) {
        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        writeBuffer(pthread_data->connfd, "425 Fail to enter pasv mode.\r\n");
        return;
    }

    //开始监听socket
    if (listen(pthread_data->data_connfd[0], 10) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        writeBuffer(pthread_data->connfd, "425 Fail to enter pasv mode.\r\n");
        return;
    }

    for(int i = 0; i < strlen(host_ip); i++)
    {
        if (host_ip[i] == '.')
        {
            host_ip[i] = ',';
        }
    }
    char sentence[50];
    strcpy(sentence, "227 Entering PassiveMode(");
    strcat(sentence, host_ip);
    strcat(sentence, ",");
    char* str = (char *)malloc(4 * sizeof(char));
    strcat(sentence, numberToString(h5, str));
    free(str);
    strcat(sentence, ",");
    str = (char *)malloc(4 * sizeof(char));
    strcat(sentence, numberToString(h6, str));
    free(str);
    strcat(sentence, ")\r\n");

    writeBuffer(pthread_data->connfd, sentence);

    pthread_data->is_passive = 1;
}

void portHandler(struct ClientPthread* pthread_data, char* argument)
{
    // 清除之前已有的数据连接
    if(pthread_data->data_connfd[0] != -1)
    {
        if(pthread_data->data_connfd[1] != -1)
        {
            close(pthread_data->data_connfd[1]);
            pthread_data->data_connfd[1] = -1;
        }
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[0] = -1;
    }

    //创建socket
    if ((pthread_data->data_connfd[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        writeBuffer(pthread_data->connfd, "425 Fail to enter port mode.\r\n");
        return;
    }

    // 从输入中解析出ip和端口
    char client_ip[16];
    int number[6] = {-1, -1, -1, -1, -1, -1};
    int client_port = 0;
    int length = strlen(argument);
    int index = 0; // 在读取第几个数
    int i, j;

    for(i = 0; i < length; i++) {
        if (argument[i] == ',') {
            if (number[index] >= 0 && number[index] <= 255) {
                index++;
            } else {
                break;
            }
            if (index == 4) {
                j = i;
            }
        } else if (argument[i] >= '0' && argument[i] <= '9') {
            if (number[index] == -1) {
                number[index] = argument[i] - '0';
            } else {
                number[index] = number[index] * 10 + argument[i] - '0';
            }
        } else {
            break;
        }
    }

    if (i != length)
    {
        //输入不合法
        writeBuffer(pthread_data->connfd, "425 Fail to enter port mode.\r\n");
        return;
    }

    for (int k = 0; k < j; k++)
    {
        if (argument[k] == ',')
        {
            client_ip[k] = '.';
        }
        else
        {
            client_ip[k] = argument[k];
        }
    }
    client_ip[j] = '\0';
    client_port = number[4] * 256 + number[5];

    //设置目标主机的ip和port(即port命令中的ip和port)
    memset(pthread_data->data_addr, 0, sizeof(&(pthread_data->data_addr)));
    pthread_data->data_addr->sin_family = AF_INET;
    pthread_data->data_addr->sin_port = htons(client_port);

    //转换ip地址:点分十进制-->二进制
    if (inet_pton(AF_INET, client_ip, &(pthread_data->data_addr->sin_addr)) <= 0)
    {
        printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        writeBuffer(pthread_data->connfd, "425 Fail to enter port mode.\r\n");
        return;
    }
    writeBuffer(pthread_data->connfd, "200 PORT command successful.\r\n");

    pthread_data->data_connfd[1] = pthread_data->data_connfd[0];
    pthread_data->is_passive = 0;
}

void retrHandler(struct ClientPthread* pthread_data, char* argument)
{
    // 已创建数据连接
    if (pthread_data->data_connfd[0] != - 1)
    {
        prepareDataConnection(pthread_data);

        writeBuffer(pthread_data->connfd, "150 Opening BINARY mode data connection.\r\n");

        char filename[ARGUMENT_SIZE];
        if (argument[0] == '/')
        {
            strcat(filename, root_path);
            strcat(filename, argument);
        }
        else
        {
            strcat(filename, pthread_data->client_pwd);
            strcat(filename, "/");
            strcat(filename, argument);
        }

        int fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            writeBuffer(pthread_data->connfd, "550 No such file or directory.\r\n");
            return;
        }
        if(lseek(fd, pthread_data->start_byte,SEEK_SET) == -1)
        {
            writeBuffer(pthread_data->connfd, "550 REST error.\r\n");
            return;
        }
        char filecontent[SIZE];
        while (1)
        {
            int readLen = read(fd, filecontent, SIZE);
            if (!readLen)
            {
                break;
            }
            else if (readLen < 0)
            {
                writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                return;
            }
            int writeLen = 0;
            while (1)
            {
                int n = write(pthread_data->data_connfd[1], filecontent + writeLen, readLen - writeLen);
                if (n < 0)
                {
                    printf("Error write(): %s(%d)\n", strerror(errno), errno);
                    close(fd);
                    writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                    return;
                }
                else if (n == 0) {
                    break;
                }
                else {
                    writeLen += n;
                    if (writeLen == readLen) {
                        break;
                    }
                }
            }
        }
        writeBuffer(pthread_data->connfd, "226 Transfer complete.\r\n");
        close(fd);
        close(pthread_data->data_connfd[1]);
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[1] = -1;
        pthread_data->data_connfd[0] = -1;
        pthread_data->start_byte = 0;
    }
    else
    {
        writeBuffer(pthread_data->connfd, "425 No data connection.\r\n");
    }
}

void storHandler(struct ClientPthread* pthread_data, char* argument)
{
    // 已创建数据连接
    if (pthread_data->data_connfd[0] != - 1)
    {
        prepareDataConnection(pthread_data);
        writeBuffer(pthread_data->connfd, "150 Opening BINARY mode data connection.\r\n");

        char filename[ARGUMENT_SIZE];
        if (argument[0] == '/')
        {
            strcat(filename, root_path);
            strcat(filename, argument);
        }
        else{
            strcat(filename, pthread_data->client_pwd);
            strcat(filename, "/");
            strcat(filename, argument);
        }

        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR);
        if (fd <= 0)
        {
            writeBuffer(pthread_data->connfd, "550 No such file or directory.\r\n");
            return;
        }
        char filecontent[SIZE];
        while (1)
        {
            int readLen = read(pthread_data->data_connfd[1], filecontent, SIZE);
            if (!readLen)
            {
                break;
            }
            else if (readLen < 0)
            {
                writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                return;
            }
            int writeLen =0;
            while (1)
            {
                int n = write(fd, filecontent, readLen);
                if (n < 0)
                {
                    printf("Error read(): %s(%d)\n", strerror(errno), errno);
                    close(fd);
                    writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                    return;
                }
                else if (n == 0)
                {
                    break;
                }
                else {
                    writeLen += n;
                    if (writeLen == readLen) {
                        break;
                    }
                }
            }

        }

        writeBuffer(pthread_data->connfd, "226 Transfer complete.\r\n");
        close(fd);

        close(pthread_data->data_connfd[1]);
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[1] = -1;
        pthread_data->data_connfd[0] = -1;
    }
    else
    {
        writeBuffer(pthread_data->connfd, "425 No data connection.\r\n");
    }
}

void restHandler(struct ClientPthread* pthread_data, char* argument)
{
    pthread_data->start_byte = atoi(argument);
    writeBuffer(pthread_data->connfd, "350 Restart position accepted.\r\n");
}

void appeHandler(struct ClientPthread* pthread_data, char* argument)
{
    // 已创建数据连接
    if (pthread_data->data_connfd[0] != - 1)
    {
        prepareDataConnection(pthread_data);
        writeBuffer(pthread_data->connfd, "150 Opening BINARY mode data connection.\r\n");

        char filename[ARGUMENT_SIZE];
        if (argument[0] == '/')
        {
            strcat(filename, root_path);
            strcat(filename, argument);
        }
        else{
            strcat(filename, pthread_data->client_pwd);
            strcat(filename, "/");
            strcat(filename, argument);
        }

        int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND);
        if (fd <= 0)
        {
            writeBuffer(pthread_data->connfd, "550 open error.\r\n");
            return;
        }
        char filecontent[SIZE];
        while (1)
        {
            int readLen = read(pthread_data->data_connfd[1], filecontent, SIZE);
            if (!readLen)
            {
                break;
            }
            else if (readLen < 0)
            {
                writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                return;
            }
            int writeLen =0;
            while (1)
            {
                int n = write(fd, filecontent, readLen);
                if (n < 0)
                {
                    printf("Error read(): %s(%d)\n", strerror(errno), errno);
                    close(fd);
                    writeBuffer(pthread_data->connfd, "550 Cannot read the file or directory.\r\n");
                    return;
                }
                else if (n == 0)
                {
                    break;
                }
                else {
                    writeLen += n;
                    if (writeLen == readLen) {
                        break;
                    }
                }
            }

        }

        writeBuffer(pthread_data->connfd, "226 Transfer complete.\r\n");
        close(fd);

        close(pthread_data->data_connfd[1]);
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[1] = -1;
        pthread_data->data_connfd[0] = -1;
    }
    else
    {
        writeBuffer(pthread_data->connfd, "425 No data connection.\r\n");
    }
}

void listHandler(struct ClientPthread* pthread_data)
{
    // 已创建数据连接
    if (pthread_data->data_connfd[0] != - 1)
    {
        prepareDataConnection(pthread_data);
        writeBuffer(pthread_data->connfd, "150 Opening BINARY mode data connection.\r\n");
        char command[] = "ls -al ";
        strcat(command, pthread_data->client_pwd);
        FILE * f = popen(command, "r");
        if (f != NULL)
        {
            char buf[SIZE];
            while(1)
            {
                int readLen = fread(buf,sizeof(char),SIZE,f);
                if(!readLen)
                {
                    if (ferror(f)){
                        printf("File read error.\n");
                        writeBuffer(pthread_data->data_connfd[1], "550 read error");
                        return;
                    }
                    break;
                }
                int writeLen = 0;
                while (1)
                {
                    int n = write(pthread_data->data_connfd[1], buf + writeLen, readLen - writeLen);
                    if (n < 0)
                    {
                        printf("Error write(): %s(%d)\n", strerror(errno), errno);
                        pclose(f);
                        writeBuffer(pthread_data->connfd, "550 write error.\r\n");
                        return;
                    }
                    else if (n == 0) {
                        break;
                    }
                    else {
                        writeLen += n;
                        if (writeLen == readLen) {
                            break;
                        }
                    }
                }
            }
            pclose(f);
        }
        else
        {
            writeBuffer(pthread_data->data_connfd[1], "550 error");
        }
        close(pthread_data->data_connfd[1]);
        close(pthread_data->data_connfd[0]);
        pthread_data->data_connfd[1] = -1;
        pthread_data->data_connfd[0] = -1;
        writeBuffer(pthread_data->connfd, "226 Transfer complete.\r\n");
    }
    else
    {
        writeBuffer(pthread_data->connfd, "425 No data connection.\r\n");
    }
};

void mkdHandler(struct ClientPthread* pthread_data, char* argument)
{
    char filepath[ROOT_PATH_SIZE] = "";
    // 相对于根路径
    if (argument[0] == '/')
    {
        strcpy(filepath, root_path);
    }
    else
    {
        strcpy(filepath,pthread_data->client_pwd);
        strcat(filepath, "/");
    }
    strcat(filepath, argument);
    if(mkdir(filepath, 0777) < 0)
    {
        writeBuffer(pthread_data->connfd, "550 Can't make the directory.\r\n");
    }
    else
    {
        writeBuffer(pthread_data->connfd, "257 The directory was successfully created.\r\n");
    }
};

void rmdHandler(struct ClientPthread* pthread_data, char* argument)
{
    char filepath[ROOT_PATH_SIZE] = "";
    // 相对于根路径
    if (argument[0] == '/')
    {
        strcpy(filepath, root_path);
    }
    else
    {
        strcpy(filepath,pthread_data->client_pwd);
        strcat(filepath, "/");
    }
    strcat(filepath, argument);
    if(rmdir(filepath) < 0)
    {
        writeBuffer(pthread_data->connfd, "550 Can't remove the directory.\r\n");
    }
    else
    {
        writeBuffer(pthread_data->connfd, "250 The directory was successfully removed.\r\n");
    }
};

void pwdHandler(struct ClientPthread* pthread_data)
{
    char buffer[ROOT_PATH_SIZE] = "257 \"";
    strcat(buffer, pthread_data->client_pwd);
    strcat(buffer, "\" is your current location.\r\n");
    writeBuffer(pthread_data->connfd, buffer);
}

void cwdHandler(struct ClientPthread* pthread_data, char* argument)
{
    char filepath[ROOT_PATH_SIZE] = "";
    // 相对于根路径
    int length = strlen(argument);
    if (argument[length - 1] == '/')
    {
        argument[length - 1] = '\0';
    }
    if (argument[0] == '/')
    {
        strcpy(filepath, root_path);
    }
    else
    {
        strcpy(filepath,pthread_data->client_pwd);
        strcat(filepath, "/");
    }
    strcat(filepath, argument);
    if ((access(filepath,F_OK))!=-1)
    {
        strcpy(pthread_data->client_pwd, filepath);
        char buffer[ROOT_PATH_SIZE] = "257 OK. Current directory is ";
        strcat(buffer, filepath);
        strcat(buffer, "\r\n");
        writeBuffer(pthread_data->connfd, buffer);
    }
    else
    {
        writeBuffer(pthread_data->connfd, "550 Can't change to the directory.\r\n");
    }
};

void rnfrHandler(struct ClientPthread* pthread_data, char* argument)
{
    char filepath[500] = "";
    // 相对于根路径
    int length = strlen(argument);
    if (argument[length - 1] == '/')
    {
        argument[length - 1] = '\0';
    }
    if (argument[0] == '/')
    {
        strcpy(filepath, root_path);
    }
    else
    {
        strcpy(filepath,pthread_data->client_pwd);
        strcat(filepath, "/");
    }
    strcat(filepath, argument);
    if((access(argument,F_OK)) != -1) //对两种相对路径的支持
    {
        if (strcmp(pthread_data->filepath, ""))  // 之前已有指定旧名
        {
            strcpy(pthread_data->filepath, argument);
            writeBuffer(pthread_data->connfd, "350-Aborting previous rename operation\n350 RNFR accepted - file exists, ready for destination\r\n");
        }
        else
        {
            strcpy(pthread_data->filepath, argument);
            writeBuffer(pthread_data->connfd, "350 RNFR accepted - file exists, ready for destination.\r\n");
        }
    }
    else
    {
        writeBuffer(pthread_data->connfd, "550 Sorry, but that file doesn't exist.\r\n");
    }
};

void rntoHandler(struct ClientPthread* pthread_data, char* argument)
{
    if (strcmp(pthread_data->filepath, ""))
    {
        if(rename(pthread_data->filepath, argument) < 0) // 重命名失败
        {
            writeBuffer(pthread_data->connfd, "451 Rename/move failure\r\n");
        }
        else
        {
            writeBuffer(pthread_data->connfd, "250 File successfully renamed\r\n");
        }
    }
    else
    {
        writeBuffer(pthread_data->connfd, "503 Need RNFR before RNTO");
    }
};

void typeHandler(int connfd, char* argument)
{
    if (!strcmp(argument, "I"))
    {
        writeBuffer(connfd, "200 Type set to I.\r\n");
    }
    else
    {
        writeBuffer(connfd, "550 Type change error.\r\n");
    }
}

void systHandler(int connfd)
{
    writeBuffer(connfd, "215 UNIX Type: L8\r\n");
}

void quitHandler(int connfd)
{
    writeBuffer(connfd, "221 Logout.\r\n");
}

void aborHandler(int connfd)
{
    writeBuffer(connfd, "221 Logout.\r\n");
}