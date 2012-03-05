#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <UrlMon.h>

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using std::exception;

#pragma comment (lib, "UrlMon.lib")
#pragma comment (lib, "Ws2_32.lib")

bool IsValidRequest (const char* Request);
const char* GetMIMEType (const char* File);
const char* ParseSpaces (const char* Path);
const char* GetFilePathFromRequest (const char* Request);
void SendFile (SOCKET Connection, const char* Request, const char* IPAddress);

int __cdecl main (int ArgCount, char* Args[])
{
	try
	{
		WSAData WSA;
		if(WSAStartup(MAKEWORD(2, 0), &WSA) != 0)
		{
			throw exception("Unable to initialize WinSock2.");
		}
		
		SOCKET Socket = 0;
		sockaddr_in Local;
		ZeroMemory(&Local, sizeof(Local));

		Local.sin_family = AF_INET;
		Local.sin_addr.s_addr = INADDR_ANY;
		Local.sin_port = htons(80);

		Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(Socket == INVALID_SOCKET || bind(Socket, reinterpret_cast<sockaddr*>(&Local), sizeof(Local)) != 0)
		{
			throw exception("Unable to create and bind server socket.");
		}

		if(listen(Socket, SOMAXCONN) != 0)
		{
			throw exception("Unable to listen for incoming connections.");
		}

		SOCKET Client;
		sockaddr_in IncomingConnection;
		ZeroMemory(&IncomingConnection, sizeof(IncomingConnection));
		int IncomingConnectionSize = sizeof(IncomingConnection);

		/*char* IPFile = new char[261];
		ZeroMemory(IPFile, 261);
		if(URLDownloadToCacheFile(nullptr, "http://automation.whatismyip.com/n09230945.asp", IPFile, 260, 0, nullptr) != S_OK)
		{
			throw exception("Unable to get your IP address.");
		}

		std::fstream IP (IPFile, std::ios::in);
		std::string IPAddress;
		std::getline(IP, IPAddress);*/

		char* CurrentDirectory = new char[261];
		ZeroMemory(CurrentDirectory, 261);
		GetCurrentDirectory(260, CurrentDirectory);
		//cout << "Serving files relative to:\n" << CurrentDirectory << "\nat IP address: " << IPAddress.c_str() << "\n" << "You can access files from your browser at, for example, if Example.txt is in the same folder as this program: http://" << IPAddress.c_str() << "/Example.txt\n\n" << endl;
		cout << "Serving files relative to:\n" << CurrentDirectory << "\n" << endl;

		while(true)
		{
			Client = accept(Socket, reinterpret_cast<sockaddr*>(&IncomingConnection), &IncomingConnectionSize);

			char* Request = new char[1001];
			ZeroMemory(Request, 1001);
			while(recv(Client, Request, 1000, 0) > 0)
			{
				try
				{
#ifdef _DEBUG
						cout << Request << endl;
#endif
					if(IsValidRequest(Request) == false)
					{
						send(Client, "HTTP/1.0 400\n", 14, 0);
						closesocket(Client);
						cout << "Received invalid request from " << inet_ntoa(IncomingConnection.sin_addr) << ". Sent back status code 400 (Invalid request) and closed connection.\n";
						break;
					}
					else
					{
						SendFile(Client, Request, inet_ntoa(IncomingConnection.sin_addr));
					}
				}
				catch(exception Exception)
				{
					send(Client, "HTTP/1.0 500\n", 14, 0);
					closesocket(Client);
					cout << Exception.what() << endl;
				}
			}

			delete[] Request;
		}
	}
	catch(exception Exception)
	{
		cout << "Exception: " << Exception.what() << " Error code: " << WSAGetLastError() << endl;
	}

	return 0;
}

bool IsValidRequest (const char* Request)
{
	char* Three = new char[4];
	ZeroMemory(Three, 4);
	memcpy(Three, Request, 3);
	if(strcmp(Three, "GET") != 0)
	{
#ifdef _DEBUG
		cout << "Invalid HTTP method.\n";
#endif
		return false;
	}

	return true;
}

