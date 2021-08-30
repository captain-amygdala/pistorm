
#include <exec/exec.h>

#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <proto/graphics.h>
#include <clib/graphics_protos.h>

#include <dos/dos.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <hardware/intbits.h>

//#include <proto/ahi.h>
#include <proto/ahi_sub.h>
#include <clib/ahi_sub_protos.h>
#include <clib/debug_protos.h>

#include <math.h>
#include <string.h>
#include <stdint.h>

#include "../pi-ahi-enums.h"
#include "pi-ahi.h"

#define STR(s) #s
#define XSTR(s) STR(s)

#define DEVICE_NAME "pi-ahi.audio"
#define DEVICE_DATE "(16 Aug 2021)"
#define DEVICE_ID_STRING "Pi-AHI " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE
#define DEVICE_VERSION 4
#define DEVICE_REVISION 14
#define DEVICE_PRIORITY 0

struct ExecBase     *SysBase;
struct UtilityBase  *UtilityBase;
struct Library      *AHIsubBase  = NULL;
struct DosLibrary   *DOSBase     = NULL;
struct GfxBase *GraphicsBase = NULL;

int __attribute__((no_reorder)) _start()
{
    WRITESHORT(AHI_DEBUGMSG, 111);
    return -1;
}

asm("romtag:                                \n"
    "       dc.w    "XSTR(RTC_MATCHWORD)"   \n"
    "       dc.l    romtag                  \n"
    "       dc.l    endcode                 \n"
    "       dc.b    "XSTR(RTF_AUTOINIT)"    \n"
    "       dc.b    "XSTR(DEVICE_VERSION)"  \n"
    "       dc.b    "XSTR(NT_LIBRARY)"      \n"
    "       dc.b    "XSTR(DEVICE_PRIORITY)" \n"
    "       dc.l    _device_name            \n"
    "       dc.l    _device_id_string       \n"
    "       dc.l    _auto_init_tables       \n"
    "endcode:                               \n");


const char device_name[] = DEVICE_NAME;
const char device_id_string[] = DEVICE_ID_STRING;

struct pi_ahi *pi_ahi_base = NULL;

#define debug(...)
#define debugval(...)
//#define KPrintF(...)
//#define KPrintF(a)   kprintf((CONST_STRPTR)a)
//#define debug(c, v) WRITESHORT(c, v)

//#define debugmsg(...)
//#define debugbyte(...)
//#define debugshort(...)
//#define debuglong(...)
#define debugmsg(v) WRITESHORT(AHI_DEBUGMSG, v)
#define debugbyte(c, v) WRITEBYTE(c, v)
#define debugshort(c, v) WRITESHORT(c, v)
#define debuglong(c, v) WRITELONG(c, v)

