#include <770nc.h>

enum TControlResult
{
    CR_Empty,
    CR_Quit,
    CR_Nan,
    CR_OutRange,
    CR_Valid
};

class HeadsetController {
private:
    JBL770Controller jblCtl;

    TControlResult controlEnter(int& control, int minim, int maxim, bool emptyIsErr = true);

    TSurroundControl numToSurroundMode(int num);

    void jblSurroundControl();

    TVoiceAwareMode numToVoiceAwareMode(int num);

    void jblVoiceAware();

    void jblBalanceControl();
    void onJblVoiceAwareSetted(TVoiceAwareMode mode);

public:    
    HeadsetController();

    void ShowHelp();

    void Run();
};