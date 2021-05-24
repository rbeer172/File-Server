# File-Server
A simple TCP/SSL file server using primarily C for sockets and reading/writing files and C++ for strings under the Linux OS.
### Features
The program uses a message api to communicate between client and server. The message consists of a fixed number of bytes to specify the length of the message, followed by a comma, and then the data or command to be ran. The program can send and recieve single files or all files in a directory. Directories can be viewed to see what files exist in it. Files and directories can also be deleted or copied.  

SSL was later added to encrypt data being sent between client and server using a self-signed certificate authority generated using Ubuntu.

