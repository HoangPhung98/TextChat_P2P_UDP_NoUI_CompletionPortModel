
#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE  
#define _CRT_NONSTDC_NO_DEPRECATE

#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#define DATABASE_FILE_PATH "C:\\Users\\PhungVanHoang\\source\\repos\\CompletionPort\\database.txt"
#define PADDING_NUMBER_OF_CLIENT 25;
#define MSG_LOGIN_SUCCESSFULLY "Login successfully!\n"
#define MSG_WRONG_FORM "WRONG input form, pls retry:\n"
#define MSG_WRONG_ACCOUNT "Wrong user or password, pls retry:\n"
#define MSG_PLS_LOGIN "Pls login:\n"
#define MSG_B_IS_OFFLINE "Your partner is offline now, try another time!\n"

#define LOGIN "LOGIN"
#define CALL "CALL"
#define RES_CALL_FROM "CALL_FROM"
#define ACCEPT "ACCEPT"
#define ACCEPT_FROM "ACCEPT_FROM"
#define DECLINE "DECLINE"
#define DECLINE_FROM "DECLINE_FROM"


typedef struct _PER_IO_DATA {
	OVERLAPPED overlapped;
	WSABUF wsabuf;
	char buff[1024];
}PER_IO_DATA, *LPPER_IO_DATA;

