// SPDX-License-Identifier: MIT

#include <proto/intuition.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <workbench/startup.h>
#include <clib/expansion_protos.h>
#include <libraries/reqtools.h>
#include <clib/reqtools_protos.h>

#include "pistorm_dev.h"
#include "../pistorm-dev-enums.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern unsigned int pistorm_base_addr;
struct ReqToolsBase *ReqToolsBase;

#define VERSION "v0.4.0"

#define button1w 54
#define button1h 11

#define button2w 87
#define button2h 11

#define button3w 100
#define button3h 11

#define tbox1w 162
#define tbox1h 10

#define tbox2w 164
#define tbox2h 12

#define statusbarw 507
#define statusbarh 10

static int tick_counter = 0;

struct TextAttr font =
{
        "topaz.font",
        8,
        FS_NORMAL,
        0
};

SHORT SharedBordersPairs0[] =
{
    0, 0, 0, button1h - 1, 1, button1h - 2, 1, 0, button1w - 2, 0
};
SHORT SharedBordersPairs1[] =
{
    1, button1h - 1, button1w - 2, button1h - 1, button1w - 2, 1, button1w - 1, 0, button1w - 1, button1h - 1
};

SHORT SharedBordersPairs2[] =
{
    0, 0, 0, button3h - 1, 1, button3h - 2, 1, 0, button3w - 2, 0
};
SHORT SharedBordersPairs3[] =
{
    1, button3h - 1, button3w - 2, button3h - 1, button3w - 2, 1, button3w - 1, 0, button3w - 1, button3h - 1
};

SHORT SharedBordersPairs4[] =
{
    0, 0, 0, button2h - 1, 1, button2h - 2, 1, 0, button2w - 2, 0
};
SHORT SharedBordersPairs5[] =
{
    1, button2h - 1, button2w - 2, button2h - 1, button2w - 2, 1, button2w - 1, 0, button2w - 1, button2h - 1
};

SHORT SharedBordersPairs6[] =
{
    -2, -1, -2, tbox1h - 1, -1, tbox1h - 2, -1, -1, tbox1w - 2, -1
};

SHORT SharedBordersPairs7[] =
{
    -1, tbox1h - 1, tbox1w - 2, tbox1h - 1, tbox1w - 2, 0, tbox1w - 1, -1, tbox1w - 1, tbox1h - 1
};

SHORT SharedBordersPairs8[] =
{
    0, 0, statusbarw - 2, 0, statusbarw - 2, 0, 0, 0, 0, 0
};

SHORT SharedBordersPairs9[] =
{
    0, 0, 0, tbox2h - 1, 1, tbox2h - 2, 1, 0, tbox2w - 2, 0
};
SHORT SharedBordersPairs10[] =
{
    1, tbox2h - 1, tbox2w - 2, tbox2h - 1, tbox2w - 2, 1, tbox2w - 1, 0, tbox2w - 1, tbox2h - 1
};


