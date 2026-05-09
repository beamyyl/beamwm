#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>


#define BAR_HEIGHT  26
#define MAX_WS      9
#define MAX_CLIENTS 64
#define GAP         12
#define BORDER      2
#define COLOR_BG      0x1a1b26
#define COLOR_FOCUS   0x7aa2f7
#define COLOR_UNFOCUS 0x414868
#define COLOR_FLOAT   0xbb9af7
#define MOUSEMASK (ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

Display *dis;
Window root, barwin;
int sw, sh, cur_ws = 0;
float mfact = 0.55;
XftFont *font;
XftColor xft_white, xft_gray;

Window ws_wins[MAX_WS][MAX_CLIENTS];
int    ws_count[MAX_WS];
int    ws_sel[MAX_WS];
int    ws_float[MAX_WS][MAX_CLIENTS];
int    ws_full[MAX_WS][MAX_CLIENTS];

static int    cached_bat = -1;
static int    cached_vol = -1;
static char   cached_time[64] = "";
static time_t last_bat  = 0;
static time_t last_vol  = 0;
static time_t last_time = 0;

/* ── helpers ───────────────────────────────────────────────── */

int ws_find(int ws, Window w) {
    for (int i = 0; i < ws_count[ws]; i++)
        if (ws_wins[ws][i] == w) return i;
    return -1;
}

void ws_add(int ws, Window w) {
    if (ws_count[ws] >= MAX_CLIENTS) return;
    if (ws_find(ws, w) >= 0) return;
    ws_wins[ws][ws_count[ws]] = w;
    ws_float[ws][ws_count[ws]] = 0;
    ws_full[ws][ws_count[ws]]  = 0;
    ws_count[ws]++;
    ws_sel[ws] = ws_count[ws] - 1;
}

void ws_remove(int ws, Window w) {
    int idx = ws_find(ws, w);
    if (idx < 0) return;
    for (int i = idx; i < ws_count[ws] - 1; i++) {
        ws_wins[ws][i]  = ws_wins[ws][i + 1];
        ws_float[ws][i] = ws_float[ws][i + 1];
        ws_full[ws][i]  = ws_full[ws][i + 1];
    }
    ws_count[ws]--;
    if (ws_count[ws] == 0)
        ws_sel[ws] = -1;
    else if (ws_sel[ws] >= ws_count[ws])
        ws_sel[ws] = ws_count[ws] - 1;
}

Window cur_sel() {
    int idx = ws_sel[cur_ws];
    if (idx < 0 || idx >= ws_count[cur_ws]) return 0;
    return ws_wins[cur_ws][idx];
}

int win_is_floating(Window w) {
    int idx = ws_find(cur_ws, w);
    if (idx < 0) return 0;
    return ws_float[cur_ws][idx];
}

int win_is_full(Window w) {
    int idx = ws_find(cur_ws, w);
    if (idx < 0) return 0;
    return ws_full[cur_ws][idx];
}

/* ── X boilerplate ─────────────────────────────────────────── */

int xerror(Display *d, XErrorEvent *ee) { (void)d; (void)ee; return 0; }

void spawn(char *cmd[]) {
    if (fork() == 0) {
        if (dis) close(ConnectionNumber(dis));
        setsid();
        execvp(cmd[0], cmd);
        exit(0);
    }
}

int get_battery() {
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (!f) f = fopen("/sys/class/power_supply/BAT1/capacity", "r");
    if (!f) return -1;
    int cap;
    if (fscanf(f, "%d", &cap) != 1) cap = -1;
    fclose(f);
    return cap;
}

int get_volume() {
    FILE *f = popen("wpctl get-volume @DEFAULT_SINK@ | awk '{if ($NF==\"[MUTED]\") print \"-1\"; else print $2*100}'", "r");
    if (!f) return -1;
    char line[64];
    int vol = -1;
    if (fgets(line, sizeof(line), f))
        vol = atoi(line);
    pclose(f);
    return vol;
}

