#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <ws2bth.h>
#include <bluetoothapis.h>
#include <string>

enum TSurroundControl : uint8_t
{
    SC_OFF = 0,
    SC_ANC = 1,
    SC_TalkThru = 9,
    SC_AmbientAware = 10
};

enum TVoiceAware
{
    VA_Off,
    VA_On
};

enum TVoiceAwareMode
{
    VAM_Low,
    VAM_Mid,
    VAM_High
};

template<typename T>
struct EnumItem {
    T Enummeration;
    const uint8_t* Data;
    int Size;
};

//Commands captured by recording the btsnoop_hci file while using JBL android app
static const uint8_t jblOffHeadphones[] = {0x5, 0x5a, 0x4, 0x0, 0x1, 0x11, 0x18, 0x0};

static const uint8_t jblAnsOff[] = {0x05,0x5a,0x04,0x00,0x06,0x0e,0x00,0x0b};
static const uint8_t jblAnsOn[] = {0x05,0x5a,0x06,0x00,0x06,0x0e,0x00,0x0a,0x01,0x00};

static const uint8_t jblAmbientAware[]  = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0xa, 0x4};
static const uint8_t jblTalkThru[] = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0x9, 0x4};

static const uint8_t jblVoiceAwareOff[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x0};
static const uint8_t jblVoiceAwareOn[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x1};

static const uint8_t jblVoiceAwareLow[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x1, 0x0};
static const uint8_t jblVoiceAwareMid[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x2, 0x0};
static const uint8_t jblVoiceAwareHigh[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x3, 0x0};

//headphones asking for voice aware mode
static const uint8_t jblVoiceAwareReq[] = {0x5, 0x5b, 0x5, 0x0, 0x82, 0x2c, 0x0, 0x7, 0x0};

static EnumItem<TSurroundControl> surroundCmds[] = {
    {SC_OFF, jblAnsOff, sizeof(jblAnsOff)},
    {SC_ANC, jblAnsOn, sizeof(jblAnsOn)},
    {SC_TalkThru, jblTalkThru, sizeof(jblTalkThru)},
    {SC_AmbientAware, jblAmbientAware, sizeof(jblAmbientAware)}
};

static EnumItem<TVoiceAware> voiceAwareCmds[] = {
    {VA_Off, jblVoiceAwareOff, sizeof(jblVoiceAwareOff)},
    {VA_On, jblVoiceAwareOn, sizeof(jblVoiceAwareOn)},
};

static EnumItem<TVoiceAwareMode> voiceAwareMode[] = {
    {VAM_Low, jblVoiceAwareLow, sizeof(jblVoiceAwareLow)},
    {VAM_Mid, jblVoiceAwareMid, sizeof(jblVoiceAwareMid)},
    {VAM_High, jblVoiceAwareHigh, sizeof(jblVoiceAwareHigh)}
};

template <typename T, int Size>
EnumItem<T>* GetEnumItemFromList(EnumItem<T>(&list)[Size], T enumeration)
{
    if(!list) return nullptr;
    for (int i = 0; i < Size; i++) {
        if(list[i].Enummeration == enumeration) {
            return &list[i];
        }
    }
    return nullptr;
}

template <typename T, int Size>
int GetJblCmd(T enumeration, EnumItem<T>(&list)[Size], uint8_t* cmd, int maxCmdSize)
{
    if(!cmd || !list) return -1;
    auto* item = GetEnumItemFromList(list,enumeration);
    if(!item || maxCmdSize < item->Size) return -1;
    memcpy(cmd, item->Data, item->Size);
    return item->Size;
}

class JblBalanceCmd
{
public:
    enum {
        MinBalance = -16,
        MaxBalance = 16,
        CmdBaseSize = 6,
        BalanceZero = 100
    };
    JblBalanceCmd()
    {
        isOn = 0;
        balance = BalanceZero;
    }
    /// @brief
    /// @param b left/right balance [-16..16], negative for left 
    /// @return 0 - success, -1 - error
    int SetBalance(int b)
    {
        if(b > MaxBalance || b < MinBalance) {
            return -1;
        }
        balance = static_cast<uint8_t>(BalanceZero + b);
        return 0;
    }
    
