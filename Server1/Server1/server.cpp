#pragma once
#pragma comment (lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS 1 
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <winsock2.h>
#include <iostream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>
#include <ws2tcpip.h>
#include "Thread.h"
#include "server.h"
#include <map>
#include <fstream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <regex>

//int sockets[2];
using namespace std;
int k = 0;

std::map<string, int> sockets;
//socket number


TcpServer::TcpServer()
{
	WSADATA wsadata;
	if (WSAStartup(0x0202, &wsadata) != 0)
		TcpThread::err_sys("Starting WSAStartup() error\n");

	//Display name of local host
	if (gethostname(servername, HOSTNAME_LENGTH) != 0) //get the hostname
		TcpThread::err_sys("Get the host name error,exit");

	printf("Server: %s waiting to be contacted for mail transfer...\n", servername);


	//Create the server socket
	if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		TcpThread::err_sys("Create socket error,exit");

	//Fill-in Server Port and Address info.
	//inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4

	ServerPort = REQUEST_PORT;
	memset(&ServerAddr, 0, sizeof(ServerAddr));      /* Zero out structure */
	ServerAddr.sin_family = AF_INET;                 /* Internet address family */
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* Any incoming interface */
	ServerAddr.sin_port = htons(ServerPort);         /* Local port */
	//inet_pton(AF_INET, "10.10.167.74", &(ServerAddr.sin_addr));//change this to server ip

	//Bind the server socket
	if (bind(serverSock, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) < 0)
		TcpThread::err_sys("Bind socket error,exit");

	//Successfull bind, now listen for Server requests.
	if (listen(serverSock, MAXPENDING) < 0)
		TcpThread::err_sys("Listen socket error,exit");
}

TcpServer::~TcpServer()
{
	WSACleanup();
}


void TcpServer::start()
{
	for (;;) /* Run forever */
	{
		/* Set the size of the result-value parameter */
		clientLen = sizeof(ClientAddr);

		/* Wait for a Server to connect */
		if ((clientSock = accept(serverSock, (struct sockaddr*)&ClientAddr,
			&clientLen)) < 0)
			TcpThread::err_sys("Accept Failed ,exit");

		int n, rbytes;

		//receive client mode and client name using the IDENTITY struct
		IDENTITY* id = new IDENTITY;
		for (rbytes = 0; rbytes < sizeof(IDENTITY); rbytes += n)
			if ((n = recv(clientSock, (char*)id, sizeof(IDENTITY), 0)) <= 0)
				TcpThread::err_sys("Recv Mode Error");
		printf("%d : ", n);
		cout << id->hostname << ", mode : " << id->mode << endl;
		//add the client socket number and name in the map
		//pair<char[HOSTNAME_LENGTH], char> pair (id->hostname, id->mode);
		sockets.insert(pair<string, int>(id->hostname, clientSock)); ///socket number
		cout << sockets[id->hostname] << endl; 
		//receive the client mode and start the required thread
		Thread* pt;
		switch (id->mode) {
		case 'R':
			pt = new TcpThreadReceiver(clientSock);
			//pt->start();
			break;
		default:
			pt = new TcpThread(clientSock);
			//pt->start();
			break;
		}
		pt->start();
	}
}

