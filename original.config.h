#pragma once

#define MOD   Mod4Mask    
#define ALT   Mod1Mask
#define CTRL  ControlMask
#define SHIFT ShiftMask

/* ── appearance ─────────────────────────────────────────────────────────── */
#define FONT          "Iosevka Nerd Font:size=11"
#define BAR_HEIGHT    26
#define GAP           12
#define BORDER        2
#define MFACT_INIT    0.55f
#define MAX_WS        9
#define MAX_CLIENTS   64
#define TRAY_MAX      32

/* border colors (0xRRGGBB) */
#define COLOR_FOCUS   0x7aa2f7
#define COLOR_UNFOCUS 0x414868
#define COLOR_FLOAT   0xbb9af7

/* bar text colors ("#rrggbb") */
#define COLOR_BG      0x1a1b26 // Background
#define COLOR_BAR_FG  "#ffffff" // Text color
#define COLOR_BAR_DIM "#3b4261" // Unselected text color

/* ── keybindings ────────────────────────────────────────────────────────────
   actions: ACT_SPAWN, ACT_VOL_SPAWN (spawn + refresh vol), ACT_TOGGLE_FLOAT,
            ACT_TOGGLE_FULL, ACT_KILL, ACT_CYCLE_FOCUS,
            ACT_FOCUS_PREV, ACT_FOCUS_NEXT, ACT_MOVE_PREV, ACT_MOVE_NEXT
    For bar applets (clock, audio,...) customization, they have to be done from 'main.c'.
   Super+1-9 / Super+Shift+1-9 (switch / send to workspace) are always bound. These can be changed from 'main.c'*/

static char *termcmd[]   = { "st", NULL };
static char *runcmd[]    = { "rofi", "-show", "drun", NULL };
static char *fmcmd[]     = { "pcmanfm", NULL };
static char *killxcmd[]  = { "pkill", "X", NULL };
static char *screencmd[] = { "sh", "-c", "maim -s | xclip -selection clipboard -t image/png", NULL };
static char *volup[]     = { "wpctl", "set-volume", "@DEFAULT_SINK@", "5%+", NULL };
static char *voldown[]   = { "wpctl", "set-volume", "@DEFAULT_SINK@", "5%-", NULL };
static char *volmute[]   = { "wpctl", "set-mute",   "@DEFAULT_SINK@", "toggle", NULL };
static char *briup[]     = { "brightnessctl", "set", "5%+", NULL };
static char *bridown[]   = { "brightnessctl", "set", "5%-", NULL };

static Keybind keys[] = {
    /* modifier      key                           action            cmd      */
    { CTRL|ALT,      XK_t,                         ACT_SPAWN,        termcmd   },
    { MOD,           XK_r,                         ACT_SPAWN,        runcmd    },
    { MOD,           XK_e,                         ACT_SPAWN,        fmcmd     },
    { MOD|SHIFT,     XK_e,                         ACT_SPAWN,        killxcmd  },
    { 0,             XK_Print,                     ACT_SPAWN,        screencmd },
    { MOD,           XK_t,                         ACT_TOGGLE_FLOAT, NULL      },
    { MOD,           XK_f,                         ACT_TOGGLE_FULL,  NULL      },
    { ALT,           XK_F4,                        ACT_KILL,         NULL      },
    { ALT,           XK_Tab,                       ACT_CYCLE_FOCUS,  NULL      },
    { MOD,           XK_Left,                      ACT_FOCUS_PREV,   NULL      },
    { MOD,           XK_Up,                        ACT_FOCUS_PREV,   NULL      },
    { MOD,           XK_Right,                     ACT_FOCUS_NEXT,   NULL      },
    { MOD,           XK_Down,                      ACT_FOCUS_NEXT,   NULL      },
    { MOD|SHIFT,     XK_Left,                      ACT_MOVE_PREV,    NULL      },
    { MOD|SHIFT,     XK_Up,                        ACT_MOVE_PREV,    NULL      },
    { MOD|SHIFT,     XK_Right,                     ACT_MOVE_NEXT,    NULL      },
    { MOD|SHIFT,     XK_Down,                      ACT_MOVE_NEXT,    NULL      },
    { 0,             XF86XK_AudioRaiseVolume,       ACT_VOL_SPAWN,    volup     },
    { 0,             XF86XK_AudioLowerVolume,       ACT_VOL_SPAWN,    voldown   },
    { 0,             XF86XK_AudioMute,              ACT_VOL_SPAWN,    volmute   },
    { 0,             XF86XK_MonBrightnessUp,        ACT_SPAWN,        briup     },
    { 0,             XF86XK_MonBrightnessDown,      ACT_SPAWN,        bridown   },
};
