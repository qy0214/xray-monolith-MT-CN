#include "stdafx.h"

#include "resource.h"
#include "log.h"
#include "TimeUtils.h"

#include "profiler.h"
#include "string_concatenations.h"

static xrLogger* theLogger = nullptr;
XRCORE_API xr_queue <xrLogger::LogRecord>* xrLogger::logData;

xr_string FormatString(LPCSTR fmt, ...)
{
	va_list mark;
	string2048 buf;
	va_start(mark, fmt);
	int sz = _vsnprintf(buf, sizeof(buf) - 1, fmt, mark);
	buf[sizeof(buf) - 1] = 0;
	va_end(mark);
	if (sz) return xr_string(buf);
	return xr_string("");
}

// Timestamp flag and helpers
BOOL logTimestamps = FALSE;
enum Console_mark;
extern bool is_console_mark(Console_mark type);

void Log(const char* s)
{
    theLogger->SimpleMessage(s);
}

void Msg(const char *format, ...)
{
	if (!format)
		return;

	va_list		mark;
	va_start	(mark, format );
	theLogger->Msg(format, mark);
    va_end		(mark);
}

void Log(const char* msg, const char* dop)
{
	if (!msg)
		return;

	if (!dop)
	{
		Log(msg);
		return;
	}

	u32 buffer_size = (xr_strlen(msg) + 1 + xr_strlen(dop) + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);
	strconcat(buffer_size, buf, msg, " ", dop);
	Log(buf);
}

