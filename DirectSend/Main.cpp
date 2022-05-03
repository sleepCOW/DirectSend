#include "Server.h"
#include "Client.h"
#include "DirectSend.h"

struct CMDArgs
{
	String Role;
	String Mode;
	String FileName;
	String Path;
	String Ip;
	String Port;
	size_t MaxRetries = 100u;
	bool   bInfiniteReconnects = false;
};

ConstexprMap<StringView, int, 3> Errors = { {{
	{"NotEnoughArguments"sv, -1},
	{"WrongRole", -2},
	{"WrongMode", -3}
}} };

const char* Usage =
"-----------------------------------HOW TO-----------------------------------\n"
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
	auto ReadStringArg = [Argv](const char* ArgumentName, int& Index, String& OutArgument)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			++Index;
			OutArgument = Argv[Index];
		}
	};
	auto ReadNumArg = [Argv](const char* ArgumentName, int& Index, size_t& OutArgument)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			++Index;
			OutArgument = atoi(Argv[Index]);
		}
	};
	auto ReadArg = [Argv](const char* ArgumentName, int& Index, bool& OutArgument)
	{
		if (strcmp(Argv[Index], ArgumentName) == 0)
		{
			OutArgument = true;
		}
	};

	for (int i = 1; i < Argc; ++i)
	{
		ReadStringArg("-ip", i, Args.Ip);
		ReadStringArg("-port", i, Args.Port);
		ReadStringArg("-path", i, Args.Path);
		ReadStringArg("-file", i, Args.FileName);
		ReadNumArg("-retries", i, Args.MaxRetries);
		ReadArg("-infinite", i, Args.bInfiniteReconnects);
	}
}

int StartApplication(CMDArgs& Args)
{
	NetworkDevice* NetDevice = nullptr;
	// Create proper NetworkDevice
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
		CMD::PrintDebug() << "Wrong role!!!!!!\n\n" << Usage;
		return Errors.At("WrongRole");
	}

	// send or receive file depending on mode
	try
	{
		if (Args.Mode == "send")
			return SendFile(*NetDevice, Args.FileName);
		else if (Args.Mode == "receive")
			return ReceiveFile(*NetDevice, Args.FileName);
	}
	catch (NetworkException& Err)
	{
		CMD::Print() << "Error occurred!\n" << Err.what() << std::endl;
		delete NetDevice;
		throw;
	}
}

int main(int argc, char* argv[])
{
	// At least 6 arguments need to be provided for the program to work, show help if not enough arguments
	if (argc < 6)
	{
		CMD::Print() << Usage;
		return Errors.At("NotEnoughArguments");
	}

	CMDArgs Args;
	Args.Role = argv[1];
	Args.Mode = argv[2];

	FillArguments(Args, argc, argv);

	if (Args.Mode != "send" and Args.Mode != "receive")
	{
		CMD::Print() << "Wrong mode!!!!!!\n\n" << Usage;
		return Errors.At("WrongMode");
	}

	size_t Tries = 0;
	while (Tries < Args.MaxRetries || Args.bInfiniteReconnects)
	{
		++Tries;
		try
		{
			return StartApplication(Args);
		}
		catch (...)
		{
			CMD::Print() << "Trying to restart application in 5 seconds!" << std::endl;
			Sleep(5000);
			CMD::Print() << "Restarting application! " << Args.MaxRetries - Tries << " tries left!" << std::endl;
		}
	}
}