/* ── bar ───────────────────────────────────────────────────── */

void draw_bar() {
    time_t now = time(NULL);
    if (now - last_vol  >= 1)  { cached_vol  = get_volume();  last_vol  = now; }
    if (now - last_bat  >= 10) { cached_bat  = get_battery(); last_bat  = now; }
    if (now - last_time >= 10) {
        struct tm *t = localtime(&now);
        strftime(cached_time, sizeof(cached_time), "󰥔 %H:%M | 󰃭 %d %b %Y", t);
        last_time = now;
    }

    Visual *visual = DefaultVisual(dis, 0);
    Colormap cmap = DefaultColormap(dis, 0);
    XftDraw *d = XftDrawCreate(dis, barwin, visual, cmap);
    XSetForeground(dis, DefaultGC(dis, 0), COLOR_BG);
    XFillRectangle(dis, barwin, DefaultGC(dis, 0), 0, 0, sw, BAR_HEIGHT);

    /* left: workspace indicators */
    int x = 12;
    for (int i = 0; i < MAX_WS; i++) {
        char label[4];
        snprintf(label, sizeof(label), "%d", i + 1);
        XftColor *col = (i == cur_ws || ws_count[i] > 0) ? &xft_white : &xft_gray;
        XGlyphInfo ext;
        XftTextExtentsUtf8(dis, font, (FcChar8*)label, strlen(label), &ext);
        XftDrawStringUtf8(d, col, font, x, 18, (FcChar8*)label, strlen(label));
        x += ext.xOff + 10;
    }

    /* right: bat | vol | time */
    char stat_msg[192] = "";
    char tmp[64];
    int first = 1;
    if (cached_bat != -1) {
        snprintf(tmp, sizeof(tmp), "󰁹 %d%%", cached_bat);
        strncat(stat_msg, tmp, sizeof(stat_msg) - strlen(stat_msg) - 1);
        first = 0;
    }
    if (cached_vol != -1) {
        if (!first) strncat(stat_msg, " | ", sizeof(stat_msg) - strlen(stat_msg) - 1);
        snprintf(tmp, sizeof(tmp), "󰕾 %d%%", cached_vol);
        strncat(stat_msg, tmp, sizeof(stat_msg) - strlen(stat_msg) - 1);
        first = 0;
    }
    if (cached_time[0]) {
        if (!first) strncat(stat_msg, " | ", sizeof(stat_msg) - strlen(stat_msg) - 1);
        strncat(stat_msg, cached_time, sizeof(stat_msg) - strlen(stat_msg) - 1);
    }

    XGlyphInfo ext;
    XftTextExtentsUtf8(dis, font, (FcChar8*)stat_msg, strlen(stat_msg), &ext);
    XftDrawStringUtf8(d, &xft_white, font, sw - ext.xOff - 12, 18,
                      (FcChar8*)stat_msg, strlen(stat_msg));
    XftDrawDestroy(d);
}

/* ── arrange ───────────────────────────────────────────────── */

