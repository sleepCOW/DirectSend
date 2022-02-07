#include "ProgressBar.h"
#include <iostream>

ProgressBar::ProgressBar(std::wstring InitialText, size_t BarSize) : Text(InitialText), ConsoleHandle(nullptr)
{
	ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	if (!ConsoleHandle)
	{
		CMD::PrintError() << "ProgressBar can't GetStdHandle error = " << GetLastError() << std::endl;
		return;
	}

	CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;
	GetConsoleScreenBufferInfo(ConsoleHandle, &ScreenBufferInfo);

	PositionToWrite = ScreenBufferInfo.dwCursorPosition;
	
	// Add new line so ProgressBar couldn't overwrite useful log
	CMD::Print() << "\n";

	BarInfo.BarSize = BarSize;
	Text.push_back('[');
	BarInfo.ProgressStart = Text.size();
	for (size_t i = 0; i < BarInfo.BarSize; ++i)
	{
		Text.push_back(' ');
	}
	BarInfo.ProgressEnd = Text.size() - 1;
	Text.push_back(']');
}

void ProgressBar::SetPosition(COORD InPosition)
{
	PositionToWrite = InPosition;
}

void ProgressBar::SetFileSize(LargeInteger FileSize)
{
	TotalSize = FileSize;
}

void ProgressBar::SetCurrentSize(LargeInteger CurrentSize)
{
	CurrentProgress = CurrentSize;
}

LargeInteger ProgressBar::GetCurrentProgress() const
{
	return CurrentProgress;
}

LargeInteger ProgressBar::GetTotalSize() const
{
	return TotalSize;
}

void ProgressBar::Print()
{
	UpdateText();
	DWORD Written;

	if (!WriteConsoleOutputCharacter(ConsoleHandle, Text.data(), Text.size(), PositionToWrite, &Written))
	{
		CMD::Print() << GetLastError() << std::endl;
	}

	double Percent = ((double)CurrentProgress / (double)TotalSize) * 100.;
	COORD PercentToWrite = PositionToWrite;
	PercentToWrite.X += BarInfo.ProgressEnd + 4;
	wchar_t Str[40];
	int StrSize = swprintf(Str, 40, L"%.1f%%  [%.1f/%.1f]GB", Percent, CurrentProgress.ToGBd(), TotalSize.ToGBd());

	if (!WriteConsoleOutputCharacter(ConsoleHandle, Str, StrSize, PercentToWrite, &Written))
	{
		CMD::Print() << GetLastError() << std::endl;
	}
}

void ProgressBar::operator+=(LONGLONG AddProgress)
{
	CurrentProgress += AddProgress;
}

void ProgressBar::UpdateText()
{
	size_t Filled = (double)CurrentProgress / (double)TotalSize * BarInfo.BarSize;

	for (size_t i = 0; i < Filled; ++i)
	{
		Text[BarInfo.ProgressStart + i] = '#';
	}
}
