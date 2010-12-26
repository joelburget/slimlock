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
bool AuthenticateUser(bool focuspass);
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

    if (geteuid() != 0)
        die("Wrong permissions: slimlock must be owned by root with setuid "
            "flagged.\n\n"
            "Set permissions as follows:\n"
            "sudo chown root slimlock && sudo chmod ug+s slimlock\n\n"
            "You should get something like this:\n"
            "[joel@arch slimlock]$ ls -l | grep \"slimlock\"\n"
            "-rwsr-sr-x 1 root users 172382 Dec 24 21:51 slimlock\n\n");


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

    // Read current user's theme
    cfg = new Cfg;
    cfg->readConf(CFGFILE);
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

    // This hides the cursor if the user has that option enabled in their
    // configuration
    HideCursor();

    // Create panel
    loginPanel = new Panel(dpy, scr, root, cfg, themedir);
    bool panelClosed = true;

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
        if (!AuthenticateUser(true))
        {
            panelClosed = false;
            loginPanel->ClearPanel();
            XBell(dpy, 100);
            continue;
        }

        loginPanel->ClosePanel();
        delete loginPanel;
        XCloseDisplay(dpy);
        break;
    }

    return 0;
}

void setBackground(const string& themedir) {
    string filename;
    filename = themedir + "/background.png";
    image = new Image;
    bool loaded = image->Read(filename.c_str());
    if (!loaded){ // try jpeg if png failed
        filename = "";
        filename = themedir + "/background.jpg";
        loaded = image->Read(filename.c_str());
    }
    if (loaded) {
        string bgstyle = cfg->getOption("background_style");
        if (bgstyle == "stretch") {
            image->Resize(XWidthOfScreen(ScreenOfDisplay(dpy, scr)), XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
        } else if (bgstyle == "tile") {
            image->Tile(XWidthOfScreen(ScreenOfDisplay(dpy, scr)), XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
        } else if (bgstyle == "center") {
            string hexvalue = cfg->getOption("background_color");
            hexvalue = hexvalue.substr(1,6);
            image->Center(XWidthOfScreen(ScreenOfDisplay(dpy, scr)), XHeightOfScreen(ScreenOfDisplay(dpy, scr)),
                        hexvalue.c_str());
        } else { // plain color or error
            string hexvalue = cfg->getOption("background_color");
            hexvalue = hexvalue.substr(1,6);
            image->Center(XWidthOfScreen(ScreenOfDisplay(dpy, scr)), XHeightOfScreen(ScreenOfDisplay(dpy, scr)),
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

bool AuthenticateUser(bool focuspass)
{
    if (!focuspass){
        loginPanel->EventHandler(Panel::Get_Name);
        switch(loginPanel->getAction()){
            case Panel::Exit:
            case Panel::Console:
                cerr << APPNAME << ": Got a special command (" << loginPanel->GetName() << ")" << endl;
                return true; // <--- This is simply fake!
            default:
                break;
        }
    }
    loginPanel->EventHandler(Panel::Get_Passwd);
    
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