void arrange() {
    draw_bar();

    for (int ws = 0; ws < MAX_WS; ws++) {
        if (ws == cur_ws) continue;
        for (int i = 0; i < ws_count[ws]; i++)
            XMoveWindow(dis, ws_wins[ws][i], -2 * sw, 0);
    }

    int n = ws_count[cur_ws];
    if (n == 0) {
        XSetInputFocus(dis, root, RevertToParent, CurrentTime);
        XSync(dis, False);
        return;
    }

    Window sel = cur_sel();

    if (sel && win_is_full(sel)) {
        XSetWindowBorderWidth(dis, sel, 0);
        XMoveResizeWindow(dis, sel, 0, 0, sw, sh);
        XRaiseWindow(dis, sel);
        XSetInputFocus(dis, sel, RevertToParent, CurrentTime);
        for (int i = 0; i < n; i++)
            if (ws_wins[cur_ws][i] != sel)
                XMoveWindow(dis, ws_wins[cur_ws][i], -2 * sw, 0);
        XSync(dis, False);
        return;
    }

    int tiled = 0;
    for (int i = 0; i < n; i++)
        if (!ws_float[cur_ws][i] && !ws_full[cur_ws][i]) tiled++;

    int ti = 0;
    for (int i = 0; i < n; i++) {
        if (ws_float[cur_ws][i] || ws_full[cur_ws][i]) continue;
        Window w = ws_wins[cur_ws][i];
        XSetWindowBorderWidth(dis, w, BORDER);
        XSetWindowBorder(dis, w, w == sel ? COLOR_FOCUS : COLOR_UNFOCUS);
        if (tiled == 1) {
            XMoveResizeWindow(dis, w,
                GAP, BAR_HEIGHT + GAP,
                sw - GAP * 2 - BORDER * 2,
                sh - BAR_HEIGHT - GAP * 2 - BORDER * 2);
        } else {
            int mw   = (int)(sw * mfact) - (int)(GAP * 1.5);
            int sw2  = sw - mw - GAP * 3;
            int sn   = tiled - 1;
            int slot = (sh - BAR_HEIGHT - GAP * (sn + 1)) / sn;
            if (ti == 0) {
                XMoveResizeWindow(dis, w,
                    GAP, BAR_HEIGHT + GAP,
                    mw - BORDER * 2,
                    sh - BAR_HEIGHT - GAP * 2 - BORDER * 2);
            } else {
                XMoveResizeWindow(dis, w,
                    mw + GAP * 2,
                    BAR_HEIGHT + GAP + (ti - 1) * (slot + GAP),
                    sw2 - BORDER * 2,
                    slot - BORDER * 2);
            }
        }
        ti++;
    }

    for (int i = 0; i < n; i++) {
        if (!ws_float[cur_ws][i]) continue;
        Window w = ws_wins[cur_ws][i];
        XSetWindowBorderWidth(dis, w, BORDER);
        XSetWindowBorder(dis, w, w == sel ? COLOR_FLOAT : COLOR_UNFOCUS);
        XRaiseWindow(dis, w);
    }

    if (sel) {
        if (win_is_floating(sel)) XRaiseWindow(dis, sel);
        XSetInputFocus(dis, sel, RevertToParent, CurrentTime);
    }

    XSync(dis, False);
}

/* ── focus / remove ────────────────────────────────────────── */

void focus_window(Window w) {
    if (!w || w == barwin || w == root) return;
    int idx = ws_find(cur_ws, w);
    if (idx < 0) return;
    ws_sel[cur_ws] = idx;
    arrange();
}

void remove_window(Window w) {
    int found = 0;
    for (int ws = 0; ws < MAX_WS; ws++) {
        if (ws_find(ws, w) >= 0) { ws_remove(ws, w); found = 1; }
    }
    if (found) arrange();
}

void cycle_focus() {
    int n = ws_count[cur_ws];
    if (n < 2) return;
    ws_sel[cur_ws] = (ws_sel[cur_ws] + 1) % n;
    arrange();
}

void focus_step(int dir) {
    int n = ws_count[cur_ws];
    if (n < 2) return;
    ws_sel[cur_ws] = (ws_sel[cur_ws] + n + dir) % n;
    arrange();
}

void move_step(int dir) {
    int n = ws_count[cur_ws];
    if (n < 2) return;
    int idx  = ws_sel[cur_ws];
    int nidx = (idx + n + dir) % n;
    Window tmp_w = ws_wins[cur_ws][idx];
    int    tmp_f = ws_float[cur_ws][idx];
    int    tmp_F = ws_full[cur_ws][idx];
    ws_wins[cur_ws][idx]   = ws_wins[cur_ws][nidx];
    ws_float[cur_ws][idx]  = ws_float[cur_ws][nidx];
    ws_full[cur_ws][idx]   = ws_full[cur_ws][nidx];
    ws_wins[cur_ws][nidx]  = tmp_w;
    ws_float[cur_ws][nidx] = tmp_f;
    ws_full[cur_ws][nidx]  = tmp_F;
    ws_sel[cur_ws] = nidx;
    arrange();
}

