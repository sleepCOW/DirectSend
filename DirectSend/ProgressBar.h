#pragma once

#include "Helpers.h"
#include "Common.h"

/**
 * Command line progress bar that holds information in following format
 * Upload: [####################]  77%  [4.3/16.5]GB
 */
class ProgressBar
{
public:
	ProgressBar(std::wstring InitialText, size_t BarSize);

	void SetPosition(COORD InPosition);
	void SetFileSize(LargeInteger FileSize);
	void SetCurrentSize(LargeInteger CurrentSize);

	LargeInteger GetCurrentProgress() const;
	LargeInteger GetTotalSize() const;

	/** Print current progress bar state in formatted manner to the cout stream */
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

