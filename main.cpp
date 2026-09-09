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

struct EnumItem {
    int enummeration;
    const uint8_t* data;
    int size;
};

static const uint8_t jblAnsOff[] = {0x05,0x5a,0x04,0x00,0x06,0x0e,0x00,0x0b,0x01,0x00};
static const uint8_t jblAnsOn[] = {0x05,0x5a,0x06,0x00,0x06,0x0e,0x00,0x0a,0x01,0x00};

static const uint8_t jblAmbientAware[]  = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0xa, 0x4};
static const uint8_t jblTalkThru[] = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0x9, 0x4};

static EnumItem jblCmds[] = {
    {SC_OFF, jblAnsOff, sizeof(jblAnsOff)},
    {SC_ANC, jblAnsOn, sizeof(jblAnsOn)},
    {SC_TalkThru, jblTalkThru, sizeof(jblTalkThru)},
    {SC_AmbientAware, jblAmbientAware, sizeof(jblAmbientAware)}
};

EnumItem* GetEnumItemFromList(EnumItem* list, int listSize, int enumeration)
{
    for (int i = 0; i < listSize; i++) {
        if(list[i].enummeration == enumeration) {
            return &list[i];
        }
    }
    return nullptr;
}

int GetJblSurroundCmd(TSurroundControl surround, uint8_t* cmd, int size)
{
    if(!cmd) return -1;
    auto* item = GetEnumItemFromList(jblCmds,sizeof(jblCmds),surround);
    if(!item || size < item->size) return -1;
    memcpy(cmd, item->data, item->size);
    return item->size;
}

class JBLController
{
private:
    SOCKET rfcommSock;
    TSurroundControl jblSurroundControl = SC_OFF;
    static constexpr uint16_t rfcommJblPort = 21;

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

    int SendJblCmd(TSurroundControl cmd)
    {
        // if(jblSurroundControl == cmd) return -1;
        jblSurroundControl = cmd;
        constexpr int maxCmdSize = 500;
        uint8_t data[maxCmdSize] = {0};
        int res = GetJblSurroundCmd(cmd,data,maxCmdSize);
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
        printf("  C      - Reconnect to jbl device\n");
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
                        jblCtl.SendJblCmd(SC_OFF);
                        break;
                    case '2':
                    case '@':
                        jblCtl.SendJblCmd(SC_ANC);
                        break;
                    case '3':
                    case '#':
                        jblCtl.SendJblCmd(SC_TalkThru);
                        break;
                    case '4':
                    case '$':
                        jblCtl.SendJblCmd(SC_AmbientAware);
                        break;
                    case 'c':
                    case 'C':
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