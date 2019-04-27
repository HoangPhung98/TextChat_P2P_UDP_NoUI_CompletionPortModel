
#include "pch.h"
#include <iostream>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")


typedef struct _PER_IO_DATA {
	OVERLAPPED overlapped;
	WSABUF wsabuf;
	char buff[1024];
}PER_IO_DATA, *LPPER_IO_DATA;

typedef struct _PER_HANDLE_DATA {
	SOCKET socket;
	SOCKADDR_STORAGE socketaddr;
}PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

DWORD WINAPI WorkerThread(LPVOID);

int main()
{
	WSADATA wsa;
	SOCKET thisServer;
	SOCKADDR_IN thisaddr;
	
	HANDLE completionPort;
	SYSTEM_INFO systemInfo;

	LPPER_HANDLE_DATA perHandleData = NULL;
	LPPER_IO_DATA perIOData = NULL;
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
		SOCKET client;

		//accept new client, 
		client = accept(thisServer, (SOCKADDR *)&clientaddr, &lengthaddr);
		printf("accept new client \n");

		//and attach it to Completion Port
		perHandleData = (LPPER_HANDLE_DATA)GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA));
		perHandleData->socket = client;
		memcpy(&perHandleData->socketaddr, &clientaddr, lengthaddr);

		CreateIoCompletionPort((HANDLE)client, completionPort, (DWORD)perHandleData, 0);

		//create IO data structure to store data
		perIOData = (LPPER_IO_DATA)GlobalAlloc(GPTR, sizeof(PER_IO_DATA));
		perIOData->wsabuf.buf = perIOData->buff;
		perIOData->wsabuf.len = 1024;

		//recv data for the first time
		bytes = 0;
		flags = 0;
		WSARecv(perHandleData->socket, &(perIOData->wsabuf), 1, &bytes, &flags, &(perIOData->overlapped), NULL);
	}

	closesocket(thisServer);
	WSACleanup();
}
DWORD WINAPI WorkerThread(LPVOID lpparam) {
	printf("Created new thread\n");
	HANDLE completionPort = (HANDLE)lpparam;
	DWORD numberOfByteTranffered;
	DWORD flags = 0;
	LPPER_HANDLE_DATA perHandleData;
	LPPER_IO_DATA perIOData;

	while (true) {
		printf("threading\n");
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

			//clear overlapped structure
			ZeroMemory(&(perIOData->overlapped), sizeof(OVERLAPPED));

			//keep receiving data
			WSARecv(perHandleData->socket, &(perIOData->wsabuf),1, &numberOfByteTranffered, &flags,
				&(perIOData->overlapped), NULL);
	}
 }