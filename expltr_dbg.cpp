#include <windows.h>
#include <iostream>
#include <array>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <shellapi.h>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <mutex>
#include <atomic>   
#include <processthreadsapi.h>
#include <fstream>

std::mutex regMutex;
std::atomic<bool> keepSending{false};
std::unordered_map<DWORD, HANDLE> threadHandles;
std::string oldRip = "";
std::string oldRbp = "";
std::string file_pth;
std::string rip = "";
std::string global_rip_anal = "";
std::string global_rbp_anal = "";
std::string rbp = "";
int processID = 0;
std::fstream analyticsFile = nullptr;

struct EnumWindowData {
	DWORD targetPid;
	HWND foundHwnd;
};

HWND GetConsoleWindowForProcess(DWORD pid)
{
	HWND hwnd = nullptr;

	if (AttachConsole(pid))
	{
		hwnd = GetConsoleWindow();
		FreeConsole();
	}

	return hwnd;
}

void ForceFocusOnProcess(DWORD pid)
{
	AllowSetForegroundWindow(pid);

	HWND hwnd = nullptr;

	// Konsole braucht evtl. kurz, bis sie existiert
	for (int i = 0; i < 20 && hwnd == nullptr; i++)
	{
		hwnd = GetConsoleWindowForProcess(pid);
		if (hwnd == nullptr)
			Sleep(50);
	}

	if (hwnd != nullptr)
	{
		ShowWindow(hwnd, SW_SHOW);
		SetForegroundWindow(hwnd);
		BringWindowToTop(hwnd);
	}
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
std::string toPrintableAscii(const std::string& data)
{
    std::string result;
    result.reserve(data.size());
    for (unsigned char c : data)
        result += (c >= 32 && c <= 126) ? (char)c : '.';
    return result;
}

std::string getMemContent(HANDLE process, ULONG_PTR address, SIZE_T ReadUntil_val){
    //morgen HIER als erstes die Size rausfinden
    MEMORY_BASIC_INFORMATION mbi = {};
	if (!VirtualQueryEx(process, (LPCVOID)address, &mbi, sizeof(mbi)))
		return "<unreadable>";
	if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS) || (mbi.Protect & PAGE_GUARD))
		return "<invalid addr>";

	std::string content(ReadUntil_val, '\0');
	SIZE_T bytesRead = 0;
	if (!ReadProcessMemory(process, (LPCVOID)address, content.data(), ReadUntil_val, &bytesRead) || bytesRead == 0)
		return "<read failed>";

	content.resize(bytesRead);
	return toPrintableAscii(content);
}



void sendRipRbp(){
    std::string oldRip = "";
    std::string oldRbp = "";

    while (keepSending.load())
    {
        std::string curRip, curRbp;
        {
            std::lock_guard<std::mutex> lock(regMutex);
            curRip = rip;
            curRbp = rbp;
        }

        if (curRip != oldRip || curRbp != oldRbp)
        {
            oldRip = curRip;
            oldRbp = curRbp;
            
            HANDLE globalHandle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processID);
            if (globalHandle == NULL){
                MessageBoxA(nullptr, "Couldn't create Process-Handle to read content. Please run Exploitra as administrator!", "Warning", MB_OK | MB_ICONWARNING);
                exit(0);
            }   
            uint64_t  rip_int;
            uint64_t  rbp_int;
            try{
                rip_int = std::stoull(oldRip, nullptr, 16);
                rbp_int = std::stoull(oldRbp, nullptr, 16);
            }catch (const std::exception&){
                Sleep(30);
                continue;
            }


            ULONG_PTR startAddr = static_cast<ULONG_PTR>(rip_int);
            ULONG_PTR startMax = 0x50;
            std::string rip_content = getMemContent(globalHandle, startAddr, startMax);
            
            startAddr = static_cast<ULONG_PTR>(rbp_int);
            startMax = 0x100;
            std::string rbp_content = getMemContent(globalHandle, startAddr, startMax);

            //hier dann später (morgen) die inhalte als Value global speichern für die Analyse datei!!


            auto readOrFallback = [&](uint64_t reg_val, SIZE_T size) -> std::string {
                std::string content = getMemContent(globalHandle, (ULONG_PTR)reg_val, size);
                if (content.empty() || content[0] == '<') {
                    uint8_t bytes[8];
                    memcpy(bytes, &reg_val, 8);
                    return toPrintableAscii(std::string((char*)bytes, 8));
                }
                return content;
            };

            std::string rip_ascii = readOrFallback(rip_int, 0x50);
            std::string rbp_ascii = readOrFallback(rbp_int, 0x50);
            global_rip_anal = rip_ascii;
            global_rbp_anal = rbp_ascii;
            printf("RIP_VAL:0x%s (%s)_\n", oldRip.c_str(), rip_ascii.c_str());
            printf("RBP_VAL:0x%s (%s)_\n", oldRbp.c_str(), rbp_ascii.c_str());
            fflush(stdout);
        }
        Sleep(30);
    }
}



