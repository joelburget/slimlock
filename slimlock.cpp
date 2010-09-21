/* See LICENSE file for license details. */
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
		die("slock: cannot retrieve password entry (make sure to suid slock)\n");
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
		die("slock: cannot drop privileges\n");
	return rval;
}
#endif

int
main(int argc, char **argv) {
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	char buf[32], passwd[256];
	int num, screen;

#ifndef HAVE_BSD_AUTH
	const char *pws;
#endif
	unsigned int len;
	Bool running = True;
	Cursor invisible;
	Display *dpy;
	KeySym ksym;
	Pixmap pmap;
	Window root, w;
	XColor black, dummy;
	XEvent ev;
	XSetWindowAttributes wa;

	if((argc == 2) && !strcmp("-v", argv[1]))
		die("slock-"VERSION", © 2006-2008 Anselm R Garbe\n");
	else if(argc != 1)
		die("usage: slock [-v]\n");

#ifndef HAVE_BSD_AUTH
	pws = get_password();
#endif

	if(!(dpy = XOpenDisplay(0)))
		die("slock: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

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
        filename = themedir + "/background.jpg";
        loaded = image->Read(filename.c_str());
    }
    if (loaded) {
    }

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
