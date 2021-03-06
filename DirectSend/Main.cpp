#include "Server.h"
#include "Client.h"
#include <iostream>
#include <thread>
#include <list>
#include <mutex>
#include "ProgressBar.h"

#include <chrono>
#include <fileapi.h>

using std::string;
using std::list;
using std::mutex;
using std::lock_guard;
using Thread = std::thread;

struct CMDArgs
{
	string Role;
	string Mode;
	string FileName;
	string Path;
	string Ip;
	string Port;
};

const char* Usage =
"-----------------------------------HOW TO-----------------------------------n"
"Specify your role as the first argument: \'server\' or \'client\'\n"
"Next specify mode of usage: \'send\' or \'receive\'\n"
"If you are a client you need to specify \'-ip\' and \'-port\' of server\n"
"If you are a server you only need to specify \'-port\' on which connection should be established\n"
"With \'send\' mode you have to specify \'-file\' to send\n"
"With \'receive\' mode you can specify \'-path\' where to store received file, if you leave this empty it will save in current directory\n"
"Examples:\n"
"client receive -ip 192.168.1.1 -port 5666 -path \"S:\\Folder\"\n"
"server send -port 5666 -file \"S:\\Folder\\Archive.zip\"\n";

void FillArguments(CMDArgs& Args, int Argc, char* Argv[])
{
	auto ReadArgument = [Argv](const char* ArgumentName, int& Index, string& OutArgument)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			++Index;
			OutArgument = Argv[Index];
		}
	};

	for (int i = 1; i < Argc; ++i)
	{
		ReadArgument("-ip", i, Args.Ip);
		ReadArgument("-port", i, Args.Port);
		ReadArgument("-path", i, Args.Path);
		ReadArgument("-file", i, Args.FileName);
	}
}

int SendFile(NetworkDevice& NetDevice, string& FileName);
int ReceiveFile(NetworkDevice& NetDevice, string& Path);

int main(int argc, char* argv[])
{
	if (argc < 6)
	{
		std::cerr << Usage;
		return -1;
	}

	CMDArgs Args;
	Args.Role = argv[1];
	Args.Mode = argv[2];

	FillArguments(Args, argc, argv);

	NetworkDevice* NetDevice = nullptr;
	// Create proper netdevice
	if (Args.Role == "client")
	{
		NetDevice = new Client(Args.Ip, Args.Port);
	}
	else if (Args.Role == "server")
	{
		NetDevice = new Server(Args.Port);
	}
	else
	{
		std::cerr << "Wrong role!!!!!!\n\n" << Usage;
		return -1;
	}

	// send or receive file depending on mode
	if (Args.Mode == "send")
	{
		return SendFile(*NetDevice, Args.FileName);
	}
	else
	{
		return ReceiveFile(*NetDevice, Args.Path);
	}

	std::cerr << "Wrong role or mode!!!!!!\n\n" << Usage;
	return -1;
}

