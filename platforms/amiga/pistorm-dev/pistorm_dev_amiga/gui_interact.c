// SPDX-License-Identifier: MIT

#include <proto/intuition.h>
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <workbench/startup.h>
#include <clib/expansion_protos.h>

#include "pistorm_dev.h"
#include "../pistorm-dev-enums.h"

#include <stdio.h>
#include <string.h>

extern unsigned int pistorm_base_addr;

#define VERSION "v0.1"

#define button1w 54
#define button1h 20

#define button2w 87
#define button2h 20

#define button3w 100
#define button3h 20

#define tbox1w 130
#define tbox1h 18

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
};

struct Border SharedBordersInvert[] =
{
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs0[0], &SharedBordersInvert[1], // Button 1
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs1[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs2[0], &SharedBordersInvert[3], // Button 3
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs3[0], NULL,
    0, 0, 1, 0, JAM2, 5, (SHORT *) &SharedBordersPairs4[0], &SharedBordersInvert[5], // Button 2
    0, 0, 2, 0, JAM2, 5, (SHORT *) &SharedBordersPairs5[0], NULL,
};

struct IntuiText ConfigDefault_text =
{
    1, 0, JAM2, 2, 6, NULL, (UBYTE *)"Load Default", NULL
};

#define GADCONFIGDEFAULT 6

struct Gadget ConfigDefault =
{
    NULL, 304, 39, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &ConfigDefault_text, 0, NULL, GADCONFIGDEFAULT, NULL
};

struct IntuiText ConfigCommit_text =
{
    1, 0, JAM2, 2, 6, NULL, (UBYTE *)"Commit", NULL
};

#define GADCONFIGCOMMIT 5

struct Gadget ConfigCommit =
{
    &ConfigDefault, 244, 39, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &ConfigCommit_text, 0, NULL, GADCONFIGCOMMIT, NULL
};


UBYTE ConfigFileValue_buf[255];

struct StringInfo ConfigFileValue =
{
    ConfigFileValue_buf, NULL, 0, 255, 0, 0, 0, 0, 4, 4, NULL, 0, NULL
};

struct IntuiText ConfigFile_text =
{
    1, 0, JAM2, -98, 4, NULL, "Config file:", NULL
};

#define GADCONFIGFILE 4

struct Gadget ConfigFile =
{
    &ConfigCommit, 108, 41, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    STRGADGET,
    (APTR) &SharedBorders[6], NULL,
    &ConfigFile_text, 0, (APTR)&ConfigFileValue, GADCONFIGFILE, NULL
};

UBYTE RTGStatus_buf[64] = "RTG status";

struct IntuiText RTGStatus_text =
{
    1, 0, JAM2, 4, 4, NULL, (UBYTE *)RTGStatus_buf, NULL
};

#define GADRTGSTATUS 3

struct Gadget RTGStatus =
{
    &ConfigFile, 14, 15, tbox1w, tbox1h,
    GADGHIMAGE,
    0,
    BOOLGADGET,
    (APTR) &SharedBorders[6], NULL,
    &RTGStatus_text, 0, NULL, GADRTGSTATUS, NULL
};

UBYTE RTG_buf[64] = "RTG Enable";

struct IntuiText RTG_text =
{
    1, 0, JAM2, 8, 6, NULL, (UBYTE *)RTG_buf, NULL
};

#define GADRTGBUTTON 2

struct Gadget RTGButton =
{
    &RTGStatus, 150, 13, button3w, button3h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[2], (APTR) &SharedBordersInvert[2],
    &RTG_text, 0, NULL, GADRTGBUTTON, NULL
};

struct IntuiText AboutButton_text =
{
    1, 0, JAM2, 8, 6, NULL, (UBYTE *)"About", NULL
};

#define GADABOUT 1

struct Gadget AboutButton =
{
    &RTGButton, 356, 170, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &AboutButton_text, 0, NULL, GADABOUT, NULL
};


struct IntuiText QuitButton_text =
{
    1, 0, JAM2, 12, 6, NULL, (UBYTE *)"Quit", NULL
};

#define GADQUIT 0

struct Gadget QuitButton =
{
    &AboutButton, 438, 170, button1w, button1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[0], (APTR) &SharedBordersInvert[0],
    &QuitButton_text, 0, NULL, GADQUIT, NULL
};


struct NewWindow winlayout =
{
    20, 20,
    512, 200,
    -1, -1,
    CLOSEWINDOW | GADGETUP | GADGETDOWN,
    ACTIVATE | WINDOWCLOSE | WINDOWDRAG | WINDOWDEPTH,
    &QuitButton, NULL,
    (STRPTR)"PiStorm Interaction Tool",
    NULL, NULL,
    0, 0,
    600, 400,
    WBENCHSCREEN
};

// Pads what we are writing to screen with spaces, otherwise we get bits of
// old text still showing
static void WriteGadgetText(const char *text, UBYTE *buffer, struct Window *window, struct Gadget *gadget)
{
    ULONG newlen = strlen(text);
    ULONG oldlen = strlen((char *)buffer);

    if (newlen < oldlen)
    {
        snprintf((char *)buffer, 64, "%s%*.*s", text, (int)(oldlen - newlen),
                 (int)(oldlen - newlen), " ");
    }
    else
    {
        strncpy((char *)buffer, text, 64);
    }

    RefreshGadgets(gadget, window, NULL);
}
static void updateRTG(struct Window *window)
{
    unsigned short rtg = pi_get_rtg_status();
    if (rtg & 0x01)
    {
        WriteGadgetText("Disable RTG", RTG_buf, window, &RTGButton);
        if (rtg & 0x02)
        {
            WriteGadgetText("RTG in use", RTGStatus_buf, window, &RTGStatus);
        }
        else
        {
            WriteGadgetText("RTG not in use", RTGStatus_buf, window, &RTGStatus);
        }
    }
    else
    {
        WriteGadgetText("Enable RTG", RTG_buf, window, &RTGButton);
        WriteGadgetText("RTG disabled", RTGStatus_buf, window, &RTGStatus);
    }
}

int main()
{
    struct Window *myWindow;

    IntuitionBase = (struct IntuitionBase *) OpenLibrary("intuition.library", 0);
    if (IntuitionBase == NULL)
    {
        return RETURN_FAIL;
    }

    pistorm_base_addr = pi_find_pistorm();
    myWindow = OpenWindow(&winlayout);
    BOOL no_board = FALSE;

    if (pistorm_base_addr == 0xFFFFFFFF)
    {
        static struct IntuiText msg, pos;
        msg.IText = "Unable to find PiStorm autoconf device.";
        pos.IText = "OK";
        AutoRequest(myWindow, &msg, NULL, &pos, 0, 0, 0, 0);
        no_board = TRUE;
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
                            UBYTE buf[64], buf2[64], buf3[64];
                            if (!no_board)
                            {
                                unsigned short hw_rev = pi_get_hw_rev();
                                unsigned short sw_rev = pi_get_sw_rev();
                                snprintf((char*)buf2, 64, "PiStorm hardware: %d.%d", (hw_rev >> 8), (hw_rev & 0xFF));
                                snprintf((char*)buf3, 64, "PiStorm software: %d.%d", (sw_rev >> 8), (sw_rev & 0xFF));
                            }
                            else
                            {
                                snprintf((char*)buf2, 64, "PiStorm hardware not found!");
                            }
                            struct IntuiText msg[] =
                            {
                                1, 0, JAM2, 0, 0, NULL, (UBYTE *)buf, &msg[1],
                                1, 0, JAM2, 0, 10, NULL, "Tool written by beeanyew and LinuxJedi", &msg[2],
                                1, 0, JAM2, 0, 20, NULL, (UBYTE*)buf2, &msg[3],
                                1, 0, JAM2, 0, 30, NULL, (UBYTE*)buf3, &msg[4],
                                1, 0, JAM2, 0, 50, NULL, "Now with 53% more Nibbles!", NULL,
                            };
                            snprintf(buf, 64, "PiStorm Interaction Tool %s", VERSION);
                            pos.IText = "OK";
                            AutoRequest(myWindow, msg, NULL, &pos, 0, 0, 0, 0);
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
                            unsigned short ret = pi_handle_config(PICFG_LOAD, ConfigFileValue_buf);
                            if (ret == PI_RES_FILENOTFOUND)
                            {
                                static struct IntuiText msg, pos;
                                msg.IText = "PiStorm says \"file not found\"";
                                pos.IText = "OK";
                                AutoRequest(myWindow, &msg, NULL, &pos, 0, 0, 0, 0);
                            }
                            break;
                        }
                    case GADCONFIGDEFAULT:
                        {
                            pi_handle_config(PICFG_DEFAULT, NULL);
                            break;
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
    return 0;
}
