/* slimlock
 * Copyright (c) 2010 Joel Burget <joelburget@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */ 

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>
#include <security/pam_appl.h>
#include <pthread.h>
#include <err.h>
#include <signal.h>

#include "cfg.h"
#include "util.h"
#include "panel.h"

using namespace std;

void setBackground(const string& themedir);
void HideCursor();
bool AuthenticateUser();
static int ConvCallback(int num_msg, const struct pam_message **msg,
                        struct pam_response **resp, void *appdata_ptr);
string findValidRandomTheme(const string& set);
void HandleSignal(int sig);
void *RaiseWindow(void *data);

// I really didn't wanna put these globals here, but it's the only way...
Display* dpy;
int scr;
Window win;
Cfg* cfg;
Image* image;
Panel* loginPanel;
string themeName = "";

pam_handle_t *pam_handle;
struct pam_conv conv = {ConvCallback, NULL};

CARD16 dpms_standby, dpms_suspend, dpms_off, dpms_level;
BOOL dpms_state, using_dpms;

static void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    if((argc == 2) && !strcmp("-v", argv[1]))
        die("slimlock-"VERSION", © 2010 Joel Burget\n");
    else if(argc != 1)
        die("usage: slimlock [-v]\n");

    int term;
    /* disable tty switching */
    if ((term = open("/dev/console", O_RDWR)) == -1) {
        perror("error opening console");
    }

    if ((ioctl(term, VT_LOCKSWITCH)) == -1) {
        perror("error locking console"); 
    }

    void (*prev_fn)(int);

    // restore DPMS settings should slimlock be killed in the line of duty
    prev_fn = signal(SIGTERM, HandleSignal);
    if (prev_fn == SIG_IGN) signal(SIGTERM, SIG_IGN);

    // create a lock file to solve mutliple instances problem
    int lock_file = open("/var/lock/slimlock.lock", O_CREAT | O_RDWR, 0666);
    int rc = flock(lock_file, LOCK_EX | LOCK_NB);
    if(rc) {
        if(EWOULDBLOCK == errno)
            die("slimlock already running\n");
    }

    unsigned int cfg_passwd_timeout;
    // Read user's current theme
    cfg = new Cfg;
    cfg->readConf(CFGFILE);
    cfg->readConf(SLIMLOCKCFG);
    string themebase = "";
    string themefile = "";
    string themedir = "";
    themeName = "";
    themebase = string(THEMESDIR) + "/";
    themeName = cfg->getOption("current_theme");
    string::size_type pos;
    if ((pos = themeName.find(",")) != string::npos) {
        themeName = findValidRandomTheme(themeName);
    }

    bool loaded = false;
    while (!loaded) {
        themedir =  themebase + themeName;
        themefile = themedir + THEMESFILE;
        if (!cfg->readConf(themefile)) {
            if (themeName == "default") {
                cerr << APPNAME << ": Failed to open default theme file "
                     << themefile << endl;
                exit(ERR_EXIT);
            } else {
                cerr << APPNAME << ": Invalid theme in config: "
                     << themeName << endl;
                themeName = "default";
            }
        } else {
            loaded = true;
        }
    }

    if(!(dpy = XOpenDisplay(DISPLAY)))
        die("slimlock: cannot open display\n");
    scr = DefaultScreen(dpy);

    XSetWindowAttributes wa;
    wa.override_redirect = 1;
    wa.background_pixel = BlackPixel(dpy, scr);

    // Create a full screen window
    Window root = RootWindow(dpy, scr);
    win = XCreateWindow(dpy, 
      root, 
      0, 
      0, 
      DisplayWidth(dpy, scr), 
      DisplayHeight(dpy, scr), 
      0, 
      DefaultDepth(dpy, scr), 
      CopyFromParent,
      DefaultVisual(dpy, scr),
      CWOverrideRedirect | CWBackPixel,
      &wa);
    XMapWindow(dpy, win);

    XFlush(dpy);
    for(int len = 1000; len; len--) {
        if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
            == GrabSuccess)
            break;
        usleep(1000);
    }
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    
    // This hides the cursor if the user has that option enabled in their
    // configuration
    HideCursor();

    // Create panel
    loginPanel = new Panel(dpy, scr, win, cfg, themedir);

    // Set up DPMS
    unsigned int cfg_dpms_standby, cfg_dpms_off;
    cfg_dpms_standby = Cfg::string2int(cfg->getOption("dpms_standby_timeout").c_str());
    cfg_dpms_off = Cfg::string2int(cfg->getOption("dpms_off_timeout").c_str());
    using_dpms = DPMSCapable(dpy) && (cfg_dpms_standby > 0);
    if (using_dpms) {
        DPMSGetTimeouts(dpy, &dpms_standby, &dpms_suspend, &dpms_off);
        
        DPMSSetTimeouts(dpy, cfg_dpms_standby,
                        cfg_dpms_standby, cfg_dpms_off);
    
        DPMSInfo(dpy, &dpms_level, &dpms_state);
        if (!dpms_state)
            DPMSEnable(dpy);
    }

    // Get password timeout
    cfg_passwd_timeout = Cfg::string2int(cfg->getOption("wrong_passwd_timeout").c_str());
    // Let's just make sure it has a sane value
    cfg_passwd_timeout = cfg_passwd_timeout > 60 ? 60 : cfg_passwd_timeout;

    // Set up PAM
    int ret = pam_start("slimlock", loginPanel->GetName().c_str(), &conv, &pam_handle);
    // If we can't start PAM, just exit because slimlock won't work right
    if (ret != PAM_SUCCESS)
        errx(EXIT_FAILURE, "PAM: %s\n", pam_strerror(pam_handle, ret));
    
    pthread_t raise_thread;
    pthread_create(&raise_thread, NULL, RaiseWindow, NULL);

    // Main loop
    while (true)
    {
        loginPanel->ResetPasswd();

        // AuthenticateUser returns true if authenticated
        if (!AuthenticateUser())
        {            
            loginPanel->WrongPassword(cfg_passwd_timeout);
            continue;
        }
        loginPanel->ClosePanel();
        delete loginPanel;
        
        // Get DPMS stuff back to normal
        if (using_dpms) {
            DPMSSetTimeouts(dpy, dpms_standby, dpms_suspend, dpms_off);
            // turn off DPMS if it was off when we entered
            if (!dpms_state)
                DPMSDisable(dpy);
        }
        
        XCloseDisplay(dpy);
        break;
    }

    return 0;
}

