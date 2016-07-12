#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <time.h>  
#include "pbmessage.pb-c.c"

#define MAXSTRINGSIZE 250 /* Максимальная длина сообщения */
#define MAXSLEEPTIME 10 /*Максимальное время сна */

void DieWithError(char *errorMessage) // Для вывода ошибки и завершения работы
{
    perror(errorMessage);
    exit(1);
}

int main(int argc, char *argv[])
{
    int brcstSock;                    /* Socket */
    struct sockaddr_in broadcastAddr; /* Broadcast Address */
    unsigned short broadcastPort = 32002;     /* Port */
    int sign = 0; 						  /* Полученная датаграмма */
    
    int serverSock;                        /* Socket descriptor */
    struct sockaddr_in serverAddr;    /* Адрес сервера */
    unsigned short serverPort = 32000;     /* Port */
    socklen_t len = sizeof(serverAddr);   /* Размер структуры сервера */

	int T;
    
    PbMessage msg = PB_MESSAGE__INIT;
    void *buf;                     // Buffer to store serialized data
    char *message;
    unsigned msglen;
    
    int8_t firstConnect = 1;
    if (argc != 1)                     // Test for correct number of parameters 
    {
        fprintf(stderr,"Usage:  %s <Server Port>\n", argv[0]);
        exit(1);
    }
    //serverPort = atoi(argv[1]);
    
    /* Создаем UDP сокет для приёма ШР */
    if ((brcstSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    /* Создаем TCP сокет для передачи сообщения */
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

	serverAddr.sin_port = htons(serverPort);//htons(broadcastPort);
	if(firstConnect==1)
	{
	/* Подключаемся к серверу */
	if (connect(serverSock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    DieWithError("connect() failed");
	firstConnect=0;
	}
    
    //Генерируем случайное сообщение
    int length;
	srand(time(NULL) - (broadcastPort-32000)*5000);
	length = (rand() % MAXSTRINGSIZE) + 1;
	message = calloc(length, sizeof(char));
	for(int i=0; i<length; i++)
	{
		message[i] = 65 + (rand() % 57);
	}
	message[length]='\0';
    /* Генерируем время T */
    T = (rand() % MAXSLEEPTIME) + 1;
    /* Упаковываем в протобуфер */
    msg.t = T;
    msg.message.data = (uint8_t *)message;
    msg.message.len = strlen(message);
    msglen = pb_message__get_packed_size(&msg);
    buf = malloc(msglen);
    pb_message__pack(&msg,buf);
	
    if (send(serverSock, buf, msglen, 0) != msglen)
    DieWithError("send() sent a different number of bytes than expected");
        
    printf("\033[0;32mSent message:\033[0m %s, length = %d, sleep time = %d\n", msg.message.data, (int)msg.message.len, msg.t);    /* Вывод отправленной строки */
    
    sleep(T);
     
   } 
    free(message);
    free(buf);  
    close(serverSock);
    close(brcstSock);
    exit(0);
}
    
