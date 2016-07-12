#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>

#define MAXPENDING 5    /* Maximum outstanding connection requests */
#define MAX_MSG_SIZE  255 /* Максимальная длина сообщения */
#define K 3
#define L 3
#define N 5

struct { // Mutex для синхронизации очереди
	pthread_mutex_t	mutex;
	uint8_t mesQueue[N][MAX_MSG_SIZE];
	int mesCount;
} shared = {
	PTHREAD_MUTEX_INITIALIZER
};

struct toThreads
{
	int sockDescr;
	struct sockaddr_in sockAddr;
};

void DieWithError(char *errorMessage) // Для вывода ошибки и завершения работы
{
    perror(errorMessage);
    exit(1);
}

void *sendUDPtoSnds(void *first)
{
	int mesCount;
	uint8_t sendMsg=1;
	int brcstSock = ((struct toThreads*)first)->sockDescr;
	struct sockaddr_in broadcastAddr = ((struct toThreads*)first)->sockAddr;
	int initPort = 32002;
	for(;;)
	{
    pthread_mutex_lock(&shared.mutex);
		mesCount = shared.mesCount;
    pthread_mutex_unlock(&shared.mutex);
    if(mesCount==N) {
		printf("Broadcast message to senders NOT SENT\n");
		}
    else {
		for(int i=0;i<10;i++)
		{
			broadcastAddr.sin_port=htons(initPort+i);
			//printf("initport=%d, i=%d, broadcastAddr.sin_port=%d \n",htons(initPort),i,htons(broadcastAddr.sin_port));
			if (sendto(brcstSock, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr *) 
				&broadcastAddr, sizeof(broadcastAddr)) != sizeof(sendMsg))
				DieWithError("sendto() sent a different number of bytes than expected");
		}
		printf("Broadcast message to senders SENT\n");
	}
	sleep(K);
	}
}

void *sendUDPtoRcvs(void *first)
{
	int mesCount;
	uint8_t sendMsg=5;
	int brcstSock = ((struct toThreads*)first)->sockDescr;
	struct sockaddr_in broadcastAddr = ((struct toThreads*)first)->sockAddr;
	int initPort = 32012;
	for(;;)
	{
    pthread_mutex_lock(&shared.mutex);
		mesCount = shared.mesCount;
    pthread_mutex_unlock(&shared.mutex);
    if(mesCount==0) {
		printf("Broadcast message to receivers NOT SENT\n");
		}
    else {
		for(int i=0;i<10;i++)
		{
			broadcastAddr.sin_port=htons(initPort+i);
			if (sendto(brcstSock, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr *) 
				&broadcastAddr, sizeof(broadcastAddr)) != sizeof(sendMsg))
				DieWithError("sendto() sent a different number of bytes than expected");
		}
		printf("Broadcast message to receivers SENT\n");
	}
	sleep(L);
	}
}

void *clientSender(void *sock)
{
	int clntSock = *((int*)sock);
	pthread_detach(pthread_self()); 
	for(;;)
	{
	uint8_t buf[MAX_MSG_SIZE];
    memset(buf, 0, sizeof(buf));
	if ((recv(clntSock, &buf, MAX_MSG_SIZE, 0)) <= 0)
	{
		printf("Sender disconnected\n");
		break;
	}
        //DieWithError("recv() failed");
    
    pthread_mutex_lock(&shared.mutex);
    if(shared.mesCount==5)
    {
		pthread_mutex_unlock(&shared.mutex);
		continue;
	}
		strcpy((char*)shared.mesQueue[shared.mesCount], (char*)buf);
		shared.mesCount++;
		//for(int i=0;i<5;i++) printf("%s\n", shared.mesQueue[i]);
		printf("\033[0;32mMessage received\033[0m\nMessages in queue: %d\n", shared.mesCount);
    pthread_mutex_unlock(&shared.mutex);
	}
    close(clntSock);    // Close client socket 
    return(NULL);
}

void *clientReceiver(void *sock)
{
	int clntSock = *((int*)sock);
    void* buf;
    int signal;
    int mesCount;
    pthread_detach(pthread_self()); 
    for(;;)
	{
		signal=0;
		if ((recv(clntSock, &signal, sizeof(int), 0)) <= 0)
        {	
			printf("Receiver disconnected\n");
			break;
		}
        if(signal!=5) break;
    pthread_mutex_lock(&shared.mutex);
		if(shared.mesCount==0)
		{		
			do
			{
				pthread_mutex_unlock(&shared.mutex);	
				sleep(2);
				pthread_mutex_lock(&shared.mutex);
			}while(shared.mesCount<=0);
		}
		buf = calloc(strlen((char*)shared.mesQueue[0]), sizeof(char));
		strcpy((char*)buf, (char*)shared.mesQueue[0]);	

		for(int i=0; i<shared.mesCount-1; i++)
		{
			strcpy((char*)shared.mesQueue[i], (char*)shared.mesQueue[i+1]);
		}
		memset(shared.mesQueue[shared.mesCount-1], 0, sizeof(shared.mesQueue[shared.mesCount-1]));
		shared.mesCount--;
		mesCount=shared.mesCount;
	pthread_mutex_unlock(&shared.mutex);	
	
	if (send(clntSock, buf, strlen((char*)buf), 0) != strlen((char*)buf))
			DieWithError("send() sent a different number of bytes than expected");
    printf("\033[0;34mMessage sent\033[0m\nMessages in queue: %d\n", mesCount);
    
    
	}
	free(buf);
    close(clntSock);    // Close client socket  
    return(NULL);
}