//////////////////////////////TcpThread Class (Sender) //////////////////////////////////////////
void TcpThread::err_sys(const char* fmt, ...)
{
	perror(NULL);
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

int TcpThread::msg_confirmation_send(int sock, Resp *respp)//to sender //this needs fixing
{
	int n;
	if ((n = send(sock, (char*)respp, sizeof(Resp), 0)) != (sizeof(Resp))) {
		cout << n;
		err_sys("Send confirmation Error");
	}
	return (n);
}

unsigned long TcpThread::ResolveName(char name[])
{
	struct hostent* host;            /* Structure containing host information */

	if ((host = gethostbyname(name)) == NULL)
		err_sys("gethostbyname() failed");

	/* Return the binary, network byte ordered address */
	return *((unsigned long*)host->h_addr_list[0]);
}

/*
msg_recv returns the length of bytes in the msg_ptr->buffer,which have been recevied successfully.
*/
//changes here
int TcpThread::msg_recv(int sock, HEADER* msg_ptr)
{
	int rbytes, n, count = 0;

	for (rbytes = 0; rbytes < sizeof(HEADER); rbytes += n) {
		if ((n = recv(sock, (char*)msg_ptr + rbytes, sizeof(HEADER), 0)) <= 0)
			err_sys("Recv HEADER Error");
		count += n;
	}

	return count;
}
int TcpThread::msg_recv(int sock, MESSAGEBODY* msg_ptr, int size)
{
	printf("inside\n");
	int rbytes, n, count = 0;
	char buffer[BUFFER_LENGTH];
	if (size < BUFFER_LENGTH) {
		for (rbytes = 0; rbytes < size; rbytes += n) {
			if ((n = recv(sock, (char*)buffer + rbytes, size, 0)) <= 0)
				err_sys("Recv BODY Error");
			count += n;
		}
		buffer[size] = '\0';
		msg_ptr->body = buffer;
		//std::cout << size << msg_ptr->body << std::endl;
	}
	else {
		int counter = size;
		while (counter > BUFFER_LENGTH) {
			std::cout << "here!!";
			for (rbytes = 0; rbytes < BUFFER_LENGTH - 1; rbytes += n) {
				if ((n = recv(sock, (char*)buffer + rbytes, BUFFER_LENGTH - 1, 0)) <= 0)
					err_sys("Recv BODY inside Error");
			}
			buffer[BUFFER_LENGTH - 1] = '\0';
			msg_ptr->body += buffer;
			counter -= (BUFFER_LENGTH - 1);

		}
		for (rbytes = 0; rbytes < counter; rbytes += n) {
			if ((n = recv(sock, (char*)buffer + rbytes, counter, 0)) <= 0)
				err_sys("Recv BODY Error");
		}
		buffer[counter] = '\0';
		msg_ptr->body += buffer;
		count = size;
		//std::cout << msg_ptr->body;

	}


	return count;
}
//sending an attachment here



void TcpThread::run() //cs: Server socket
{
	//not sure what to do with the first two but they might come handy  
	Resp respp;//a pointer to response
	Req* reqp; //a pointer to the Request Packet


	HEADER smsg, rmsg; //send_message receive_message
	MESSAGEBODY message;
	struct _stat stat_buf;
	int receiversock = -1;
	string receivername;
	AttachedFile fileheader;
	char* attachedfile;
	//those two are not that usefull (using them to print stuff)
	int result;
	int what;



	//receive the header and text message

	if (msg_recv(cs, &rmsg) != sizeof(HEADER))
		err_sys("Receive Req error,exit");//might want to add if there is an attachment and the size of the attachment if that's possible

	if (msg_recv(cs, &message, rmsg.datalength) != rmsg.datalength){
		err_sys("Receiveing the data error,exit");}
	else {
		//validate header before receving
		if (!(isValid(rmsg.from) && isValid(rmsg.to))) {
		//invalid heaeders
			sprintf(respp.response, "501 Error");//to sender
		}
		else {
			//save the message header and body
			fstream messagefile;
			string filename, time;

			//trying to get the time part here... but...
			/*time = rmsg.timestamp;
			replace(time.begin(), time.end(), ':', '.');
			filename = rmsg.subject;
			filename += "_";
			for (int i = 0; i < 24; i++) {
				filename += time[i];
			}*/
			time = to_string(rmsg.timestamp);
			filename = rmsg.subject;
			// int written = 0;
			messagefile.open(filename + ".txt", ios::out);//Subject_time.txt   
			if (messagefile.is_open()) {
				messagefile << rmsg.from << endl;
				messagefile << rmsg.to << endl;
				messagefile << rmsg.subject << endl;
				messagefile << message.body << endl;
				messagefile << rmsg.timestamp << endl;
				messagefile.close();
				// written = 1;
			}
		}
		
		
	}


	//receive the attachment
	if (rmsg.attachment == 1) {
		if (attach_header_recv(cs, &fileheader) != sizeof(AttachedFile))
			err_sys("Receiveing the attached file header error,exit");
		attachedfile = (char*)malloc(fileheader.size);
		cout << fileheader.size;
		cout << *attachedfile;
		if (attach_recv(cs, attachedfile, fileheader.size) != fileheader.size)
			err_sys("Receiving the attached file error,exit");

		//writing to a file just to test it
#pragma warning(suppress : 4996)
		FILE* fp;
		fp = fopen("subject_t", "wb");//server saves here
		if (!fp)
		{
			free(attachedfile);
		}
		cout << endl << "we are writing: " << *attachedfile;
		// Write the entire buffer to file
		cout << "Writing the buffer into a file";
		if (!fwrite(attachedfile, fileheader.size, 1, fp))
		{
			free(attachedfile);
			fclose(fp);
		}
		fclose(fp);
		//free(attachedfile);
	}
	else {
		attachedfile = (char*)malloc(1);
	}

	//check if the receiver is in the mapping file
	//if (mappedReceiver(receivername, rmsg.to) == 0) {
		//output an error about client not existing
	//	cout << "The client doesnt exist!";
	//}
	if (findReceiver(receivername, rmsg.to)) {
		cout << "receiver found\n";
	}


	cout << "The receiver: " << receivername << endl;

	//we now have the client name so check it on the sockets map
	if (sockets[receivername.c_str()] == 0) {
		//client not connected message
		printf("The client is not connected\n");
	}
	else {
		receiversock = sockets[receivername.c_str()];
		cout << "Done receiving!\n";
	}
	

	//check if the receiver is connected (might want to do this by sending some of the message and check if it returns 0)
	//send the header (copy the code from the client)
	if (msg_send(receiversock, &rmsg) != sizeof(HEADER)) {
		//output an error could not send properly or something like that
		err_sys("Sending Header error");
	}
	//send the message body
	if (msg_send(receiversock, message) != rmsg.datalength) {
		//output an error could not send properly or something like that
		err_sys("Sending Body error");
	}
	//send the attachment here if its there
	if (rmsg.attachment == 1) {
		//send the file header here
		if (attach_header_send(receiversock, &fileheader) != sizeof(AttachedFile)) {
			//output an error could not send properly or something like that
			cout << endl << "problem with sending the file header";
		}
		//send the actual attachment file here
		if (attach_send(receiversock, attachedfile, fileheader.size) != fileheader.size) {
			//output an error could not send properly or something like that
			cout << endl << "problem with sending the file";
		}
	}

	////////The culprit???????????
	
	//send confirmation if all went well
	
	strcpy(respp.response, "250 OK");
	respp.timestamp = 3;//check this get the time stamp
	if (msg_confirmation_send(cs,&respp) != sizeof(Resp))
		err_sys("Sending the confirmation error");
	//remove the socket and hostname from the sockets map
	
	
	closesocket(cs);
}

int TcpThread::msg_send(int sock, MESSAGEBODY msg_ptr)
{
	int n, size;
	size = msg_ptr.body.length();
	char buffer[BUFFER_LENGTH];
	//hold the string in the buffer
	if (size < BUFFER_LENGTH) {
		//sprintf(*buffer, msg_ptr.body.c_str());
		printf("HERE");
		msg_ptr.body.copy(buffer, size, 0);
		buffer[size] = '\0';
		printf("Here 2");
		if ((n = send(sock, (char*)&buffer, size, 0)) != (size)) {
			err_sys("Send Error");
		}

	}
	else {
		int count = size;
		while (count > BUFFER_LENGTH) {

			msg_ptr.body.copy(buffer, BUFFER_LENGTH - 1, (size - count));
			buffer[BUFFER_LENGTH - 1] = '\0';
			if ((n = send(sock, (char*)&buffer, BUFFER_LENGTH - 1, 0)) != (BUFFER_LENGTH - 1)) {
				err_sys("Send Error 1");
			}
			count -= (BUFFER_LENGTH - 1);
		}
		msg_ptr.body.copy(buffer, count, (size - count));
		buffer[count] = '\0';
		cout << buffer;
		if ((n = send(sock, (char*)&buffer, count, 0)) != (count)) {
			err_sys("Send Error 2");
		}
		//cout << buffer;
		n = size;
	}

	printf(" message size : %d\n", n);
	return (n);
}

int TcpThread::attach_header_send(int sock, AttachedFile* msg_ptr)
{
	int n;
	if ((n = send(sock, (char*)msg_ptr, sizeof(AttachedFile), 0)) != (sizeof(AttachedFile))) {
		cout << n;
		err_sys("Send attachment HEADER Error");
	}
	return (n);
}


int TcpThread::msg_send(int sock, HEADER* msg_ptr)
{
	int n;
	if ((n = send(sock, (char*)msg_ptr, sizeof(HEADER), 0)) != (sizeof(HEADER))) {
		cout << n;
		err_sys("Send HEADER Error");
	}
	return (n);
}


int TcpThread::mappedReceiver(string& clientName, char mailaddress[]) {
	//This function takes the given receiver email address and returns the client name and true/false if the client is in the mapping
	//using a string pointer (using char array proved to be problematic

	char client_name[20];
	char client_address[20];
	string line, word;
	FILE* mappingfile;
	string fname = "testfile2.csv"; //using csv file might be helpfull if we want to manage is manually
	//open the mapping file
	fstream file(fname, ios::in);
	if (file.is_open()) {
		while (getline(file, line))
		{//reading the whole line in 'line' and then separating them to deal with the address and name separately 
			stringstream str(line);//not the most efficient way but could be improved
			getline(str, word, ',');
			strcpy(client_name, word.c_str());
			getline(str, word, ',');
			strcpy(client_address, word.c_str());
			if (strcmp(client_address, mailaddress) == 0) {
				clientName = client_name;
				file.close();
				return 1;
			}

		}
		file.close();
		return 0;
	}

}
bool TcpThread::findReceiver(string& receiverfound, char email_addres[]) {
	fstream myFile;
	bool found = false;
	map<string, string> addresses;
	string email_address(email_addres);
	myFile.open(MappingFile, ios::in);//read mode//change the file to read a constant filename
	if (myFile.is_open()) {
		string clientName;
		string clientEmail;
		while (!myFile.eof()) {
			getline(myFile, clientName, ',') && getline(myFile, clientEmail);
			addresses.insert(pair<string, string>(clientEmail, clientName));
			//cout << clientEmail;
		}
		myFile.close();
		if (addresses[email_address].length() != 0) {
			found = true;
			receiverfound = addresses[email_address];
		}
		myFile.close();
		return found;
	}
	//could not open file message and return
	return found;
}

int TcpThread::attach_header_recv(int sock, AttachedFile* msg_ptr) {
	int rbytes, n, count = 0;

	for (rbytes = 0; rbytes < sizeof(AttachedFile); rbytes += n) {
		if ((n = recv(sock, (char*)msg_ptr + rbytes, sizeof(AttachedFile), 0)) <= 0)
			err_sys("Recv file HEADER Error");
		count += n;
	}

	return count;
}

int TcpThread::attach_recv(int sock, char* container, int size) {
	//better to put it in a buffer
	char buffer[BUFFER_LENGTH];
	int rbytes, n, count = 0;
	if (size <= BUFFER_LENGTH) {
		for (rbytes = 0; rbytes < size; rbytes += n) {
			if ((n = recv(sock, (char*)&buffer + rbytes, size, 0)) <= 0)
				err_sys("Recv file HEADER Error");
			count += n;
			//strcat(container, buffer);
			strncat(container, buffer, size);
		}
		return count;
	}
	else {
		int counter = size;
		while (counter > BUFFER_LENGTH) {
			for (rbytes = 0; rbytes < BUFFER_LENGTH; rbytes += n) {
				if ((n = recv(sock, (char*)&buffer + rbytes, BUFFER_LENGTH, 0)) <= 0)
					err_sys("Recv file HEADER Error");

			}
			counter -= BUFFER_LENGTH;
			//strcat(container, buffer);
			strncat(container, buffer, BUFFER_LENGTH);
			cout << endl << "The container: " << container;
		}
		for (rbytes = 0; rbytes < counter; rbytes += n) {
			if ((n = recv(sock, (char*)&buffer + rbytes, counter, 0)) <= 0)
				err_sys("Recv file HEADER Error");

		}

		cout << buffer;
		//strcat(container, buffer);
		strncat(container, buffer, counter);
		count = size;

		cout << endl << "The container outside: " << container;
	}
	return count;
}



/////////////////////////////TcpThreadReceiver Class////////////////////////////////////
//this thread only waits to receive the response from the receiving client
void TcpThreadReceiver::err_sys(const char* fmt, ...)
{
	perror(NULL);
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

void TcpThreadReceiver::run() {//write the final code of the receiver thread here
	//display a message of connection "client "HOSTNAME" is connected to receive email"
	//use the map and the socket number to do that

	//wait to receive the confirmation from the receiver
	char message[20];
	Resp* resp = new Resp;
	int n;
	if (msg_recv(cs, resp) != sizeof(Resp))
		err_sys("Error receiving the confirmation from the receiver");//
	//print something depending on message

	//close the connection and remove the socket cs from the map
	closesocket(cs);

}

int TcpThreadReceiver::msg_recv(int sock, Resp* message) {//make sure to change the number to a defined value from the #define
	//receive the confirmation from the receiver
	int n, rbytes;
	for (rbytes = 0; rbytes < sizeof(Resp); rbytes += n)
		if ((n = recv(sock, (char*)message + rbytes, sizeof(Resp), 0)) <= 0)
			err_sys("Recv Message Error");

	return n;

}
int TcpThread :: isValid(char email[]) {
	string email_(email);
	if (regex_match(email_, regex("(\\w+)(\\.|_)?(\\w*)@(\\w+)(\\.(\\w+))+")))
		return 1;

	return 0;
}
int TcpThread::attach_send(int sock, char* filename, int size)
{
	//do some sort of tricketry to get the size of the file
	int n, counter = 0;
	char buffer[BUFFER_LENGTH];

	if (size < BUFFER_LENGTH) {
		if ((n = send(sock, (char*)&filename, size, 0)) != (size)) {
			err_sys("Send attachment length Error");
		}
	}

	else {
		int count = size;
		while (count > BUFFER_LENGTH) {
			//Take part of the filename then send that part
			memcpy(buffer, filename + (size - count), BUFFER_LENGTH);
			if ((n = send(sock, (char*)&buffer, BUFFER_LENGTH, 0)) != (BUFFER_LENGTH)) {
				err_sys("Send attachment length Error");
			}
			count -= BUFFER_LENGTH;
		}
		//grab the rest of filename
		memcpy(buffer, filename + (size - count), count);
		if ((n = send(sock, (char*)&buffer, count, 0)) != (count)) {
			err_sys("Send MSGHDRSIZE+length Error");
		}
		n = size;


	}
	printf("%d\n", n);
	return (n);
}

////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{

	TcpServer ts;
	ts.start();

	return 0;
}


