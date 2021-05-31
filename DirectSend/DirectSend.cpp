#include "DirectSend.h"
#include "NetworkDevice.h"
#include "ProgressBar.h"

//#include <fileapi.h>

int SendFile(NetworkDevice& NetDevice, String& FileName)
{
	CMD::Print() << "Getting ready to send " << FileName << "\n";

	// Open a file, also lock file until transfer is done (accept only reading for other processes)
	FileHandle FileToTransfer = CreateFile(CharToWChar(FileName.data()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileToTransfer == INVALID_HANDLE_VALUE)
	{
		CMD::PrintError() << "Failed to open file: " << GetLastError() << std::endl;
		return GetLastError();
	}

	// Send file name to the receiver
	CutToFileName(FileName);
	NetDevice.Send(OperationCode::ReceiveFileName, FileName);

	// Init progress bar
	LargeInteger FileSize;
	GetFileSizeEx(FileToTransfer, FileSize);
	ProgressBar Bar(L"Upload: ", 20);
	Bar.SetFileSize(FileSize);

	// Send file size to the receiver
	NetDevice.Send(OperationCode::ReceiveFileSize, FileSize, sizeof(FileSize));

	Mutex BufferMutex;
	List<Buffer*> SharedBuffer;
	bool bTransferFinished = false;
	// Create thread task to send file
	SendFileDataTask SendTask(NetDevice, BufferMutex, SharedBuffer, bTransferFinished);
	Thread FileThread(SendTask);

	// Read bytes and send until file end
	DWORD BytesRead;
	bool ReadResult = false;
	char* Data = NetDevice.GetData();
	TimePoint TransferStart = std::chrono::high_resolution_clock::now();
	do {
		ReadResult = ReadFile(FileToTransfer, Data, NetDevice.GetBufferSize(), &BytesRead, NULL);
		if (ReadResult and BytesRead > 0)
		{
			while (SharedBuffer.size() > 10000)
			{
				Sleep(3 * 1000);
			}

			{
				LockGuard<Mutex> Lock(BufferMutex);
				Buffer* NewBuffer = new Buffer(uint16_t(BytesRead));
				memcpy(NewBuffer->Data, Data, BytesRead);
				SharedBuffer.push_back(NewBuffer);
				Bar += BytesRead;
			}

			if (EACH_N_SECONDS(1))
			{
				Bar.Print();
			}
		}
		else if (ReadResult == false)
		{
			CMD::PrintError() << "Read failed: " << GetLastError() << "\n";
			return GetLastError();
		}
	} while (ReadResult and BytesRead == NetDevice.GetBufferSize());
	bTransferFinished = true;
	Bar.Print();
	FileThread.join();
	NetDevice.Send(OperationCode::FinishedTransfer, "Gratz!");
	Sleep(4000);

	// Record end time
	CMD::Print() << "Transfer is finished in " << GetTimePast(TransferStart).count() << " seconds!" << std::endl;

	return 0;
}

int ReceiveFile(NetworkDevice& NetDevice, String& Path)
{
	FileHandle File(nullptr);
	ProgressBar Bar(L"Download: ", 20);
	OperationCode CurrentState = OperationCode::Invalid;
	Mutex BufferMutex;
	List<Buffer*> CommonBuffer;
	bool bTranferFinished = false;

	CMD::Print() << "Getting ready to receive file" << "\n";

	auto WriteToFileTask = [&CommonBuffer, &BufferMutex, &NetDevice, &bTranferFinished, &File, &Bar]()
	{
		Buffer* BufferToWrite = nullptr;
		do
		{
			{
				LockGuard<Mutex> LockGuard(BufferMutex);

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
					CMD::PrintError() << "Failed to write to the file: " << GetLastError() << "\n";
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

			String FileName(NetDevice.GetData(), NetDevice.GetBytesReceived());
			Path.append(FileName);

			CMD::Print() << "Receiving file: " << FileName << "\n";

			LPWSTR WFileName = CharToWChar(Path.data());
			File = CreateFile(WFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			delete[] WFileName;

			if (File == INVALID_HANDLE_VALUE)
			{
				CMD::PrintError() << "Failed to create file: " << FileName.data() << " with error: " << GetLastError() << "\n";
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
				LockGuard<Mutex> Lock(BufferMutex);

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

SendFileDataTask::SendFileDataTask(NetworkDevice& InDevice, Mutex& InBufferMutex, List<Buffer*>& InSharedBuffer, bool& InbFinished) :
	Device(InDevice), BufferMutex(InBufferMutex), SharedBuffer(InSharedBuffer), bFinished(InbFinished)
{}

void SendFileDataTask::operator()()
{
	Buffer* BufferToWrite = nullptr;
	do
	{
		{
			// Make sure we are not reading shared buffer in parallel
			LockGuard<Mutex> LockGuard(BufferMutex);

			if (not SharedBuffer.empty())
			{
				BufferToWrite = SharedBuffer.front();
				SharedBuffer.pop_front();
			}
		}

		// Send any new buffers to the receiver
		if (BufferToWrite != nullptr)
		{
			Device.Send(OperationCode::ReceiveFileData, BufferToWrite->Data, BufferToWrite->Size);
			delete BufferToWrite;
			BufferToWrite = nullptr;
		}
	} while (not bFinished || not SharedBuffer.empty());
}
