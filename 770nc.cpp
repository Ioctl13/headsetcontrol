#include <enumitem.h>
#include <770nc.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2bth.h>
#include <bluetoothapis.h>
#include <string>

//Commands captured by recording the btsnoop_hci file while using JBL android app
static const uint8_t jblOffHeadphones[] = {0x5, 0x5a, 0x4, 0x0, 0x1, 0x11, 0x18, 0x0};

//off surround control
static const uint8_t jblAncOff[] = {0x05,0x5a,0x04,0x00,0x06,0x0e,0x00,0x0b};
//on anc (active noise canselling)
static const uint8_t jblAncOn[] = {0x05,0x5a,0x06,0x00,0x06,0x0e,0x00,0x0a,0x01,0x00};
//ambient aware
static const uint8_t jblAmbientAware[]  = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0xa, 0x4};
//talk thru
static const uint8_t jblTalkThru[] = {0x5, 0x5a, 0x6, 0x0, 0x6, 0xe, 0x0, 0xa, 0x9, 0x4};

static const uint8_t jblVoiceAwareOff[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x0};
static const uint8_t jblVoiceAwareOn[] = {0x5, 0x5a, 0x5, 0x0, 0x82, 0x2c, 0x7, 0x0, 0x1};

static const uint8_t jblVoiceAwareLow[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x1, 0x0};
static const uint8_t jblVoiceAwareMid[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x2, 0x0};
static const uint8_t jblVoiceAwareHigh[] = {0x5, 0x5a, 0x6, 0x0, 0x82, 0x2c, 0x6, 0x0, 0x3, 0x0};

//headphones asking for voice aware mode
static const uint8_t jblVoiceAwareReq[] = {0x5, 0x5b, 0x5, 0x0, 0x82, 0x2c, 0x0, 0x7, 0x0};

static EnumItem<TSurroundControl> surroundCmds[] = {
    {SC_OFF, jblAncOff, sizeof(jblAncOff)},
    {SC_ANC, jblAncOn, sizeof(jblAncOn)},
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
static int GetJblCmd(T enumeration, EnumItem<T>(&list)[Size], uint8_t* cmd, int maxCmdSize)
{
    if(!cmd || !list) return -1;
    auto* item = GetEnumItemFromList(list,enumeration);
    if(!item || maxCmdSize < item->Size) return -1;
    memcpy(cmd, item->Data, item->Size);
    return item->Size;
}

void JBL770Controller::recvParams()
{
    // static const char firstCmd[] = {0x5, 0x5a, 0x6, 0x0, 0x0, 0xa, 0x2, 0x10, 0xe8, 0x3};
    // sendRfcomm(firstCmd, sizeof(firstCmd));
    //return something like chip name

    static const char askForSurroundCtl[] = {0x5, 0x5a, 0x3, 0x0, 0x6, 0x0, 0x1};
    int res = sendRfcomm(askForSurroundCtl, sizeof(askForSurroundCtl));
    if(res < 0) {
        return;
    }
    surroundReq = true;
}

void JBL770Controller::initRfcomm()
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

int JBL770Controller::recvWithSelect(char* buf, size_t len) 
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(rfcommSock, &readfds);

    struct timeval tv;
    tv.tv_sec  = rcvTimeout / 1000;
    tv.tv_usec = (rcvTimeout % 1000) * 1000;

    int ret = ::select(rfcommSock + 1, &readfds, nullptr, nullptr, &tv);

    if (ret <= 0) return -1;          //timed out

    return recv(rfcommSock, buf, len, 0);
}

int JBL770Controller::sendRfcomm(const char* data, int size)
{
    int res = send(rfcommSock, (const char*)data, size, 0);
    if(res <= 0) {
        socketOk = false;
        return -1;
    }
    return res;
}

int JBL770Controller::sendVoiceAware(TVoiceAware va, TVoiceAwareMode mode)
{
    if(!socketOk) return -1;
    constexpr int maxCmdSize = 500;
    uint8_t data[maxCmdSize] = {0};
    int res = GetJblCmd(va,voiceAwareCmds,data,maxCmdSize);
    if(res <= 0) return -1;
    if(sendRfcomm((const char*)data, res) < 0) return -1;
    vaMode = mode;
    //after receiving command, headphones asking for voice aware mode.
    vaReq = true;
    return 0;
}

int JBL770Controller::sendVoiceAwareMode()
{
    vaReq = false;
    constexpr int bufSize = 500; 
    uint8_t cmd[bufSize] = {0};
    int res = GetJblCmd(vaMode,voiceAwareMode,cmd,bufSize);
    if(res > 0) {
        if(sendRfcomm((const char*)cmd, res) < 0) {
            return -1;
        }
    } else {
        return -1;
    }
    if(awareModeSettedCb) {
        awareModeSettedCb(vaMode);
    }
    return 0;
}

JBL770Controller::JBL770Controller() 
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    initRfcomm();
}

JBL770Controller::~JBL770Controller()
{
    closesocket(rfcommSock);
    WSACleanup();
}

void JBL770Controller::Reconnect()
{
    if(socketOk) {
        closesocket(rfcommSock);
    }
    initRfcomm();
}

void JBL770Controller::CheckRecv()
{
    if(!socketOk) return;
    constexpr int bufSize = 500;
    while (true) {
        char buf[bufSize] = {0};
        int rcvLen = recvWithSelect(buf,bufSize);
        if(rcvLen <= 0) break;
        if(surroundReq && rcvLen == sizeof(SurroundCtlInfo)) {
            if(!memcmp(buf,&jblSurroundControl,SurroundCtlInfo::HeadSize)) {
                jblSurroundControl.Mode = (TSurroundControl)buf[sizeof(SurroundCtlInfo)-1];
            }
            surroundReq = false;
            continue;
        }
        if(vaReq && !memcmp(buf,jblVoiceAwareReq,sizeof(jblVoiceAwareReq))) {
            if(sendVoiceAwareMode() < 0) {
                break;
            }
            continue;
        }
    }
}

int JBL770Controller::SendSurroundCmd(TSurroundControl cmd)
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

int JBL770Controller::OffVoiceAware()
{
    return sendVoiceAware(VA_Off,vaMode);
}

int JBL770Controller::SendVoiceAware(TVoiceAwareMode mode)
{
    vaMode = mode;
    return sendVoiceAware(VA_On, mode);
}

int JBL770Controller::OffHeadphones()
{
    if(!socketOk) return -1;
    int res = sendRfcomm((const char*)jblOffHeadphones, sizeof(jblOffHeadphones));
    return res;
}

int JBL770Controller::SendChannelBalance(bool on, int balance)
{
    if(!socketOk) return -1;
    balanceCmd.SetIsOn(on);
    int res = balanceCmd.SetBalance(balance);
    if(res < 0) return res;
    res = sendRfcomm((const char*)jblOffHeadphones, sizeof(jblOffHeadphones));
    return res;
}

void JBL770Controller::SetAwareModeSettedCb(AwareModeSettedCb cb)
{
    if(!cb) return;
    awareModeSettedCb = cb;
}