void move_to_idx(int from, int to) {
    if (from == to) return;
    int n = ws_count[cur_ws];
    if (from < 0 || to < 0 || from >= n || to >= n) return;
    Window tmp_w = ws_wins[cur_ws][from];
    int    tmp_f = ws_float[cur_ws][from];
    int    tmp_F = ws_full[cur_ws][from];
    int dir = (to > from) ? 1 : -1;
    for (int i = from; i != to; i += dir) {
        ws_wins[cur_ws][i]  = ws_wins[cur_ws][i + dir];
        ws_float[cur_ws][i] = ws_float[cur_ws][i + dir];
        ws_full[cur_ws][i]  = ws_full[cur_ws][i + dir];
    }
    ws_wins[cur_ws][to]  = tmp_w;
    ws_float[cur_ws][to] = tmp_f;
    ws_full[cur_ws][to]  = tmp_F;
    ws_sel[cur_ws] = to;
}

int slot_at(int rx, int ry) {
    int n = ws_count[cur_ws];
    int tiled = 0;
    for (int i = 0; i < n; i++)
        if (!ws_float[cur_ws][i] && !ws_full[cur_ws][i]) tiled++;
    if (tiled == 0) return -1;
    if (tiled == 1) return 0;
    int mw   = (int)(sw * mfact) - (int)(GAP * 1.5);
    int sw2  = sw - mw - GAP * 3;
    int sn   = tiled - 1;
    int slot = (sh - BAR_HEIGHT - GAP * (sn + 1)) / sn;
    if (rx < mw + GAP * 2) return 0;
    int stack_x = mw + GAP * 2;
    if (rx < stack_x || rx > stack_x + sw2) return -1;
    for (int i = 0; i < sn; i++) {
        int sy = BAR_HEIGHT + GAP + i * (slot + GAP);
        if (ry >= sy && ry < sy + slot) return i + 1;
    }
    return sn;
}

void toggle_fullscreen() {
    int idx = ws_sel[cur_ws];
    if (idx < 0 || idx >= ws_count[cur_ws]) return;
    ws_full[cur_ws][idx] ^= 1;
    if (ws_full[cur_ws][idx]) ws_float[cur_ws][idx] = 0;
    arrange();
}

void toggle_float() {
    int idx = ws_sel[cur_ws];
    if (idx < 0 || idx >= ws_count[cur_ws]) return;
    ws_float[cur_ws][idx] ^= 1;
    if (ws_float[cur_ws][idx]) ws_full[cur_ws][idx] = 0;
    arrange();
}

void view(int ws) {
    if (ws == cur_ws) return;
    cur_ws = ws;
    arrange();
}

/* ── mouse move / resize ───────────────────────────────────── */

