#include <headsetcontrol.h>
#include <windows.h>
#include <conio.h>
#include <string>

HeadsetController::HeadsetController()
{
    jblCtl.SetAwareModeSettedCb([this](TVoiceAwareMode m){onJblVoiceAwareSetted(m);});
}

TControlResult HeadsetController::controlEnter(int& control, int minim, int maxim, bool emptyIsErr)
{
    char buf[100] = {0};
    if (fgets(buf, sizeof(buf), stdin) == NULL || buf[0] == '\n') {
        if(emptyIsErr) {
            printf("Error: enter a number in [%d..%d]\n", minim, maxim);
        }
        return CR_Empty;
    }
    if (buf[0] == 'q' || buf[0] == 'Q') {
        return CR_Quit;
    }
    if (sscanf(buf, "%d", &control) != 1) {
        char str[100]= {0};
        sscanf(buf, "%s", str);

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

TSurroundControl HeadsetController::numToSurroundMode(int num)
{
    switch(num) {
        case 1: return SC_OFF;
        case 2: return SC_ANC;
        case 3: return SC_TalkThru;
        case 4: return SC_AmbientAware;
    }
    return SC_OFF;
}

void HeadsetController::jblSurroundControl()
{
    printf("Choose surround control mode:\
    1 - Off surround control\
    2 - Active noise cancellation (ANC)\
    3 - Talk thru\
    4 - Ambient aware\n");
    int num = -1;
    TControlResult res = controlEnter(num, 1, 4);
    if(res == CR_Quit) {
        return;
    }
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

TVoiceAwareMode HeadsetController::numToVoiceAwareMode(int num)
{
    switch (num) {
        case 2: return VAM_Low;
        case 3: return VAM_Mid;
        case 4: return VAM_High;
    }
    return VAM_Low;
}

void HeadsetController::jblVoiceAware()
{
    printf("Choose voice aware mode:\
    1 - Off voice aware\
    2 - Low voice aware\
    3 - Middle voice aware\
    4 - High voice aware\n");
    int voiceAware = -1;
    TControlResult res = controlEnter(voiceAware, 1, 4);
    if(res == CR_Quit) {
        return;
    }
    if(res == CR_Valid) {
        if(voiceAware == 1){
            int res = jblCtl.OffVoiceAware();
            if(res < 0) {
                printf("Can't send command, check if headphones is on and try \"Reconnect\"\n");
            } else {
                printf("Voice aware off\n");
            }
        } else { 
            int res = jblCtl.SendVoiceAware(numToVoiceAwareMode(voiceAware));
            if(res < 0) {
                printf("Can't send command, check if headphones is on and try \"Reconnect\"\n");
            }
        }
    }
}

void HeadsetController::jblBalanceControl()
{
    printf("Enter balance [%d..%d] or press Enter for turn off balance feature: ",
        JblBalanceCmd::MinBalance, JblBalanceCmd::MaxBalance);
    int balance = 0;
    TControlResult res = controlEnter(balance, JblBalanceCmd::MinBalance, 
        JblBalanceCmd::MaxBalance, false);
    if(res == CR_Quit) {
        return;
    }
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

void HeadsetController::onJblVoiceAwareSetted(TVoiceAwareMode mode)
{
    switch(mode) {
        case VAM_Low: printf("Low voice aware\n"); break;
        case VAM_Mid: printf("Middle voice aware\n"); break;
        case VAM_High: printf("High voice aware\n"); break;
    }
}

void HeadsetController::ShowHelp() {
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

void HeadsetController::Run() {
    if(jblCtl.IsSockOk()){
        ShowHelp();
    }
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