typedef struct _PER_HANDLE_DATA {
	SOCKET socket;
	SOCKADDR_STORAGE socketaddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;


typedef struct _CLIENT {
	LPPER_HANDLE_DATA lpPerHandleData;
	LPPER_HANDLE_DATA lpPerHandleData_TargetClient;
	LPPER_IO_DATA lpPerIOData;
	char user[256], password[256];
	bool isLoggedIn = false , 
		 isRighUser = false, 
		 isRighPassword = false, 
		 isInAConverstation = false, 
		 isDialing = false;
}Client;

typedef struct _LIST_CLIENT {
	Client* list;
	int n = 0;
}ListClient, *LPListClient;

//prototype
DWORD WINAPI WorkerThread(LPVOID);
ListClient ReadDataBase();
ListClient CreateListClient(int);
void showListClient(ListClient);
void DeleteListClient(ListClient);
int CheckLoginInfo(char[], char[]);
int FindIndexOfClient(char[]);

//global vars
ListClient listClient, listOnlineClient, listOnProcessingClient;

int main()
{
	//init database
	listClient = ReadDataBase();
	//remove: showListClient(listClient);
	//dont use: listOnlineClient = CreateListClient(0);
	//dont use: listOnProcessingClient = CreateListClient(0);

	//winsock
	WSADATA wsa;
	SOCKET thisServer;
	SOCKADDR_IN thisaddr;
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;
	DWORD bytes, flags;

	//create completion port
	completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//get number of processor
	GetSystemInfo(&systemInfo);

	//create thread equal number of processors in system
	//??? what if move handle?
	for (int i = 0; i < systemInfo.dwNumberOfProcessors; i++) {
		HANDLE threadHandle;
		threadHandle = CreateThread(NULL, 0, WorkerThread, completionPort, 0, NULL);
		CloseHandle(threadHandle);
	}

	//init winsock
	WSAStartup(MAKEWORD(2, 2), &wsa);
	thisServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	thisaddr.sin_addr.s_addr = htonl(ADDR_ANY);
	thisaddr.sin_port = htons(9000);
	thisaddr.sin_family = AF_INET;
	bind(thisServer, (SOCKADDR *)&thisaddr, sizeof(thisaddr));
	listen(thisServer, 5);

	while (true) {

		SOCKADDR_IN clientaddr;
		int lengthaddr = sizeof(clientaddr);
		SOCKET newClient;
		LPPER_HANDLE_DATA perHandleData = NULL;
		LPPER_IO_DATA perIOData = NULL;

		//accept new client, 
		newClient = accept(thisServer, (SOCKADDR *)&clientaddr, &lengthaddr);
		printf("**Accept new client \n");

		//and attach it to Completion Port
		perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));
		perHandleData->socket = newClient;
		memcpy(&perHandleData->socketaddr, &clientaddr, lengthaddr);

		CreateIoCompletionPort((HANDLE)newClient, completionPort, (DWORD)perHandleData, 0);

		//create IO data structure to store data
		perIOData = (LPPER_IO_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_DATA));
		perIOData->wsabuf.buf = perIOData->buff;
		perIOData->wsabuf.len = 1024;
		
		//recv data for the first time
		bytes = 0;
		flags = 0;
		
		WSARecv(perHandleData->socket, &(perIOData->wsabuf), 1, &bytes, &flags, &(perIOData->overlapped), NULL);
	}



	//close program
	closesocket(thisServer);
	WSACleanup();
	DeleteListClient(listClient);	
	DeleteListClient(listOnlineClient);
}
DWORD WINAPI WorkerThread(LPVOID lpparam) {
	printf("Created new thread\n");
	HANDLE completionPort = (HANDLE)lpparam;
	DWORD numberOfByteTranffered;
	DWORD flags = 0;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_DATA perIOData;

	char buff[1024];
	char type[5];
	char arg1[512], arg2[512], tmp[10];

	while (true) {	
		//get data tranffered
		GetQueuedCompletionStatus(
			completionPort, 
			&numberOfByteTranffered, 
			(LPDWORD)&perHandleData,
			(LPOVERLAPPED*)&perIOData, 
			INFINITE);

		if (numberOfByteTranffered == 0) {
			closesocket(perHandleData->socket);
			GlobalFree(perHandleData);
			GlobalFree(perIOData);
			printf("Error\n");
			continue;
		}

			//get data from buff and print
			perIOData->buff[numberOfByteTranffered] = 0;
			printf("%d: %s\n", perHandleData->socket, perIOData->buff);

			//split request TYPE, arg1, arg2
			int n = sscanf(perIOData->buff, "%s %s %s %s", type, arg1, arg2, tmp);
			int index_user_A = FindIndexOfClient(arg1);
			int index_user_B = FindIndexOfClient(arg2);

			//handle request
			if (n != 3) send(perHandleData->socket, MSG_WRONG_FORM, strlen(MSG_WRONG_FORM),0);
			else 
				if (!listClient.list[index_user_A].isLoggedIn) {
					if (strcmp(type, LOGIN) == 0) {
						int index = CheckLoginInfo(arg1, arg2);
						if (index >= 0) {

							listClient.list[index].lpPerHandleData = perHandleData;
							listClient.list[index].lpPerIOData = perIOData;
							listClient.list[index].isLoggedIn = true;
							printf("**New Online client : %d\n", listClient.list[index].lpPerHandleData->socket);
							send(perHandleData->socket, MSG_LOGIN_SUCCESSFULLY, strlen(MSG_LOGIN_SUCCESSFULLY), 0);
						}
						else {
							send(perHandleData->socket, MSG_WRONG_ACCOUNT, strlen(MSG_WRONG_ACCOUNT), 0);
						}
					}
					else {
						send(perHandleData->socket, MSG_PLS_LOGIN, strlen(MSG_PLS_LOGIN), 0);
					}
				}
				else {
					if (strcmp(type, CALL) == 0) {
						if (listClient.list[index_user_A].isLoggedIn) {
							if (index_user_B >= 0 && listClient.list[index_user_B].isLoggedIn) {
								char buff[50];
								sprintf(buff, "%s %s %s", RES_CALL_FROM, arg1, arg2);
								LPPER_HANDLE_DATA lpPerHandleData_B = listClient.list[index_user_B].lpPerHandleData;
								struct sockaddr_in* sin = (struct sockaddr_in*) & (lpPerHandleData_B->socketaddr);
								struct in_addr* ip = (struct in_addr*) & (sin->sin_addr.s_addr);
								//int saddrlength = sizeof(SOCKADDR);
								//char buff2[100];
								//unsigned char* ip = (unsigned char*) & (sin->sin_addr.s_addr);
								//char buff_sock[100];
								//itoa(sin->sin_port,buff_sock, 10);
								//printf("ip of %s: %d %d %d %d\n", listClient.list[index_user_B].user, ip[0],ip[1],ip[2],ip[3]);

								char buff_ntoa[100];
								strcpy(buff_ntoa, inet_ntoa(*ip));
								printf("ip ntoa : %s\n", buff_ntoa);
											
								send(lpPerHandleData_B->socket, buff, strlen(buff), 0);
							}
							else {
								send(listClient.list[index_user_A].lpPerHandleData->socket, MSG_B_IS_OFFLINE, strlen(MSG_B_IS_OFFLINE), 0);
							}
						}
						else {
							send(perHandleData->socket, MSG_PLS_LOGIN, strlen(MSG_PLS_LOGIN), 0);
						}

					}
					else if (strcmp(type, ACCEPT) == 0) {
						//B send ACCEPT A B to server, 
						//then server get ip of B from lpPerHandleData_B and send back to A
						//arg1 = userA, arg2 = port of user B
						LPPER_HANDLE_DATA lpPerHandleData_B = listClient.list[index_user_B].lpPerHandleData;
						struct sockaddr_in* sin = (struct sockaddr_in*) & (lpPerHandleData_B->socketaddr);
						struct in_addr* ip = (struct in_addr*) & (sin->sin_addr.s_addr);
						char buff_ip_of_B[100];
						strcpy(buff_ip_of_B, inet_ntoa(*ip));

						//append ip addr of B to msg
						char buff_append[256];
						sprintf(buff_append, "%s %s %s", ACCEPT_FROM, buff_ip_of_B, arg2);
						//send msg   ACCEPT_FROM "ip" B   back to A
						send(listClient.list[index_user_A].lpPerHandleData->socket, buff_append, strlen(buff_append), 0);
					}
					else if (strcmp(type, DECLINE)) {
						char buff_decline[100];
						sprintf(buff_decline, "%s %s %s", DECLINE_FROM, arg1, arg2);
						send(listClient.list[index_user_A].lpPerHandleData->socket, buff_decline, strlen(buff_decline), 0);
					}
				}
			
			//clear overlapped structure
			ZeroMemory(&(perIOData->overlapped), sizeof(OVERLAPPED));

			//keep receiving data
			WSARecv(perHandleData->socket, &(perIOData->wsabuf),1, &numberOfByteTranffered, &flags,
				&(perIOData->overlapped), NULL);
	}
 }