int SendFile(NetworkDevice& NetDevice, string& FileName)
{
	std::cout << "Getting ready to send " << FileName << "\n";

	// Open a file, also lock file until transfer is done (accept only reading for other processes)
	FileHandle FileToTransfer = CreateFile(CharToWChar(FileName.data()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileToTransfer == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Failed to open file: " << GetLastError() << std::endl;
		return GetLastError();
	}

	CutToFileName(FileName);
	NetDevice.Send(OperationCode::ReceiveFileName, FileName);

	LargeInteger FileSize;
	GetFileSizeEx(FileToTransfer, FileSize);
	ProgressBar Bar(L"Upload: ", 20);
	Bar.SetFileSize(FileSize);

	NetDevice.Send(OperationCode::ReceiveFileSize, FileSize, sizeof(FileSize));

	mutex BufferMutex;
	list<Buffer*> CommonBuffer;
	bool bTranferFinished = false;
	auto SendFileData = [&BufferMutex, &CommonBuffer, &NetDevice, &bTranferFinished]()
	{
		Buffer* BufferToWrite = nullptr;
		do 
		{
			{
				lock_guard<mutex> LockGuard(BufferMutex);

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
	Thread FileThread(SendFileData);
	
	// Read bytes and send until file end
	DWORD BytesRead;
	bool ReadResult = false;
	char* Data = NetDevice.GetData();
	TimePoint TransferStart = std::chrono::high_resolution_clock::now();
	do {
		ReadResult = ReadFile(FileToTransfer, Data, NetDevice.GetBufferSize(), &BytesRead, NULL);
		if (ReadResult and BytesRead > 0)
		{
			while (CommonBuffer.size() > 10000)
			{
				Sleep(3 * 1000);
			} 

			{
				lock_guard<mutex> Lock(BufferMutex);
				Buffer* NewBuffer = new Buffer(uint16_t(BytesRead));
				memcpy(NewBuffer->Data, Data, BytesRead);
				CommonBuffer.push_back(NewBuffer);
				Bar += BytesRead;
			}

			if (EACH_N_SECONDS(1))
			{
				Bar.Print();
			}
		}
		else if (ReadResult == false)
		{
			std::cerr << "Read failed: " << GetLastError() << "\n";
			return GetLastError();
		}
	} while (ReadResult and BytesRead == NetDevice.GetBufferSize());
	bTranferFinished = true;
	Bar.Print();
	FileThread.join();
	NetDevice.Send(OperationCode::FinishedTransfer, "Gratz!");
	Sleep(4000);

	// Record end time
	std::cout << "Transfer is finished in " << GetTimePast(TransferStart).count() << " seconds!" << std::endl;

	return 0;
}

int ReceiveFile(NetworkDevice& NetDevice, string& Path)
{
	FileHandle File(nullptr);
	ProgressBar Bar(L"Download: ", 20);
	OperationCode CurrentState = OperationCode::Invalid;
	mutex BufferMutex;
	list<Buffer*> CommonBuffer;
	bool bTranferFinished = false;

	std::cout << "Getting ready to receive file" << "\n";

	auto WriteToFileTask = [&CommonBuffer, &BufferMutex, &NetDevice, &bTranferFinished, &File, &Bar]()
	{
		Buffer* BufferToWrite = nullptr;
		do
		{
			{
				lock_guard<mutex> LockGuard(BufferMutex);

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
				Bar += BytesWritten;
				if (!bSuccess)
				{
					std::cerr << "Failed to write to the file: " << GetLastError() << "\n";
				}
				if (EACH_N_SECONDS(1))
				{
					Bar.Print();
				}
				delete BufferToWrite;
				BufferToWrite = nullptr;
			}
		} while (not bTranferFinished || not CommonBuffer.empty());
	};
	Thread WriteFileThread(WriteToFileTask);

	auto WriteToFile = [&NetDevice, &Path, &CurrentState, &File, &bTranferFinished, &BufferMutex, &CommonBuffer, &Bar](OperationCode OpCode)
	{
		CurrentState = OpCode;

		if (OpCode == OperationCode::ReceiveFileName)
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

			std::cout << "Receiving file: " << FileName << "\n";

			LPWSTR WFileName = CharToWChar(Path.data());
			File = CreateFile(WFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			delete[] WFileName;

			if (File == INVALID_HANDLE_VALUE)
			{
				std::cerr << "Failed to create file: " << FileName.data() << " with error: "<< GetLastError() << "\n";
				return;
			}
			MemoryBarrier();
			Sleep(1);
		}
		else if (OpCode == OperationCode::ReceiveFileSize)
		{
			LargeInteger FileSize = NetDevice.GetData();
			Bar.SetFileSize(FileSize);
		}
		else if (OpCode == OperationCode::ReceiveFileData)
		{
			int ReceivedBytesNow = NetDevice.GetBytesReceived();
			{
				lock_guard<mutex> Lock(BufferMutex);

				Buffer* NewBuffer = new Buffer(ReceivedBytesNow);
				memcpy(NewBuffer->Data, NetDevice.GetData(), NewBuffer->Size);
				CommonBuffer.push_back(NewBuffer);
			}
		}
		else if (OpCode == OperationCode::FinishedTransfer)
		{
			bTranferFinished = true;
			Bar.Print();
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

	return !bResult;
}