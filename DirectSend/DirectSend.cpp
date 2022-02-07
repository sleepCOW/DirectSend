#include "DirectSend.h"
#include "NetworkDevice.h"
#include "ProgressBar.h"

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

	bool bTaskFailed = false;
	LargeInteger ReceivedFileSeek;
	auto Task = [&bTaskFailed, &ReceivedFileSeek, &NetDevice](OperationCode OpCode)
	{
		if (OpCode == OperationCode::ReceiveFileSeek)
		{
			// we received start position
			ReceivedFileSeek = LargeInteger(NetDevice.GetData());
		}
		else
		{
			bTaskFailed = true;
		}
	};
	auto ReceiveCond = [&bTaskFailed](OperationCode OpCode)
	{
		return !(bTaskFailed || OpCode == OperationCode::ReceiveFileSeek);
	};
	NetDevice.TryListen(Task, ReceiveCond);

	// Init progress bar
	LargeInteger FileSize;
	GetFileSizeEx(FileToTransfer, FileSize);
	ProgressBar Bar(L"Upload: ", 20);
	Bar.SetFileSize(FileSize);
	Bar.SetCurrentSize(ReceivedFileSeek);

	SetFilePointerEx(FileToTransfer, ReceivedFileSeek, nullptr, FILE_BEGIN);

	// Send file size to the receiver
	NetDevice.Send(OperationCode::ReceiveFileSize, FileSize, sizeof(FileSize));

	if (ReceivedFileSeek == FileSize)
	{
		CMD::Print() << "File is already received by the client!\n";
		NetDevice.TrySend(OperationCode::FinishedTransfer, "File is already received!");
		Sleep(3000);
		return 0;
	}

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
	NetDevice.Send(OperationCode::FinishedTransfer, "File is successfully received!");
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
	List<Buffer*> SharedBuffer;
	bool bTransferFinished = false;

	CMD::Print() << "Getting ready to receive file" << "\n";

	WriteToFileTask WriteTask(NetDevice, BufferMutex, SharedBuffer, File, Bar, bTransferFinished);
	Thread WriteFileThread(WriteTask);

	ReceiveFileListenTask ListenTask(NetDevice, BufferMutex, SharedBuffer, File, Bar, Path, CurrentState, bTransferFinished);
	auto WhileJobNotFinished = [&CurrentState](OperationCode OpCode)
	{
		return not (OpCode == OperationCode::FinishedTransfer || OpCode == OperationCode::Invalid || CurrentState == OperationCode::Invalid);
	};
	bool bResult = NetDevice.Listen(ListenTask, WhileJobNotFinished);
	String Message = bResult ? "Transfer successfully finished!" : "Transfer failed, connection lost!!!";
	CMD::Print() << Message << "\n";
	
	if (!bResult)
	{
		WriteFileThread.detach();
	}
	else
	{
		// Wait until thread is finished it's job
		WriteFileThread.join();
	}

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

WriteToFileTask::WriteToFileTask(NetworkDevice& InDevice, Mutex& InBufferMutex, List<Buffer*>& InSharedBuffer, FileHandle& InFile, ProgressBar& InPB, bool& InbFinished) :
	Device(InDevice), BufferMutex(InBufferMutex), SharedBuffer(InSharedBuffer), bFinished(InbFinished), File(InFile), PB(InPB)
{}

void WriteToFileTask::operator()()
{
	Buffer* BufferToWrite = nullptr;
	do
	{
		{
			LockGuard<Mutex> LockGuard(BufferMutex);

			if (not SharedBuffer.empty())
			{
				BufferToWrite = SharedBuffer.front();
				SharedBuffer.pop_front();
			}
		}

		if (BufferToWrite != nullptr)
		{
			DWORD BytesWritten;
			bool bSuccess = WriteFile(File, BufferToWrite->Data, BufferToWrite->Size, &BytesWritten, NULL);
			if (!bSuccess)
			{
				CMD::PrintError() << "Failed to write to the file: " << GetLastError() << "\n";
			}

			// Update progress bar
			PB += BytesWritten;
			if (EACH_N_SECONDS(1))
			{
				PB.Print();
			}
			delete BufferToWrite;
			BufferToWrite = nullptr;
		}
	} while (not bFinished || not SharedBuffer.empty());
}

ReceiveFileListenTask::ReceiveFileListenTask(NetworkDevice& InDevice, Mutex& InBufferMutex, List<Buffer*>& InSharedBuffer, FileHandle& InFile, ProgressBar& InPB, String& InPath, OperationCode& InOpCode, bool& InbFinished) :
	Device(InDevice), BufferMutex(InBufferMutex), SharedBuffer(InSharedBuffer), bFinished(InbFinished), File(InFile), PB(InPB), Path(InPath), CurrentState(InOpCode)
{}

void ReceiveFileListenTask::operator()(OperationCode OpCode)
{
	CurrentState = OpCode;

	if (OpCode == OperationCode::ReceiveFileName)
	{
		ReceiveFileName();
	}
	else if (OpCode == OperationCode::ReceiveFileSize)
	{
		LargeInteger FileSize = Device.GetData();
		PB.SetFileSize(FileSize);
	}
	else if (OpCode == OperationCode::ReceiveFileData)
	{
		ReceiveFileData();
	}
	else if (OpCode == OperationCode::FinishedTransfer)
	{
		bFinished = true;
		PB.Print();
		String Message(Device.GetData(), Device.GetBytesReceived());
		CMD::Print() << Message << "\n";
	}
	else
	{
		CurrentState = OperationCode::Invalid;
	}
}

void ReceiveFileListenTask::ReceiveFileName()
{
	if (Path.empty())
	{
		Path = ".\\";
	}
	else
	{
		Path.push_back('\\');
	}

	String FileName(Device.GetData(), Device.GetBytesReceived());
	Path.append(FileName);

	CMD::Print() << "Receiving file: " << FileName << "\n";

	// Create file with the provided name
	LPWSTR WFileName = CharToWChar(Path.data());

	// Try to open if any first
	File = CreateFile(WFileName, GENERIC_READ | GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	LargeInteger FileSize = LargeInteger(int64_t(0));
	if (File != INVALID_HANDLE_VALUE)
	{
		CMD::Print() << "Found file with the same name, trying to continue downloading..." << FileName << "\n";

		GetFileSizeEx(File, FileSize);
		PB.SetCurrentSize(FileSize);
		SetFilePointerEx(File, FileSize, nullptr, FILE_BEGIN);
	}
	else
	{
		// Create new file if we haven't found existing one
		File = CreateFile(WFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	Device.TrySend(OperationCode::ReceiveFileSeek, (char*)FileSize, sizeof(LargeInteger));
	
	delete[] WFileName;

	if (File == INVALID_HANDLE_VALUE)
	{
		CMD::PrintError() << "Failed to create file: " << FileName.data() << " with error: " << GetLastError() << "\n";
		// Make sure we stopped receiving data if we failed to create the file
		CurrentState = OperationCode::Invalid;
		return;
	}
	MemoryBarrier();
	Sleep(1);
}

void ReceiveFileListenTask::ReceiveFileData()
{
	int ReceivedBytesNow = Device.GetBytesReceived();
	{
		LockGuard<Mutex> Lock(BufferMutex);

		Buffer* NewBuffer = new Buffer(ReceivedBytesNow);
		memcpy(NewBuffer->Data, Device.GetData(), NewBuffer->Size);
		SharedBuffer.push_back(NewBuffer);
	}
}
