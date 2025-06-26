#include <Server.h>

// TestTftpLib.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

#include <iostream>
#include "DebugManager.h"
#include "UdpSocketWindows.h"
#include "PoolOfBuffers.h"
#include "DatagramFactory.h"
#include "tftp_messages.h"
#include <Winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <string>
#include <Mswsock.h>
#include <tchar.h>
#include <combaseapi.h>

#include <thread>
#include <chrono>
#include "Signal.h"


int strLenS(const char* str, uint16_t maxSz) {
	int sz = 0;
	while (*str++ && maxSz--) {
		sz++;
	}

	if (*(str-1)) return -1;
	return sz;
}

int main()
{
	DebugManager::Init();

	if(0)
	{
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			// Handle error
		}

		DWORD bufSize = 0;
		int r = WSAEnumProtocols(nullptr, nullptr, &bufSize);
		if (r == SOCKET_ERROR && WSAGetLastError() == WSAENOBUFS) {
			std::cout << "boop enobufs" << std::endl;
		}

		char* buf = new char[bufSize];
		WSAPROTOCOL_INFO* arr = (WSAPROTOCOL_INFO*)buf;
		r = WSAEnumProtocols(nullptr, arr, &bufSize);

		std::cout << "Proto count " << r << std::endl;
		WCHAR GuidString[40] = { 0 };

		for (int i = 0; i < r; i++) {
			if (arr[i].dwServiceFlags1 & XP1_CONNECTIONLESS
				&& arr[i].iProtocol == IPPROTO_UDP 
				&& arr[i].iAddressFamily == AF_INET) {
				StringFromGUID2(arr[i].ProviderId,
					(LPOLESTR)&GuidString, 39);

				std::wcout << L"Provider : " << GuidString << std::endl;
				std::wcout << arr[i].szProtocol << std::endl;
			}
		}

		return 0;
	}


    tftplib::Server server;

    server.SetRootDirectory("C:\\tftproot")
        .SetOutStream(&std::cout)
        .SetErrStream(&std::cerr);

	server.Out() << "Starting TFTP server..." << std::endl;
    
	server.Start();


    std::cin.ignore();

}

// Exécuter le programme : Ctrl+F5 ou menu Déboguer > Exécuter sans débogage
// Déboguer le programme : F5 ou menu Déboguer > Démarrer le débogage

// Astuces pour bien démarrer : 
//   1. Utilisez la fenêtre Explorateur de solutions pour ajouter des fichiers et les gérer.
//   2. Utilisez la fenêtre Team Explorer pour vous connecter au contrôle de code source.
//   3. Utilisez la fenêtre Sortie pour voir la sortie de la génération et d'autres messages.
//   4. Utilisez la fenêtre Liste d'erreurs pour voir les erreurs.
//   5. Accédez à Projet > Ajouter un nouvel élément pour créer des fichiers de code, ou à Projet > Ajouter un élément existant pour ajouter des fichiers de code existants au projet.
//   6. Pour rouvrir ce projet plus tard, accédez à Fichier > Ouvrir > Projet et sélectionnez le fichier .sln.
