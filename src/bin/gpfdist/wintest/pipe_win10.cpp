// gpload_win10.cpp : Defines the entry point for the console application.
//

#include <Windows.h>
#include <stdio.h>
#include <string>
#define PIPEBUFF 4096	

int main(int argc, char* argv[])
{
	std::string tmp = "\\\\.\\pipe\\public_test_0_pipe0";
	HANDLE hPipe = CreateNamedPipe(
		tmp.c_str(),             // pipe name 
		PIPE_ACCESS_DUPLEX,       // read/write access 
		PIPE_TYPE_MESSAGE |       // message type pipe 
		PIPE_READMODE_MESSAGE |   // message-read mode 
		PIPE_WAIT,                // blocking mode 
		1,                        // max. instances  
		0,                  // output buffer size 
		0, // input buffer size
		5000,                     // client time-out 
		NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create pipe. Invalid pipe handle.\n");
		return -1;
	}
	ConnectNamedPipe(hPipe, 0);
	//Write data to file.
	char DataBuffer[] = "1|one\n2|two\n3|three\n99|ninety nine";
	printf("Data: [%s]\n", DataBuffer);
	DWORD nBytesWritten = 0;
	int nBytesToWrite = strlen(DataBuffer);
	printf("Bytes to write = %d.\n", nBytesToWrite);
	if (WriteFile(hPipe, DataBuffer, (DWORD)nBytesToWrite, &nBytesWritten, NULL) == 0)
	{
		printf("Failed to write to the pipe.\n");
		return -1;
	}
	printf("Write to file succeded!\n");
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
	return 0;
}