struct Border SharedBorders[] =
{
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs0[0], &SharedBorders[1], // Button 1
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs1[0], NULL,
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs2[0], &SharedBorders[3], // Button 3
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs3[0], NULL,
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs4[0], &SharedBorders[5], // Button 2
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs5[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs6[0], &SharedBorders[7], // TBox
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs7[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs8[0], NULL, // Statusbar
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs9[0], &SharedBorders[10], // TBox inverted
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs10[0], NULL,
};

struct Border SharedBordersInvert[] =
{
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs0[0], &SharedBordersInvert[1], // Button 1
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs1[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs2[0], &SharedBordersInvert[3], // Button 3
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs3[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs4[0], &SharedBordersInvert[5], // Button 2
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs5[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs9[0], &SharedBordersInvert[7], // TBox inverted
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs10[0], NULL,
};

UBYTE RTG_filter_buf[64] = "";

struct IntuiText RTG_filter_text =
{
    1, 0, JAM2, 8, 2, &font, (UBYTE *)RTG_filter_buf, NULL
};

#define GADRTGFILTERBUTTON 16

struct Gadget RTGFilterButton =
{
    NULL, 410, 19, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &RTG_filter_text, 0, NULL, GADRTGFILTERBUTTON, NULL
};

UBYTE RTG_scale_buf[64] = "";

struct IntuiText RTG_scale_text =
{
    1, 0, JAM2, 8, 2, &font, (UBYTE *)RTG_scale_buf, NULL
};

#define GADRTGSCALEBUTTON 15

struct Gadget RTGScaleButton =
{
    &RTGFilterButton, 300, 19, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &RTG_scale_text, 0, NULL, GADRTGSCALEBUTTON, NULL
};

struct IntuiText KickstartCommit_text =
{
    1, 0, JAM2, 2, 2, &font, (UBYTE *)"Commit", NULL
};

#define GADKICKSTARTCOMMIT 14

struct Gadget KickstartCommit =
{
    &RTGScaleButton, 433, 49, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &KickstartCommit_text, 0, NULL, GADKICKSTARTCOMMIT, NULL
};

#define KICKSTART_TXT_SIZE 255
UBYTE KickstartFileValue_buf[KICKSTART_TXT_SIZE];

struct StringInfo KickstartFileValue =
{
    KickstartFileValue_buf, NULL, 0, 255, 0, 0, 0, 0, 4, 4, NULL, 0, NULL
};

struct IntuiText KickstartFile_text =
{
    1, 0, JAM2, 0, -10, &font, "Kickstart file:", NULL
};

#define GADKICKSTARTFILE 13

struct Gadget KickstartFile =
{
    &KickstartCommit, 266, 50, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    STRGADGET,
    (APTR) &SharedBorders[6], NULL,
    &KickstartFile_text, 0, (APTR)&KickstartFileValue, GADKICKSTARTFILE, NULL
};

struct IntuiText ShutdownButton_text =
{
    1, 0, JAM2, 2, 2, &font, (UBYTE *)"Shutdown Pi", NULL
};

#define GADSHUTDOWN 12

struct Gadget ShutdownButton =
{
    &KickstartFile, 60, 166, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &ShutdownButton_text, 0, NULL, GADSHUTDOWN, NULL
};

#define DESTINATION_TXT_SIZE 21

UBYTE DestinationValue_buf[DESTINATION_TXT_SIZE];
UBYTE Destination_buf[255];

struct IntuiText Destination_text[] =
{
    1, 0, JAM2, -98, 1, &font, "Destination:", &Destination_text[1],
    1, 0, JAM2, 1, 1, &font, DestinationValue_buf, NULL,
};

#define GADGETDESTINATION 11

struct Gadget GetDestination =
{
    &ShutdownButton, 107, 105, tbox2w, tbox2h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[9], (APTR) &SharedBordersInvert[6],
    Destination_text, 0, NULL, GADGETDESTINATION, NULL
};

struct IntuiText RebootButton_text =
{
    1, 0, JAM2, 2, 2, &font, (UBYTE *)"Reboot", NULL
};

#define GADREBOOT 10

struct Gadget RebootButton =
{
    &GetDestination, 4, 166, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &RebootButton_text, 0, NULL, GADREBOOT, NULL
};

#define STATUSBAR_TXT_SIZE 128

UBYTE StatusBar_buf[STATUSBAR_TXT_SIZE] = "";

struct IntuiText StatusBar_text =
{
    1, 0, JAM2, 4, 2, &font, (UBYTE *)StatusBar_buf, NULL
};

#define GADSTATUSBAR 9

struct Gadget StatusBar =
{
    &RebootButton, 3, 188, 508, 10,
    GADGHIMAGE,
    0,
    BOOLGADGET,
    (APTR) &SharedBorders[8], NULL,
    &StatusBar_text, 0, NULL, GADSTATUSBAR, NULL
};


struct IntuiText RetrieveButton_text =
{
    1, 0, JAM2, 10, 2, &font, (UBYTE *)"Retrieve", NULL
};

#define GADRETRIEVEBUTTON 8

struct Gadget RetrieveButton =
{
    &StatusBar, 276, 99, button2w, button2h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[4], (APTR) &SharedBordersInvert[4],
    &RetrieveButton_text, 0, NULL, GADRETRIEVEBUTTON, NULL
};

#define GETFILE_TXT_SIZE 255

UBYTE GetFileValue_buf[GETFILE_TXT_SIZE];

struct StringInfo GetFileValue =
{
    GetFileValue_buf, NULL, 0, 255, 0, 0, 0, 0, 4, 4, NULL, 0, NULL
};

struct IntuiText GetFile_text[] =
{
    1, 0, JAM2, -98, -10, &font, "Get file from PiStorm:", &GetFile_text[1],
    1, 0, JAM2, -59, 1, &font, "Source:", NULL,
};

#define GADGETFILE 7

struct Gadget GetFile =
{
    &RetrieveButton, 108, 93, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    STRGADGET,
    (APTR) &SharedBorders[6], NULL,
    GetFile_text, 0, (APTR)&GetFileValue, GADGETFILE, NULL
};

struct IntuiText ConfigDefault_text =
{
    1, 0, JAM2, 2, 2, &font, (UBYTE *)"Load Default", NULL
};

#define GADCONFIGDEFAULT 6

struct Gadget ConfigDefault =
{
    &GetFile, 9, 62, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &ConfigDefault_text, 0, NULL, GADCONFIGDEFAULT, NULL
};

struct IntuiText ConfigCommit_text =
{
    1, 0, JAM2, 2, 2, &font, (UBYTE *)"Commit", NULL
};

#define GADCONFIGCOMMIT 5

struct Gadget ConfigCommit =
{
    &ConfigDefault, 176, 49, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &ConfigCommit_text, 0, NULL, GADCONFIGCOMMIT, NULL
};

#define CONFIGFILE_TXT_SIZE 255

UBYTE ConfigFileValue_buf[CONFIGFILE_TXT_SIZE];

struct StringInfo ConfigFileValue =
{
    ConfigFileValue_buf, NULL, 0, 255, 0, 0, 0, 0, 4, 4, NULL, 0, NULL
};

struct IntuiText ConfigFile_text =
{
    1, 0, JAM2, 0, -10, &font, "Config file:", NULL
};

#define GADCONFIGFILE 4

struct Gadget ConfigFile =
{
    &ConfigCommit, 10, 50, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    STRGADGET,
    (APTR) &SharedBorders[6], NULL,
    &ConfigFile_text, 0, (APTR)&ConfigFileValue, GADCONFIGFILE, NULL
};

#define RTGSTATUS_TXT_SIZE 64

UBYTE RTGStatus_buf[RTGSTATUS_TXT_SIZE] = "RTG status";

struct IntuiText RTGStatus_text =
{
    1, 0, JAM2, 1, 1, &font, (UBYTE *)RTGStatus_buf, NULL
};

#define GADRTGSTATUS 3

struct Gadget RTGStatus =
{
    &ConfigFile, 10, 20, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    BOOLGADGET,
    (APTR) &SharedBorders[6], NULL,
    &RTGStatus_text, 0, NULL, GADRTGSTATUS, NULL
};

#define RTGENABLE_TXT_SIZE 64

UBYTE RTG_buf[64] = "RTG Enable";

struct IntuiText RTG_text =
{
    1, 0, JAM2, 8, 2, &font, (UBYTE *)RTG_buf, NULL
};

#define GADRTGBUTTON 2

struct Gadget RTGButton =
{
    &RTGStatus, 176, 19, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &RTG_text, 0, NULL, GADRTGBUTTON, NULL
};

struct IntuiText AboutButton_text =
{
    1, 0, JAM2, 8, 2, &font, (UBYTE *)"About", NULL
};

#define GADABOUT 1

struct Gadget AboutButton =
{
    &RTGButton, 356, 166, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &AboutButton_text, 0, NULL, GADABOUT, NULL
};


struct IntuiText QuitButton_text =
{
    1, 0, JAM2, 12, 2, &font, (UBYTE *)"Quit", NULL
};

#define GADQUIT 0

struct Gadget QuitButton =
{
    &AboutButton, 438, 166, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &QuitButton_text, 0, NULL, GADQUIT, NULL
};


struct NewWindow winlayout =
{
    0, 0,
    512, 200,
    -1, -1,
    CLOSEWINDOW | GADGETUP | GADGETDOWN | INTUITICKS,
    ACTIVATE | WINDOWCLOSE | WINDOWDRAG | WINDOWDEPTH,
    &QuitButton, NULL,
    (STRPTR)"PiStorm Interaction Tool",
    NULL, NULL,
    0, 0,
    600, 400,
    WBENCHSCREEN
};

static void WriteGadgetText(const char *text, UBYTE *buffer, struct Window *window, struct Gadget *gadget, int gad_max)
{
    ULONG newlen = strlen(text);
    ULONG oldlen = strlen((char *)buffer);

    if (newlen < oldlen)
    {
        snprintf((char *)buffer, gad_max-1, "%s%*.*s", text, (int)(oldlen - newlen),
                 (int)(oldlen - newlen), " ");
    }
    else
    {
        strncpy((char *)buffer, text, gad_max-1);
    }

    RefreshGadgets(&QuitButton, window, NULL);
}
static void updateRTG(struct Window *window)
{
    unsigned short rtg = pi_get_rtg_status();
    if (rtg & 0x01)
    {
        WriteGadgetText("Disable RTG", RTG_buf, window, &RTGButton, RTGENABLE_TXT_SIZE);
        if (rtg & 0x02)
        {
            WriteGadgetText("RTG in use", RTGStatus_buf, window, &RTGStatus, RTGSTATUS_TXT_SIZE);
        }
        else
        {
            WriteGadgetText("RTG not in use", RTGStatus_buf, window, &RTGStatus, RTGSTATUS_TXT_SIZE);
        }
    }
    else
    {
        WriteGadgetText("Enable RTG", RTG_buf, window, &RTGButton, RTGENABLE_TXT_SIZE);
        WriteGadgetText("RTG disabled", RTGStatus_buf, window, &RTGStatus, RTGSTATUS_TXT_SIZE);
    }
    unsigned short filter = pi_get_rtg_scale_mode();
    switch (filter)
    {
        case PIGFX_SCALE_NONE:
            WriteGadgetText("None", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_INTEGER_MAX:
            WriteGadgetText("Max integer", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_FULL_ASPECT:
            WriteGadgetText("Full aspect", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_FULL_43:
            WriteGadgetText("Full 4:3", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_FULL_169:
            WriteGadgetText("Full 16:9", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_FULL:
            WriteGadgetText("Full", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_SCALE_CUSTOM:
        case PIGFX_SCALE_CUSTOM_RECT:
        case PIGFX_SCALE_NUM:
        default:
            WriteGadgetText("Custom", RTG_scale_buf, window, &RTGScaleButton, RTGENABLE_TXT_SIZE);
            break;
    }

    unsigned short scale = pi_get_rtg_scale_filter();
    switch (scale)
    {
        case PIGFX_FILTER_POINT:
            WriteGadgetText("Point", RTG_filter_buf, window, &RTGFilterButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_FILTER_SMOOTH:
            WriteGadgetText("Smooth", RTG_filter_buf, window, &RTGFilterButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_FILTER_SHADER:
            WriteGadgetText("Shader", RTG_filter_buf, window, &RTGFilterButton, RTGENABLE_TXT_SIZE);
            break;
        case PIGFX_FILTER_NUM:
        default:
            WriteGadgetText("Custom", RTG_filter_buf, window, &RTGFilterButton, RTGENABLE_TXT_SIZE);
            break;
    }

}

static char *GetSavePath()
{
    struct rtFileRequester *filereq;
    char filename[128];
    char *fullpath = malloc(256 * sizeof(char));
    UBYTE *buf = NULL;

    if ((filereq = (struct rtFileRequester*)rtAllocRequestA (RT_FILEREQ, NULL)))
    {
        filename[0] = 0;

        if (!rtFileRequest(filereq, filename, "Pick a destination directory",
                           RTFI_Flags, FREQF_NOFILES, TAG_END))
        {
            free(fullpath);
            return NULL;
        }

    }
    else
    {
        rtEZRequest("Out of memory!", "Oh no!", NULL, NULL);
        return NULL;
    }

    strncpy(fullpath, (char*)filereq->Dir, 256);
    rtFreeRequest((APTR)filereq);
    return fullpath;
}

int questionReboot()
{
    int res = rtEZRequest("This will restart the emulator, rebooting the Amiga\n"
                          "Continue anyway?",
                          "Yes|No", NULL, NULL);
    return res;
}

int main()
{
    struct Window *myWindow;

    IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 0);
    if (IntuitionBase == NULL)
    {
        return RETURN_FAIL;
    }

    if (!(ReqToolsBase = (struct ReqToolsBase *)
                         OpenLibrary (REQTOOLSNAME, REQTOOLSVERSION)))
    {
        static struct IntuiText pos;
        struct IntuiText msg[] =
        {
            1, 0, JAM2, 0, 0, &font, "You need reqtools.library V38 or higher!.", &msg[1],
            1, 0, JAM2, 0, 10, &font, "Please install it in your Libs: drirectory.", NULL,
        };
        AutoRequest(NULL, msg, NULL, &pos, 0, 0, 0, 0);
        return RETURN_FAIL;
    }

    pistorm_base_addr = pi_find_pistorm();
    myWindow = OpenWindow(&winlayout);
    BOOL no_board = FALSE;

    if (pistorm_base_addr == 0xFFFFFFFF)
    {
        rtEZRequest("Unable to find PiStorm autoconf device.",
                    "OK", NULL, NULL);
        no_board = TRUE;
        WriteGadgetText("PiStorm not found", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
    }
    else
    {
        WriteGadgetText("PiStorm found!", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
    }
    if (!no_board)
    {
        updateRTG(myWindow);
    }

    FOREVER
    {
        BOOL closewin = FALSE;
        struct IntuiMessage *message;
        Wait(1 << myWindow->UserPort->mp_SigBit);

        while ((message = (struct IntuiMessage*)GetMsg(myWindow->UserPort)))
        {
            ULONG class = message->Class;
            struct Gadget *address = (struct Gadget*)message->IAddress;
            ReplyMsg((struct Message*)message);

            if (class == CLOSEWINDOW)
            {
                closewin = TRUE;
            }
            else if ((class == INTUITICKS) && (!no_board))
            {
                tick_counter++;
                if ((tick_counter % 10) == 0)
                {
                    char buf[32];
                    unsigned short temp = pi_get_temperature();
                    snprintf(buf, 32, "CPU Temperature: %u%cC", temp, 0xb0);
                    WriteGadgetText(buf, StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                }
            }
            else if (class == GADGETUP)
            {
                if (no_board && (address->GadgetID != GADQUIT) && (address->GadgetID != GADABOUT))
                {
                    continue;
                }
                switch (address->GadgetID)
                {
                    case GADQUIT:
                        closewin = TRUE;
                        break;
                    case GADABOUT:
                        {
                            static struct IntuiText pos;
                            char buf2[64], buf3[64];
                            if (!no_board)
                            {
                                unsigned short hw_rev = pi_get_hw_rev();
                                unsigned short sw_rev = pi_get_sw_rev();
                                snprintf(buf2, 64, "PiStorm hardware: %d.%d", (hw_rev >> 8), (hw_rev & 0xFF));
                                snprintf(buf3, 64, "PiStorm software: %d.%d", (sw_rev >> 8), (sw_rev & 0xFF));
                            }
                            else
                            {
                                snprintf(buf2, 64, "PiStorm hardware not found!");
                            }
                            rtEZRequest("PiStorm Interaction Tool %s\n"
                                        "Tool written by beeanyew and LinuxJedi\n"
                                        "%s\n%s\n\n"
                                        "Now with 53%% more Nibbles!",
                                        "More Nibbles!", NULL, NULL, VERSION, buf2, buf3);
                            break;
                        }
                    case GADRTGBUTTON:
                        {
                            unsigned short rtgStatus = pi_get_rtg_status() & 0x01;
                            pi_enable_rtg(!rtgStatus);
                            updateRTG(myWindow);
                            break;
                        }
                    case GADCONFIGCOMMIT:
                        {
                            if (!questionReboot())
                            {
                                break;
                            }
                            Disable();
                            unsigned short ret = pi_handle_config(PICFG_LOAD, ConfigFileValue_buf);
                            if (ret == PI_RES_FILENOTFOUND)
                            {
                                rtEZRequest("PiStorm says: \"file not found\"",
                                            "OK", NULL, NULL);
                            }
                            break;
                        }
                    case GADCONFIGDEFAULT:
                        {
                            if (!questionReboot())
                            {
                                break;
                            }
                            pi_handle_config(PICFG_DEFAULT, NULL);
                            break;
                        }
                    case GADRETRIEVEBUTTON:
                        {
                            unsigned int filesize = 0;
                            char outpath[128];
                            unsigned char *buf;

                            if (pi_get_filesize(GetFileValue_buf, &filesize) == PI_RES_FILENOTFOUND)
                            {
                                rtEZRequest("PiStorm says: \"file not found\"",
                                            "OK", NULL, NULL);
                                break;
                            }
                            buf = malloc(filesize);
                            if (buf == NULL)
                            {
                                rtEZRequest("Could not allocate enough memory to transfer file",
                                            "OK", NULL, NULL);
                                break;
                            }
                            WriteGadgetText("Retrieving file...", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                            if (pi_transfer_file(GetFileValue_buf, buf) != PI_RES_OK)
                            {
                                rtEZRequest("PiStorm says: \"something went wrong with the file transfer\"",
                                            "OK", NULL, NULL);
                                WriteGadgetText("File transfer failed", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                                free(buf);
                                break;
                            }
                            char *fname = strrchr(GetFileValue_buf, '/');
                            if (!fname)
                            {
                                fname = GetFileValue_buf;
                            }
                            else
                            {
                                // Remove leading slash
                                fname++;
                            }
                            char *destfile = malloc(256);
                            // Turns out WB doesn't like DF0:/filename.ext
                            if (Destination_buf[(strlen(Destination_buf) - 1)] == ':')
                            {
                                snprintf(destfile, 255, "%s%s", Destination_buf, fname);
                            }
                            else if (!strlen(Destination_buf))
                            {
                                snprintf(destfile, 255, "%s", fname);
                            }
                            else
                            {
                                snprintf(destfile, 255, "%s/%s", Destination_buf, fname);
                            }
                            BPTR fh = Open(destfile, MODE_NEWFILE);
                            if (!fh)
                            {
                                char errbuf[64];
                                snprintf(errbuf, 64, "Error code: %ld", IoErr());
                                rtEZRequest("Could not open file for writing\n"
                                            "%s\n%s",
                                            "OK", NULL, NULL, destfile, errbuf);
                                WriteGadgetText("File transfer failed", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                                free(buf);
                                free(destfile);
                                break;
                            }
                            Write(fh, buf, filesize);
                            Close(fh);
                            free(destfile);
                            WriteGadgetText("File transfer complete", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                            free(buf);
                            break;
                        }
                    case GADREBOOT:
                        {
                            WriteGadgetText("Rebooting Amiga", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                            pi_reset_amiga(0);
                            break;
                        }
                    case GADGETDESTINATION:
                        {
                            char *fileName = GetSavePath();
                            if (fileName)
                            {
                                strncpy((char*)Destination_buf, fileName, 255);
                                WriteGadgetText(fileName, DestinationValue_buf, myWindow, &GetDestination, DESTINATION_TXT_SIZE);
                                free(fileName);
                            }
                            break;
                        }
                    case GADSHUTDOWN:
                        {
                            int res = rtEZRequest("This will shutdown the Pi and cause the Amiga to freeze\n"
                                                  "Continue anyway?",
                                                  "Yes|No", NULL, NULL);
                            if (!res)
                            {
                                break;
                            }
                            WriteGadgetText("Shutting down PiStorm...", StatusBar_buf, myWindow, &StatusBar, STATUSBAR_TXT_SIZE);
                            int confirm = pi_shutdown_pi(0);
                            pi_confirm_shutdown(confirm);
                            break;
                        }
                    case GADKICKSTARTCOMMIT:
                        {
                            if (!questionReboot())
                            {
                                break;
                            }
                            Disable();
                            unsigned short ret = pi_remap_kickrom(KickstartFileValue_buf);
                            if (ret == PI_RES_FILENOTFOUND)
                            {
                                rtEZRequest("PiStorm says: \"file not found\"",
                                            "OK", NULL, NULL);
                            }
                            break;
                        }
                    case GADRTGSCALEBUTTON:
                        {
                            unsigned short scale = pi_get_rtg_scale_mode();
                            switch (scale)
                            {
                                case PIGFX_SCALE_NONE:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_INTEGER_MAX);
                                    break;
                                case PIGFX_SCALE_INTEGER_MAX:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_FULL_ASPECT);
                                    break;
                                case PIGFX_SCALE_FULL_ASPECT:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_FULL_43);
                                    break;
                                case PIGFX_SCALE_FULL_43:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_FULL_169);
                                    break;
                                case PIGFX_SCALE_FULL_169:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_FULL);
                                    break;
                                case PIGFX_SCALE_FULL:
                                case PIGFX_SCALE_CUSTOM:
                                case PIGFX_SCALE_CUSTOM_RECT:
                                case PIGFX_SCALE_NUM:
                                default:
                                    pi_set_rtg_scale_mode(PIGFX_SCALE_NONE);
                                    break;
                            }
                            updateRTG(myWindow);
                        }
                    case GADRTGFILTERBUTTON:
                        {
                            unsigned short filter = pi_get_rtg_scale_filter();
                            switch (filter)
                            {
                                case PIGFX_FILTER_POINT:
                                    pi_set_rtg_scale_filter(PIGFX_FILTER_SMOOTH);
                                    break;
                                case PIGFX_FILTER_SMOOTH:
                                case PIGFX_FILTER_SHADER:
                                case PIGFX_FILTER_NUM:
                                default:
                                    pi_set_rtg_scale_filter(PIGFX_FILTER_POINT);
                                    break;
                            }
                            updateRTG(myWindow);
                        }
                }
            }
            if (closewin)
            {
                break;
            }
        }
        if (closewin)
        {
            break;
        }
    };
    if (myWindow) CloseWindow(myWindow);
    CloseLibrary((struct Library*)ReqToolsBase);
    return 0;
}
