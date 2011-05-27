/* slimlock
 * Copyright (c) 2010 Joel Burget <joelburget@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */ 

#if HAVE_SHADOW
#include <shadow.h>
#endif

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

#include "cfg.h"
#include "util.h"
#include "panel.h"

using namespace std;

void setBackground(const string& themedir);
void HideCursor();
bool AuthenticateUser();
string findValidRandomTheme(const string& set);

// I really didn't wanna put these globals here, but it's the only way...
Display* dpy;
int scr;
Window root;
Cfg* cfg;
Image* image;
Panel* loginPanel;
string themeName = "";

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
        
    // create a lock file to solve mutliple instances problem
    int pid_file = open("/var/run/slimlock.pid", O_CREAT | O_RDWR, 0666);
    int rc = flock(pid_file, LOCK_EX | LOCK_NB);
    if(rc) {
        if(EWOULDBLOCK == errno)
            exit(EXIT_FAILURE);
    }

    if (geteuid() != 0)
        die("Wrong permissions: slimlock must be owned by root with setuid "
            "flagged.\n\n"
            "Set permissions as follows:\n"
            "sudo chown root slimlock && sudo chmod ug+s slimlock\n\n"
            "You should get something like this:\n"
            "[joel@arch slimlock]$ ls -l | grep \"slimlock\"\n"
            "-rwsr-sr-x 1 root users 172382 Dec 24 21:51 slimlock\n\n");


    CARD16 dpms_standby, dpms_suspend, dpms_off, dpms_level;
    BOOL dpms_state, using_dpms;
    unsigned int cfg_dpms_timeout;

    unsigned int cfg_passwd_timeout;
    // Read current user's theme
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
    root = RootWindow(dpy, scr);

    XSetWindowAttributes wa;
    wa.override_redirect = 1;
    wa.background_pixel = BlackPixel(dpy, scr);

    // Create a full screen window
    Window RealRoot = RootWindow(dpy, scr);
    root = XCreateWindow(dpy, 
      RealRoot, 
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
    XMapWindow(dpy, root);
    XFlush(dpy);
    XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime);

    // This hides the cursor if the user has that option enabled in their
    // configuration
    HideCursor();

    // Create panel
    loginPanel = new Panel(dpy, scr, root, cfg, themedir);
    bool panelClosed = true;

    // Set up DPMS
    cfg_dpms_timeout = Cfg::string2int(cfg->getOption("dpms_timeout").c_str());
    using_dpms = DPMSCapable(dpy) && (cfg_dpms_timeout > 0);
    if (using_dpms) {
        DPMSGetTimeouts(dpy, &dpms_standby, &dpms_suspend, &dpms_off);
        DPMSSetTimeouts(dpy, cfg_dpms_timeout,
                        dpms_suspend, dpms_off);
    
        DPMSInfo(dpy, &dpms_level, &dpms_state);
        if (!dpms_state)
            DPMSEnable(dpy);
    }

    // Get password timeout
    cfg_passwd_timeout = Cfg::string2int(cfg->getOption("wrong_passwd_timeout").c_str());
    // Let's just make sure it has a sane value
    cfg_passwd_timeout = cfg_passwd_timeout > 60 ? 60 : cfg_passwd_timeout;
    
    // Main loop
    while (true)
    {
        if (panelClosed)
        {
            setBackground(themedir);
            loginPanel->OpenPanel();
        }
        loginPanel->Reset();

        char message[100];
        char* name = getenv("USER");
        strcpy(message, "User: ");
        strcat(message, name);
        loginPanel->SetName(name);
        loginPanel->Message(message);

        // AuthenticateUser returns true if authenticated
        if (!AuthenticateUser())
        {
            panelClosed = false;
            loginPanel->ClearPanel();
            XBell(dpy, 100);
            sleep(cfg_passwd_timeout);
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

void setBackground(const string& themedir) {
    string filename;
    filename = themedir + "/background.png";
    image = new Image(dpy);
    bool loaded = image->Read(filename.c_str());
    if (!loaded){ // try jpeg if png failed
        filename = "";
        filename = themedir + "/background.jpg";
        loaded = image->Read(filename.c_str());
    }
    if (loaded) {
        string bgstyle = cfg->getOption("background_style");
        if (bgstyle == "stretch") {
            image->Resize(XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                          XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
        } else if (bgstyle == "tile") {
            image->Tile(XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                        XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
        } else if (bgstyle == "center") {
            string hexvalue = cfg->getOption("background_color");
            hexvalue = hexvalue.substr(1,6);
            image->Center(XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                          XHeightOfScreen(ScreenOfDisplay(dpy, scr)),
                          hexvalue.c_str());
        } else { // plain color or error
            string hexvalue = cfg->getOption("background_color");
            hexvalue = hexvalue.substr(1,6);
            image->Center(XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                          XHeightOfScreen(ScreenOfDisplay(dpy, scr)),
                          hexvalue.c_str());
        }
        Pixmap p = image->createPixmap(dpy, scr, root);
        XSetWindowBackgroundPixmap(dpy, root, p);
    }
    XClearWindow(dpy, root);
    
    XFlush(dpy);
    delete image;
}

void HideCursor() 
{
    if (cfg->getOption("hidecursor") == "true") {
        XColor            black;
        char            cursordata[1];
        Pixmap            cursorpixmap;
        Cursor            cursor;
        cursordata[0]=0;
        cursorpixmap=XCreateBitmapFromData(dpy,root,cursordata,1,1);
        black.red=0;
        black.green=0;
        black.blue=0;
        cursor=XCreatePixmapCursor(dpy,cursorpixmap,cursorpixmap,&black,&black,0,0);
        XDefineCursor(dpy,root,cursor);
    }
}

bool AuthenticateUser()
{
    loginPanel->EventHandler(Panel::GET_PASSWD);
    
    char *encrypted, *correct;
    struct passwd *pw;

    pw = getpwnam(loginPanel->GetName().c_str());
    endpwent();
    if (pw == 0)
      return false;

#ifdef HAVE_SHADOW
    struct spwd *sp = getspnam(pw->pw_name);    
    endspent();
    if(sp)
        correct = sp->sp_pwdp;
    else
#endif        // HAVE_SHADOW
        correct = pw->pw_passwd;

    if(correct == 0 || correct[0] == '\0')
        return true;

    encrypted = crypt(loginPanel->GetPasswd().c_str(), correct);

    return ((strcmp(encrypted, correct) == 0) ? true : false);
}

string findValidRandomTheme(const string& set)
{
    // extract random theme from theme set; return empty string on error
    string name = set;
    struct stat buf;

    if (name[name.length()-1] == ',') {
        name = name.substr(0, name.length() - 1);
    }

    Util::srandom(Util::makeseed());

    vector<string> themes;
    string themefile;
    Cfg::split(themes, name, ',');
    do {
        int sel = Util::random() % themes.size();

        name = Cfg::Trim(themes[sel]);
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
