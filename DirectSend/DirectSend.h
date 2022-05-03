#pragma once

#include "Helpers.h"
#include "Common.h"

class NetworkDevice;
struct FileHandle;
class ProgressBar;

/**
 * Send file to the host
 * @NetDevice - network device to use
 * @FileName - path to the desired file
 * @return - error code, 0 on success
 */
int SendFile(NetworkDevice& NetDevice, String& FilePath);

/**
 * Receive file to the netdevice
 * @NetDevice - network device to use
 * @FileName - path where to store received file
 * @return - error code, 0 on success
 */
int ReceiveFile(NetworkDevice& NetDevice, String& Path);

/** Send file tasks begin */

/**
 * Functor for send file data thread
 */
class SendFileDataTask
{
public:
	SendFileDataTask(NetworkDevice& InDevice, Mutex& InBufferMutex,
					 List<Buffer*>& InSharedBuffer, bool& InbFinished, bool& InbLostConnection);

	void operator()();
private:
	NetworkDevice& Device;
	Mutex& BufferMutex;
	List<Buffer*>& SharedBuffer;
	// Set from other thread
	const bool& bFinished;
	// Used to indicate connection lost for the main thread
	bool& bLostConnection;
};

/** Send file tasks end */

/** Receive file tasks begin */

/**
 * Functor for write to file thread
 */
class WriteToFileTask
{
public:
	WriteToFileTask(NetworkDevice& InDevice, Mutex& InBufferMutex,
					List<Buffer*>& InSharedBuffer, FileHandle& InFile,
					ProgressBar& InPB, bool& InbFinished, bool& InbLostConnection);

	void operator()();
private:
	NetworkDevice& Device;
	Mutex& BufferMutex;
	List<Buffer*>& SharedBuffer;
	FileHandle& File;
	ProgressBar& PB;
	// Set from other thread
	const bool& bFinished;
	const bool& bLostConnection;
};

/**
 * Functor for device to receive file, used in network device listen function
 */
class ReceiveFileListenTask
{
public:
	ReceiveFileListenTask(NetworkDevice& InDevice, Mutex& InBufferMutex,
						  List<Buffer*>& InSharedBuffer, FileHandle& InFile, 
						  ProgressBar& InPB, String InPath, OperationCode& InOpCode, 
						  bool& InbFinished, bool& InbLostConnection);

	void operator()(OperationCode OpCode);

private:
	void ReceiveFileName();
	void ReceiveFileData();


private:
	NetworkDevice& Device;
	Mutex& BufferMutex;
	List<Buffer*>& SharedBuffer;
	FileHandle& File;
	ProgressBar& PB;
	String Path;
	OperationCode& CurrentState;
	bool& bFinished;
	bool& bLostConnection;
};

/** Receive file tasks end */