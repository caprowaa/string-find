#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <tlhelp32.h>

// Функция для извлечения строк (ASCII и UTF-16) из буфера памяти
void ExtractStrings(const std::vector<char>& buffer, std::set<std::string>& foundStrings) {
    std::string currentAscii;
    std::string currentUnicode;

    for (size_t i = 0; i < buffer.size(); ++i) {
        // 1. Поиск ASCII
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            currentAscii += buffer[i];
        } else {
            if (currentAscii.length() >= 4) foundStrings.insert(currentAscii);
            currentAscii.clear();
        }

        // 2. Поиск Unicode (UTF-16LE)
        if (i + 1 < buffer.size()) {
            char c1 = buffer[i];
            char c2 = buffer[i + 1];
            if (c1 >= 32 && c1 <= 126 && c2 == 0) {
                currentUnicode += c1;
                i++; // Пропускаем нулевой байт
            } else {
                if (currentUnicode.length() >= 4) foundStrings.insert(currentUnicode);
                currentUnicode.clear();
            }
        }
    }
}

// Функция обхода всех регионов памяти процесса
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

// Поиск процесса по имени
DWORD GetPidByName(const std::wstring& name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(entry) };
    if (Process32FirstW(snap, &entry)) {
        do {
            if (name == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

int main() {
    // Настройка кодировки UTF-8 для консоли
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    DWORD pid = GetPidByName(L"javaw.exe");
    if (!pid) {
        std::cout << "[-] javaw.exe (Minecraft) not found! / Майнкрафт не найден!" << std::endl;
        system("pause");
        return 1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        std::cout << "[-] Access Denied. Run as Admin! / Запустите от имени АДМИНИСТРАТОРА!" << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "[*] Scanning memory BEFORE injection... / Снимаю базу ДО инъекции..." << std::endl;
    auto before = GetAllStrings(hProcess);
    std::cout << "[+] Base ready (" << before.size() << " strings). Press ENTER AFTER you inject the cheat... / База готова. Жми ENTER ПОСЛЕ инъекции..." << std::endl;
    std::cin.get();

    std::cout << "[*] Scanning memory AFTER injection... / Снимаю второй снимок..." << std::endl;
    auto after = GetAllStrings(hProcess);

    std::cout << "\n[!] NEW STRINGS DETECTED / НОВЫЕ СТРИНГИ:\n--------------------------------" << std::endl;
    int count = 0;
    for (const auto& s : after) {
        if (before.find(s) == before.end()) {
            std::cout << "[NEW] " << s << std::endl;
            count++;
        }
    }

    CloseHandle(hProcess);
    std::cout << "\n[*] Analysis finished. New strings found: " << count << std::endl;
    std::cout << "[*] Анализ завершен. Найдено новых строк: " << count << std::endl;
    system("pause");
    return 0;
}
