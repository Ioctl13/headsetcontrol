#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <ws2bth.h>
#include <bluetoothapis.h>
#include <string_view>
#include <string>

enum TSurroundControl
{
    SC_OFF,
    SC_ANC,
    SC_TalkThru,
    SC_AmbientAware
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
    T enummeration;
    const uint8_t* data;
    int size;
};

static const uint8_t jblOffHeadphones[] = {0x5, 0x5a, 0x4, 0x0, 0x1, 0x11, 0x18, 0x0};

static const uint8_t jblAnsOff[] = {0x05,0x5a,0x04,0x00,0x06,0x0e,0x00,0x0b,0x01,0x00};
static const uint8_t jblAnsOn[] = {0x05,0x5a,0x06,0x00,0x06,0x0e,0x00,0x0a,0x01,0x00};

static const uint8_t jblAmbientAware[]  = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0xa, 0x4};
static const uint8_t jblTalkThru[] = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0x9, 0x4};

static EnumItem<TSurroundControl> surroundCmds[] = {
    {SC_OFF, jblAnsOff, sizeof(jblAnsOff)},
    {SC_ANC, jblAnsOn, sizeof(jblAnsOn)},
    {SC_TalkThru, jblTalkThru, sizeof(jblTalkThru)},
    {SC_AmbientAware, jblAmbientAware, sizeof(jblAmbientAware)}
};

static const uint8_t jblVoiceAwareOff[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x0};
static const uint8_t jblVoiceAwareOn[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x1};

static EnumItem<TVoiceAware> voiceAwareCmds[] = {
    {VA_Off, jblVoiceAwareOff, sizeof(jblVoiceAwareOff)},
    {VA_On, jblVoiceAwareOn, sizeof(jblVoiceAwareOn)},
};

static const uint8_t jblVoiceAwareLow[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x1, 0x0};
static const uint8_t jblVoiceAwareMid[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x2, 0x0};
static const uint8_t jblVoiceAwareHigh[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x3, 0x0};

static EnumItem<TVoiceAwareMode> voiceAwareMode[] = {
    {VAM_Low, jblVoiceAwareLow, sizeof(jblVoiceAwareLow)},
    {VAM_Mid, jblVoiceAwareMid, sizeof(jblVoiceAwareMid)},
    {VAM_High, jblVoiceAwareHigh, sizeof(jblVoiceAwareHigh)}
};

//headphones asking for voice aware mode
static const uint8_t jblVoiceAwareReq[] = {0x5, 0x5b, 0x5, 0x0, 0x82, 0x2c, 0x0, 0x7, 0x0};

template <typename T, int Size>
EnumItem<T>* GetEnumItemFromList(EnumItem<T>(&list)[Size], T enumeration)
{
    if(!list) return nullptr;
    int y = Size;
    for (int i = 0; i < Size; i++) {
        if(list[i].enummeration == enumeration) {
            return &list[i];
        }
    }
    return nullptr;
}

template <typename T, int Size>
int GetJblCmd(T enumeration, EnumItem<T>(&list)[Size], uint8_t* cmd, int size)
{
    if(!cmd || !list) return -1;
    auto* item = GetEnumItemFromList(list,enumeration);
    if(!item || size < item->size) return -1;
    memcpy(cmd, item->data, item->size);
    return item->size;
}

class JBLController
{
private:
    SOCKET rfcommSock;
    TSurroundControl jblSurroundControl = SC_OFF;
    TVoiceAwareMode vaMode = VAM_Low;
    static constexpr uint16_t rfcommJblPort = 21;
    static constexpr int rcvTimeout = 70;//ms
    //create socket, find jbl device, connect to port rfcommJblPort
    void initRfcomm()
    {
        BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {0};
        searchParams.dwSize = sizeof(searchParams);
        searchParams.fReturnConnected = TRUE;
        searchParams.fReturnRemembered = TRUE;
        searchParams.fIssueInquiry = FALSE; // Only known devices
        
        BLUETOOTH_DEVICE_INFO deviceInfo = {0};
        deviceInfo.dwSize = sizeof(deviceInfo);
        
        HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
        
        if (!hFind) {
            printf("No Bluetooth devices found\n");
            return;
        }
        
        ULONGLONG deviceAddress = 0;
        
        do {            
            if (deviceInfo.fConnected) {
                std::wstring_view wsw = deviceInfo.szName;
                if(wsw.find(L"JBL") != std::wstring::npos) {
                    deviceAddress = deviceInfo.Address.ullLong;
                    break;
                }
            }
        } while (BluetoothFindNextDevice(hFind, &deviceInfo));
        
        BluetoothFindDeviceClose(hFind);
        
        if (!deviceAddress) {
            printf("No connected JBL device found\n");
            return;
        }
        
        rfcommSock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
        
        if (rfcommSock == INVALID_SOCKET) {
            printf("Socket creation failed: %d\n", WSAGetLastError());
            return;
        }

        SOCKADDR_BTH addr = {0};
        addr.addressFamily = AF_BTH;
        addr.btAddr = deviceAddress;
        addr.port = rfcommJblPort;
        
        int result = connect(rfcommSock, (SOCKADDR*)&addr, sizeof(addr));
        
        if (result == SOCKET_ERROR && result != WSAEWOULDBLOCK) {
            printf("Connection failed: %d\n", WSAGetLastError());
        }
    }