static uint32_t __attribute__((used)) init (BPTR seg_list asm("a0"), struct Library *dev asm("d0"))
{
    SysBase = *(struct ExecBase **)4L;

    if (pi_ahi_base != NULL) {
        debugmsg(101);
        return 0;
    }

    debuglong(AHI_U321, (uint32_t)dev);
    debugmsg(1);

    char prefs[10] = "0";
    if (prefs[0] == '0') {} // TODO: prefs?

    debugmsg(103);
    if(!(DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library",0)))
        return 0;

    if(!(UtilityBase = (struct UtilityBase *)OpenLibrary((STRPTR)"utility.library",0)))
        return 0;

    if (!pi_ahi_base) {
        debugmsg(102);
        pi_ahi_base = AllocVec(sizeof(struct pi_ahi), MEMF_PUBLIC | MEMF_CLEAR);
        pi_ahi_base->ahi_base = dev;
        AHIsubBase = dev;
    }

    debugmsg(104);

    return (uint32_t)dev;
}

static uint8_t* __attribute__((used)) expunge(struct Library *libbase asm("a6"))
{
    debugmsg(999);

    if(DOSBase)       { CloseLibrary((struct Library *)DOSBase); DOSBase = NULL; }
    if(UtilityBase)   { CloseLibrary((struct Library *)UtilityBase); UtilityBase = NULL; }

    return 0;
}

static uint8_t __attribute__((used)) null()
{
    debugmsg(998);
    return 0;
}

static void __attribute__((used)) open(struct Library *dev asm("a6"), struct IORequest *iotd asm("a1"), uint32_t num asm("d0"), uint32_t flags asm("d1"))
{
    debugmsg(2);
    debugmsg((uint32_t)dev);

    if (!AHIsubBase) {
        debugmsg(201);
        AHIsubBase = dev;
    }
    
    iotd->io_Error = 0;
    dev->lib_OpenCnt++;
}

static uint8_t* __attribute__((used)) close(struct Library *dev asm("a6"), struct IORequest *iotd asm("a1"))
{
    debugmsg(3);

    return 0;
}

static void __attribute__((used)) begin_io(struct Library *dev asm("a6"), struct IORequest *io asm("a1"))
{
    debugmsg(4);
    if (pi_ahi_base == NULL || io == NULL)
        return;

    if (!(io->io_Flags & IOF_QUICK)) {
        ReplyMsg(&io->io_Message);
    }
}

static uint32_t __attribute__((used)) abort_io(struct Library *dev asm("a6"), struct IORequest *io asm("a1"))
{
    debugmsg(5);
    if (!io) return IOERR_NOCMD;
    io->io_Error = IOERR_ABORTED;

    return IOERR_ABORTED;
}

static uint32_t __attribute__((used)) SoundFunc(struct Hook *hook asm("a0"), struct AHIAudioCtrlDrv *actrl asm("a2"), struct AHISoundMessage *chan asm("a1"))
{
    debugmsg(9992);
    return 0;
}

static uint32_t __attribute__((used)) PlayFunc(struct pi_ahi *ahi_data asm("a1")) {
    debugmsg(21);
    /*uint16_t intchk = READSHORT(AHI_INTCHK);
    if (intchk) {
        WRITESHORT(AHI_INTCHK, 1);
        if (!ahi_data->disable_cnt && ahi_data->slave_process) {
            WRITESHORT(AHI_INTCHK, 2);*/
            Signal((struct Task *)ahi_data->slave_process, 1L << ahi_data->play_signal);
        /*}
        return 1;
    } else {
        WRITESHORT(AHI_INTCHK, 3);
    }*/

    return 0;
}

static void __attribute__((used)) MixFunc() {
    struct pi_ahi *ahi_data = pi_ahi_base;
    debugmsg(22);
}

static void SlaveProcess()
{
    struct Process *me = (struct Process *) FindTask(NULL);
    struct pi_ahi *ahi_data = pi_ahi_base;
    struct AHIAudioCtrlDrv *AudioCtrl = ahi_data->audioctrl;
    debuglong(AHI_U321, (uint32_t)ahi_data);
    debuglong(AHI_U322, (uint32_t)AudioCtrl->ahiac_PlayerFunc);
    debuglong(AHI_U323, AudioCtrl->ahiac_PlayerFreq);
    debuglong(AHI_U324, (uint32_t)AudioCtrl->ahiac_PreTimer);
    debugmsg(20);

    uint32_t cur_buf = 0;

    if (me) {} // TODO: Do something with me.

    ahi_data->flags &= ~(1 | 2);

    ahi_data->slave_signal  = AllocSignal(-1);
    ahi_data->play_signal   = AllocSignal(-1);
    ahi_data->mix_signal    = AllocSignal(-1);

    if((ahi_data->slave_signal != -1) && (ahi_data->play_signal != -1) && (ahi_data->mix_signal != -1)) {
        // Tell Master we're alive
        Signal(ahi_data->t_master, 1L << ahi_data->master_signal);

        //AudioCtrl->ahiac_SoundFunc->h_Entry = (void *)SoundFunc;

        for(;;) {
            uint32_t signalset;
            
            debugmsg(2004);
            signalset = Wait((1L << ahi_data->slave_signal) | (1L << ahi_data->play_signal) | (1L << ahi_data->mix_signal));

            if(signalset & (1L << ahi_data->slave_signal)) {
                debugmsg(2002);
                break;
            }

            if(signalset & (1L << ahi_data->play_signal)) {
                //StartPlaying(AudioCtrl, me);
                debuglong(AHI_U321, AudioCtrl->ahiac_BuffSamples);
                debugmsg(2001);
                if (AudioCtrl->ahiac_PreTimer && AudioCtrl->ahiac_MixerFunc) {
                    CallHookPkt(AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL);
                    if(!((*AudioCtrl->ahiac_PreTimer)())) {
                        CallHookPkt(AudioCtrl->ahiac_MixerFunc, AudioCtrl, ahi_data->mix_buf[cur_buf]);
                        WRITELONG(AHI_U321, AudioCtrl->ahiac_BuffSamples);
                        WRITELONG(AHI_U322, AudioCtrl->ahiac_MixFreq);
                        WRITELONG(AHI_U323, AudioCtrl->ahiac_Channels);
                        WRITELONG(AHI_U324, AudioCtrl->ahiac_BuffType);
                        WRITELONG(AHI_ADDR2, AudioCtrl->ahiac_PlayerFreq);
                        WRITELONG(AHI_ADDR1, (uint32_t)ahi_data->mix_buf[cur_buf]);
                        WRITESHORT(AHI_COMMAND, AHI_CMD_PLAY);
                        cur_buf++;
                        if (cur_buf == 8)
                            cur_buf = 0;
                    }
                    (*AudioCtrl->ahiac_PostTimer)();
                } else {
                    debugmsg(2002);
                }
                debugmsg(2005);
            }

            if(signalset & (1L << ahi_data->mix_signal)) {
                // This never really happens
            }
        }
    } else {
        debugmsg(2006);
    }

    ahi_data->flags &= ~(1 | 2);

    Forbid();
    FreeSignal(ahi_data->slave_signal);    ahi_data->slave_signal   = -1;
    FreeSignal(ahi_data->play_signal);     ahi_data->play_signal    = -1;
    FreeSignal(ahi_data->mix_signal);      ahi_data->mix_signal     = -1;

    ahi_data->slave_process = NULL;
    Signal((struct Task *)ahi_data->t_master, 1L << ahi_data->master_signal);
}

static uint32_t __attribute__((used)) intAHIsub_AllocAudio(struct TagItem *tagList asm("a1"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(6);
    SysBase = *(struct ExecBase **)4L;

    if (!DOSBase) {
        DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library",37);
    }
    if(!UtilityBase) {
        UtilityBase = (struct UtilityBase *)OpenLibrary((STRPTR)"utility.library",37);
    }

    debuglong(AHI_U321, sizeof(struct pi_ahi));
    debugmsg(601);
    if((AudioCtrl->ahiac_DriverData = AllocVec(sizeof(struct pi_ahi), MEMF_PUBLIC | MEMF_ANY | MEMF_CLEAR)))
    {
        debugmsg(603);
        struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;
        ahi_data->audioctrl = AudioCtrl;
        if (pi_ahi_base != ahi_data) {
            pi_ahi_base = ahi_data;
        }
        ahi_data->ahi_base    = AHIsubBase;
        ahi_data->slave_signal   = -1;
        ahi_data->play_signal    = -1;
        ahi_data->fart_signal    = -1;
        ahi_data->mix_signal     = -1;

        debugmsg(604);
        ahi_data->t_master = FindTask(NULL);
        ahi_data->master_signal = AllocSignal(-1);
        if(ahi_data->master_signal != -1) {
            debuglong(AHI_U321, (uint32_t)ahi_data);
            debugmsg(605);
            Forbid();
            if(ahi_data->slave_process = CreateNewProcTags(NP_Entry, (uint32_t)&SlaveProcess, NP_Name, (uint32_t)device_name, NP_Priority, 127, TAG_DONE)) {
                debugmsg(606);
                ahi_data->slave_process->pr_Task.tc_UserData = AudioCtrl;
            }
            Permit();
            debugmsg(607);

            if(ahi_data->slave_process) {
                debugmsg(608);
                Wait(1L << ahi_data->master_signal);   // Wait for slave to come alive
                if(ahi_data->slave_process != NULL) {
                    debugmsg(609);
                    ahi_data->flags |= 4;
                }
            }
        }
    }

    debugmsg(610);

    WRITELONG(AHI_U321, AudioCtrl->ahiac_PlayerFreq);
    WRITELONG(AHI_U322, AudioCtrl->ahiac_MixFreq);
    WRITELONG(AHI_U323, AudioCtrl->ahiac_Channels);
    WRITELONG(AHI_ADDR1, AudioCtrl->ahiac_MinPlayerFreq);
    WRITELONG(AHI_ADDR2, AudioCtrl->ahiac_MaxPlayerFreq);
    WRITESHORT(AHI_COMMAND, AHI_CMD_ALLOCAUDIO);

    struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;
    ahi_data->ahi_base = AHIsubBase;
    debugshort(AHI_U1, AudioCtrl->ahiac_MixFreq);
    debugmsg(602);
    ahi_data->mix_freq = AudioCtrl->ahiac_MixFreq;
    AudioCtrl->ahiac_BuffSamples = AudioCtrl->ahiac_MixFreq / 50;

    return AHISF_KNOWSTEREO | AHISF_MIXING;// | AHISF_TIMING;
}

static void __attribute__((used)) intAHIsub_FreeAudio(struct AHIAudioCtrlDrv *AudioCtrl asm("a2")) {
    debugmsg(7);
    WRITESHORT(AHI_COMMAND, AHI_CMD_FREEAUDIO);
    if(AudioCtrl->ahiac_DriverData)
    {
        struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;

        if(ahi_data->slave_process)
        {
            if(ahi_data->slave_signal != -1)
            {
                Signal((struct Task *)ahi_data->slave_process, 1L << ahi_data->slave_signal);
                ahi_data->quitting = 1;
            }
            Wait(1L << ahi_data->master_signal);
        }

        FreeSignal(ahi_data->master_signal);
        FreeVec(AudioCtrl->ahiac_DriverData);
        AudioCtrl->ahiac_DriverData = NULL;
    }
}

static uint32_t __attribute__((used)) intAHIsub_Stop(uint32_t Flags asm("d0"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2")) {
    debugmsg(9);
    WRITESHORT(AHI_COMMAND, AHI_CMD_STOP);
    if(Flags & AHISF_PLAY) {
        struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;

        if (ahi_data->play_soft_int) {
            debugmsg(91);
            Forbid();
            RemIntServer(INTB_VERTB, ahi_data->play_soft_int);
            Permit();
            FreeVec(ahi_data->play_soft_int); ahi_data->play_soft_int = NULL;
        }

        debugmsg(93);
        for (int i = 0; i < 8; i++) {
            debuglong(AHI_U321, i);
            debugmsg(94);
            if (ahi_data->samp_buf[i]) { FreeVec(ahi_data->samp_buf[i]); ahi_data->samp_buf[i] = NULL; }
            if (ahi_data->mix_buf[i]) { FreeVec(ahi_data->mix_buf[i]); ahi_data->mix_buf[i] = NULL; }
        }

        ahi_data->playing = 0;
    }

    return AHIE_OK;
}

static uint32_t __attribute__((used)) intAHIsub_Start(uint32_t flags asm("d0"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2")) {
    debugmsg(8);
    if(flags & AHISF_PLAY) {
        intAHIsub_Stop(flags, AudioCtrl);
        struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;
        debugshort(AHI_U1, AudioCtrl->ahiac_BuffType);
        debuglong(AHI_U321, AudioCtrl->ahiac_BuffSize);
        debugmsg(81);

        ahi_data->play_soft_int = AllocVec(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_ANY | MEMF_CLEAR);
        ahi_data->mix_soft_int =  AllocVec(sizeof(struct Interrupt), MEMF_PUBLIC | MEMF_ANY | MEMF_CLEAR);
        if (!ahi_data->play_soft_int || !ahi_data->mix_soft_int) {
            debugmsg(82);
            if (ahi_data->play_soft_int) { FreeVec(ahi_data->play_soft_int); ahi_data->play_soft_int = 0; }
            return AHIE_NOMEM;
        }

        ahi_data->buffer_type = AudioCtrl->ahiac_BuffType;
        ahi_data->buffer_size = AudioCtrl->ahiac_BuffSize;

        for (int i = 0; i < 8; i++) {
            ahi_data->samp_buf[i] = AllocVec(512 * 4, MEMF_PUBLIC | MEMF_ANY | MEMF_CLEAR);
            ahi_data->mix_buf[i]  = AllocVec(0x1000, MEMF_PUBLIC | MEMF_ANY | MEMF_CLEAR);
        }

        ahi_data->play_soft_int->is_Code = (void (* )())PlayFunc;
        ahi_data->play_soft_int->is_Node.ln_Type = NT_INTERRUPT;
        ahi_data->play_soft_int->is_Node.ln_Pri = -60;
        ahi_data->play_soft_int->is_Node.ln_Name = (char *)device_name;
        ahi_data->play_soft_int->is_Data = ahi_data;
        Forbid();
        AddIntServer(INTB_VERTB, ahi_data->play_soft_int);
        Permit();
        WRITESHORT(AHI_COMMAND, AHI_CMD_START);

        ahi_data->playing = 1;
    }

    return AHIE_OK;
}

static uint32_t __attribute__((used)) intAHIsub_GetAttr(uint32_t attr_ asm("d0"), int32_t arg_ asm("d1"), int32_t def_ asm("d2"), struct TagItem *tagList asm("a1"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2")) {
    const uint16_t freqs[] = {
        4096,
        8192,
        11025,
        22050,
        44100,
        48000,
    };

    uint32_t attr = attr_;
    int32_t arg = arg_, def = def_;
    debugmsg(10);
    debuglong(AHI_U321, attr);
    debuglong(AHI_U322, arg);
    debuglong(AHI_U323, def);
    debugmsg(1001);
    switch(attr)
    {
        case AHIDB_Bits:
            debugmsg(1002);
            return 16;
        case AHIDB_Frequencies:
            debugmsg(1003);
            return 6;
        case AHIDB_Frequency:
            debugmsg(1004);
            return freqs[arg];
        case AHIDB_Index:
            debuglong(AHI_U321, arg_);
            debugmsg(1005);
            for (int i = 0; i < 6; i++) {
                if (freqs[i] == arg)
                    return i;
            }
            return -1;
        case AHIDB_Author:
            debugmsg(1006);
            return (int32_t) "PiStorm AHI";
        case AHIDB_Copyright:
            debugmsg(1007);
            return (int32_t) "The PiStorms";
        case AHIDB_Version:
            debugmsg(1008);
            return (int32_t) device_id_string;
        case AHIDB_Annotation:
            debugmsg(1009);
            return (int32_t) "Based in part on the Toccata driver";
        case AHIDB_Record:
            debugmsg(1010);
            return FALSE;
        case AHIDB_FullDuplex:
            debugmsg(1011);
            return TRUE;
        case AHIDB_Realtime:
            debugmsg(1012);
            return TRUE;
        case AHIDB_MaxChannels:
            debugmsg(1013);
            return 16;
        case AHIDB_MaxPlaySamples:
            debugmsg(1014);
            return def + 0x8000;
        case AHIDB_MaxRecordSamples:
            debugmsg(1015);
            return 0;
        case AHIDB_MinMonitorVolume:
            debugmsg(1016);
            return 0x0;
        case AHIDB_MaxMonitorVolume:
            debugmsg(1017);
            return 0x10000;
        case AHIDB_MinInputGain:
            debugmsg(1018);
            return 0x0;
        case AHIDB_MaxInputGain:
            debugmsg(1019);
            return 0x0;
        case AHIDB_MinOutputVolume:
            debugmsg(1020);
            return 0x0;
        case AHIDB_MaxOutputVolume:
            debugmsg(1021);
            return 0x10000;
        case AHIDB_Inputs:
            debugmsg(1022);
            return 0;
        case AHIDB_Input:
            debugmsg(1023);
            return 0;
        case AHIDB_Outputs:
            debugmsg(1024);
            return 1;
        case AHIDB_Output:
            debugmsg(1025);
            switch (arg) {
                case 0: return (int32_t) "Snake 1";
                case 1: return (int32_t) "Snake 2";
                case 2: return (int32_t) "Snake 3";
                case 3: return (int32_t) "Snake 4";
                case 4: return (int32_t) "Snake 5";
                case 5: return (int32_t) "Snake 6";
                case 6: return (int32_t) "Snake 7";
                case 7: return (int32_t) "Snake 8";
            }
        default:
            debugmsg(1099);
            return def;
    }
}

static int32_t __attribute__((used)) intAHIsub_HardwareControl(uint32_t attr asm("d0"), uint32_t arg asm("d1"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    return 0;

    debugmsg(11);
    int32_t rc = TRUE;
    struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;

    switch (attr) {
        case AHIC_MonitorVolume:
            debugmsg(1101);
            ahi_data->monitor_volume = arg;
            break;
        case AHIC_MonitorVolume_Query:
            debugmsg(1102);
            rc = ahi_data->monitor_volume;
            break;
        case AHIC_InputGain:
            debugmsg(1103);
            ahi_data->input_gain = arg;
            break;
        case AHIC_InputGain_Query:
            debugmsg(1104);
            rc = ahi_data->input_gain;
            break;
        case AHIC_OutputVolume:
            debugmsg(1105);
            ahi_data->output_volume = arg;
            break;
        case AHIC_OutputVolume_Query:
            debugmsg(1106);
            rc = ahi_data->output_volume;
            break;
        case AHIC_Input:
            debugmsg(1107);
            break;
        case AHIC_Input_Query:
            debugmsg(1108);
            break;
        case AHIC_Output:
            debugmsg(1109);
            rc = TRUE;
            break;
        case AHIC_Output_Query:
            debugmsg(1110);
            rc = 0;
            break;
        default:
            debugmsg(1199);
            rc = FALSE;
            break;
    }

    return rc;
}

static uint32_t __attribute__((used)) intAHIsub_SetEffect(uint8_t *effect asm("a0"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(12);
    return AHIS_UNKNOWN;
}

static uint32_t __attribute__((used)) intAHIsub_LoadSound(uint16_t sound asm("d0"), uint32_t type asm("d1"), struct AHISampleInfo *info asm("a0"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(13);
    debugshort(AHI_U1, sound);
    debuglong(AHI_U321, type);
    debuglong(AHI_U322, (uint32_t)info);
    debugmsg(131);
    if (type == AHIST_DYNAMICSAMPLE) {
        debuglong(AHI_U321, info->ahisi_Type);
        debuglong(AHI_U322, (uint32_t)info->ahisi_Address);
        debuglong(AHI_U323, info->ahisi_Length);
        debugmsg(134);
    }
    else {
        debugmsg(135);
    }

    return AHIS_UNKNOWN;
}

static uint32_t __attribute__((used)) intAHIsub_UnloadSound(uint16_t sound asm("d0"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(132);
    debugshort(AHI_U1, sound);
    debugmsg(133);

    return AHIS_UNKNOWN;
}

static void __attribute__((used)) intAHIsub_Enable(struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(14);
    Enable();
    //WRITESHORT(AHI_DISABLE, 0);
}

static void __attribute__((used)) intAHIsub_Disable(struct AHIAudioCtrlDrv *AudioCtrl asm("a2"))
{
    debugmsg(15);
    Disable();
    //WRITESHORT(AHI_DISABLE, 1);
}

static uint32_t __attribute__((used)) intAHIsub_Update(uint32_t flags asm("d0"), struct AHIAudioCtrlDrv *AudioCtrlDrv asm("a2"))
{
    debugmsg(16);
    return AHIS_UNKNOWN;
}

static uint32_t __attribute__((used)) intAHIsub_SetVol(uint16_t channel asm("d0"), uint32_t volume asm("d1"), uint32_t pan asm("d2"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"), uint32_t flags asm("d3"))
{
    debugmsg(17);
    debugshort(AHI_U1, channel);
    debuglong(AHI_U321, volume);
    debuglong(AHI_U322, pan);
    debuglong(AHI_U323, flags);
    debugmsg(171);

    return AHIS_UNKNOWN;
}

static uint32_t __attribute__((used)) intAHIsub_SetFreq(uint16_t channel asm("d0"), uint32_t freq asm("d1"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"), uint32_t flags asm("d2"))
{
    debugmsg(18);
    debugshort(AHI_U1, channel);
    debuglong(AHI_U321, freq);
    debuglong(AHI_U323, flags);
    debugmsg(181);
    WRITELONG(AHI_U321, freq);
    WRITELONG(AHI_COMMAND, AHI_CMD_RATE);

    return AHIS_UNKNOWN;
}

static uint32_t __attribute__((used)) intAHIsub_SetSound(uint16_t channel asm("d0"), uint16_t sound asm("d1"), uint32_t offset asm("d2"), int32_t length asm("d3"), struct AHIAudioCtrlDrv *AudioCtrl asm("a2"), uint32_t flags asm("d4"))
{
    debugmsg(19);
    debugshort(AHI_U1, channel);
    debuglong(AHI_U321, sound);
    debuglong(AHI_U323, offset);
    debuglong(AHI_U323, length);
    debuglong(AHI_U324, flags);
    debugmsg(191);

    struct pi_ahi *ahi_data = (struct pi_ahi*)AudioCtrl->ahiac_DriverData;

    debuglong(AHI_U321, (uint32_t)ahi_data);
    debuglong(AHI_U322, (uint32_t)AudioCtrl->ahiac_PlayerFunc);
    debuglong(AHI_U323, AudioCtrl->ahiac_PlayerFreq);
    debuglong(AHI_U324, (uint32_t)AudioCtrl->ahiac_PreTimer);
    debugmsg(192);

    Signal((struct Task *)ahi_data->slave_process, 1L << ahi_data->mix_signal);

    Permit();

    return AHIS_UNKNOWN;
}

static uint32_t function_table[] = {
    (uint32_t)open,
    (uint32_t)close,
    (uint32_t)expunge,
    (uint32_t)null,
    (uint32_t)intAHIsub_AllocAudio,             // AllocAudio
    (uint32_t)intAHIsub_FreeAudio,              // FreeAudio
    (uint32_t)intAHIsub_Disable,                // Disable
    (uint32_t)intAHIsub_Enable,                 // Enable
    (uint32_t)intAHIsub_Start,                  // Start
    (uint32_t)intAHIsub_Update,                 // Update
    (uint32_t)intAHIsub_Stop,                   // Stop
    (uint32_t)intAHIsub_SetVol,                 // SetVol
    (uint32_t)intAHIsub_SetFreq,                // SetFreq
    (uint32_t)intAHIsub_SetSound,               // SetSound
    (uint32_t)intAHIsub_SetEffect,              // SetEffect
    (uint32_t)intAHIsub_LoadSound,              // LoadSound
    (uint32_t)intAHIsub_UnloadSound,            // UnloadSound
    (uint32_t)intAHIsub_GetAttr,                // GetAttr
    (uint32_t)intAHIsub_HardwareControl,        // HardwareControl
    (uint32_t)null,
    (uint32_t)null,
    (uint32_t)null,
    -1
};

const uint32_t auto_init_tables[4] = {
    sizeof(struct pi_ahi),
    (uint32_t)function_table,
    0,
    (uint32_t)init,
};
