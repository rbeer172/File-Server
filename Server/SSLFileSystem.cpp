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

namespace SSLFileSystem
{
    //max amount of digits allowed for size of file/message
    //ex. if length is 3, then max amount of characters for a message is 999
    #define FILE_MAX_LENGTH 10
    #define MESSAGE_MAX_LENGTH 3
    int SOCKET;
    SSL *ssl;

    //sets the socket for the namespace
    // must be declared every time a new socket file descriptor is used
    int setSSL(Socket::baseSocket object) { ssl = object.getSSL(); }

    // revisit this
    double storageSize()
    {
        FILE *fpipe;
        char command[20] = {"df -BG /mnt/c"};
        char c = 0;
        std::string data;
        std::vector<std::string> subData;

        if (0 == (fpipe = (FILE*)popen(command, "r")))
        {
            perror("popen() failed.");
            exit(EXIT_FAILURE);
        }

        while (fread(&c, sizeof c, 1, fpipe))
            data += c;

        std::istringstream ss(data);
        std::string token;
        while(std::getline(ss, token, ' ')) 
            subData.push_back(token);
        
        pclose(fpipe);
        return std::stod(subData.at(36)) * 1000000000.0;
    }

    //sends a message to a connecting socket
    //to be used with recvMessage()
    void sendMessage(std::string message)
    {
        std::string messagelength = std::to_string(message.length());
        int messageSize = message.size();

        if(messageSize < MESSAGE_MAX_LENGTH && messageSize > 0)
            for (int i = messageSize; i <= 3; i++)
                messagelength += "x";

        if(SSL_write(ssl, messagelength.c_str(), MESSAGE_MAX_LENGTH) < 0)
        {
            std::cout << "ERROR SENDMESSAGE1: " << strerror(errno) << std::endl;
            return;
        }
        if(SSL_write(ssl, message.c_str(), message.size()) < 0)
        {
            std::cout << "ERROR SENDMESSAGE2: " << strerror(errno) << std::endl;
            return;
        } 
    }

    //returns a message, in the form of a string vector, from a connecting socket
    //To be used with sendMessage()
    std::vector<std::string> recvMessage(int *error)
    {
        std::string message;
        int bytesRead, messageSize = 0;
        std::vector<char> buffer (256);
        std::vector<std::string> data;

        bytesRead = SSL_read(ssl, buffer.data(), 3);
        if(bytesRead < 0)
        {
            std::cout << "ERROR1 RECVMESSAGE1: " << strerror(errno) << std::endl;
            *error = -1;
        }
        messageSize = atoi(buffer.data());
        buffer.clear();

        while (messageSize > 0)
        {
            bytesRead = SSL_read(ssl, buffer.data(), messageSize);
            if(bytesRead < 0)
            {
                 std::cout << "ERROR2 RECVMESSAGE2: " << strerror(errno) << std::endl;
                *error = -1;
            }
            message += buffer.data();
            messageSize -= bytesRead;
        }
        std::istringstream ss(message);
        std::string token;
        while(std::getline(ss, token, ','))
            data.push_back(token);
        
        if(data.empty())
        {
            std::cout << "ERROR2 RECVMESSAGE3: message empty" << std::endl;
            *error = -1;
        }
        if(*error != -1)
            std::cout << " message received" << std::endl;
             
        return data;
    }

    // sends the file names in the directory to a connected socket
    int showDirectory(std::string &dir)
    {
        std::string filePath, data = "";
        DIR *dp;
        struct dirent *dirp;
        struct stat filestat;

        dp = opendir(dir.c_str());
        if (dp == NULL)
        {
            std::cout << "ERROR SHOWDIRECTORY: " << strerror(errno) << std::endl;
            return -1;
        }

        while ((dirp = readdir(dp)))
        {
            filePath = dir + "/" + dirp->d_name;
            if (stat( filePath.c_str(), &filestat )) continue;
            if (S_ISDIR( filestat.st_mode ))
            {   
                if (strcmp(dirp->d_name, ".") && strcmp(dirp->d_name, ".."))
                    data = data + "[" + dirp->d_name + "]" + ",";

                continue;
            }
            else
            data = data + dirp->d_name + "|" + std::to_string(filestat.st_size) + ",";
        }
        closedir(dp);
        sendMessage(data);
        return 0;
    }

