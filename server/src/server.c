#include "processor.h"

int host_port = 21;
char root_path[ROOT_PATH_SIZE] = "/tmp";
char host_ip[16] = "127.0.0.1";

void parseArguments(int argc, char **argv) {
    const char *optstring = "p::r::";
    int c, index;
    struct option opts[] = {{"port", required_argument, NULL, 'p'},
                            {"root", required_argument, NULL, 'r'}};

    while ((c = getopt_long_only(argc, argv, optstring, opts, &index)) != -1) {
        switch (c) {
            case 'p':
                host_port = atoi(optarg);
                break;
            case 'r':
                strcpy(root_path, optarg);
                break;
            default:
                printf("wrong argument\n");
                break;
        }
    }
}

int parseInstruction(struct ClientPthread* pthread_data, char sentence[])
{
    char order[5];
    char argument[ARGUMENT_SIZE];

    int index = -1;
    int length = strlen(sentence);
    for (int i = 0; i < length; i++)
    {
        if (sentence[i] == ' ')
        {
            index = i;
            break;
        }
    }
    if (index >= 5)
    {
        // �����������
        return processor(pthread_data, "", "");
    }
    else if(index == -1)
    {
        // ������ǲ�������������
        int j = -1;
        for(int i = 0; i < length; i++)
        {
            if (i >= 4)
            {
                j = -2;
                break;
            }
            else if (isspace(sentence[i]))
            {
                j = i;
                break;
            }
            else
            {
                order[i] = sentence[i];
            }
        }
        if (j == -1)
        {
            order[length] = '\0';
        }
        else if(j >= 0)
        {
            order[j] = '\0';
        }
        return processor(pthread_data, order, "");
    }
    else
    {
        for(int i = 0; i < index; i++)
        {
            order[i] = sentence[i];
        }
        order[index] = '\0';
        int j = -1;
        for(int i = index + 1; i < length; i++)
        {
            if (!isspace(sentence[i]))
            {
                argument[i - index - 1] = sentence[i];
            }
            else
            {
                j = i;
                break;
            }
        }
        if (j == -1)
        {
            argument[length - index - 1] = '\0';
        }
        else
        {
            argument[j - index - 1] = '\0';
        }
        return processor(pthread_data, order, argument);
    }
}

void* pthreadFunc(void *arg)
{
    struct ClientPthread* pthread_data = (struct ClientPthread*)arg;
    writeBuffer(pthread_data->connfd, "220 Anonymous FTP server ready.\r\n");
    while (1) {
        //ե��socket����������
        char sentence[SIZE];  // �ַ���
        readBuffer(pthread_data->connfd, sentence);
        // �����û�����
        if (parseInstruction(pthread_data, sentence) < 0)
        {
            close(pthread_data->connfd);
            free(pthread_data->data_addr);
            free(pthread_data);
            break;
        }
    }
}


int main(int argc, char **argv) {

    // �����в�������
    parseArguments(argc, argv);

	int listenfd, connfd;		//����socket������socket
	struct sockaddr_in addr;

	//����socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//���ñ�����ip��port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(host_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);	//����"0.0.0.0"

	//��������ip��port��socket��
	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//��ʼ����socket
	if (listen(listenfd, 10) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    //����������������
    while (1) {
        //�ȴ�client������ -- ��������
        if ((connfd = accept(listenfd, NULL, NULL)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            continue;
        }

        // Ϊÿ���ͻ��˵Ŀ��̼߳�¼����
        struct ClientPthread* pthread_data = (struct ClientPthread*)malloc(sizeof(struct ClientPthread));
        pthread_t id;
        pthread_data->connfd = connfd;
        pthread_data->data_connfd[0] = -1;
        pthread_data->data_connfd[1] = -1;
        struct sockaddr_in *data_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        pthread_data->userStatus = no_log;
        pthread_data->is_passive = 1;
        pthread_data->data_addr = data_addr;
        strcpy(pthread_data->client_pwd, root_path);
        strcpy(pthread_data->filepath,"");
        pthread_data->start_byte = 0;
        pthread_create(&id, NULL, pthreadFunc, (void *)pthread_data);
    }

    close(listenfd);
    return 0;
}