ListClient ReadDataBase() {
	char buff[1024];
	char buff_user[556], buff_password[556];
	char n_char[3];
	int numberOfReadingClient = 0;

	FILE* f = fopen(DATABASE_FILE_PATH, "r");
	int n = (atoi)(fgets(n_char, sizeof(n_char), f));
	listClient.n = n;
	
	ListClient listClient = CreateListClient(n);
	while (fgets(buff, sizeof(buff), f) != NULL) {
		sscanf(buff, "%s %s", buff_user, buff_password);
		strcpy((listClient.list[numberOfReadingClient]).user,buff_user);
		strcpy(listClient.list[numberOfReadingClient].password, buff_password);
		numberOfReadingClient++;
	};
	fclose(f);
	return listClient;
}
void showListClient(ListClient listClient) {
	printf("=======Show List Client=======\n");
	for (int i = 0; i < listClient.n; i++) {
		printf("%s : %s\n", listClient.list[i].user, listClient.list[i].password);
	}
}
ListClient CreateListClient(int n) {
	ListClient listClient;
	int numberOfClientForProgram = n + PADDING_NUMBER_OF_CLIENT;
	listClient.n = n;
	listClient.list = new Client[numberOfClientForProgram];
	return listClient;
}
void DeleteListClient(ListClient listClient) {
	delete[] listClient.list;
}
//return index of client in listClient if right
int CheckLoginInfo(char user[512],char password[512]) {
	for (int i = 0; i < listClient.n; i++) {
		if ((strcmp(listClient.list[i].user, user) == 0)
			&& (strcmp(listClient.list[i].password, password) == 0))
			return i;
	}
	return -1;
}
int FindIndexOfClient(char user[512]) {
	for (int i = 0; i < listClient.n; i++) {
		if (strcmp(listClient.list[i].user, user) == 0) return i;
	}
	return -1;
}

//TODO: fix 1024
//**TODO: IP to client object
//TODO: how to close a IOCompletionPort when a client log out
