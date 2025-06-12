#include <Server.h>

// TestTftpLib.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

#include <iostream>
#include "DebugManager.h"
#include "UdpSocketWindows.h"
#include "PoolOfBuffers.h"
#include "DatagramFactory.h"

int main()
{
	DebugManager::Init();

    tftplib::Server server;

    server.SetRootDirectory("C:\\tftproot")
        .SetOutStream(&std::cout)
        .SetErrStream(&std::cerr);

	server.Out() << "Starting TFTP server..." << std::endl;

    server.Start();

    tftplib::UdpSocketWindows socket{};
	socket.SetOutStream(&std::cout)
		.SetErrStream(&std::cerr);

	socket.Bind("0.0.0.0", 10099);

    std::cin.ignore();

	std::cout << socket.HasDatagram() << std::endl;
    if (socket.HasDatagram()) {
		auto factory = tftplib::DatagramFactory::Instantiate();
		auto datagram = socket.Receive(*factory);

		if (datagram == nullptr) 
		{
			server.Out() << "Datagram is null" << std::endl;
		}
		else 
		{
			std::cout 
				<< "from " << datagram->GetSourceAddress() << ":" << datagram->GetSourcePort()
				<< " to " << datagram->GetDestAddress() << ":" << datagram->GetDestPort()
				<< " with size " << datagram->GetDataSize() 
				<< std::endl;
		}
	}

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