    void SetIsOn(bool on)
    {
        on ? isOn = 1 : isOn = 0;
    }
private:
    const uint8_t cmdBase[CmdBaseSize] = {0x5, 0x5a, 0x4, 0x0, 0x36, 0x0};
    uint8_t isOn;
    uint8_t balance;
};

struct SurroundCtlInfo {
    enum {
        BaseSize = 9,
        CmdModeInfo = 1,
    };
    const uint8_t BaseInfo[BaseSize] = {0x5, 0x5c, 0x6, 0x0, 0x1, 0x9, 0x5, 0x0, 0x0};
    TSurroundControl Mode = SC_OFF;
};

class JBLController
{
private:
    SOCKET rfcommSock;
    bool socketOk = false;
    SurroundCtlInfo jblSurroundControl;
    TVoiceAwareMode vaMode = VAM_Low;
    static constexpr uint16_t rfcommJblPort = 21;
    static constexpr int rcvTimeout = 10;//ms
    JblBalanceCmd balanceCmd;
    bool vaReq = false;
    bool surroundReq = false;

    void recvParams()
    {
        // static const char firstCmd[] = {0x5, 0x5a, 0x6, 0x0, 0x0, 0xa, 0x2, 0x10, 0xe8, 0x3};
        // sendRfcomm(firstCmd, sizeof(firstCmd));
        //return something like chip name

        static const char askForSurroundCtl[] = {0x5, 0x5a, 0x3, 0x0, 0x6, 0x0, 0x1};
        int res = sendRfcomm(askForSurroundCtl, sizeof(askForSurroundCtl));
        if(res < 0) {
            int y = 0;
            y++;
            return;
        }
        surroundReq = true;
    }

    //create socket, find jbl device, connect to port rfcommJblPort
    void initRfcomm()
    {
        printf("Connecting to jbl headphones...\n");
        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {0};
        searchParams.dwSize = sizeof(searchParams);
        searchParams.fReturnConnected = TRUE;
        searchParams.fReturnRemembered = TRUE;
        searchParams.fIssueInquiry = FALSE; // Only known devices
        
        BLUETOOTH_DEVICE_INFO deviceInfo = {0};
        deviceInfo.dwSize = sizeof(deviceInfo);
        
        HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        
        if (!hFind) {
            printf("Error no Bluetooth devices found\n");
            socketOk = false;
            return;
        }
        
        ULONGLONG deviceAddress = 0;
        
        do {            
            if (deviceInfo.fConnected) {
                std::wstring_view wsw = deviceInfo.szName;
                if(wsw.find(L"JBL") != std::wstring::npos) {
                    wprintf(L"Found device %s\n", deviceInfo.szName);
                    deviceAddress = deviceInfo.Address.ullLong;
                    break;
                }
            }
        } while (BluetoothFindNextDevice(hFind, &deviceInfo));
        
        BluetoothFindDeviceClose(hFind);
        
        if (!deviceAddress) {
            printf("Error no connected JBL device found\n");
            socketOk = false;
            return;
        }
        
        rfcommSock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        
        if (rfcommSock == INVALID_SOCKET) {
            printf("Error socket creation failed: %d\n", WSAGetLastError());
            socketOk = false;
            return;
        }

        SOCKADDR_BTH addr = {0};
        addr.addressFamily = AF_BTH;
        addr.btAddr = deviceAddress;
        addr.port = rfcommJblPort;
        
        int result = connect(rfcommSock, (SOCKADDR*)&addr, sizeof(addr));
        
        if (result == SOCKET_ERROR && result != WSAEWOULDBLOCK) {
            printf("Error connection failed: %d\n", WSAGetLastError());
            socketOk = false;
            return;
        }
        socketOk = true;
        printf("Connected successfully\n");

        recvParams();
    }

    //recv with blocking maximum for rcvTimeout ms
    int recvWithSelect(char* buf, size_t len) 
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(rfcommSock, &readfds);

        struct timeval tv;
        tv.tv_sec  = rcvTimeout / 1000;
        tv.tv_usec = (rcvTimeout % 1000) * 1000;

        int ret = ::select(rfcommSock + 1, &readfds, nullptr, nullptr, &tv);

        if (ret <= 0) return -1;          //timed out