    // sends a single file to a connected socket
    int sendFile(std::string &filepath)
    {
        struct stat fileStat;	
        char buffer[32000];
        int fileSize, bytesWritten, bytesRead, sizeLength;
        std::string size;
        off_t offset = 0;

        strcpy(buffer, filepath.c_str());
        int file = open(buffer, O_RDONLY);
        if (file < 0)
        {		
                std::cout << "ERROR SENDFILE1: " << strerror(errno) << std::endl;
                return -1;
        }
        if (fstat(file, &fileStat) < 0)
        {
            std::cout << "ERROR SENDFILE2: " << strerror(errno) << std::endl;
            return -1;
        }	
        std::cout << "Sending file...." << std::endl;

        fileSize = fileStat.st_size;
        size = std::to_string(fileSize);
        sizeLength = size.length();
        if(sizeLength < FILE_MAX_LENGTH && sizeLength > 0)
            for (int i = sizeLength; i <= FILE_MAX_LENGTH; i++)
                size += "x";
        SSL_write(ssl, size.c_str(), FILE_MAX_LENGTH);

        while (fileSize > 0) 
        {
            if(fileSize < sizeof(buffer))
                bytesRead = read(file, buffer, fileSize);
            else
                bytesRead = read(file, buffer, sizeof(buffer));
            if(bytesRead < 0)
                {
                    std::cout << "ERROR SENDFILE3: " << strerror(errno) << std::endl;
                    return -1;
                }
            fileSize -= bytesRead;
            while(bytesRead > 0)
            {
                bytesWritten = SSL_write(ssl, buffer, bytesRead);
                if(bytesWritten < 0)
                {
                    std::cout << "ERROR SENDFILE4: " << strerror(errno) << std::endl;
                    return -1;
                }
                bytesRead -= bytesWritten;
            }
        }
        close(file);
        std::cout << "done"<< std::endl;
        return 0;
    }

    // receives a file from a connected socket
    // returns -1 if error occurs
    int storeFile(std::string &filepath)
    {
        char buffer[32000];
        int fileSize, bytesRead;

        if (SSL_read(ssl, buffer, FILE_MAX_LENGTH) < 0) 
        {
            std::cout << "ERROR STOREFILE1: " << strerror(errno) << std::endl;
            return -1;
        }
        fileSize = atoi(buffer);
        bzero(buffer,32000);
        if (fileSize > storageSize())
        {
            std::cout << "ERROR STOREFILE2: no space available" << std::endl;
            return -1;
        }
        strcpy(buffer, filepath.c_str());
        int file = open(buffer, O_CREAT | O_WRONLY, 00700);
        if (file < 0)
        {		
                std::cout << "ERROR STOREFILE3: " << strerror(errno) << std::endl;
                return -1;
        }
        bzero(buffer,32000);
        std::cout << "storing file..." << std::endl;
        while (fileSize > 0)
        {	
            if(fileSize < 32000)
                bytesRead = SSL_read(ssl, buffer, fileSize);
            else
                bytesRead = SSL_read(ssl, buffer, 31999);
            
            if (bytesRead < 0) 
                {
                    std::cout << "ERROR STOREFILE4: " << strerror(errno) << std::endl;
                    return -1;
                }
            fileSize -= bytesRead;

            while (bytesRead > 0)
            {
                int bytesWritten = write(file, buffer, bytesRead);
                if (bytesWritten < 0) 
                {
                    std::cout << "ERROR STOREFILE5: " << strerror(errno) << std::endl;
                    return -1;
                }
                    bytesRead -= bytesWritten;
            }
            bzero(buffer,32000);
        }
        close(file);
        std::cout << "done" << std::endl;
        return 0;
    }

    // removes a file from given path
    //returns -1 if error occurs
    int deleteFile(std::string &path)
    {
        if(unlink(path.c_str()) < 0)
        {
            std::cout << "ERROR DELETEFILE: " << strerror(errno) << std::endl;
            return -1;
        }

        return 0;
    }

