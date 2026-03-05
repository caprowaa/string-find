#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <tlhelp32.h>

// Функция для извлечения строк
void ExtractStrings(const std::vector<char>& buffer, std::set<std::string>& foundStrings) {
    std::string currentAscii;
    std::string currentUnicode;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            currentAscii += buffer[i];
        } else {
            if (currentAscii.length() >= 4) foundStrings.insert(currentAscii);
            currentAscii.clear();
        }
        if (i + 1 < buffer.size()) {
            char c1 = buffer[i];
            char c2 = buffer[i + 1];
            if (c1 >= 32 && c1 <= 126 && c2 == 0) {
                currentUnicode += c1;
                i++;
            } else {
                if (currentUnicode.length() >= 4) foundStrings.insert(currentUnicode);
                currentUnicode.clear();
            }
        }
    }
}

std::set<std::string> GetAllStrings(HANDLE hProcess) {
    std::set<std::string> allStrings;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    MEMORY_BASIC_INFORMATION mbi;
    uint8_t* address = (uint8_t*)si.lpMinimumApplicationAddress;
    while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE))) {
            std::vector<char> buffer(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead)) {
                buffer.resize(bytesRead);
                ExtractStrings(buffer, allStrings);
            }
        }
        address += mbi.RegionSize;
        if (address >= (uint8_t*)si.lpMaximumApplicationAddress) break;
    }
    return allStrings;
}

DWORD GetPidByName(const std::wstring& name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(entry) };
    if (Process32FirstW(snap, &entry)) {
        do { if (name == entry.szExeFile) { pid = entry.th32ProcessID; break; } } while (Process32NextW(snap, &
