#pragma once
#include <cstdint>
#include <string>
#include <vector>

// UTF-8(std::string)をUTF-16(std::wstring)に変換する。LPCWSTRを要求するSDK関数に渡す用途。
std::wstring Utf8ToWide(const std::string& s);

// UTF-16(std::wstring)をUTF-8(std::string)に変換する。SDKから返るLPCWSTRをjsonに格納する用途。
std::string WideToUtf8(const std::wstring& s);

// バイナリデータをBase64文字列に変換する。画像データ等をJSONに埋め込む用途。
std::string Base64Encode(const std::vector<uint8_t>& data);
