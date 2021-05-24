#include <iostream>
#include <vector>
#include <SSLFileSystem.cpp>
#include <socketWrapper.cpp>

using namespace Socket;
using namespace SSLFileSystem;

int main()
{
	int status;
	std::vector<std::string> clientRequest;
	char cert[64] = {"/myCA/server_crt.pem"};
	char key[64] = {"/myCA/server_key.pem"};

	Server fileServer(17285);
	fileServer.startSSL(cert, key);
	std::cout << "\nServer Online" << std::endl;
	signal (SIGPIPE, SIG_IGN);
	for(;;)
	{
		status = 1;

		if(fileServer.SSL_acceptConnection() < 0)
		{
			fileServer.SSL_close();
			continue;
		}
		setSSL(fileServer);
		fileServer.setTimeOut(10, 0);

		clientRequest = recvMessage(&status);
        if (status < 0)
		{	
			std::cout << "ERROR: did not get message\nclosing connection" << std::endl;
			fileServer.SSL_close();
            continue;
		}

		if (clientRequest.at(0) == "requestFile")
			status = sendFile(clientRequest.at(1));
		else if(clientRequest.at(0) == "storeFile")
			status = storeFile(clientRequest.at(1));
		else if(clientRequest.at(0) == "showDirectory")
			status = showDirectory(clientRequest.at(1));
		else if(clientRequest.at(0) == "copyDirectory")
			status = copyDirectory(clientRequest.at(1));
		else if(clientRequest.at(0) == "recvDirectory")
			status = recvDirectory(clientRequest.at(1));
		
		switch (status)
		{
			case 0: sendMessage("request complete");
				break;
			case 1: sendMessage("ERROR: Invalid request");
				break;
			//default: sendMessage("ERROR: request not completed");
				//break;
		}

		fileServer.SSL_close();
		std::cout << "connection terminated" << std::endl;
	}
	fileServer.cleanup_openssl();
}