        // Сокет готов — recv вернётся немедленно
        return recv(rfcommSock, buf, len, 0);
    }

    int sendRfcomm(const char* data, int size)
    {
        int res = send(rfcommSock, (const char*)data, size, 0);
        if(res <= 0) {
            socketOk = false;
            return -1;
        }
        return res;
    }

    int sendVoiceAware(TVoiceAware va, TVoiceAwareMode mode)
    {
        if(!socketOk) return -1;
        constexpr int maxCmdSize = 500;
        uint8_t data[maxCmdSize] = {0};
        int res = GetJblCmd(va,voiceAwareCmds,data,maxCmdSize);
        if(res <= 0) return -1;
        if(sendRfcomm((const char*)data, res) < 0) return -1;
        //after receiving command, headphones asking for voice aware mode.
        vaReq = true;
        return 0;
    }

public:
    JBLController() 
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        initRfcomm();
    }

    ~JBLController()
    {
        closesocket(rfcommSock);
        WSACleanup();
    }

    void Reconnect()
    {
        if(socketOk) {
            closesocket(rfcommSock);
        }
        initRfcomm();
    }

    void CheckRecv()
    {
        if(!socketOk) return;
        constexpr int bufSize = 500;
        while (true) {
            char buf[bufSize] = {0};
            int rcvLen = recvWithSelect(buf,bufSize);
            if(rcvLen <= 0) break;
            if(surroundReq && rcvLen == sizeof(SurroundCtlInfo)) {
                if(!memcmp(buf,&jblSurroundControl,SurroundCtlInfo::BaseSize)) {
                    jblSurroundControl.Mode = (TSurroundControl)buf[sizeof(SurroundCtlInfo)-1];
                }
                surroundReq = false;
                continue;
            }
            if(vaReq && !memcmp(buf,jblVoiceAwareReq,sizeof(jblVoiceAwareReq))) {
                vaReq = false;
                uint8_t cmd[bufSize] = {0};
                int res = GetJblCmd(vaMode,voiceAwareMode,cmd,bufSize);
                if(res > 0) {
                    if(sendRfcomm((const char*)cmd, res) < 0) {
                        break;
                    }
                }
                continue;
            }
        }
    }

    int SendSurroundCmd(TSurroundControl cmd)
    {
        if(!socketOk) return -1;
        jblSurroundControl.Mode = cmd;
        constexpr int maxCmdSize = 500;
        uint8_t data[maxCmdSize] = {0};
        int res = GetJblCmd(cmd,surroundCmds,data,maxCmdSize);
        if(res > 0) {
            int result = sendRfcomm((const char*)data, res);
            if(result < 0) return -1;
            return result;
        }
        return -1;
    }

    int OffVoiceAware()
    {
        return sendVoiceAware(VA_Off,vaMode);
    }

    int SendVoiceAware(TVoiceAwareMode mode)
    {
        vaMode = mode;
        return sendVoiceAware(VA_On, mode);
    }

    int OffHeadphones()
    {
        if(!socketOk) return -1;
        int res = sendRfcomm((const char*)jblOffHeadphones, sizeof(jblOffHeadphones));
        return res;
    }

    int SendChannelBalance(bool on, int balance = 0)
    {
        if(!socketOk) return -1;
        balanceCmd.SetIsOn(on);
        int res = balanceCmd.SetBalance(balance);
        if(res < 0) return res;
        res = sendRfcomm((const char*)jblOffHeadphones, sizeof(jblOffHeadphones));
        return res;
    }
};

class HeadsetController {
private:
    JBLController jblCtl;
    enum TControlResult
    {
        CR_Empty,
        CR_Nan,
        CR_OutRange,
        CR_Valid
    };

    TControlResult controlEnter(int& control, int minim, int maxim, bool emptyIsErr = true)
    {
        char buf[100] = {0};
        if (fgets(buf, sizeof(buf), stdin) == NULL || buf[0] == '\n') {
            if(emptyIsErr) {
                printf("Error: enter a number in [%d..%d]\n", minim, maxim);
            }
            return CR_Empty;
        }
        if (sscanf(buf, "%d", &control) != 1) {
            printf("Error: enter a number in [%d..%d]\n", minim, maxim);
            return CR_Nan;
        }
        if(control > maxim || control < minim)
        {
            printf("Error: enter a number in [%d..%d]\n", minim, maxim);
            return CR_OutRange;
        }
        return CR_Valid;
    }

