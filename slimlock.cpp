/* slimlock
 * Copyright (c) 2010 Joel Burget <joelburget@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */ 

#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

#if HAVE_BSD_AUTH
#include <login_cap.h>
#include <bsd_auth.h>
#endif

/* New Stuff */
#include "panel.h"
#include "cfg.h"

using namespace std;

void Login();
void blankScreen();
void setBackground(const string& themedir);
void HideCursor();
void KillAllClients(bool top);
bool AuthenticateUser(bool focuspass);
int CatchErrors(Display *dpy, XErrorEvent *ev);

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

#ifndef HAVE_BSD_AUTH
static const char *
get_password() { /* only run as root */
	const char *rval;
	struct passwd *pw;

	if(geteuid() != 0)
		die("slimlock: cannot retrieve password entry (make sure to suid slimlock)\n");
	pw = getpwuid(getuid());
	endpwent();
	rval =  pw->pw_passwd;

#if HAVE_SHADOW_H
	{
		struct spwd *sp;
		sp = getspnam(getenv("USER"));
		endspent();
		rval = sp->sp_pwdp;
	}
#endif

	/* drop privileges */
	if(setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0)
		die("slimlock: cannot drop privileges\n");
	return rval;
}
#endif

int
main(int argc, char **argv) {

	if((argc == 2) && !strcmp("-v", argv[1]))
		die("slimlock-"VERSION", © 2010 Joel Burget\n");
	else if(argc != 1)
		die("usage: slimlock [-v]\n");

	if(!(dpy = XOpenDisplay(0)))
		die("slimlock: cannot open display\n");
	scr= DefaultScreen(dpy);
	root = RootWindow(dpy, scr);

/*
  Window RealRoot = RootWindow(dpy, scr);
  root = XCreateSimpleWindow(dpy, RealRoot, 0, 0, 1280, 1024, 0, 0, 0);
  XMapWindow(dpy, root);
  XFlush(dpy);
*/

    // 
    // My additions
    //
    //
    
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
        // input is a set

        // I may choose to implement this later but for now I want simplicity
        // themeName = findValidRandomTheme(themeName);

        if (themeName == "") {
            themeName = "default";
        }
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

    blankScreen();
    HideCursor();


    // Create panel
    loginPanel = new Panel(dpy, scr, root, cfg, themedir);
    loginPanel->SetName(cfg->getOption("default_user"));
    bool panelClosed = true;

    // Main loop
    while (true)
    {
        if (panelClosed)
        {
            setBackground(themedir);
            KillAllClients(false);
            KillAllClients(true);
            loginPanel->OpenPanel();
        }
        loginPanel->Reset();
        loginPanel->SetName(cfg->getOption("default_user"));
        //action = loginPanel->getAction();
        loginPanel->ClosePanel();

        // AuthenticateUser returns true if authenticated
        if (!AuthenticateUser(true))
        {
            panelClosed = false;
            loginPanel->ClearPanel();
            XBell(dpy, 100);
            continue;
        }
        // I think we can just exit here!
        //Login();
        break;
    }

    return 0;
}

void blankScreen()
{
    GC gc = XCreateGC(dpy, root, 0, 0);
    XSetForeground(dpy, gc, BlackPixel(dpy, scr));
    XFillRectangle(dpy, root, gc, 0, 0,
                   XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                   XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
    XFlush(dpy);
    XFreeGC(dpy, gc);
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

void KillAllClients(bool top)
{
    Window dummywindow;
    Window *children;
    unsigned int nchildren;
    unsigned int i;
    XWindowAttributes attr;

    XSync(dpy, 0);
    XSetErrorHandler(CatchErrors);

    nchildren = 0;
    XQueryTree(dpy, root, &dummywindow, &dummywindow, &children, &nchildren);
    if(!top) {
        for(i=0; i<nchildren; i++) {
            if(XGetWindowAttributes(dpy, children[i], &attr) && (attr.map_state == IsViewable))
                children[i] = XmuClientWindow(dpy, children[i]);
            else
                children[i] = 0;
        }
    }

    for(i=0; i<nchildren; i++) {
        if(children[i])
            XKillClient(dpy, children[i]);
    }
    XFree((char *)children);

    XSync(dpy, 0);
    XSetErrorHandler(NULL);
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

int CatchErrors(Display *dpy, XErrorEvent *ev)
{
    return 0;
}
