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

#define VERSION "v0.2"

#define button1w 54
#define button1h 20

#define button2w 87
#define button2h 20

#define button3w 100
#define button3h 20

#define tbox1w 130
#define tbox1h 18

#define statusbarw 507
#define statusbarh 10

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
    0, 0, 0, tbox1h - 1, 1, tbox1h - 2, 1, 0, tbox1w - 2, 0
};
SHORT SharedBordersPairs10[] =
{
    1, tbox1h - 1, tbox1w - 2, tbox1h - 1, tbox1w - 2, 1, tbox1w - 1, 0, tbox1w - 1, tbox1h - 1
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

UBYTE DestinationValue_buf[255];

struct IntuiText Destination_text[] =
{
    1, 0, JAM2, -98, 6, NULL, "Destination:", &Destination_text[1],
    1, 0, JAM2, 4, 4, NULL, DestinationValue_buf, NULL,
};

#define GADGETDESTINATION 11

struct Gadget GetDestination =
{
    NULL, 108, 117, tbox1w, tbox1h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[9], (APTR) &SharedBordersInvert[6],
    Destination_text, 0, NULL, GADGETDESTINATION, NULL
};

struct IntuiText RebootButton_text =
{
    1, 0, JAM2, 2, 6, NULL, (UBYTE *)"Reboot", NULL
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

UBYTE StatusBar_buf[128] = "PiStorm...";

struct IntuiText StatusBar_text =
{
    1, 0, JAM2, 4, 2, NULL, (UBYTE *)StatusBar_buf, NULL
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
    1, 0, JAM2, 10, 6, NULL, (UBYTE *)"Retrieve", NULL
};

#define GADRETRIEVEBUTTON 8

struct Gadget RetrieveButton =
{
    &StatusBar, 244, 117, button2w, button2h,
    GADGHIMAGE,
    RELVERIFY,
    BOOLGADGET,
    (APTR) &SharedBorders[4], (APTR) &SharedBordersInvert[4],
    &RetrieveButton_text, 0, NULL, GADRETRIEVEBUTTON, NULL
};

UBYTE GetFileValue_buf[255];

struct StringInfo GetFileValue =
{
    GetFileValue_buf, NULL, 0, 255, 0, 0, 0, 0, 4, 4, NULL, 0, NULL
};

struct IntuiText GetFile_text[] =
{
    1, 0, JAM2, -98, -15, NULL, "Get file", &GetFile_text[1],
    1, 0, JAM2, -98, 4, NULL, "Source:", NULL,
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
    1, 0, JAM2, 2, 6, NULL, (UBYTE *)"Load Default", NULL
};

#define GADCONFIGDEFAULT 6

struct Gadget ConfigDefault =
{
    &GetFile, 304, 39, button3w, button3h,
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
    &RTGButton, 356, 166, button1w, button1h,
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
    &AboutButton, 438, 166, button1w, button1h,
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
    }

    strncpy(fullpath, (char*)filereq->Dir, 256);
    return fullpath;
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
            1, 0, JAM2, 0, 0, NULL, "You need reqtools.library V38 or higher!.", &msg[1],
            1, 0, JAM2, 0, 10, NULL, "Please install it in your Libs: drirectory.", NULL,
        };
        AutoRequest(NULL, msg, NULL, &pos, 0, 0, 0, 0);
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
        WriteGadgetText("PiStorm not found", StatusBar_buf, myWindow, &StatusBar);
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
                                msg.IText = "PiStorm says: \"file not found\"";
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
                    case GADRETRIEVEBUTTON:
                        {
                            unsigned int filesize = 0;
                            char outpath[128];
                            unsigned char *buf;

                            if (pi_get_filesize(GetFileValue_buf, &filesize) == PI_RES_FILENOTFOUND)
                            {
                                static struct IntuiText msg, pos;
                                msg.IText = "PiStorm says: \"file not found\"";
                                pos.IText = "OK";
                                AutoRequest(myWindow, &msg, NULL, &pos, 0, 0, 0, 0);
                                break;
                            }
                            buf = malloc(filesize);
                            if (buf == NULL)
                            {
                                static struct IntuiText msg, pos;
                                msg.IText = "Could not allocate enough memory to transfer file";
                                pos.IText = "OK";
                                AutoRequest(myWindow, &msg, NULL, &pos, 0, 0, 0, 0);
                                break;
                            }
                            WriteGadgetText("Retrieving file...", StatusBar_buf, myWindow, &StatusBar);
                            if (pi_transfer_file(GetFileValue_buf, buf) != PI_RES_OK)
                            {
                                static struct IntuiText msg, pos;
                                msg.IText = "PiStorm says: \"something went wrong with the file transfer\"";
                                pos.IText = "OK";
                                AutoRequest(myWindow, &msg, NULL, &pos, 0, 0, 0, 0);
                                WriteGadgetText("File transfer failed", StatusBar_buf, myWindow, &StatusBar);
                                free(buf);
                                break;
                            }
                            char *fname = strrchr(GetFileValue_buf, '/');
                            if (!fname)
                            {
                                fname = GetFileValue_buf;
                            }
                            char *destfile = malloc(256);
                            // Turns out WB doesn't like DF0:/filename.ext
                            if (DestinationValue_buf[(strlen(DestinationValue_buf) - 1)] == ':')
                            {
                                snprintf(destfile, 255, "%s%s", DestinationValue_buf, GetFileValue_buf);
                            }
                            else if (!strlen(DestinationValue_buf))
                            {
                                snprintf(destfile, 255, "%s", GetFileValue_buf);
                            }
                            else
                            {
                                snprintf(destfile, 255, "%s/%s", DestinationValue_buf, GetFileValue_buf);
                            }
                            BPTR fh = Open(destfile, MODE_NEWFILE);
                            if (!fh)
                            {
                                char errbuf[64];
                                snprintf(errbuf, 64, "Error code: %ld", IoErr());
                                struct IntuiText msg[] =
                                {
                                    1, 0, JAM2, 0, 0, NULL, "Could not open file for writing", &msg[1],
                                    1, 0, JAM2, 0, 10, NULL, destfile, &msg[2],
                                    1, 0, JAM2, 0, 20, NULL, (UBYTE*)errbuf, NULL,
                                };
                                static struct IntuiText pos;
                                pos.IText = "OK";
                                AutoRequest(myWindow, msg, NULL, &pos, 0, 0, 0, 0);
                                WriteGadgetText("File transfer failed", StatusBar_buf, myWindow, &StatusBar);
                                free(buf);
                                free(destfile);
                                break;
                            }
                            Write(fh, buf, filesize);
                            Close(fh);
                            free(destfile);
                            WriteGadgetText("File transfer complete", StatusBar_buf, myWindow, &StatusBar);
                            free(buf);
                            break;
                        }
                    case GADREBOOT:
                        {
                            WriteGadgetText("Rebooting PiStorm", StatusBar_buf, myWindow, &StatusBar);
                            pi_reset_amiga(0);
                            break;
                        }
                    case GADGETDESTINATION:
                        {
                            char *fileName = GetSavePath();
                            if (fileName)
                            {
                                WriteGadgetText(fileName, DestinationValue_buf, myWindow, &GetDestination);
                                free(fileName);
                            }
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