    TSurroundControl numToSurroundMode(int num)
    {
        switch(num) {
            case 1: return SC_OFF;
            case 2: return SC_ANC;
            case 3: return SC_TalkThru;
            case 4: return SC_AmbientAware;
        }
    }

    void jblSurroundControl()
    {
        printf("Choose surround control mode:\
        1 - Off surround control\
        2 - Active noise cancellation (ANC)\
        3 - Talk thru\
        4 - Ambient aware\n");
        int num = -1;
        TControlResult res = controlEnter(num, 1, 4);
        if(res == CR_Valid) {
            if(jblCtl.SendSurroundCmd(numToSurroundMode(num)) < 0) {
                printf("Can't send command, check if headphones is on and try \"Reconnect\"\n");
            }else {
                switch(num) {
                    case 1: printf("Surround control off\n"); break;
                    case 2: printf("ANC mode\n"); break;
                    case 3: printf("Talk thru mode\n"); break;
                    case 4: printf("Ambient aware mode\n"); break;
                }
            }
        }
    }

    void jblVoiceAware()
    {
        printf("Choose voice aware mode:\
        1 - Off voice aware\
        2 - Low voice aware\
        3 - Middle voice aware\
        4 - High voice aware\n");
        int voiceAware = -1;
        TControlResult res = controlEnter(voiceAware, 1, 4);
        if(res == CR_Valid) {
            int result = voiceAware == 1 ? jblCtl.OffVoiceAware() : 
                jblCtl.SendVoiceAware((TVoiceAwareMode)(voiceAware-2));
            if(result < 0) {
                printf("Can't send command, check if headphones is on and try \"Reconnect\"\n");
            } else {
                switch(voiceAware) {
                    case 1: printf("Voice aware off\n"); break;
                    case 2: printf("Low voice aware\n"); break;
                    case 3: printf("Middle voice aware\n"); break;
                    case 4: printf("High voice aware\n"); break;
                }
            }
        }
    }

    void jblBalanceControl()
    {
        printf("Enter balance [%d..%d] or press Enter for turn off balance feature: ",
            JblBalanceCmd::MinBalance, JblBalanceCmd::MaxBalance);
        int balance = 0;
        TControlResult res = controlEnter(balance, JblBalanceCmd::MinBalance, 
            JblBalanceCmd::MaxBalance, false);
        int result = -1;
        if(res == CR_Empty) {
            result = jblCtl.SendChannelBalance(false);
        } else if(res == CR_Valid) {
            result = jblCtl.SendChannelBalance(true, balance);
        }
        if(result < 0) {
            printf("Can't send command, check if headphones is on and try \"Reconnect\"\n");
        } else {
            if(res == CR_Empty) {
                printf("Balance feature off\n");
            } else {
                printf("Balance set %d\n", balance);
            }
        }
    }

public:    
    void ShowHelp() {
        printf("=== Bluetooth Headset Controller ===\n");
        printf("====================================\n");
        printf("Commands:\n");
        printf("  1      - JBL surround control (noisecancelling etc.)\n");
        printf("  2      - Voice aware strength\n");
        printf("  3      - Left/right balance\n");
        printf("  C      - Off headphones\n");
        printf("  R      - Reconnect to jbl device\n");
        printf("  H      - Show this help\n");
        printf("  Q      - Quit\n");
        printf("====================================\n");
    }

    void Run() {
        ShowHelp();
        
        bool running = true;
        while (running) {
            jblCtl.CheckRecv();
            if (_kbhit()) {
                char ch = _getch();
                switch (ch) {                                 
                    case '1':
                    case '!': 
                        jblSurroundControl();
                        break;
                    case '2':
                    case '@':
                        jblVoiceAware();
                        break;
                    case '3':
                    case '#':
                        jblBalanceControl();
                        break;
                    case 'c':
                    case 'C':
                        jblCtl.OffHeadphones();
                        break;
                    case 'r':
                    case 'R':
                        jblCtl.Reconnect();
                        break;
                    case 'h':
                    case 'H':
                        ShowHelp();
                        break;
                    case 'q':
                    case 'Q':
                        running = false;
                        printf("Goodbye!\n");
                        break;
                }
            }
            Sleep(50);
        }
    }
};

int main() {
    HeadsetController controller;
    controller.Run();
    return 0;
}