void HideCursor() 
{
    if (cfg->getOption("hidecursor") == "true") {
        XColor black;
        char cursordata[1];
        Pixmap cursorpixmap;
        Cursor cursor;
        cursordata[0] = 0;
        cursorpixmap=XCreateBitmapFromData(dpy, win, cursordata, 1, 1);
        black.red = 0;
        black.green = 0;
        black.blue = 0;
        cursor = XCreatePixmapCursor(dpy, cursorpixmap, cursorpixmap,
                                     &black, &black, 0, 0);
        XDefineCursor(dpy, win, cursor);
    }
}

static int ConvCallback(int msgs, const struct pam_message **msg,
                        struct pam_response **resp, void *appdata_ptr)
{
    loginPanel->EventHandler();

    // PAM expects an array of responses, one for each message
    if (msgs == 0 ||
        (*resp = (pam_response*) calloc(msgs, sizeof(struct pam_message))) == NULL)
        return 1;
    
    for (int i = 0; i < msgs; i++) {
        if (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF &&
            msg[i]->msg_style != PAM_PROMPT_ECHO_ON)
            continue;

        // return code is currently not used but should be set to zero
        resp[i]->resp_retcode = 0;
        if ((resp[i]->resp = strdup(loginPanel->GetPasswd().c_str())) == NULL)
            return 1;
    }


    return 0;
}

bool AuthenticateUser()
{
    return(pam_authenticate(pam_handle, 0) == PAM_SUCCESS);
}

string findValidRandomTheme(const string& set)
{
    // extract random theme from theme set; return empty string on error
    string name = set;
    struct stat buf;

    if (name[name.length() - 1] == ',') {
        name.erase(name.length() - 1);
    }

    Util::srandom(Util::makeseed());

    vector<string> themes;
    string themefile;
    Cfg::split(themes, name, ',');
    do {
        int sel = Util::random() % themes.size();

        name = Cfg::trim(themes[sel]);
        themefile = string(THEMESDIR) +"/" + name + THEMESFILE;
        if (stat(themefile.c_str(), &buf) != 0) {
            themes.erase(find(themes.begin(), themes.end(), name));
            cerr << APPNAME << ": Invalid theme in config: "
                 << name << endl;
            name = "";
        }
    } while (name == "" && themes.size());
    return name;
}

void HandleSignal(int sig)
{
    // Get DPMS stuff back to normal
    if (using_dpms) {
        DPMSSetTimeouts(dpy, dpms_standby, dpms_suspend, dpms_off);
        // turn off DPMS if it was off when we entered
        if (!dpms_state)
            DPMSDisable(dpy);
    }

    loginPanel->ClosePanel();
    delete loginPanel;

    die("Caught signal; dying\n");
}

void* RaiseWindow(void *data) {
    while(1) {
        XRaiseWindow(dpy, win);
        sleep(1);
    }
}