    // creates a directory
    // returns -1 if error occurs
    int makeDirectory(std::string &directory)
    {
        if(mkdir(directory.c_str(), 0777) < 0)
            {
                std::cout << "ERROR MAKEDIRECTORY: " << strerror(errno) << std::endl;
                return -1;
            }
        return 0;
    }

    //Copies the files in the directory, including subdirectories, to the socket
    //to be used with recvDirectory()
    //returns -1 if error occurs
    int copyDirectory(std::string &path)
    {
        DIR *dp;
        struct dirent *dirp;
        struct stat filestat;
        std::string filePath, directory, basePath;
        std::stack<std::string> directoryStack;
        std::vector<std::string> data;

        std::istringstream ss(path);
        std::string token;
        while(std::getline(ss, token, '/'))
            data.push_back(token);
        for (int i = 0; i < data.size() - 1; i++)
            basePath = basePath + data.at(i) + "/";
        directory = data.at(data.size() - 1);

        directoryStack.push(directory);

        while (!directoryStack.empty())
        {
            directory = directoryStack.top();
            directoryStack.pop();
            dp = opendir((basePath + directory).c_str());
            if (dp == NULL)
            {
                std::cout << "ERROR COPYDIRECTORY: " << strerror(errno) << std::endl;
                return -1;
            }
            sendMessage("directory," + directory);

            while ((dirp = readdir(dp)))
            {
                filePath = directory + "/" + dirp->d_name;
                if (stat((basePath + filePath).c_str(), &filestat)) continue;
                if (S_ISDIR( filestat.st_mode ))
                {   
                    if (strcmp(dirp->d_name, ".") && strcmp(dirp->d_name, ".."))
                        directoryStack.push(filePath);

                    continue;
                }
                else
                {
                    token = "file," + filePath;
                    sendMessage(token);
                    token = basePath + filePath;
                    if(sendFile(token) < 0)
                        return -1;
                }
            }
            closedir(dp);
        }
        sendMessage("done");
        return 0;
    }

    // receives all files in a directory, including any sub-directories
    // to be used with copyDirectory()
    //returns -1 if error occurs
    int recvDirectory(std::string &path)
    {
        int status = 0;
        std::string absolutePath;
        std::vector<std::string> request;

        while (1)
        {   
            request = recvMessage(&status);
            if (status < 0)
                return -1;

            if (request.at(0) == "done")
            {
                std::cout << "Done reading" << std::endl;
                return 0;
            } 
            absolutePath = path + request.at(1);
 
            if (request.at(0) == "directory")
                status = makeDirectory(absolutePath);

            else if (request.at(0) == "file")
               status = storeFile(absolutePath);

            if (status < 0)
               return -1;

            request.clear();   
        }
    }

    // removes all contents within the directory and the directory itself, including subdirectories
    // returns -1 if error occurs
    int deleteDirectory(std::string &path)
    {
        std::string filePath;
        DIR *dp;
        struct dirent *dirp;
        struct stat filestat;

        dp = opendir(path.c_str());
        if (dp == NULL)
        {
            std::cout << "ERROR 1 DELETEDIRECTORY: " << strerror(errno) << std::endl;
            return -1;
        }

        while ((dirp = readdir(dp)))
        {
            filePath = path + "/" + dirp->d_name;
            if (stat( filePath.c_str(), &filestat )) continue;
            if (S_ISDIR( filestat.st_mode ))
            {   
                if (strcmp(dirp->d_name, ".") && strcmp(dirp->d_name, ".."))
                    deleteDirectory(filePath);

                continue;
            }
            else 
            {
                if(unlink(filePath.c_str()) < 0)
                {
                    std::cout << "ERROR 2 DELETEDIRECTORY: " << strerror(errno) << std::endl;
                    return -1;
                }
            }          
        }
        closedir(dp);
        if(rmdir(path.c_str()) < 0)
        {
            std::cout << "ERROR 3 DELETEDIRECTORY: " << strerror(errno) << std::endl;
            return -1;
        }
        return 0;
    }
}