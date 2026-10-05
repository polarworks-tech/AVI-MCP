#include "string_utils.h"

#include <windows.h>

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) {
        return "";
    }
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string Base64Encode(const std::vector<uint8_t>& data) {
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        result += kTable[(n >> 18) & 0x3F];
        result += kTable[(n >> 12) & 0x3F];
        result += kTable[(n >> 6) & 0x3F];
        result += kTable[n & 0x3F];
    }

    const size_t remaining = data.size() - i;
    if (remaining == 1) {
        const uint32_t n = data[i] << 16;
        result += kTable[(n >> 18) & 0x3F];
        result += kTable[(n >> 12) & 0x3F];
        result += "==";
    } else if (remaining == 2) {
        const uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        result += kTable[(n >> 18) & 0x3F];
        result += kTable[(n >> 12) & 0x3F];
        result += kTable[(n >> 6) & 0x3F];
        result += "=";
    }

    return result;
}