const char* GetMIMEType (const char* File)
{
	unsigned int i = 0;
	unsigned int LastPeriod = 0;
	while(i < strlen(File))
	{
		if(File[i] == '.')
		{
			LastPeriod = i;
		}

		i++;
	}

	if(LastPeriod == 0)
	{
		return "text/plain";
	}

	char* Extension = new char[strlen(&File[LastPeriod]) + 1];
	ZeroMemory(Extension, strlen(&File[LastPeriod]) + 1);
	memcpy(Extension, &File[LastPeriod], strlen(&File[LastPeriod]));

	HKEY Key;
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT, Extension, 0, KEY_READ, &Key) != 0)
	{
		throw exception("Unable to open registry key to get file MIME-Type.");
	}
	delete[] Extension;

	static char MIMEType[101];
	ZeroMemory(MIMEType, 101);
	unsigned long Length = 100;

	unsigned long Type = REG_SZ;
	if(RegQueryValueEx(Key, "Content Type", 0, &Type, reinterpret_cast<unsigned char*>(MIMEType), &Length) != 0)
	{
		return "text/plain";
	}

	return MIMEType;
}

const char* GetFilePathFromRequest (const char* Request)
{
	unsigned int i = 0;
	while(Request[i] != '\n' && std::string(Request).find(" HTTP/1") > i)
	{
		i++;
	}

	static char Path[261];
	ZeroMemory(Path, 261);
	GetCurrentDirectory(261, Path);

	static char End[261];
	ZeroMemory(End, 261);
	memcpy(End, &Request[4], i - 4);

	strcat(Path, End);
	return ParseSpaces(Path);
}

const char* ParseSpaces (const char* Path)
{
	std::string TempPath (Path);

	while(TempPath.find("%20") != std::string::npos)
	{
		TempPath.replace(TempPath.find("%20"), 3, " ");
	}

	char* ResultPath = new char[261];
	ZeroMemory(ResultPath, 261);
	strcpy(ResultPath, TempPath.c_str());
	//memcpy(ResultPath, TempPath.c_str(), 260);
	return ResultPath;
}

void SendFile (SOCKET Connection, const char* Request, const char* IPAddress)
{
	std::string Response ("HTTP/1.0 ");
	const char* FilePath = GetFilePathFromRequest(Request);
	std::fstream File (FilePath, std::ios::in);
	if(File.is_open() == false)
	{
		cout << IPAddress << " requested file \"" << FilePath << "\". File does not exist. Sent status code 404 (File not found).\n\n";
		Response.append("404\n");
		send(Connection, Response.c_str(), Response.length(), 0);
		closesocket(Connection);
		return;
	}

	Response.append("200 OK\n");
	Response.append("Date: Thu, 31 Dec 1970 23:59:60 GMT");
	Response.append("\nContent-Type: ");
	const char* MIMEType = GetMIMEType(FilePath);
#ifdef _DEBUG
	cout << "Got file MIME-Type as: " << MIMEType << "\n";
#endif
	Response.append(MIMEType);
	Response.append("\nContent-Length: ");

	WIN32_FILE_ATTRIBUTE_DATA FileInfo;
	GetFileAttributesEx(FilePath, GetFileExInfoStandard, &FileInfo);

	char* FileLength = new char[11];
	ZeroMemory(FileLength, 11);
	itoa(FileInfo.nFileSizeLow, FileLength, 10);
	Response.append(FileLength);
	delete[] FileLength;
	Response.append("\n\n");

	unsigned int BufferLength = Response.length() + FileInfo.nFileSizeLow + 1;
	unsigned char* ResponseBuffer = new unsigned char[BufferLength];
	ZeroMemory(ResponseBuffer, BufferLength);
	
	memcpy(ResponseBuffer, Response.c_str(), Response.length());
	File.seekp(std::ios::beg);
	File.read(reinterpret_cast<char*>(&ResponseBuffer[Response.length()]), FileInfo.nFileSizeLow);

	send(Connection, reinterpret_cast<char*>(ResponseBuffer), BufferLength, 0);
	delete[] ResponseBuffer;
	closesocket(Connection);
	cout << IPAddress << " requested file \"" << FilePath << "\". Responded with file.\n\n";
	return;
}