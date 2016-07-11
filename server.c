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
	int timetoBCast;
	int sendMsg;
	int sockDescr;
	struct sockaddr_in broadcastAddr;
};

void DieWithError(char *errorMessage) // Для вывода ошибки и завершения работы
{
    perror(errorMessage);
    exit(1);
}

void *sendUDPBC(void *first)
{
	int sleepTime = ((struct toThreads*)first)->timetoBCast;
	int sendMsg = ((struct toThreads*)first)->sendMsg;
	int brcstSock = ((struct toThreads*)first)->sockDescr;
	int mesCount;
	struct sockaddr_in broadcastAddr = ((struct toThreads*)first)->broadcastAddr;
	int initPort = broadcastAddr.sin_port;
	for(;;)
	{
    pthread_mutex_lock(&shared.mutex);
		mesCount = shared.mesCount;
    pthread_mutex_unlock(&shared.mutex);
    printf("Broadcast message to clients of type \"%d\": ", sendMsg);
    if((sendMsg==1 && (mesCount)==N)||(sendMsg==2 && mesCount==0)) {
		printf("NOT SENT\n");
		}
    else {
		for(int i=0;i<10;i++)
		{
			broadcastAddr.sin_port=htons(ntohs(initPort)+i);
			//printf("initport=%d, i=%d, broadcastAddr.sin_port=%d ",htons(initPort),i,htons(broadcastAddr.sin_port));
			if (sendto(brcstSock, &sendMsg, sizeof(sendMsg), 0, (struct sockaddr *) 
				&broadcastAddr, sizeof(broadcastAddr)) != sizeof(sendMsg))
				DieWithError("sendto() sent a different number of bytes than expected");
		}
		printf("SENT, mesCount = %d\n", mesCount);
	}
	sleep(sleepTime);
	}
}

void *clientSender(void *sock)
{
	int clntSock = *((int*)sock);
    printf("asdasds: %d ", clntSock);
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
    pthread_mutex_unlock(&shared.mutex);
    printf("%s\n", buf);
    printf("Message received\n");
	}
    close(clntSock);    // Close client socket 
    return(NULL);
}

void *clientReceiver(void *sock)
{
	int clntSock = *((int*)sock);
    void* buf;
    int signal;
    pthread_detach(pthread_self()); 
    for(;;)
	{
		signal=0;
		if ((recv(clntSock, &signal, sizeof(int), 0)) <= 0)
        {	
			printf("Receiver disconnected\n");
			break;
		}
        if(signal!=2) break;
        
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
	pthread_mutex_unlock(&shared.mutex);	
	
	if (send(clntSock, buf, strlen((char*)buf), 0) != strlen((char*)buf))
			DieWithError("send() sent a different number of bytes than expected");
    printf("Message sent\n");
    
    
	}
	free(buf);
    close(clntSock);    // Close client socket  
    return(NULL);
}

int main(int argc, char *argv[])
{
	shared.mesCount = 0;
	int servSock;     
	struct sockaddr_in servAddr;
	unsigned short servPort;
	
	int clntSock[30];
	int clntCount = 0;
	struct sockaddr_in clntAddr;
	unsigned int clntLen = sizeof(clntAddr);
	int clntType;
	
	int brcstSock;                    /* Дескриптор сокета для широковещательной рассылки (ШР)*/
    struct sockaddr_in broadcastAddr; /* Структура ШР */
    char *broadcastIP;                /* IP адрес ШР */
    int broadcastPermission;          /* Опция для сокета, дающая права на ШР */
    
    pthread_t threadID;              /* ID потока из pthread_create() */
    
     if (argc != 3)                     // Test for correct number of parameters 
    {
        fprintf(stderr,"Usage:  %s <IP Address> <Port>\n", argv[0]);
        exit(1);
    }
    broadcastIP = argv[1];            /* First arg:  broadcast IP address */ 
    servPort = atoi(argv[2]);
    
    /* Создаем сокет для рассылки */
    if ((brcstSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    /* TCP сокет сервера */
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        DieWithError("socket() failed");
        
    /* Даём сокету права на ШР*/
    broadcastPermission = 1;
    if (setsockopt(brcstSock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0)
        DieWithError("setsockopt() failed");
        
    /* Конфигурируем структуру сокета ШР*/
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));   /* Заполняем структуру нулями */
    broadcastAddr.sin_family = AF_INET;                 /* Internet address family */
    broadcastAddr.sin_addr.s_addr = inet_addr(broadcastIP);/* Broadcast IP address */
    broadcastAddr.sin_port = htons(servPort);         /* Broadcast port */
    
    /* Конфигурируем структуру сокета сервера*/
    memset(&servAddr, 0, sizeof(servAddr));   /* Zero out structure */
    servAddr.sin_family = AF_INET;                /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    servAddr.sin_port = htons(servPort);      /* Local port */
    
    /* Привязка сокета к структуре */
    if (bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        DieWithError("bind() failed");
        
    struct toThreads sendersThr;
    sendersThr.timetoBCast = K;
    sendersThr.sendMsg = 1;
    sendersThr.sockDescr = brcstSock;
    sendersThr.broadcastAddr = broadcastAddr;
    pthread_create(&threadID, NULL, sendUDPBC, &sendersThr);
    
    struct toThreads receiversThr;
    receiversThr.timetoBCast = L;
    receiversThr.sendMsg = 2;
    receiversThr.sockDescr = brcstSock;
    receiversThr.broadcastAddr = broadcastAddr;
    pthread_create(&threadID, NULL, sendUDPBC, &receiversThr);
    
    if (listen(servSock, MAXPENDING) < 0)
        DieWithError("listen() failed");
        
    for (;;) /* Run forever */
    {
        /* Ждем подключения */
        //clntSock = 0;
        if ((clntSock[clntCount] = accept(servSock, (struct sockaddr *) &clntAddr, &clntLen)) < 0)
            DieWithError("accept() failed");

        printf("Handling client %s\nPort: %d\n", inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));
        
        if (recv(clntSock[clntCount], &clntType, sizeof(clntType), 0) < 0)
        DieWithError("recv() failed");
        
        printf("%d ", *clntSock);
        if(clntType==1)
		pthread_create(&threadID, NULL, clientSender, &clntSock[clntCount]); //Создаем поток для обработки сообщения клиента-отправителя
		else
		pthread_create(&threadID, NULL, clientReceiver, &clntSock[clntCount]); //Создаем поток для обработки сообщения клиента-получателя      
		clntCount++;
    }
}