void *listener(void  *sock)
{
	pthread_t threadID;
	int clntSock[30];
	int clntCount = 0;
	struct sockaddr_in clntAddr;
	unsigned int clntLen = sizeof(clntAddr);
	int rcvsSock = ((struct toThreads*)sock)->sockDescr;
	for(;;)
	{
		if ((clntSock[clntCount] = accept(rcvsSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");
		
        printf("Handling client (Receiver)%s\nPort: %d\n", inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));
		pthread_create(&threadID, NULL, clientReceiver, &clntSock[clntCount]); //Создаем поток для обработки сообщения клиента-получателя 
		clntCount++;
	}
}

int main(int argc, char *argv[])
{
	shared.mesCount = 0;
	int sndsSock;     
	struct sockaddr_in sndsAddr;
	int rcvsSock;     
	struct sockaddr_in rcvsAddr;
	//unsigned short servPort;
	
	int clntSock[30];
	int clntCount = 0;
	struct sockaddr_in clntAddr;
	unsigned int clntLen = sizeof(clntAddr);
	
	int brcstSock;                    /* Дескриптор сокета для широковещательной рассылки (ШР)*/
    struct sockaddr_in broadcastAddr; /* Структура ШР */
    char *broadcastIP;                /* IP адрес ШР */
    int broadcastPermission;          /* Опция для сокета, дающая права на ШР */
    
    pthread_t threadID;              /* ID потока из pthread_create() */
    
     if (argc != 3)                     // Test for correct number of parameters 
    {
        fprintf(stderr,"Usage:  %s <IP Address>\n", argv[0]);
        exit(1);
    }
    broadcastIP = argv[1];            /* First arg:  broadcast IP address */ 
    
    /* Создаем сокет для рассылки */
    if ((brcstSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    /* TCP сокет отправителей */
    if ((sndsSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");
    /* TCP сокет получателей */
    if ((rcvsSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");
        
    /* Даём сокету права на ШР*/
    broadcastPermission = 1;
    if (setsockopt(brcstSock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
        DieWithError("setsockopt() failed");
        
    if (setsockopt(brcstSock, SOL_SOCKET, SO_REUSEADDR, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
		DieWithError("setsockopt() failed");
        
    /* Конфигурируем структуру сокета ШР*/
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Заполняем структуру нулями */
    broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
    broadcastAddr.sin_addr.s_addr = inet_addr(broadcastIP);/* Broadcast IP address */
    broadcastAddr.sin_port = htons(32002);         /* Broadcast port */

    /* Конфигурируем структуру сокета для отправителей*/
    memset(&sndsAddr, 0, sizeof(sndsAddr));   /* Zero out structure */
    sndsAddr.sin_family = AF_INET;                /* Internet address family */
    sndsAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    sndsAddr.sin_port = htons(32000);      /* Local port */
    
    /* Конфигурируем структуру сокета для получателей*/
    memset(&rcvsAddr, 0, sizeof(rcvsAddr));   /* Zero out structure */
    rcvsAddr.sin_family = AF_INET;                /* Internet address family */
    rcvsAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    rcvsAddr.sin_port = htons(32001);      /* Local port */
    
    /* Привязка сокета отправителей к структуре */
    if (bind(sndsSock, (struct sockaddr *) &sndsAddr, sizeof(sndsAddr)) < 0)
        DieWithError("bind() failed");
    /* Привязка сокета получателей к структуре */
    if (bind(rcvsSock, (struct sockaddr *) &rcvsAddr, sizeof(rcvsAddr)) < 0)
        DieWithError("bind() failed");
        
    struct toThreads broadcastSockInfo;
    broadcastSockInfo.sockDescr = brcstSock;
    broadcastSockInfo.sockAddr = broadcastAddr;
    pthread_create(&threadID, NULL, sendUDPtoSnds, &broadcastSockInfo);
    pthread_create(&threadID, NULL, sendUDPtoRcvs, &broadcastSockInfo);
    
    if (listen(sndsSock, MAXPENDING) < 0)
        DieWithError("listen() failed");
    if (listen(rcvsSock, MAXPENDING) < 0)
        DieWithError("listen() failed");
        
    struct toThreads socketInfo;
	socketInfo.sockDescr = rcvsSock;
	socketInfo.sockAddr = rcvsAddr;
	pthread_create(&threadID, NULL, listener, &socketInfo);
		
    for (;;) /* Run forever */
    {
        /* Ждем подключения */
        //clntSock = 0;
        if ((clntSock[clntCount] = accept(sndsSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");

        printf("Handling client (Sender)%s\nPort: %d\n", inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));   
		pthread_create(&threadID, NULL, clientSender, &clntSock[clntCount]); //Создаем поток для обработки сообщения клиента-отправителя
		clntCount++;
    }
}
