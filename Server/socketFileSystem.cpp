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

// A basic file system made for utillizing sockets
//non-SSL version
namespace socketFileSystem
{
    //max amount of digits allowed for size of file/message
    //ex. if length is 3, then max amount of characters is 999
    #define FILE_MAX_LENGTH 10
    #define MESSAGE_MAX_LENGTH 3
    int SOCKET;

    //sets the socket for the namespace
    // must be declared every time a new socket file descriptor is used
    int setSOCKET(Socket::baseSocket object) { SOCKET = object.getSocket(); }

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
                   
        if(write(SOCKET, messagelength.c_str(), MESSAGE_MAX_LENGTH) < 0)
        {
            std::cout << "ERROR SENDMESSAGE: " << errno << std::endl;
            return;
        }
        if(write(SOCKET, message.c_str(), message.size()) < 0)
        {
            std::cout << "ERROR SENDMESSAGE: " << errno << std::endl;
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

        bytesRead = read(SOCKET, buffer.data(), 3);
        if(bytesRead < 0)
        {
            std::cout << "ERROR1 RECVMESSAGE: " << errno << std::endl;
            *error = -1;
        }
        messageSize = atoi(buffer.data());
        buffer.clear();

        while (messageSize > 0)
        {
            bytesRead = read(SOCKET, buffer.data(), messageSize);
            if(bytesRead < 0)
            {
                 std::cout << "ERROR2 RECVMESSAGE: " << errno << std::endl;
                *error = -1;
            }
            message += buffer.data();
            messageSize -= bytesRead;
        }
        std::istringstream ss(message);
        std::string token;
        while(std::getline(ss, token, ','))
            data.push_back(token);
        
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
            std::cout << "ERROR SHOWDIRECTORY: " << errno << std::endl;
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
        char buffer[1024];
        int fileSize, bytesWritten, sizeLength;
        std::string size;
        off_t offset = 0;

        strcpy(buffer, filepath.c_str());
        int file = open(buffer, O_RDONLY);
        if (file < 0)
        {		
                std::cout << "ERROR SENDFILE: " << errno << std::endl;
                return -1;
        }
        if (fstat(file, &fileStat) < 0)
        {
            std::cout << "ERROR SENDFILE: " << errno << std::endl;
            return -1;
        }	
        std::cout << "Sending file...." << std::endl;

        fileSize = fileStat.st_size;
        size = std::to_string(fileSize);
        sizeLength = size.length();
        if(sizeLength < 10 && sizeLength > 0)
            for (int i = sizeLength; i <= 10; i++)
                size += "x";
        write(SOCKET, size.c_str(), 10);

        while (fileSize > 0) 
        {
            bytesWritten = sendfile(SOCKET, file, &offset, fileSize);
            if(bytesWritten < 0)
            {
                std::cout << "ERROR SENDFILE: " << errno << std::endl;
                return -1;
            }
            fileSize -= bytesWritten;
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

        if (read(SOCKET, buffer, FILE_MAX_LENGTH) < 0) 
        {
            std::cout << "ERROR STOREFILE1: " << errno << std::endl;
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
                std::cout << "ERROR STOREFILE3: " << errno << std::endl;
                return -1;
        }
        bzero(buffer,32000);
        std::cout << "storing file..." << std::endl;
        while (fileSize > 0)
        {	
            if(fileSize < 32000)
                bytesRead = read(SOCKET, buffer, fileSize);
            else
                bytesRead = read(SOCKET, buffer, 31999);
            
            if (bytesRead < 0) 
                {
                    std::cout << "ERROR STOREFILE4: " << errno << std::endl;
                    return -1;
                }
            fileSize -= bytesRead;

            while (bytesRead > 0)
            {
                int bytesWritten = write(file, buffer, bytesRead);
                if (bytesWritten < 0) 
                {
                    std::cout << "ERROR STOREFILE5: " << errno << std::endl;
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
            std::cout << "ERROR DELETEFILE: " << errno << std::endl;
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
                std::cout << "ERROR RECVDIRECTORY: " << errno << std::endl;
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
                std::cout << "ERROR COPYDIRECTORY: cannot open directory" << std::endl;
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
            std::cout << "ERROR 1 DELETEDIRECTORY: " << errno << std::endl;
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
                    std::cout << "ERROR 2 DELETEDIRECTORY: " << errno << std::endl;
                    return -1;
                }
            }          
        }
        closedir(dp);
        if(rmdir(path.c_str()) < 0)
        {
            std::cout << "ERROR 3 DELETEDIRECTORY: " << errno << std::endl;
            return -1;
        }
        return 0;
    }
}