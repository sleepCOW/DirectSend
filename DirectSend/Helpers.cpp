#include "Helpers.h"

OStream& CMD::Print()
{
	return std::cout;
}

// #TODO: Print error for both Debug/Release with buffer flush!

// Works for non-release builds
OStream& CMD::PrintDebug()
{
	return std::cerr;
}
