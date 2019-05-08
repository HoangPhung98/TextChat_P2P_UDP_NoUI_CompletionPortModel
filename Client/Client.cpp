
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#include <WS2tcpip.h>

#define SIZE_OF_BUFF 1024
#define SIZE_OF_BUFF_FROM_SERVER 128
#define ACCEPT "ACCEPT"
#define ACCEPT_FROM "ACCEPT_FROM"
#define DECLINE "DECLINE"
#define DECLINE_FROM "DECLINE_FROM"
#define PORT_CLIENT_A 8000
#define PORT_CLIENT_B 8500
#define PORT_SERVER 9000

DWORD WINAPI ReceiverFromServerThread(LPVOID);
DWORD WINAPI ReceiverFromFriendThread(LPVOID);
DWORD WINAPI ReadKeyBoardThread(LPVOID);
void callUDP_P2P(char[]);


WSADATA wsa;
SOCKET p2p, client_to_server;
SOCKADDR_IN friendaddr, myaddr, serveraddr;
char friendUserName[32];
bool isCallingToFriend = false;

int main()
{
	WSAStartup(MAKEWORD(2, 2), &wsa);
	p2p = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	client_to_server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);


	//sockaddr for socket that listen from client friend
	myaddr.sin_addr.s_addr = htonl(ADDR_ANY);
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(PORT_CLIENT_A);


	//sockaddr for socket to send msg to friend
	friendaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	friendaddr.sin_family = AF_INET;
	friendaddr.sin_port = htons(PORT_CLIENT_B);

	//sockaddr for socket that listen from server
	serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(PORT_SERVER);
	connect(client_to_server, (SOCKADDR*)& serveraddr, sizeof(SOCKADDR_IN));

	CreateThread(0, 0, ReceiverFromServerThread, 0, 0, 0);
	CreateThread(0, 0, ReadKeyBoardThread, 0, 0, 0);

	char buff_keyboard[SIZE_OF_BUFF];
	while (true) {
		//main thread to record sound then send to friend
		if (isCallingToFriend) {
			gets_s(buff_keyboard, sizeof(buff_keyboard));
			sendto(p2p, buff_keyboard, strlen(buff_keyboard), 0, (SOCKADDR*)&friendaddr, sizeof(friendaddr));
		}
	}

	closesocket(client_to_server);
	WSACleanup();
}

DWORD WINAPI ReceiverFromServerThread(LPVOID lpParam) {
	//***new thread to recv data from server
	int ret;
	char buff_RESPONSE[32], buff_arg1[32], buff_arg2[32];
	char buff[SIZE_OF_BUFF_FROM_SERVER];

	while (true) {
		ret = recv(client_to_server, buff, sizeof(buff), 0);
		if (ret < 0) continue;
		else if (ret < 1024) {
			buff[ret] = 0;
			//form of buff: ACCEPT_FROM 127.0.0.1 B
			sscanf(buff,"%s %s %s", buff_RESPONSE, buff_arg1, buff_arg2);
			if (strcmp(buff_RESPONSE, ACCEPT_FROM) == 0) {
				callUDP_P2P(buff_arg2);
			}
			else if (strcmp(buff_RESPONSE, DECLINE_FROM) == 0) {

			}
			
		}
		printf("%s\n", buff);
	}
}
DWORD WINAPI ReceiverFromFriendThread(LPVOID lpParam) {
	SOCKET p2p = *(SOCKET*)lpParam;
	int ret;
	char buff[SIZE_OF_BUFF];
	while (true) {
		ret = recvfrom(p2p, buff, sizeof(buff), 0, NULL, NULL);
		if (ret < 0)continue;
		if (ret < SIZE_OF_BUFF) {
			buff[ret] = 0;
			printf("recv from [ %s ] : %s\n", friendUserName, buff);
		}
	}
}
DWORD WINAPI ReadKeyBoardThread(LPVOID lpParam) {
	char buff_keyboard[1024];
	char type[32];
	char arg1[32], arg2[32], tmp[10];
	int n;

	while (!isCallingToFriend) {
		gets_s(buff_keyboard, sizeof(buff_keyboard));
		n = sscanf(buff_keyboard, "%s %s %s %s", type, arg1, arg2, tmp);

		if (strcmp(type, ACCEPT) == 0) {
			//form: ACCEPT a b
			callUDP_P2P(arg1);
		}
		else if (strcmp(DECLINE, type) == 0) {

		}
		send(client_to_server, buff_keyboard, strlen(buff_keyboard), 0);
	}
	return 0;
}

void callUDP_P2P(char friendName[32]) {
	strcpy(friendUserName, friendName);
	isCallingToFriend = true;
	//create listener from client friend
	bind(p2p, (SOCKADDR*)& myaddr, sizeof(SOCKADDR));
	CreateThread(0, 0, ReceiverFromFriendThread, &p2p, 0, 0);
}

