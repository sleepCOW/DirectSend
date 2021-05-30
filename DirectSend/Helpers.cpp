#include "Helpers.h"

OStream& CMD::Print()
{
	return std::cout;
}

OStream& CMD::PrintError()
{
	return std::cerr;
}
