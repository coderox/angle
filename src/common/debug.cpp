//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.cpp: Debugging utilities.

#include "common/debug.h"

#include <stdarg.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "common/angleutils.h"
#include "common/Optional.h"

namespace gl
{

namespace
{

DebugAnnotator *g_debugAnnotator = nullptr;

constexpr std::array<const char *, LOG_NUM_SEVERITIES> g_logSeverityNames = {
    {"EVENT", "WARN", "ERR"}};

constexpr const char *LogSeverityName(int severity)
{
    return (severity >= 0 && severity < LOG_NUM_SEVERITIES) ? g_logSeverityNames[severity]
                                                            : "UNKNOWN";
}

bool ShouldCreateLogMessage(LogSeverity severity)
{
#if defined(ANGLE_TRACE_ENABLED)
    return true;
#elif defined(ANGLE_ENABLE_ASSERTS)
    return severity == LOG_ERR;
#else
    return false;
#endif
}

}  // namespace

namespace priv
{

bool ShouldCreatePlatformLogMessage(LogSeverity severity)
{
#if defined(ANGLE_TRACE_ENABLED)
    return true;
#else
    return severity != LOG_EVENT;
#endif
}

}  // namespace priv

bool DebugAnnotationsActive()
{
#if defined(ANGLE_ENABLE_DEBUG_ANNOTATIONS)
    return g_debugAnnotator != nullptr && g_debugAnnotator->getStatus();
#else
    return false;
#endif
}

bool DebugAnnotationsInitialized()
{
    return g_debugAnnotator != nullptr;
}

void InitializeDebugAnnotations(DebugAnnotator *debugAnnotator)
{
    UninitializeDebugAnnotations();
    g_debugAnnotator = debugAnnotator;
}

void UninitializeDebugAnnotations()
{
    // Pointer is not managed.
    g_debugAnnotator = nullptr;
}

ScopedPerfEventHelper::ScopedPerfEventHelper(const char *format, ...)
{
#if !defined(ANGLE_ENABLE_DEBUG_TRACE)
    if (!DebugAnnotationsActive())
    {
        return;
    }
#endif  // !ANGLE_ENABLE_DEBUG_TRACE

    va_list vararg;
    va_start(vararg, format);
    std::vector<char> buffer(512);
    size_t len = FormatStringIntoVector(format, vararg, buffer);
    ANGLE_LOG(EVENT) << std::string(&buffer[0], len);
    va_end(vararg);
}

ScopedPerfEventHelper::~ScopedPerfEventHelper()
{
    if (DebugAnnotationsActive())
    {
        g_debugAnnotator->endEvent();
    }
}

LogMessage::LogMessage(const char *function, int line, LogSeverity severity)
    : mFunction(function), mLine(line), mSeverity(severity)
{
}

LogMessage::~LogMessage()
{
    if (DebugAnnotationsInitialized() && (mSeverity == LOG_ERR || mSeverity == LOG_WARN))
    {
        g_debugAnnotator->logMessage(*this);
    }
    else
    {
        trace();
    }
}

void LogMessage::trace() const
{
    if (!ShouldCreateLogMessage(mSeverity))
    {
        return;
    }

    std::ostringstream stream;
    stream << LogSeverityName(mSeverity) << ": ";
    // EVENT() don't require additional function(line) info
    if (mSeverity != LOG_EVENT)
    {
        stream << mFunction << "(" << mLine << "): ";
    }
    stream << mStream.str() << std::endl;
    std::string str(stream.str());

    if (DebugAnnotationsActive())
    {
        std::wstring formattedWideMessage(str.begin(), str.end());

        switch (mSeverity)
        {
            case LOG_EVENT:
                g_debugAnnotator->beginEvent(formattedWideMessage.c_str());
                break;
            default:
                g_debugAnnotator->setMarker(formattedWideMessage.c_str());
                break;
        }
    }

    if (mSeverity == LOG_ERR)
    {
        std::cerr << str;
#if !defined(NDEBUG) && defined(_MSC_VER)
        OutputDebugStringA(str.c_str());
#endif  // !defined(NDEBUG) && defined(_MSC_VER)
    }

#if defined(ANGLE_ENABLE_DEBUG_TRACE)
#if defined(NDEBUG)
    if (mSeverity == LOG_EVENT || mSeverity == LOG_WARN)
    {
        return;
    }
#endif  // NDEBUG
    static std::ofstream file(TRACE_OUTPUT_FILE, std::ofstream::app);
    if (file)
    {
        file.write(str.c_str(), str.length());
        file.flush();
    }

#if defined(ANGLE_ENABLE_DEBUG_TRACE_TO_DEBUGGER)
    OutputDebugStringA(str.c_str());
#endif  // ANGLE_ENABLE_DEBUG_TRACE_TO_DEBUGGER

#endif  // ANGLE_ENABLE_DEBUG_TRACE
}

LogSeverity LogMessage::getSeverity() const
{
    return mSeverity;
}

std::string LogMessage::getMessage() const
{
    std::ostringstream stream;
    stream << mFunction << "(" << mLine << "): " << mStream.str() << std::endl;
    return stream.str();
}

#if defined(ANGLE_PLATFORM_WINDOWS)
std::ostream &operator<<(std::ostream &os, const FmtHR &fmt)
{
    os << "HRESULT: ";
    return FmtHexInt(os, fmt.mHR);
}
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

}  // namespace gl
