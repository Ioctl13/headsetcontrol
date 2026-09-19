#include <stdint.h>
#include <functional>

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

//represents balance cmd
class JblBalanceCmd
{
public:
    enum {
        MinBalance = -16,
        MaxBalance = 16,
        HeadSize = 6,
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
    //head
    const uint8_t cmdBase[HeadSize] = {0x5, 0x5a, 0x4, 0x0, 0x36, 0x0};
    uint8_t isOn;
    //100±16 ("-" for left)
    uint8_t balance;
};

//represents answer from headphones about surround control mode 
struct SurroundCtlInfo {
    enum {
        HeadSize = 9,
        CmdModeInfo = 1,
    };
    //head
    const uint8_t BaseInfo[HeadSize] = {0x5, 0x5c, 0x6, 0x0, 0x1, 0x9, 0x5, 0x0, 0x0};
    //current mode
    TSurroundControl Mode = SC_OFF;
};

typedef std::function<void(TVoiceAwareMode)> AwareModeSettedCb;

class JBL770Controller
{
private:
    uint64_t rfcommSock;
    bool socketOk = false;
    SurroundCtlInfo jblSurroundControl;
    TVoiceAwareMode vaMode = VAM_Low;
    static constexpr uint16_t rfcommJblPort = 21;
    static constexpr int rcvTimeout = 10;//ms
    JblBalanceCmd balanceCmd;
    bool vaReq = false;
    bool surroundReq = false;
    AwareModeSettedCb awareModeSettedCb = nullptr;
    //asking headphones about current surround control mode
    void recvParams();
    //create socket, find jbl device, connect to port rfcommJblPort
    void initRfcomm();
    //recv with blocking maximum for rcvTimeout ms
    int recvWithSelect(char* buf, size_t len);
    //send command via bluetooth
    int sendRfcomm(const char* data, int size);
    //send command via bluetooth
    int sendVoiceAware(TVoiceAware va, TVoiceAwareMode mode);
    //answer on request from heaphones about VA strength
    int sendVoiceAwareMode();
public:
    JBL770Controller();
    ~JBL770Controller();
    bool IsSockOk() {return socketOk;}
    //close socket and try to open another
    void Reconnect();
    //calls select, and if it's success calls recv
    void CheckRecv();
    //set surround control mode
    int SendSurroundCmd(TSurroundControl cmd);
    //off voice aware feature
    int OffVoiceAware();
    //set voice aware strength
    int SendVoiceAware(TVoiceAwareMode mode);
    //turn off the headphones
    int OffHeadphones();
    //send left/right channel balance
    int SendChannelBalance(bool on, int balance = 0);
    //set callback that triggers when aware mode setted
    void SetAwareModeSettedCb(AwareModeSettedCb cb);
};