void Log(const char* msg, u32 dop)
{
	if (!msg)
		return;

	u32 buffer_size = (xr_strlen(msg) + 1 + 10 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %d", msg, dop);
	Log(buf);
}

void Log(const char* msg, int dop)
{
	if (!msg)
		return;

	u32 buffer_size = (xr_strlen(msg) + 1 + 11 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %i", msg, dop);
	Log(buf);
}

void Log(const char* msg, float dop)
{
	if (!msg)
		return;

	// actually, float string representation should be no more, than 40 characters,
	// but we will count with slight overhead
	u32 buffer_size = (xr_strlen(msg) + 1 + 64 + 1) * sizeof(char);
	PSTR buf = (PSTR)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s %f", msg, dop);
	Log(buf);
}

void Log (const char *msg, const Fvector &dop)
{
	if (!msg)
		return;

	u32 buffer_size = (xr_strlen(msg) + 2 + 3 * (64 + 1) + 1) * sizeof(char);
	char* buf = (char*)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s (%f,%f,%f)", msg, VPUSH(dop));
	Log(buf);
}

void Log(const char* msg, const Fmatrix& dop)
{
	if (!msg)
		return;

	u32	buffer_size = (xr_strlen(msg) + 2 + 4 * (4 * (64 + 1) + 1) + 1) * sizeof(char);
	char* buf = (char*)_alloca(buffer_size);

	xr_sprintf(buf, buffer_size, "%s:\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n%f,%f,%f,%f\n",
		msg,
		dop.i.x, dop.i.y, dop.i.z, dop._14_,
		dop.j.x, dop.j.y, dop.j.z, dop._24_,
		dop.k.x, dop.k.y, dop.k.z, dop._34_,
		dop.c.x, dop.c.y, dop.c.z, dop._44_
	);
	Log(buf);
}

void LogWinErr(const char* msg, long err_code)
{
	if (!msg)
		return;

	Msg("%s: %s", msg, Debug.error2string(err_code));
}

void xrLogger::Msg(LPCSTR Msg, va_list argList)
{
	if (!Msg)
		return;

	string4096	formattedMessage;
	int MsgSize = _vsnprintf(formattedMessage, sizeof(formattedMessage) - 1, Msg, argList);

	if (MsgSize < 0)
		return;

	formattedMessage[MsgSize] = 0;

	{
		OutputDebugStringA(formattedMessage);
		OutputDebugStringA("\n");
	}

	SimpleMessage(formattedMessage, MsgSize);
}

void xrLogger::PauseLogging()
{
	ResetEvent(hLogThread);
}

void xrLogger::UnpauseLogging()
{
    if (bImmediateMode)
        InternalPrintAllRecords();
    else
	    SetEvent(hLogThread);
}

void xrLogger::SimpleMessage(LPCSTR Message, u32 MessageSize /*= 0*/)
{
	if (!Message)
		return;

	switch (MessageSize)
	{
	case (u32(-1)): return;
	case 0:			MessageSize = xr_strlen(Message); break;
	default:		break;
	}

	xr_string msgToLog = Message;
	if (logTimestamps)
	{
		xr_string t = msgToLog;
		xr_string c = "";
		if (!t.empty() && is_console_mark((Console_mark)t[0]))
		{
            c += t[0];
			c += " ";
			t.erase(0, 1);
		}
		msgToLog = c + "[" + timeInHMSMMM() + "] " + t;
	}

	xrCriticalSectionGuard guard(&logDataGuard);
	if (bIsAlive)
	{
		logData->emplace(LogRecord(msgToLog.c_str(), (u32)msgToLog.size()));
		UnpauseLogging();
	}
	
}

void xrLogger::OpenLogFile()
{
	static bool isLogOpened = false;
	if (!isLogOpened) {
		theLogger->InternalOpenLogFile();
		isLogOpened = true;
	}
}

const string_path& xrLogger::GetLogPath()
{
	return theLogger->logFileName;
}

void xrLogger::EnableFastDebugLog()
{
	theLogger->bFastDebugLog = true;
}

void LogThreadEntryStartup(void* nullParam)
{
	PROF_THREAD("Logger Thread");
	theLogger->UnpauseLogging();
	theLogger->LogThreadEntry();
}

void xrLogger::InitLog()
{
	if (theLogger == nullptr)
	{
		theLogger = new xrLogger;
		xrLogger::logData = new xr_queue <xrLogger::LogRecord>;
		thread_spawn(LogThreadEntryStartup, "X-Ray Log Thread", 0, nullptr);
	}	
}

void xrLogger::InternalFlushLog()
{
	xrCriticalSectionGuard g(logFlushGuard);
	PROF_EVENT("Log Flush")
	if (logFile != nullptr)
	{
		IWriter* mutableWritter = (IWriter*)logFile;
		mutableWritter->flush();
	}
}

void xrLogger::FlushLog()
{
	theLogger->bFlushRequested = true;
	theLogger->UnpauseLogging();
}

void xrLogger::SetImmediateMode(bool enable)
{
	if (theLogger == nullptr)
		return;

	theLogger->bImmediateMode = enable;		
}

void xrLogger::CloseLog()
{
	theLogger->InternalCloseLog();
}

void xrLogger::AddLogCallback(LogCallback logCb)
{
	if (logCb == nullptr)
		return;

	xrCriticalSectionGuard guard(&theLogger->logCallbackGuard);
	theLogger->logCallbackList.push_back(logCb);
}

void xrLogger::RemoveLogCallback(LogCallback logCb)
{
	xrCriticalSectionGuard guard(&theLogger->logCallbackGuard);
	theLogger->logCallbackList.remove(logCb);
}

namespace { void ArchiveLogFile(LPCSTR path); }	// defined below

void xrLogger::InternalCloseLog()
{
	SimpleMessage("[xrLogger] InternalCloseLog called, terminating thread");
	bIsAlive = false;

	InternalPrintAllRecords();
	InternalFlushLog();

	IWriter* tempCopy = (IWriter*)logFile;
	logFile = nullptr;

	if (tempCopy != nullptr)
		FS.w_close(tempCopy);

	// Archive this session too, so the timestamped log is there right after a clean
	// exit. A crash never reaches here, but the crash handlers flush the log and the
	// next launch archives it, so that session is kept as well.
	ArchiveLogFile(logFileName);

	UnpauseLogging();
}

xrLogger::xrLogger()
	: logFile(nullptr), bFastDebugLog(false), 
	bIsAlive(true),
	bFlushRequested(false),
	bImmediateMode(false)
{
	hLogThread = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

xrLogger::~xrLogger()
{
	InternalCloseLog();
}

namespace
{
	// "YYYY-MM-DD_HH-MM-SS" from a file timestamp, or from the local clock.
	void SessionStamp(string_path& out, const FILETIME* source)
	{
		SYSTEMTIME st = {};
		FILETIME local = {};
		if (source && FileTimeToLocalFileTime(source, &local) && FileTimeToSystemTime(&local, &st))
		{
			xr_sprintf(out, "%04u-%02u-%02u_%02u-%02u-%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
			return;
		}
		GetLocalTime(&st);
		xr_sprintf(out, "%04u-%02u-%02u_%02u-%02u-%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	}

	// Moves a log file to "<name>_<timestamp>.log" so sessions never overwrite each
	// other. The timestamp is the file's own last write time, i.e. the end of the
	// session that produced it (crashed sessions included, archived on the next run).
	void ArchiveLogFile(LPCSTR path)
	{
		if (path == nullptr || path[0] == 0)
			return;

		WIN32_FILE_ATTRIBUTE_DATA info = {};
		if (!::GetFileAttributesExA(path, GetFileExInfoStandard, &info))
			return;	// nothing to archive
		if (info.nFileSizeHigh == 0 && info.nFileSizeLow == 0)
			return;	// empty session, nothing worth keeping

		string_path stamp = {};
		SessionStamp(stamp, &info.ftLastWriteTime);

		xr_string base(path);
		const size_t dot = base.rfind('.');
		if (dot != xr_string::npos)
			base.erase(dot);

		xr_string archive = base + "_" + stamp + ".log";
		::MoveFileExA(path, archive.c_str(), MOVEFILE_REPLACE_EXISTING);
	}
}

void xrLogger::InternalOpenLogFile()
{
	// IXRay format
	/*string256 CurrentDate;
	string256 CurrentTime;
	Time time;
	xr_strconcat(CurrentDate, time.GetYearString().c_str(), ".", time.GetMonthString().c_str(), ".", time.GetDayString().c_str());
	xr_strconcat(CurrentTime, time.GetHoursString().c_str(), ".", time.GetMinutesString().c_str(), ".", time.GetSecondsString().c_str());
	xr_strconcat(logFileName, Core.ApplicationName, "-", CurrentDate, "-", CurrentTime, "-", Core.UserName, ".log");*/

	// Vanilla
	xr_strconcat(logFileName, Core.ApplicationName, "_", Core.UserName, ".log");

	if (FS.path_exist("$logs$"))
	{
		FS.update_path(logFileName, "$logs$", logFileName);
	}

	// Keep every session: archive the previous log under its own timestamp instead
	// of the old single ".bkp" name, which was overwritten on every launch and,
	// being relative to the working directory, usually did not match the log path.
	ArchiveLogFile(logFileName);

	logFile = FS.w_open(logFileName);
	CHECK_OR_EXIT(logFile, "Can't create log file");
}

void xrLogger::InternalPrintRecord()
{
	LogRecord theRecord;

	{
		xrCriticalSectionGuard g(&logDataGuard);
		theRecord = logData->front();
		logData->pop();
	}

	xr_vector<xr_string> LogLines = theRecord.Message.Split('\n');

	string256 TimeOfDay = {};

	PROF_EVENT("Log: Apply Messages")
		int TimeOfDaySize = 0;
	for (const xr_string& line : LogLines)
	{
		string4096 finalLine;
		xr_strconcat(finalLine, TimeOfDay, line.c_str());

		int FinalSize = TimeOfDaySize + (int)line.size();
		// line is ready, ready up everything

		// Output to MSVC debug output
		if (IsDebuggerPresent() && !bFastDebugLog)
		{
			OutputDebugStringA(finalLine);
			OutputDebugStringA("\n");
		}

		// demonized: add tempLogData in case the logFile initialization takes time
		if (logFile != nullptr)
		{
			IWriter* mutableWritter = (IWriter*)logFile;

			while (!tempLogData.empty())
			{
				auto line = tempLogData.front();
				tempLogData.pop();
				mutableWritter->w(line.c_str(), line.size());
				mutableWritter->w("\r\n", 2);
			}

			// write to file
			mutableWritter->w(finalLine, FinalSize);
			mutableWritter->w("\r\n", 2);
			if (bImmediateMode)
                InternalFlushLog();
		}
		else
		{
			tempLogData.push(finalLine);
		}

		xrCriticalSectionGuard guard(&logCallbackGuard);
		for (const LogCallback& FnCallback : logCallbackList)
		{
			FnCallback(finalLine);
		}
	}
}

void xrLogger::InternalPrintAllRecords()
{
	while (true)
	{
		{
			xrCriticalSectionGuard g(&logDataGuard);
			if (logData->empty())
				break;
		}
		InternalPrintRecord();
	}
}

void xrLogger::LogThreadEntry()
{
	auto FlushLogIfRequestedLambda = [this]()
		{
			if (bFlushRequested)
			{
				InternalFlushLog();
				bFlushRequested = false;
			}
		};

	while (true)
	{
		if (!bIsAlive)
		{
			CloseHandle(hLogThread);
			return;
		}
		
		WaitForSingleObject(hLogThread, INFINITE);
		{
			PROF_EVENT("Log Frame");
			InternalPrintAllRecords();
			PauseLogging();
			FlushLogIfRequestedLambda();
		}
	}
}

xrLogger::LogRecord::LogRecord(LPCSTR Msg, u32 sizeMsg)
	: Message(Msg, sizeMsg)
{}
