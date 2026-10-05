#include "globals.h"

EDIT_HANDLE* g_edit = nullptr;
LOG_HANDLE* g_logger = nullptr;

void LogInfo(const std::wstring& message) {
    if (g_logger) {
        g_logger->info(g_logger, message.c_str());
    }
}

void LogError(const std::wstring& message) {
    if (g_logger) {
        g_logger->error(g_logger, message.c_str());
    }
}