std::string get_sha256_powershell(const std::string& filePath)
{
    std::string command =
        "powershell.exe -NoProfile -Command "
        "\"(Get-FileHash -LiteralPath '" + filePath + "' -Algorithm SHA256).Hash\"";

    std::array<char, 128> buffer{};
    std::string result;

    std::unique_ptr<FILE, decltype(&_pclose)>
        pipe(_popen(command.c_str(), "r"), _pclose);

    if (!pipe)
        throw std::runtime_error("PowerShell konnte nicht gestartet werden.");

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))
        result += buffer.data();

    while (!result.empty() &&
           (result.back() == '\n' ||
            result.back() == '\r' ||
            result.back() == ' '  ||
            result.back() == '\t'))
    {
        result.pop_back();
    }

    return result;
}

char* GetExceptionName(DWORD code){
    switch (code)
    {
        case EXCEPTION_ACCESS_VIOLATION:         return "Access Violation";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "Array Bounds Exceeded";
        case EXCEPTION_BREAKPOINT:               return "Breakpoint";
        case EXCEPTION_DATATYPE_MISALIGNMENT:    return "Datatype Misalignment";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "Float Divide By Zero";
        case EXCEPTION_FLT_INVALID_OPERATION:    return "Float Invalid Operation";
        case EXCEPTION_FLT_OVERFLOW:             return "Float Overflow";
        case EXCEPTION_FLT_UNDERFLOW:            return "Float Underflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION:      return "Illegal Instruction";
        case EXCEPTION_IN_PAGE_ERROR:            return "In Page Error";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "Integer Divide By Zero";
        case EXCEPTION_INT_OVERFLOW:             return "Integer Overflow";
        case EXCEPTION_PRIV_INSTRUCTION:         return "Privileged Instruction";
        case EXCEPTION_SINGLE_STEP:              return "Single Step";
        case EXCEPTION_STACK_OVERFLOW:           return "Stack Overflow";
        case 0xC0000409:                         return "Stack Buffer Overrun (/GS)";
        case 0xC0000374:                         return "Heap Corruption";
        case 0xE06D7363:                         return "C++ Exception (throw)";
        case 0xE0434352:                         return ".NET (CLR) Exception";
        case 0x406D1388:                         return "Thread Naming (harmlos, MSVC-Debug-Trick)";
        default:                                 return "Unknown";
    }
}
std::string generate_random_string(size_t length) {
    // Character set: alphanumeric
    const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    
    
    std::random_device rd;
    std::mt19937 generator(rd());
    
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += characters[distribution(generator)];
    }
    
    return result;
}
int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::getline(std::cin, file_pth);
    printf("Received File-Path %s [DBG]\n", file_pth.c_str()); 
    fflush(stdout);
    //virus check for file hash
    int choice1 = MessageBoxA(nullptr, "WARNING! This Binary COULD be harmful if you never ran it before. Do you want to take a quick Virus-Total Scan?", "Warning!", MB_YESNO | MB_ICONWARNING);
    if (choice1 == IDYES){
        std::cout << "Building file-hash.. [DBG]\n";
        std::string hashToBuild = get_sha256_powershell(file_pth);
        if (hashToBuild.size() != 64) {
            std::cerr << "Invalid SHA-256: " << hashToBuild << '\n';
            return 1;
        }
        std::string fullUrl = "https://www.virustotal.com/gui/file/"+hashToBuild;
        ShellExecuteA(0, 0, fullUrl.c_str(), 0, 0, SW_SHOW);
    }

    int choice = MessageBoxA(nullptr, "WARNING: This software could be harmful if you don't know what it does or\nif you're unaware of who the publisher is.\nAre you sure you want to run the binary? This is the last opportunity to stop!", "STOP RIGHT HERE!", MB_YESNO | MB_ICONWARNING);
    
    if (choice == IDYES)
    {
        std::string::size_type const p(file_pth.find_last_of("."));
        std::string fileFul = file_pth.substr(0, p);
        std::string analyticFileFullName = fileFul + ".txt";
        
        std::fstream analyticsFile_original(analyticFileFullName, std::ios::out | std::ios::app);
        if (!analyticsFile_original.is_open()){
            MessageBoxA(nullptr, "Couldn't create a .txt-Analytics-file here.\nTry moving the installation-path of Exploitra to a location\nwhere you have access to\n, as an example Documents or Desktop.", "Warning", MB_OK | MB_ICONWARNING);
        }
        SECURITY_ATTRIBUTES secAttrStruct = {0};
        STARTUPINFOA startInfoStruct = {0};
        PROCESS_INFORMATION procInfo = {0};
        DEBUG_EVENT debugEvent = {0};
        startInfoStruct.cb = sizeof(STARTUPINFOA);
        std::cout << "Starting Process.. [DBG]\n";
        analyticsFile_original << "\n=.=.= Analytics-File Generated (or Re-Opened) by Exploitra =.=.=\n";
        auto start = std::chrono::system_clock::now();
        std::time_t end_time = std::chrono::system_clock::to_time_t(start);
        std::string time_and_date = std::ctime(&end_time);
        analyticsFile_original << "Generated at " + time_and_date;
        analyticsFile_original << "\nFile-Path > " + file_pth + "\n";
        analyticsFile_original << "Optional Full-File-Name (if Provided by the Researcher) > \n";
        int res =  CreateProcessA(file_pth.c_str(), nullptr, &secAttrStruct, nullptr, TRUE, DEBUG_ONLY_THIS_PROCESS  | CREATE_NEW_CONSOLE, nullptr, nullptr, &startInfoStruct, &procInfo);
        if (res == 0){
            printf("Process creation failed -> %d\n", GetLastError());
            return 0;
        }
        else{
            printf("Process created successfully. [DBG]\n");
            processID = procInfo.dwProcessId;
            ForceFocusOnProcess(procInfo.dwProcessId);
        }
        
        CONTEXT cpu_regCon = {0};
        cpu_regCon.ContextFlags = CONTEXT_FULL;
        keepSending = TRUE;
        std::thread sendRegs(sendRipRbp);
        bool sawFirstBreakpoint = false;
        while (true)
        {
            BOOL gotEvent = WaitForDebugEvent(&debugEvent, 50); 

            if (!gotEvent)
            {
              
                for (auto& [tid, hThread] : threadHandles)
                {
                    if (SuspendThread(hThread) == (DWORD)-1) continue;

                    CONTEXT ctx = {};
                    ctx.ContextFlags = CONTEXT_FULL;
                    if (GetThreadContext(hThread, &ctx))
                    {
                        char buf[32];
                        std::lock_guard<std::mutex> lock(regMutex);
                        snprintf(buf, sizeof(buf), "%llX", ctx.Rip);
                        rip = buf;
                        snprintf(buf, sizeof(buf), "%llX", ctx.Rbp);
                        rbp = buf;
                    }
                    ResumeThread(hThread);
                }
                continue; 
            }

            HANDLE hCurrentThread = nullptr;
            auto it = threadHandles.find(debugEvent.dwThreadId);
            if (it != threadHandles.end())
                hCurrentThread = it->second;

            if (hCurrentThread && GetThreadContext(hCurrentThread, &cpu_regCon))
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%llX", cpu_regCon.Rip);
                std::string newRip = buf;
                snprintf(buf, sizeof(buf), "%llX", cpu_regCon.Rbp);
                std::string newRbp = buf;

                std::lock_guard<std::mutex> lock(regMutex);
                rip = newRip;
                rbp = newRbp;
            }

            DWORD continueStatus = DBG_CONTINUE;
            
            if (debugEvent.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
            {
                threadHandles[debugEvent.dwThreadId] = debugEvent.u.CreateProcessInfo.hThread;
                if (debugEvent.u.CreateProcessInfo.hFile)
                    CloseHandle(debugEvent.u.CreateProcessInfo.hFile); // sonst handle leak
            }
            else if (debugEvent.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
            {
                threadHandles[debugEvent.dwThreadId] = debugEvent.u.CreateThread.hThread;
            }
            else if (debugEvent.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
            {
                if (debugEvent.u.LoadDll.hFile)
                    CloseHandle(debugEvent.u.LoadDll.hFile); // sonst Handle leak
            }
            else if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                DWORD exCode = debugEvent.u.Exception.ExceptionRecord.ExceptionCode;
                char* exceptionName = GetExceptionName(exCode);

                bool isInitialBreakpoint = (exCode == EXCEPTION_BREAKPOINT && !sawFirstBreakpoint);

                if (isInitialBreakpoint)
                {
                    sawFirstBreakpoint = true;
                    continueStatus = DBG_CONTINUE;
                   
                }else{
                    if (debugEvent.u.Exception.dwFirstChance == 0)
                    {
                        auto start = std::chrono::system_clock::now();
                        std::time_t end_time2 = std::chrono::system_clock::to_time_t(start);
                        analyticsFile_original << "\n\n[EXCEPTION CATCHED BY EXPLOITRA AT " << std::ctime(&end_time2) << "]\n";
                        analyticsFile_original << "DESCRIPTION STARTS HERE> \n";
                        analyticsFile_original << "Exception-Type: " << exceptionName << std::endl;
                        analyticsFile_original << "FirstChance-Flag (by Debugger) > " << debugEvent.u.Exception.dwFirstChance << std::endl;
                        analyticsFile_original
                            << "RIP-Value: 0x"
                            << std::hex
                            << cpu_regCon.Rip
                            << std::dec
                            << std::endl;

                        analyticsFile_original
                            << "RBP-Value: 0x"
                            << std::hex
                            << cpu_regCon.Rbp
                            << std::dec
                            << std::endl;

                        analyticsFile_original
                            << "RIP-Memory: "
                            << global_rip_anal
                            << std::endl;

                        analyticsFile_original
                            << "RBP-Memory: "
                            << global_rbp_anal
                            << std::endl;
                        analyticsFile_original << "Extra Information (if Provided by the Researcher) > "<< std::endl;
                        analyticsFile_original << "================================================================="<< std::endl;
                        printf("Exception catched! Name: %s, FirstChance: %d, TID: %lu, RIP: %llX [DBG]\n",
                        exceptionName, debugEvent.u.Exception.dwFirstChance, debugEvent.dwThreadId, cpu_regCon.Rip);
                    }
                    continueStatus = DBG_EXCEPTION_NOT_HANDLED;
                }
                
                fflush(stdout);

                if (exCode == EXCEPTION_BREAKPOINT && !sawFirstBreakpoint)
                {
                    sawFirstBreakpoint = true;
                    continueStatus = DBG_CONTINUE;
                }
                else
                {
                    continueStatus = DBG_EXCEPTION_NOT_HANDLED;
                }
            }
            else if (debugEvent.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT)
            {
                threadHandles.erase(debugEvent.dwThreadId);
            }
            else if (debugEvent.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                printf("Killing Process.. [DBG]\n");
                threadHandles.erase(debugEvent.dwThreadId);
                if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, DBG_CONTINUE))
                    printf("ContinueDebugEvent failed: %lu [DBG]\n", GetLastError());
                break;
            }

            if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus))
                printf("ContinueDebugEvent failed: %lu (TID: %lu) [DBG]\n", GetLastError(), debugEvent.dwThreadId);
        }
        keepSending = FALSE;
        sendRegs.join();
        printf("Process killed! [DBG]\n");
        analyticsFile_original.close();
        //hier Thread erstellen und RIP und RBP aufzeichnen, über stdin an GUI senden

    }else{
        std::cout << "Aborting. [DBG]";
        return 0;
    }
    
    

}
//Für Montag: aus dem AI Log field eine input box machen wo Analyse datei reingezogen werden soll. Dann Button eine Datei generieren lassen mit RIP, RBP, Name der Exception usw.