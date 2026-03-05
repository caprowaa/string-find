#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <fstream>
#include <tlhelp32.h>

// Базовый фильтр мусора
bool IsJunk(const std::string& s) {
    if (s.find("minecraft:") != std::string::npos) return true;
    if (s.find("textures/") != std::string::npos) return true;
    if (s.find("gui.") != std::string::npos) return true;
    if (s.length() > 60) return true; // Слишком длинные строки обычно логи рендера
    return false;
}

void ExtractStrings(const std::vector<char>& buffer, std::set<std::string>& foundStrings) {
    std::string currentAscii;
    std::string currentUnicode;
    for (size_t i = 0; i < buffer.size(); ++i) {
        unsigned char c = (unsigned char)buffer[i];
        if (c >= 32 && c <= 255) { 
            currentAscii += buffer[i];
        } else {
            if (currentAscii.length() >= 4) foundStrings.insert(currentAscii);
            currentAscii.clear();
        }
        if (i + 1 < buffer.size()) {
            unsigned char c1 = (unsigned char)buffer[i];
            unsigned char c2 = (unsigned char)buffer[i + 1];
            if (c1 >= 32 && c1 <= 255 && c2 == 0) {
                currentUnicode += (char)c1;
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
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS) && !(mbi.Protect & PAGE_GUARD)) {
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
        do { if (name == entry.szExeFile) { pid = entry.th32ProcessID; break; } } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

int main() {
    SetConsoleCP(65001); SetConsoleOutputCP(65001);
    DWORD pid = GetPidByName(L"javaw.exe");
    if (!pid) { std::cout << "[-] Minecraft not found!" << std::endl; system("pause"); return 1; }
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    std::cout << "[1/3] Снимаю базу ЧИСТОЙ игры. Жди..." << std::endl;
    auto before = GetAllStrings(hProcess);
    std::cout << "[+] База готова (" << before.size() << " строк)." << std::endl;

    std::cout << "\n[!] ИНЖЕКТИ СОФТ И НАЖМИ ENTER..." << std::endl;
    std::cin.get();
    std::cout << "[2/3] Снимаю базу ЗАИНЖЕКЧЕННОГО софта..." << std::endl;
    auto afterInject = GetAllStrings(hProcess);

    std::cout << "\n[!] ЖМИ SELF-DESTRUCT В СОФТЕ И НАЖМИ ENTER В ЭТОЙ КОНСОЛИ..." << std::endl;
    std::cin.get();
    std::cout << "[3/3] Ищу остаточные следы (Leaks)..." << std::endl;
    auto afterDestruct = GetAllStrings(hProcess);

    std::ofstream out("leaks.txt");
    int count = 0;
    for (const auto& s : afterDestruct) {
        // Условие: строки не было в начале, она была в софте и ОСТАЛАСЬ после деструкта
        if (before.find(s) == before.end() && afterInject.find(s) != afterInject.end()) {
            if (!IsJunk(s)) {
                out << s << "\n";
                count++;
            }
        }
    }
    out.close();

    std::cout << "\n[+] АНАЛИЗ ЗАВЕРШЕН!" << std::endl;
    std::cout << "[+] Найдено подозрительных строк: " << count << std::endl;
    std::cout << "[+] Все результаты сохранены в файл: leaks.txt" << std::endl;
    
    CloseHandle(hProcess);
    system("pause");
    return 0;
}
