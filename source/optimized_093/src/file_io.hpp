#pragma once
#include <filesystem>
#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace ofo {
inline std::vector<unsigned char> read_file(const std::filesystem::path& path, size_t limit=512u*1024*1024) {
#ifdef _WIN32
 struct File {HANDLE h;~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}} file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr)};
 if(file.h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open file: "+path.u8string()+" Win32="+std::to_string(GetLastError()));
 LARGE_INTEGER size{};if(!GetFileSizeEx(file.h,&size)||size.QuadPart<0||static_cast<unsigned long long>(size.QuadPart)>limit)throw std::runtime_error("File size invalid or exceeds limit: "+path.u8string());
 std::vector<unsigned char> bytes(static_cast<size_t>(size.QuadPart));size_t offset=0;
 while(offset<bytes.size()){DWORD count=0,wanted=DWORD(std::min<size_t>(bytes.size()-offset,1024*1024));if(!ReadFile(file.h,bytes.data()+offset,wanted,&count,nullptr)||!count)throw std::runtime_error("Incomplete file read: "+path.u8string());offset+=count;}return bytes;
#else
 auto size=std::filesystem::file_size(path);if(size>limit)throw std::runtime_error("File exceeds limit: "+path.u8string());std::vector<unsigned char> bytes(static_cast<size_t>(size));std::ifstream file(path,std::ios::binary);if(!file||(!bytes.empty()&&!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())))throw std::runtime_error("Incomplete file read: "+path.u8string());return bytes;
#endif
}
inline void write_file_atomic(const std::filesystem::path& path,const std::string& text) {
 auto temp=path;temp+=".tmp";
#ifdef _WIN32
 HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create file: "+temp.u8string());size_t offset=0;bool ok=true;DWORD error=0;
 while(offset<text.size()){DWORD count=0,wanted=DWORD(std::min<size_t>(text.size()-offset,1024*1024));if(!WriteFile(file,text.data()+offset,wanted,&count,nullptr)||!count){ok=false;error=GetLastError();break;}offset+=count;}
 if(ok&&!FlushFileBuffers(file)){ok=false;error=GetLastError();}CloseHandle(file);if(!ok){DeleteFileW(temp.c_str());throw std::runtime_error("Cannot write file Win32="+std::to_string(error));}
 if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=GetLastError();DeleteFileW(temp.c_str());throw std::runtime_error("Cannot replace file Win32="+std::to_string(error));}
#else
 {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(text.data(),text.size());file.flush();if(!file)throw std::runtime_error("Cannot write file: "+temp.u8string());}std::filesystem::rename(temp,path);
#endif
}
}
