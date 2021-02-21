#pragma once

#include "Common.h"

// Upload: [####################]  77%  [4.3/16.5]GB

class ProgressBar
{
public:
	ProgressBar(std::wstring InitialText, size_t BarSize);

	void SetPosition(COORD InPosition);
	void SetFileSize(LargeInteger FileSize);
	void SetCurrentSize(LargeInteger CurrentSize);

	LargeInteger GetCurrentProgress() const;
	LargeInteger GetTotalSize() const;

	void Print();

	void operator+=(LONGLONG AddProgress);

private:
	void UpdateText();

	struct FBarInfo
	{
		size_t ProgressStart = 0;
		size_t ProgressEnd = 0;
		size_t SizeStart = 0;
		size_t BarSize = 0;
	};

protected:
	HANDLE ConsoleHandle;
	COORD PositionToWrite;
	LargeInteger CurrentProgress;
	LargeInteger TotalSize;
	FBarInfo BarInfo;
	std::wstring Text;
};

