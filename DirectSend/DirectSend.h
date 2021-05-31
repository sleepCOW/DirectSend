#pragma once

#include "Helpers.h"
#include "Common.h"

class NetworkDevice;

/**
 * Send file to the host
 * @NetDevice - network device to use
 * @FileName - path to the desired file
 * @return - error code, 0 on success
 */
int SendFile(NetworkDevice& NetDevice, String& FileName);

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
	SendFileDataTask(NetworkDevice& InDevice, Mutex& InBufferMutex, List<Buffer*>& InSharedBuffer, bool& InbFinished);

	void operator()();
private:
	NetworkDevice& Device;
	Mutex& BufferMutex;
	List<Buffer*>& SharedBuffer;
	// Set from other thread
	bool& bFinished;
};

/** Send file tasks end */