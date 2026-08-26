// Minimal stand-ins for the HPL1 low-level system so the unit tests can link
// the engine's pure math sources without dragging SDL, AngelScript, or a
// window system along. Only the symbols actually referenced by the compiled
// math translation units are implemented here.
#include "system/LowLevelSystem.h"
#include "math/BoundingVolume.h"

#include <cstdio>
#include <cstdarg>

namespace hpl {

	// Math.cpp references a handful of cBoundingVolume methods (clip-rect and
	// collision helpers the tests never call). Real implementations live in
	// BoundingVolume.cpp together with the serialization system; stubbing them
	// keeps that dependency tree out of the test binary.
	cVector3f cBoundingVolume::GetMin() { return cVector3f(0, 0, 0); }
	cVector3f cBoundingVolume::GetMax() { return cVector3f(0, 0, 0); }
	void cBoundingVolume::UpdateSize() {}

	void SetLogFile(const tWString&) {}
	void FatalError(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n', stderr);
	}
	void Error(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n', stderr);
	}
	void Warning(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n', stderr);
	}
	void Log(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(stdout, fmt, args);
		va_end(args);
	}

	void SetUpdateLogFile(const tWString&) {}
	void ClearUpdateLogFile() {}
	void SetUpdateLogActive(bool) {}
	void LogUpdate(const char*, ...) {}

}
