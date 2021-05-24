#include <iostream>
#include <vector>
#include <SSLFileSystem.cpp>
#include <socketWrapper.cpp>


//using namespace socketFileSystem;
using namespace Socket;
using namespace SSLFileSystem;

int main()
{
	char cert[64] = {"/myCA/client_crt.pem"};
	char key[64] = {"/myCA/client_key.pem"};
    std::string message = "";
	int status;
	std::vector<std::string> data = {"folder/", "copyDirectory",
	 "copied_Folder"};
	//std::vector<std::string> data = {"test.txt", "storeFile", "newfile.txt"};
	
	Client client("localhost", 17285);
	client.startSSL(cert, key);
	client.SSL_connectToServer();
	setSSL(client);
	client.setTimeOut(10,0);

    message = data.at(1) + "," + data.at(2);
	sendMessage(message);
	std::cout << "message sent" << std::endl;
	
	if(recvDirectory(data.at(0)) == 0)
	{
		//data = recvMessage(&status);
		//std::cout << data.at(0) << std::endl;
	}
    return 0;
}