    //recv with blocking maximum for rcvTimeout ms
    int recvWithSelect(int sockfd, char* buf, size_t len) 
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct timeval tv;
        tv.tv_sec  = rcvTimeout / 1000;
        tv.tv_usec = (rcvTimeout % 1000) * 1000;

        int ret = ::select(sockfd + 1, &readfds, nullptr, nullptr, &tv);

        if (ret <= 0) return -1;          //timed out

        // Сокет готов — recv вернётся немедленно
        return recv(sockfd, buf, len, 0);
    }

    int sendVoiceAware(TVoiceAware va, TVoiceAwareMode mode)
    {
        constexpr int maxCmdSize = 500;
        uint8_t data[maxCmdSize] = {0};
        int res = GetJblCmd(va,voiceAwareCmds,data,maxCmdSize);
        if(res <= 0) return -1;
        res = send(rfcommSock, (const char*)data, res, 0);
        if(res <= 0) return -1;
        //after receiving command, headphones asking for voice aware mode.
        while(true) {//TODO separate from send
            memset(data,0,maxCmdSize);
            res = recvWithSelect(rfcommSock, (char*)data, maxCmdSize);
            if(res <= 0) return -2;
            if(memcmp(data,jblVoiceAwareReq,sizeof(jblVoiceAwareReq)) == 0) break;
        }
        memset(data,0,maxCmdSize);
        res = GetJblCmd(mode,voiceAwareMode,data,maxCmdSize);
        if(res <= 0) return -1;
        res = send(rfcommSock, (const char*)data, res, 0);
        if(res > 0) return res;
        return -1;
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
        closesocket(rfcommSock);
        initRfcomm();
    }

    int SendSurroundCmd(TSurroundControl cmd)
    {
        // if(jblSurroundControl == cmd) return -1;
        jblSurroundControl = cmd;
        constexpr int maxCmdSize = 500;
        uint8_t data[maxCmdSize] = {0};
        int res = GetJblCmd(cmd,surroundCmds,data,maxCmdSize);
        if(res > 0) {
            int result = send(rfcommSock, (const char*)data, res, 0);
            if(cmd == SC_OFF) {//otherwise doesn't accept any commands after SC_OFF
                closesocket(rfcommSock);
                Sleep(50);
                initRfcomm();
            }
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
        int res = send(rfcommSock, (const char*)jblOffHeadphones, sizeof(jblOffHeadphones), 0);
        return res;
    }
};

class HeadsetController {
private:
    JBLController jblCtl;
public:    
    void ShowHelp() {
        printf("=== Bluetooth Headset Controller ===\n");
        printf("====================================\n");
        printf("Commands:\n");
        printf("  1      - Off JBL surround control\n");
        printf("  2      - On ANC\n");
        printf("  3      - On TalkThru surround control mode\n");
        printf("  4      - On AmbientAware surround control mode\n");
        printf("  5      - Off voice aware\n");
        printf("  6      - Voice aware low\n");
        printf("  7      - Voice aware mid\n");
        printf("  8      - Voice aware high\n");
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
            if (_kbhit()) {
                char ch = _getch();
                switch (ch) {                                 
                    case '1':
                    case '!': 
                        jblCtl.SendSurroundCmd(SC_OFF);
                        break;
                    case '2':
                    case '@':
                        jblCtl.SendSurroundCmd(SC_ANC);
                        break;
                    case '3':
                    case '#':
                        jblCtl.SendSurroundCmd(SC_TalkThru);
                        break;
                    case '4':
                    case '$':
                        jblCtl.SendSurroundCmd(SC_AmbientAware);
                        break;
                    case '5':
                    case '%':
                        jblCtl.OffVoiceAware();
                        break;
                    case '6':
                    case '^':
                        jblCtl.SendVoiceAware(VAM_Low);
                        break;
                    case '7':
                    case '&':
                        jblCtl.SendVoiceAware(VAM_Mid);
                        break;
                    case '8':
                    case '*':
                        jblCtl.SendVoiceAware(VAM_High);
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