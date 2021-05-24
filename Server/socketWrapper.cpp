#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <bits/stdc++.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

//C++ abstract wrapper for C sockets
//creates a server or client object
//optionally can create an SSL connetion
namespace Socket
{
    class baseSocket
    {
        protected:
            int Socket;
            struct sockaddr_in serverAddress;
            SSL_CTX *ctx;
            SSL *ssl;

            void init_openssl()
            { 
                SSL_load_error_strings();	
                OpenSSL_add_ssl_algorithms();
            }

        public:
            int setTimeOut(int seconds, int microSeconds)
            {
                struct timeval tv;
                tv.tv_sec = seconds;
	            tv.tv_usec = microSeconds;
                setsockopt(Socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
		        setsockopt(Socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            }

            void SSL_close()
            {
                SSL_shutdown(ssl);
                close(Socket);
                //SSL_free(ssl);
            }

            void cleanup_openssl() { EVP_cleanup(); }
            int getSocket() { return Socket; }
            SSL *getSSL() { return ssl; }
            void closeConnection() { close(Socket); }
    };

    class Server : public baseSocket
    {
        private:
            int serverSocket;

            void create_context()
            {
                const SSL_METHOD *method;
                method = SSLv23_server_method();

                ctx = SSL_CTX_new(method);
                if (!ctx) {
                std::cout << "ssl error 3" << std::endl;
                exit(EXIT_FAILURE);
                }
            }
            void configure_context(char cert[], char key[])
            {
                SSL_CTX_set_ecdh_auto(ctx, 1);
                if (SSL_CTX_load_verify_locations(ctx,"/mnt/c/Users/beast/OneDrive/Documents/myCA/cacert.pem", NULL) != 1)
                {
                    std::cout << "ssl error 1" << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
                    std::cout << "ssl error 2" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
                    std::cout << "ssl error 3" << std::endl;
                    exit(EXIT_FAILURE);
                }            
                if (SSL_CTX_check_private_key(ctx) <= 0 ) {
                    std::cout << "ssl error 8" << std::endl;
                    exit(EXIT_FAILURE);
                }
                SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
                SSL_CTX_set_verify_depth(ctx, 4);
            }
            int check_certificate_validaty()
            {
                int status;
                X509_STORE_CTX *ctx = X509_STORE_CTX_new();
                X509 *certificate = SSL_get_peer_certificate(ssl);
                X509_STORE *store = X509_STORE_new();

                X509_STORE_add_cert(store, certificate);
                X509_STORE_CTX_init(ctx, store, certificate, NULL);

                if(X509_verify_cert(ctx) < 0)
                    return -1;

                return 0;
            }

        public:
            Server(const int &port)
            {
                serverSocket = socket(AF_INET, SOCK_STREAM, 0);
                if(serverSocket < 0)
                {
                    std::cout << "failed to create socket" << std::endl;
                    exit(-1);
                }
                bzero((char *) &serverAddress, sizeof(serverAddress));
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_port = htons(port);
                serverAddress.sin_addr.s_addr = INADDR_ANY;

                if(bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
                {
                    std::cout << "Server failed to bind address" << std::endl;
                    close(serverSocket);
                    exit(-1);
                }
                if(listen(serverSocket, 3) < 0)
                {
                    std::cout << "Server failed to listen" << std::endl;
                    close(serverSocket);
                    exit(-1);
                }
            }
            int acceptConnection()
            {   
	            Socket = accept(serverSocket, NULL, NULL);
		        if(Socket < 0)
		        {
			        std::cout << "Server did not find client" << std::endl;
                    close(Socket);
			        return -1;
		        }
                return 0;
            }

            int SSL_acceptConnection()
            {
                int code;
                Socket = accept(serverSocket, NULL, NULL);
		        if(Socket < 0)
		        {
			        std::cout << "Server did not find client" << std::endl;
                    close(Socket);
			        return -1;
		        }
                ssl = SSL_new(ctx);
                SSL_set_fd(ssl, Socket);
                if((code = SSL_accept(ssl)) < 0)
                {
                    std::cout <<ERR_error_string(ERR_get_error(), NULL)<< std::endl;
                    return -1;
                }
                if(check_certificate_validaty() < 0)
                {
                    std::cout << "ERROR: cert not verified" << std::endl;
                    return -1;
                }
                return 0;
            }

            void startSSL(char cert[], char key[])
            {
                init_openssl();
                create_context();
                configure_context(cert, key);
            }
    };
    
    class Client : public baseSocket
    {
        private:
	        struct hostent *server;

            void create_context()
            {
                const SSL_METHOD *method;
                method = SSLv23_client_method();

                ctx = SSL_CTX_new(method);
                if (!ctx) {
                std::cout << "ssl error 4" << std::endl;
                exit(EXIT_FAILURE);
                }
            }
            void configure_context(char cert[], char key[])
            {
                SSL_CTX_set_ecdh_auto(ctx, 1);
                if (SSL_CTX_load_verify_locations(ctx,"/mnt/c/Users/beast/OneDrive/Documents/myCA/cacert.pem", NULL) != 1)
                {
                    std::cout << "ssl error 1" << std::endl;
                    exit(EXIT_FAILURE);
                }

                if (SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM) <= 0) {
                    std::cout << "ssl error 5" << std::endl;
                exit(EXIT_FAILURE);
                }

                if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0 ) {
                    std::cout << "ssl error 6" << std::endl;
                exit(EXIT_FAILURE);
                }
                //SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
            }

        public:
            Client(const char address[], const int &portno)
            {
                Socket = socket(AF_INET, SOCK_STREAM, 0);
                server = gethostbyname(address);
                bzero((char *) &serverAddress, sizeof(serverAddress));
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_port = htons(portno);
            }
            int connectToServer()
            {             
	            if(connect(Socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
	            {
		            std::cout << "Client did not connect to server" << std::endl;
		            return -1;
                }
                return 0;
            }

            int SSL_connectToServer()
            {
                if(connect(Socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
	            {
		            std::cout << "Client did not connect to server" << std::endl;
		            return -1;
                }
                ssl = SSL_new(ctx);
                SSL_set_fd(ssl, Socket);
                if(SSL_connect(ssl) < 0)
                {
                    std::cout << " ERROR: SSL connect" << std::endl;
                    return -1;
                }
                return 0;
            }

            void startSSL(char cert[], char key[])
            {
                init_openssl();
                create_context();
                configure_context(cert, key);
            }
    };   
}