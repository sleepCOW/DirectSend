#include "Server.h"
#include "Client.h"
#include <iostream>
#include <thread>
#include <list>
#include <mutex>

#include <chrono>
#include <fileapi.h>

// Port, File and Pass must be specified!
constexpr int AllRequiredArguments = 6;
constexpr size_t BufferLength = 512;

int ServerMain(int Argc, char* Argv[]);
int ClientMain(int Argc, char* Argv[]);

using std::string;
using std::thread;
using std::list;
using std::mutex;

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "SPECIFY MODE!\n";
		return -1;
	}

	if (strcmp(argv[1], "server") == 0)
	{
		return ServerMain(argc, argv);
	}
	else if (strcmp(argv[1], "client") == 0)
	{
		return ClientMain(argc, argv);
	}

	return -1;
}

struct Buffer
{
	char* Data;
	uint16_t Size = 0;

	Buffer(uint16_t _Size) : Size(_Size)
	{
		Data = new char[Size];
	}

	~Buffer()
	{
		delete[] Data;
	}
};

int SendFile(NetworkDevice& NetDevice, string& FileName)
{
	// Open a file, also lock file until transfer is done (accept only reading for other processes)
	HANDLE FileToTransfer = CreateFile(CharToWChar(FileName.data()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileToTransfer == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open file: %d\n", GetLastError());
		return GetLastError();
	}

	CutToFileName(FileName);
	NetDevice.Send(OperationCode::ReceiveFileName, FileName);

	DWORD FileSize = GetFileSize(FileToTransfer, nullptr);
	DWORD SizeLeft = 0;

	NetDevice.Send(OperationCode::ReceiveFileSize, (char*)&FileSize, sizeof(DWORD));

	mutex BufferMutex;
	list<Buffer*> CommonBuffer;
	bool bTranferFinished = false;
	auto SendFileData = [&BufferMutex, &CommonBuffer, &NetDevice, &bTranferFinished]()
	{
		Buffer* BufferToWrite = nullptr;
		do 
		{
			{
				std::lock_guard<mutex> LockGuard(BufferMutex);

				if (not CommonBuffer.empty())
				{
					BufferToWrite = CommonBuffer.front();
					CommonBuffer.pop_front();
				}
			}

			if (BufferToWrite != nullptr)
			{
				NetDevice.Send(OperationCode::ReceiveFileData, BufferToWrite->Data, BufferToWrite->Size);
				delete BufferToWrite;
				BufferToWrite = nullptr;
			}
		} while (not bTranferFinished || not CommonBuffer.empty());
	};
	thread FileThread(SendFileData);

	// Read bytes and send until file end
	DWORD BytesRead;
	bool ReadResult = false;
	char* Data = NetDevice.GetData();
	auto start = std::chrono::high_resolution_clock::now();
	auto tranfer_start = std::chrono::high_resolution_clock::now();
	do {
		ReadResult = ReadFile(FileToTransfer, Data, NetDevice.GetBufferSize(), &BytesRead, NULL);
		if (ReadResult and BytesRead > 0)
		{
			{
				std::lock_guard<mutex> Lock(BufferMutex);

				Buffer* NewBuffer = new Buffer(BytesRead);
				memcpy(NewBuffer->Data, Data, BytesRead);
				CommonBuffer.push_back(NewBuffer);
			}

			if (GetTimePast(tranfer_start).count() > 2000)
			{
				SizeLeft += BytesRead;
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
	} while (ReadResult and BytesRead == NetDevice.GetBufferSize());
	bTranferFinished = true;
	FileThread.join();
	NetDevice.Send(OperationCode::FinishedTransfer, "Gratz!");
	Sleep(4000);

	// Record end time
	std::cout << "Transfer is finished in " << GetTimePast(start).count() << std::endl;

	// Close File
	CloseHandle(FileToTransfer);
	return 0;
}

mutex consoleLock;

DWORD ReceivedBytes = 0;

int ReceiveFile(NetworkDevice& NetDevice, string& Path)
{
	DWORD FileSize = 0;
	DWORD CurrentProgress = 0;
	auto tranfer_start = std::chrono::high_resolution_clock::now();
	auto tranfer_start2 = std::chrono::high_resolution_clock::now();

	// receive file name
	HANDLE File;
	OperationCode CurrentState = OperationCode::Invalid;
	mutex BufferMutex;
	list<Buffer*> CommonBuffer;
	bool bTranferFinished = false;
	auto WriteToFileTask = [&CommonBuffer, &BufferMutex, &NetDevice, &bTranferFinished, &File, &FileSize, &CurrentProgress, &tranfer_start]()
	{
		Buffer* BufferToWrite = nullptr;
		do
		{
			{
				std::lock_guard<mutex> LockGuard(BufferMutex);

				if (not CommonBuffer.empty())
				{
					BufferToWrite = CommonBuffer.front();
					CommonBuffer.pop_front();
				}
			}

			if (BufferToWrite != nullptr)
			{
				DWORD BytesWritten;
				bool bSuccess = WriteFile(File, BufferToWrite->Data, BufferToWrite->Size, &BytesWritten, NULL);
				CurrentProgress += BytesWritten;
				if (!bSuccess)
				{
					printf("Failed to write to the file: %d\n", GetLastError());
				}
				if (GetTimePast(tranfer_start).count() > 2000)
				{
					std::lock_guard<mutex> LockGuard(consoleLock);

					std::cout << "Progress: " << (float(CurrentProgress) / float(FileSize)) << "%\n";
					std::cout << "Written : " << (CurrentProgress / 1024 / 1024) << "MB\n";
					tranfer_start = std::chrono::high_resolution_clock::now();
				}
				delete BufferToWrite;
				BufferToWrite = nullptr;
			}
		} while (not bTranferFinished || not CommonBuffer.empty());
	};
	thread WriteFileThread(WriteToFileTask);

	auto WriteToFile = [&NetDevice, &Path, &CurrentState, &File, &bTranferFinished, &BufferMutex, &CommonBuffer, &FileSize, &tranfer_start2](OperationCode OpCode)
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

			string FileName(NetDevice.GetData(), NetDevice.GetBytesReceived());
			Path.append(FileName);

			LPWSTR WFileName = CharToWChar(Path.data());
			File = CreateFile(WFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			delete[] WFileName;

			if (File == INVALID_HANDLE_VALUE)
			{
				printf("Failed to create file: %s with error %d\n", FileName.data(), GetLastError());
				return;
			}
			MemoryBarrier();
			Sleep(1);
		}
		else if (OpCode == OperationCode::ReceiveFileSize)
		{
			DWORD* FileSizePtr = (DWORD*)NetDevice.GetData();
			FileSize = *FileSizePtr;
		}
		else if (OpCode == OperationCode::ReceiveFileData)
		{
			int ReceivedBytesNow = NetDevice.GetBytesReceived();
			ReceivedBytes += ReceivedBytesNow;
			{
				std::lock_guard<mutex> Lock(BufferMutex);

				Buffer* NewBuffer = new Buffer(ReceivedBytesNow);
				memcpy(NewBuffer->Data, NetDevice.GetData(), NewBuffer->Size);
				CommonBuffer.push_back(NewBuffer);
			}

			if (GetTimePast(tranfer_start2).count() > 2000)
			{
				std::lock_guard<mutex> LockGuard(consoleLock);

				std::cout << "Received: " << (ReceivedBytes / 1024 / 1024) << "MB\n";
				tranfer_start2 = std::chrono::high_resolution_clock::now();
			}
		}
		else if (OpCode == OperationCode::FinishedTransfer)
		{
			bTranferFinished = true;
			std::string Received(NetDevice.GetData(), NetDevice.GetBytesReceived());
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
	bool bResult = NetDevice.Listen(WriteToFile, WhileJobNotFinished);
	WriteFileThread.join();

	CloseHandle(File);
	return !bResult;
}

int ServerMain(int Argc, char* Argv[])
{
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
		return -1;
	}

	if (Mode != "send" and Mode != "receive")
	{
		std::cout << "Mode must be specified!" << std::endl;
		return -1;
	}

	std::cout << "Openning connection for port: " << Port << "\n"
			  << "Getting ready to " << Mode << "\n";

	Server DServer{ string(Port) };
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
        return -1;
    }

	if (Mode != "send" and Mode != "receive")
	{
		std::cout << "Mode must be specified!" << std::endl;
		return -1;
	}

    Client DClient{ Ip, Port };
	if (Mode == "send")
	{
		return SendFile(DClient, FileName);
	}
	else
	{
		return ReceiveFile(DClient, Path);
	}
}