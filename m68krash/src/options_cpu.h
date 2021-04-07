/*
  Hatari - options_cpu.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef OPTIONS_CPU_H
#define OPTIONS_CPU_H

#define MAX_CUSTOM_MEMORY_ADDRS 2

struct uae_prefs {
/*    int 	struct strlist *all_lines; */

	TCHAR description[256];
	TCHAR info[256];
	int config_version;
	TCHAR config_hardware_path[MAX_DPATH];
	TCHAR config_host_path[MAX_DPATH];

	bool illegal_mem;
	bool use_serial;
	bool serial_demand;
	bool serial_hwctsrts;
	bool serial_direct;
	bool parallel_demand;
	int parallel_matrix_emulation;
	bool parallel_postscript_emulation;
	bool parallel_postscript_detection;
	int parallel_autoflush_time;
	TCHAR ghostscript_parameters[256];
	bool use_gfxlib;
	bool socket_emu;

	bool start_debugger;
	bool start_gui;

	/*KbdLang keyboard_lang;*/

	int produce_sound;
	int sound_stereo;
	int sound_stereo_separation;
	int sound_mixed_stereo_delay;
	int sound_freq;
	int sound_maxbsiz;
	int sound_latency;
	int sound_interpol;
	int sound_filter;
	int sound_filter_type;
	int sound_volume;
	bool sound_stereo_swap_paula;
	bool sound_stereo_swap_ahi;
	bool sound_auto;

	int comptrustbyte;
	int comptrustword;
	int comptrustlong;
	int comptrustnaddr;
	bool compnf;
	bool compfpu;
	bool comp_midopt;
	bool comp_lowopt;
	bool fpu_strict;

	bool comp_hardflush;
	bool comp_constjump;
	bool comp_oldsegv;

	int cachesize;
	int optcount[10];

	bool avoid_cmov;

	int gfx_display;
	TCHAR gfx_display_name[256];
	int gfx_framerate, gfx_autoframerate;
	/*
	struct wh gfx_size_win;
	struct wh gfx_size_fs;
	struct wh gfx_size;
	struct wh gfx_size_win_xtra[4];
	struct wh gfx_size_fs_xtra[4];
	*/
    bool int_no_unimplemented;
	bool gfx_autoresolution;
	bool gfx_scandoubler;
	int gfx_refreshrate;
	int gfx_avsync, gfx_pvsync;
	int gfx_resolution;
	int gfx_vresolution;
	int gfx_lores_mode;
	int gfx_scanlines;
	int gfx_afullscreen, gfx_pfullscreen;
	int gfx_xcenter, gfx_ycenter;
	int gfx_xcenter_pos, gfx_ycenter_pos;
	int gfx_xcenter_size, gfx_ycenter_size;
	int gfx_max_horizontal, gfx_max_vertical;
	int gfx_saturation, gfx_luminance, gfx_contrast, gfx_gamma;
	bool gfx_blackerthanblack;
	int gfx_backbuffers;
	int gfx_api;
	int color_mode;

	int gfx_filter;
	TCHAR gfx_filtershader[MAX_DPATH];
	TCHAR gfx_filtermask[MAX_DPATH];
	TCHAR gfx_filteroverlay[MAX_DPATH];
	/*struct wh gfx_filteroverlay_pos;*/
	int gfx_filteroverlay_overscan;
	int gfx_filter_scanlines;
	int gfx_filter_scanlineratio;
	int gfx_filter_scanlinelevel;
	int gfx_filter_horiz_zoom, gfx_filter_vert_zoom;
	int gfx_filter_horiz_zoom_mult, gfx_filter_vert_zoom_mult;
	int gfx_filter_horiz_offset, gfx_filter_vert_offset;
	int gfx_filter_filtermode;
	int gfx_filter_bilinear;
	int gfx_filter_noise, gfx_filter_blur;
	int gfx_filter_saturation, gfx_filter_luminance, gfx_filter_contrast, gfx_filter_gamma;
	int gfx_filter_keep_aspect, gfx_filter_aspect;
	int gfx_filter_autoscale;

	bool immediate_blits;
	unsigned int chipset_mask;
	bool ntscmode;
	bool genlock;
	int chipset_refreshrate;
	int collision_level;
	int leds_on_screen;
	int keyboard_leds[3];
	bool keyboard_leds_in_use;
	int scsi;
	bool sana2;
	bool uaeserial;
	int catweasel;
	int cpu_idle;
	bool cpu_cycle_exact;
	int cpu_clock_multiplier;
	int cpu_frequency;
	bool blitter_cycle_exact;
	int floppy_speed;
	int floppy_write_length;
	int floppy_random_bits_min;
	int floppy_random_bits_max;
	bool tod_hack;
	uae_u32 maprom;
	int turbo_emulation;
	bool headless;

	int cs_compatible;
	int cs_ciaatod;
	int cs_rtc;
	int cs_rtc_adjust;
	int cs_rtc_adjust_mode;
	bool cs_ksmirror_e0;
	bool cs_ksmirror_a8;
	bool cs_ciaoverlay;
	bool cs_cd32cd;
	bool cs_cd32c2p;
	bool cs_cd32nvram;
	bool cs_cdtvcd;
	bool cs_cdtvram;
	int cs_cdtvcard;
	int cs_ide;
	bool cs_pcmcia;
	bool cs_a1000ram;
	int cs_fatgaryrev;
	int cs_ramseyrev;
	int cs_agnusrev;
	int cs_deniserev;
	int cs_mbdmac;
	bool cs_cdtvscsi;
	bool cs_a2091, cs_a4091;
	bool cs_df0idhw;
	bool cs_slowmemisfast;
	bool cs_resetwarning;
	bool cs_denisenoehb;
	bool cs_dipagnus;
	bool cs_agnusbltbusybug;

	TCHAR romfile[MAX_DPATH];
	TCHAR romident[256];
	TCHAR romextfile[MAX_DPATH];
	TCHAR romextident[256];
	TCHAR flashfile[MAX_DPATH];
	TCHAR cartfile[MAX_DPATH];
	TCHAR cartident[256];
	int cart_internal;
	TCHAR pci_devices[256];
	TCHAR prtname[256];
	TCHAR sername[256];
	TCHAR amaxromfile[MAX_DPATH];
	TCHAR a2065name[MAX_DPATH];
	/*struct cdslot cdslots[MAX_TOTAL_SCSI_DEVICES];*/
	TCHAR quitstatefile[MAX_DPATH];
	TCHAR statefile[MAX_DPATH];
	TCHAR inprecfile[MAX_DPATH];
	int inprecmode;

	TCHAR path_floppy[256];
	TCHAR path_hardfile[256];
	TCHAR path_rom[256];

	int m68k_speed;
	int cpu_model;
	int cpu_level;
	int mmu_model;
	int cpu060_revision;
	int fpu_model;
	int fpu_revision;
	bool cpu_compatible;
	bool address_space_24;
	bool picasso96_nocustom;
	int picasso96_modeflags;

	uae_u32 z3fastmem_size, z3fastmem2_size;
	uae_u32 z3fastmem_start;
	uae_u32 z3chipmem_size;
	uae_u32 z3chipmem_start;
	uae_u32 fastmem_size;
	uae_u32 chipmem_size;
	uae_u32 bogomem_size;
	uae_u32 mbresmem_low_size;
	uae_u32 mbresmem_high_size;
	uae_u32 gfxmem_size;
	uae_u32 custom_memory_addrs[MAX_CUSTOM_MEMORY_ADDRS];
	uae_u32 custom_memory_sizes[MAX_CUSTOM_MEMORY_ADDRS];

	bool kickshifter;
	bool filesys_no_uaefsdb;
	bool filesys_custom_uaefsdb;
	bool mmkeyboard;
	int uae_hide;

	int mountitems;
	/*struct uaedev_config_info mountconfig[MOUNT_CONFIG_SIZE];*/

	int nr_floppies;
	/*struct floppyslot floppyslots[4];*/
	/*TCHAR dfxlist[MAX_SPARE_DRIVES][MAX_DPATH];*/
	int dfxclickvolume;
	int dfxclickchannelmask;

	/* Target specific options */

	bool win32_middle_mouse;
	bool win32_logfile;
	bool win32_notaskbarbutton;
	bool win32_alwaysontop;
	bool win32_powersavedisabled;
	bool win32_minimize_inactive;
	int win32_statusbar;

	int win32_active_priority;
	int win32_inactive_priority;
	bool win32_inactive_pause;
	bool win32_inactive_nosound;
	int win32_iconified_priority;
	bool win32_iconified_pause;
	bool win32_iconified_nosound;

	bool win32_rtgmatchdepth;
	bool win32_rtgscaleifsmall;
	bool win32_rtgallowscaling;
	int win32_rtgscaleaspectratio;
	int win32_rtgvblankrate;
	bool win32_borderless;
	bool win32_ctrl_F11_is_quit;
	bool win32_automount_removable;
	bool win32_automount_drives;
	bool win32_automount_cddrives;
	bool win32_automount_netdrives;
	bool win32_automount_removabledrives;
	int win32_midioutdev;
	int win32_midiindev;
	int win32_uaescsimode;
	int win32_soundcard;
	int win32_samplersoundcard;
	bool win32_soundexclusive;
	bool win32_norecyclebin;
	int win32_specialkey;
	int win32_guikey;
	int win32_kbledmode;
	TCHAR win32_commandpathstart[MAX_DPATH];
	TCHAR win32_commandpathend[MAX_DPATH];
	TCHAR win32_parjoyport0[MAX_DPATH];
	TCHAR win32_parjoyport1[MAX_DPATH];

	bool statecapture;
	int statecapturerate, statecapturebuffersize;

	/* input */

	/*struct jport jports[MAX_JPORTS];*/
	int input_selected_setting;
	int input_joymouse_multiplier;
	int input_joymouse_deadzone;
	int input_joystick_deadzone;
	int input_joymouse_speed;
	int input_analog_joystick_mult;
	int input_analog_joystick_offset;
	int input_autofire_linecnt;
	int input_mouse_speed;
	int input_tablet;
	bool input_magic_mouse;
	int input_magic_mouse_cursor;
	/*
	struct uae_input_device joystick_settings[MAX_INPUT_SETTINGS][MAX_INPUT_DEVICES];
	struct uae_input_device mouse_settings[MAX_INPUT_SETTINGS][MAX_INPUT_DEVICES];
	struct uae_input_device keyboard_settings[MAX_INPUT_SETTINGS][MAX_INPUT_DEVICES];
	int dongle;
	*/
};

extern struct uae_prefs currprefs, changed_prefs;
extern void fixup_cpu (struct uae_prefs *prefs);


extern void check_prefs_changed_cpu (void);

#endif /* OPTIONS_CPU_H */
