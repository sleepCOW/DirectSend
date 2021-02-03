#include "Server.h"
#include "Client.h"
#include "Common.h"
#include <iostream>
#include <string>

#include <chrono>
#include <fileapi.h>

// Port, File and Pass must be specified!
constexpr int AllRequiredArguments = 6;
constexpr size_t BufferLength = 512;

#define ARGUMENTS_UNSPECIFIED -1
#define WRONG_PASSWORD -2

int ServerMain(int Argc, char* Argv[]);
int ClientMain(int Argc, char* Argv[]);

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "SPECIFY MODE!\n";
		return ARGUMENTS_UNSPECIFIED;
	}

	if (strcmp(argv[1], "server") == 0)
	{
		return ServerMain(argc, argv);
	}
	else if (strcmp(argv[1], "client") == 0)
	{
		return ClientMain(argc, argv);
	}

	return ARGUMENTS_UNSPECIFIED;
}

template <size_t BufferSize>
int SendFile(NetworkDevice<BufferSize>& NetworkDevice, string& FileName)
{
	// Open a file, also lock file until transfer is done (accept only reading for other processes)
	HANDLE FileToTransfer = CreateFile(CharToWChar(FileName.data()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileToTransfer == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open file: %d\n", GetLastError());
		return GetLastError();
	}

	CutToFileName(FileName);
	NetworkDevice.Send(OperationCode::ReceiveFileName, FileName);

	DWORD FileSize = GetFileSize(FileToTransfer, nullptr);
	DWORD SizeLeft = FileSize;

	// Read bytes and send until file end
	DWORD BytesRead;
	bool ReadResult = false;
	auto& Buffer = NetworkDevice.GetBuffer();
	auto start = std::chrono::high_resolution_clock::now();
	auto tranfer_start = std::chrono::high_resolution_clock::now();
	do {
		ReadResult = ReadFile(FileToTransfer, Buffer.data(), Buffer.size(), &BytesRead, NULL);
		if (ReadResult and BytesRead > 0)
		{
			NetworkDevice.Send(OperationCode::ReceiveFileData, BytesRead);

			if (GetTimePast(tranfer_start).count() > 2000)
			{
				SizeLeft -= BytesRead;
				std::cout << "Progress: " << (float(SizeLeft) / float(FileSize)) * 100.f << "%\n";
				tranfer_start = std::chrono::high_resolution_clock::now();
			}
		}
		else if (ReadResult == false)
		{
			printf("Read failed: %d\n", GetLastError());
			CloseHandle(FileToTransfer);
			return GetLastError();
		}
	} while (ReadResult and BytesRead == Buffer.size());
	NetworkDevice.Send(OperationCode::FinishedTransfer, "Gratz!");
	Sleep(4000);

	// Record end time
	std::cout << "Transfer is finished in " << GetTimePast(start).count() << std::endl;

	// Close File
	CloseHandle(FileToTransfer);
	return 0;
}

template <size_t BufferSize>
int ReceiveFile(NetworkDevice<BufferSize>& NetworkDevice, string& Path)
{
	// receive file name
	HANDLE File;
	OperationCode CurrentState = OperationCode::Invalid;
	auto WriteToFile = [&NetworkDevice, &Path, &CurrentState, &File](OperationCode OpCode)
	{
		CurrentState = OpCode;

		if (OpCode == OperationCode::Invalid)
		{
		}
		else if (OpCode == OperationCode::ReceiveFileName)
		{
			if (Path.empty())
			{
				Path = ".\\";
			}
			else
			{
				Path.push_back('\\');
			}

			string FileName(NetworkDevice.GetData(), NetworkDevice.GetBytesReceived());
			Path.append(FileName);

			LPWSTR WFileName = CharToWChar(Path.data());
			File = CreateFile(WFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			delete[] WFileName;

			if (File == INVALID_HANDLE_VALUE)
			{
				printf("Failed to create file: %s with error %d\n", FileName.data(), GetLastError());
				return GetLastError();
			}
		}
		else if (OpCode == OperationCode::ReceiveFileData)
		{
			DWORD BytesWritten;
			bool bSuccess = WriteFile(File, NetworkDevice.GetData(), NetworkDevice.GetBytesReceived(), &BytesWritten, NULL);
			if (bSuccess)
			{

			}
			else
			{
				printf("Failed to write to the file: %d\n", GetLastError());
			}
		}
		else if (OpCode == OperationCode::FinishedTransfer)
		{
			std::string Received(NetworkDevice.GetData(), NetworkDevice.GetBytesReceived());
			std::cout << Received << "\n";
		}
		else
		{
			CurrentState = OperationCode::Invalid;
		}
	};
	auto WhileJobNotFinished = [&CurrentState](OperationCode OpCode)
	{
		return not (OpCode == OperationCode::FinishedTransfer || OpCode == OperationCode::Invalid || CurrentState == OperationCode::Invalid);
	};
	bool bResult = NetworkDevice.Listen(WriteToFile, WhileJobNotFinished);

	CloseHandle(File);
	return !bResult;
}

int ServerMain(int Argc, char* Argv[])
{
	using std::string;
	string Port, FileName, Mode, Path;

	Mode = Argv[2];

	auto TryReadArg = [Argv](const char* ArgumentName, int& Index, string& Output)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			++Index;
			Output = Argv[Index];
		}
	};
	for (int i = 1; i < Argc; ++i)
	{
		char* Argument = Argv[i];

		TryReadArg("-port", i, Port);
		TryReadArg("-path", i, Path);
		TryReadArg("-file", i, FileName);
	}

	if (Port.empty())
	{
		std::cout << "Portmust be specified!" << std::endl;
		return ARGUMENTS_UNSPECIFIED;
	}

	if (Mode != "send" and Mode != "receive")
	{
		std::cout << "Mode must be specified!" << std::endl;
		return ARGUMENTS_UNSPECIFIED;
	}

	std::cout << "Openning connection for port: " << Port << "\n"
			  << "Getting ready to " << Mode << "\n";

	Server<BufferLength> DServer{ string(Port) };
	if (Mode == "send")
	{
		return SendFile(DServer, FileName);
	}
	else
	{
		return ReceiveFile(DServer, Path);
	}
}

int ClientMain(int Argc, char* Argv[])
{
	using std::string;
    string Ip, Port, Path, Mode, FileName;

	Mode = Argv[2];

	auto TryReadArg = [Argv](const char* ArgumentName, int& Index, string& Output)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			++Index;
			Output = Argv[Index];
		}
	};

    for (int i = 1; i < Argc; ++i)
    {
		TryReadArg("-port", i, Port);
		TryReadArg("-path", i, Path);
		TryReadArg("-ip", i, Ip);
		TryReadArg("-file", i, FileName);
    }

    if (Ip.empty() or Port.empty())
    {
        std::cout << "Ip and Port must be specified!\n";
        return ARGUMENTS_UNSPECIFIED;
    }

	if (Mode != "send" and Mode != "receive")
	{
		std::cout << "Mode must be specified!" << std::endl;
		return ARGUMENTS_UNSPECIFIED;
	}

    Client<BufferLength> DClient{ Ip, Port };
	if (Mode == "send")
	{
		return SendFile(DClient, FileName);
	}
	else
	{
		return ReceiveFile(DClient, Path);
	}
}