void movemouse(Window w) {
    int idx = ws_find(cur_ws, w);
    if (idx < 0) return;

    if (ws_float[cur_ws][idx]) {
        XWindowAttributes wa;
        XGetWindowAttributes(dis, w, &wa);
        int rx, ry, dummy; unsigned int dmask; Window dw;
        XQueryPointer(dis, root, &dw, &dw, &rx, &ry, &dummy, &dummy, &dmask);
        int startx = rx, starty = ry, ox = wa.x, oy = wa.y;
        if (XGrabPointer(dis, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(dis, XC_fleur), CurrentTime) != GrabSuccess) return;
        XEvent ev; Time lasttime = 0;
        do {
            XMaskEvent(dis, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
            if (ev.type == MotionNotify) {
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
                lasttime = ev.xmotion.time;
                XMoveWindow(dis, w,
                    ox + (ev.xmotion.x_root - startx),
                    oy + (ev.xmotion.y_root - starty));
                XSync(dis, False);
            }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(dis, CurrentTime);
    } else {
        if (XGrabPointer(dis, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(dis, XC_fleur), CurrentTime) != GrabSuccess) return;
        XEvent ev; Time lasttime = 0;
        int target = idx;
        do {
            XMaskEvent(dis, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
            if (ev.type == MotionNotify) {
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
                lasttime = ev.xmotion.time;
                int slot = slot_at(ev.xmotion.x_root, ev.xmotion.y_root);
                if (slot >= 0) {
                    int ti = 0, arr = -1;
                    for (int i = 0; i < ws_count[cur_ws]; i++) {
                        if (ws_float[cur_ws][i] || ws_full[cur_ws][i]) continue;
                        if (ti == slot) { arr = i; break; }
                        ti++;
                    }
                    if (arr >= 0 && arr != target) {
                        move_to_idx(target, arr);
                        target = arr;
                        arrange();
                    }
                }
            }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(dis, CurrentTime);
    }
}

void resizemouse(Window w) {
    if (win_is_floating(w)) {
        XWindowAttributes wa;
        XGetWindowAttributes(dis, w, &wa);
        if (XGrabPointer(dis, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(dis, XC_sizing), CurrentTime) != GrabSuccess) return;
        XEvent ev; Time lasttime = 0;
        do {
            XMaskEvent(dis, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
            if (ev.type == MotionNotify) {
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
                lasttime = ev.xmotion.time;
                int nw = ev.xmotion.x_root - wa.x;
                int nh = ev.xmotion.y_root - wa.y;
                if (nw < 40) nw = 40;
                if (nh < 40) nh = 40;
                XResizeWindow(dis, w, nw - BORDER * 2, nh - BORDER * 2);
                XSync(dis, False);
            }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(dis, CurrentTime);
    } else {
        if (XGrabPointer(dis, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(dis, XC_fleur), CurrentTime) != GrabSuccess) return;
        XEvent ev; Time lasttime = 0;
        do {
            XMaskEvent(dis, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
            if (ev.type == MotionNotify) {
                if ((ev.xmotion.time - lasttime) <= (1000 / 60)) continue;
                lasttime = ev.xmotion.time;
                mfact = (float)ev.xmotion.x / sw;
                if (mfact < 0.1) mfact = 0.1;
                if (mfact > 0.9) mfact = 0.9;
                arrange();
            }
        } while (ev.type != ButtonRelease);
        XUngrabPointer(dis, CurrentTime);
    }
}

/* ── input grabbing ────────────────────────────────────────── */

void grab_input() {
    XUngrabKey(dis, AnyKey, AnyModifier, root);
    unsigned int mods[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    for (int i = 0; i < 4; i++) {
        XGrabKey(dis, XKeysymToKeycode(dis, XK_t),     ControlMask | Mod1Mask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_t),     Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_r),     Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_e),     Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_e),     Mod4Mask | ShiftMask | mods[i],   root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_f),     Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_F4),    Mod1Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Tab),   Mod1Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Left),  Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Right), Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Up),    Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Down),  Mod4Mask | mods[i],               root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Left),  Mod4Mask | ShiftMask | mods[i],   root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Right), Mod4Mask | ShiftMask | mods[i],   root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Up),    Mod4Mask | ShiftMask | mods[i],   root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XK_Down),  Mod4Mask | ShiftMask | mods[i],   root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XF86XK_AudioRaiseVolume),  0 | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XF86XK_AudioLowerVolume),  0 | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XF86XK_AudioMute),         0 | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XF86XK_MonBrightnessUp),   0 | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, XKeysymToKeycode(dis, XF86XK_MonBrightnessDown), 0 | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        for (int n = 0; n < 9; n++) {
            XGrabKey(dis, XKeysymToKeycode(dis, XK_1 + n), Mod4Mask | mods[i],             root, True, GrabModeAsync, GrabModeAsync);
            XGrabKey(dis, XKeysymToKeycode(dis, XK_1 + n), Mod4Mask | ShiftMask | mods[i], root, True, GrabModeAsync, GrabModeAsync);
        }
        XGrabButton(dis, Button1, Mod4Mask | mods[i], root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, Button3, Mod4Mask | mods[i], root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

/* ── main ──────────────────────────────────────────────────── */

/* ── keybinds ──────────────────────────────────────────────────
   ctrl+alt+t            alacritty
   super+r               rofi
   super+e               pcmanfm
   super+shift+e         kill X session
   super+t               toggle float
   super+f               toggle fullscreen
   alt+f4                close focused window
   alt+tab               cycle focus
   super+left/right      focus prev/next window
   super+up/down         focus prev/next window
   super+shift+arrows    move window in layout order
   super+lmb             drag tiled to reorder / drag float freely
   super+rmb             resize float / adjust mfact (tiled)
   super+1..9            switch workspace
   super+shift+1..9      move window to workspace
   XF86 vol up/down      volume +5/-5 (wpctl)
   XF86 mute             toggle mute (wpctl)
   XF86 bright up/down   brightness +5/-5 (brightnessctl)
   Print                 take a screenshot (xclip + maim)
   ─────────────────────────────────────────────────────────── */

int main() {
    if (!(dis = XOpenDisplay(NULL))) return 1;
    setenv("XDG_CURRENT_DESKTOP", "beamwm", 1);
    XSetErrorHandler(xerror);
    root = DefaultRootWindow(dis);
    sw = DisplayWidth(dis, 0);
    sh = DisplayHeight(dis, 0);
    for (int i = 0; i < MAX_WS; i++) { ws_count[i] = 0; ws_sel[i] = -1; }

    Atom net_wm_name = XInternAtom(dis, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(dis, "UTF8_STRING", False);
    XChangeProperty(dis, root, net_wm_name, utf8_string, 8,
        PropModeReplace, (unsigned char *)"beamwm", 6);

    Visual *visual = DefaultVisual(dis, 0);
    Colormap cmap = DefaultColormap(dis, 0);
    font = XftFontOpenName(dis, 0, "Iosevka Nerd Font:size=11");
    XftColorAllocName(dis, visual, cmap, "#ffffff", &xft_white);
    XftColorAllocName(dis, visual, cmap, "#3b4261", &xft_gray);

    XSetWindowAttributes wa = { .override_redirect = True, .background_pixel = COLOR_BG };
    barwin = XCreateWindow(dis, root, 0, 0, sw, BAR_HEIGHT, 0,
        CopyFromParent, CopyFromParent, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &wa);
    XMapRaised(dis, barwin);
    XDefineCursor(dis, root, XCreateFontCursor(dis, XC_left_ptr));
    grab_input();
    XSelectInput(dis, root, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask);

    cached_vol = get_volume();  last_vol  = time(NULL);
    cached_bat = get_battery(); last_bat  = time(NULL);
    struct tm *t0 = localtime(&(time_t){time(NULL)});
    strftime(cached_time, sizeof(cached_time), "%H:%M | %d %b %Y", t0);
    last_time = time(NULL);

    int x11_fd = ConnectionNumber(dis);
    XEvent ev;
    while (1) {
        fd_set fds;
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        FD_ZERO(&fds); FD_SET(x11_fd, &fds);
        draw_bar(); XFlush(dis);
        select(x11_fd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dis)) {
            XNextEvent(dis, &ev);

            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                int super = ev.xkey.state & Mod4Mask;
                int shift = ev.xkey.state & ShiftMask;
                int ctrl  = ev.xkey.state & ControlMask;
                int alt   = ev.xkey.state & Mod1Mask;

                if (ks == XK_t && ctrl && alt)
                    { char *c[] = {"st", NULL}; spawn(c); }
                if (ks == XK_r && super)
                    { char *c[] = {"rofi", "-show", "drun", NULL}; spawn(c); }
                if (ks == XK_e && super && !shift)
                    { char *c[] = {"pcmanfm", NULL}; spawn(c); }
                if (ks == XK_e && super && shift)
                    { char *c[] = {"pkill", "X", NULL}; spawn(c); }
                if (ks == XK_Print) 
                    { char *c[] = {"sh", "-c", "maim -s | xclip -selection clipboard -t image/png", NULL}; spawn(c); } 
                if (ks == XK_t && super && !ctrl)   toggle_float();
                if (ks == XK_f && super)             toggle_fullscreen();
                if (ks == XK_F4 && alt)
                    { Window sel = cur_sel(); if (sel) XKillClient(dis, sel); }
                if (ks == XK_Tab && alt)             cycle_focus();
                if ((ks == XK_Left  || ks == XK_Up)   && super && !shift) focus_step(-1);
                if ((ks == XK_Right || ks == XK_Down) && super && !shift) focus_step(+1);
                if ((ks == XK_Left  || ks == XK_Up)   && super && shift)  move_step(-1);
                if ((ks == XK_Right || ks == XK_Down) && super && shift)  move_step(+1);
                if (ks == XF86XK_AudioRaiseVolume)
                    { char *c[] = {"wpctl", "set-volume", "@DEFAULT_SINK@", "5%+", NULL}; spawn(c); last_vol = 0; }
                if (ks == XF86XK_AudioLowerVolume)
                    { char *c[] = {"wpctl", "set-volume", "@DEFAULT_SINK@", "5%-", NULL}; spawn(c); last_vol = 0; }
                if (ks == XF86XK_AudioMute)
                    { char *c[] = {"wpctl", "set-mute", "@DEFAULT_SINK@", "toggle", NULL}; spawn(c); last_vol = 0; }
                if (ks == XF86XK_MonBrightnessUp)
                    { char *c[] = {"brightnessctl", "set", "5%+", NULL}; spawn(c); }
                if (ks == XF86XK_MonBrightnessDown)
                    { char *c[] = {"brightnessctl", "set", "5%-", NULL}; spawn(c); }
                if (ks >= XK_1 && ks <= XK_9 && super) {
                    int t = ks - XK_1;
                    if (shift) {
                        Window sel = cur_sel();
                        if (sel) {
                            int fi = ws_float[cur_ws][ws_sel[cur_ws]];
                            int fu = ws_full[cur_ws][ws_sel[cur_ws]];
                            ws_remove(cur_ws, sel);
                            ws_add(t, sel);
                            int ni = ws_find(t, sel);
                            ws_float[t][ni] = fi;
                            ws_full[t][ni]  = fu;
                            arrange();
                        }
                    } else view(t);
                }
            }

            if (ev.type == ButtonPress && (ev.xbutton.state & Mod4Mask)) {
                Window clicked = ev.xbutton.subwindow ? ev.xbutton.subwindow : ev.xbutton.window;
                if (clicked && clicked != root && clicked != barwin) {
                    focus_window(clicked);
                    if (ev.xbutton.button == Button1)
                        movemouse(clicked);
                    else if (ev.xbutton.button == Button3)
                        resizemouse(clicked);
                }
            }

            if (ev.type == MapRequest) {
                Window w = ev.xmaprequest.window;
                XSelectInput(dis, w, EnterWindowMask | FocusChangeMask);
                XMapWindow(dis, w);
                ws_add(cur_ws, w);
                arrange();
            }

            if (ev.type == EnterNotify)   focus_window(ev.xcrossing.window);
            if (ev.type == UnmapNotify)   remove_window(ev.xunmap.window);
            if (ev.type == DestroyNotify) remove_window(ev.xdestroywindow.window);
        }
    }
    return 0;
}
