#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include "pbmessage.pb-c.c"

#define MAX_MSG_SIZE 255 /* Максимальная длина сообщения */

void DieWithError(char *errorMessage) // Для вывода ошибки и завершения работы
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[])
{
	int brcstSock;                    /* Socket */
    struct sockaddr_in broadcastAddr; /* Broadcast Address */
    unsigned short broadcastPort = 32012;     /* Port */
	int sign = 0; 						  /* Полученная датаграмма */
	
	int serverSock;                        /* Socket descriptor */
    struct sockaddr_in serverAddr;    /* Адрес сервера */
    unsigned short serverPort = 32001;
    socklen_t len = sizeof(serverAddr);   /* Размер структуры сервера */
               
    PbMessage *msgBuf;
    int msgLen;
	//uint8_t buf[MAX_MSG_SIZE];
	
	uint8_t firstConnect=1;      
            
    /* Создаем UDP сокет для приёма ШР */
    if ((brcstSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    /* Создаем TCP сокет для получения сообщения */
    if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");
    
    /* Конфигурируем структуру сокета ШР */
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Zero out structure */
    broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
    broadcastAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* Any incoming interface */
    broadcastAddr.sin_port = htons(broadcastPort);      /* Broadcast port */

    /* Привязка сокета к структуре */
    while(bind(brcstSock, (struct sockaddr *) &broadcastAddr, sizeof(broadcastAddr)) < 0)
    {
		broadcastAddr.sin_port = htons(++broadcastPort);
	}
	printf("Port: %d\n", broadcastPort);
    for(;;)
    {
    /* Принимаем датаграмму */
    if (recvfrom(brcstSock, &sign, sizeof(sign), 0, (struct sockaddr *)&serverAddr, &len) < 0)
        DieWithError("recvfrom() failed");    
    
    serverAddr.sin_port = htons(serverPort);
    if(firstConnect==1)
	{
		/* Подключаемся к серверу */
		if (connect(serverSock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
		DieWithError("connect() failed");
		firstConnect=0;
	}
	/* Сообщаем серверу о готовности принять сообщение */
	sign=5;
    /*if (*/send(serverSock, &sign, sizeof(int), 0);// != sizeof(int))
    //DieWithError("send() sent a different number of bytes than expected");
    /* Принимаем сообщение */
    uint8_t buf[MAX_MSG_SIZE];
    memset(buf, 0, sizeof(buf));
    if ((msgLen=recv(serverSock, &buf, MAX_MSG_SIZE, 0)) < 0)
        DieWithError("recv() failed");
    msgBuf = pb_message__unpack(NULL, msgLen, buf);      
    printf("\033[0;34mReceived message:\033[0m %s, length = %d, sleep time = %d\n", msgBuf->message.data, (int)msgBuf->message.len, msgBuf->t);  
    sleep(msgBuf->t);
    free(msgBuf);
	}
	
    close(serverSock);
}
