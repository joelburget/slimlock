/* SLiM - Simple Login Manager
   Copyright (C) 1997, 1998 Per Liden
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include <sstream>
#include "panel.h"

#include <iostream>

using namespace std;

Panel::Panel(Display* dpy, int scr, Window root, Cfg* config,
             const string& themedir) {
	// Get user
	const char *user = getenv("USER");
	
    // Set display
    Dpy = dpy;
    Scr = scr;
    Root = root;
    cfg = config;

    // Init GC
    XGCValues gcv;
    unsigned long gcm;
    gcm = GCForeground|GCBackground|GCGraphicsExposures;
    gcv.foreground = GetColor("black");
    gcv.background = GetColor("white");
    gcv.graphics_exposures = False;
    TextGC = XCreateGC(Dpy, Root, gcm, &gcv);

    font = XftFontOpenName(Dpy, Scr, cfg->getOption("input_font").c_str());
    welcomefont = XftFontOpenName(Dpy, Scr, cfg->getOption("welcome_font").c_str());
    introfont = XftFontOpenName(Dpy, Scr, cfg->getOption("intro_font").c_str());
    enterfont = XftFontOpenName(Dpy, Scr, cfg->getOption("username_font").c_str());
    msgfont = XftFontOpenName(Dpy, Scr, cfg->getOption("msg_font").c_str());

    Visual* visual = DefaultVisual(Dpy, Scr);
    Colormap colormap = DefaultColormap(Dpy, Scr);
    // NOTE: using XftColorAllocValue() would be a better solution. Lazy me.
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("input_color").c_str(), &inputcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("input_shadow_color").c_str(), &inputshadowcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("welcome_color").c_str(), &welcomecolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("welcome_shadow_color").c_str(), &welcomeshadowcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("username_color").c_str(), &entercolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("username_shadow_color").c_str(), &entershadowcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("msg_color").c_str(), &msgcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("msg_shadow_color").c_str(), &msgshadowcolor);
    XftColorAllocName(Dpy, visual, colormap, cfg->getOption("intro_color").c_str(), &introcolor);

    // Load properties from config / theme
    input_name_x = Cfg::string2int(cfg->getOption("input_name_x").c_str());
    input_name_y = Cfg::string2int(cfg->getOption("input_name_y").c_str());
    input_pass_x = Cfg::string2int(cfg->getOption("input_pass_x").c_str());
    input_pass_y = Cfg::string2int(cfg->getOption("input_pass_y").c_str());
    inputShadowXOffset =
        Cfg::string2int(cfg->getOption("input_shadow_xoffset").c_str());
    inputShadowYOffset =
        Cfg::string2int(cfg->getOption("input_shadow_yoffset").c_str());

    if (input_pass_x < 0 || input_pass_y < 0){ // single inputbox mode
        input_pass_x = input_name_x;
        input_pass_y = input_name_y;
    }

    // Load panel and background image
    string panelpng = "";
    panelpng = panelpng + themedir +"/panel.png";
    image = new Image;
    bool loaded = image->Read(panelpng.c_str());
    if (!loaded) { // try jpeg if png failed
        panelpng = themedir + "/panel.jpg";
        loaded = image->Read(panelpng.c_str());
        if (!loaded) {
            cerr << APPNAME
                 << ": could not load panel image for theme '"
                 << basename((char*)themedir.c_str()) << "'"
                 << endl;
            exit(ERR_EXIT);
        }
    }

    Image* bg = new Image;
    string bgstyle = cfg->getOption("background_style");
    if (bgstyle != "color") {
        panelpng = themedir +"/background.png";
        loaded = bg->Read(panelpng.c_str());
        if (!loaded) { // try jpeg if png failed
            panelpng = themedir + "/background.jpg";
            loaded = bg->Read(panelpng.c_str());
            if (!loaded){
                cerr << APPNAME
                     << ": could not load background image for theme '"
                     << basename((char*)themedir.c_str()) << "'"
                     << endl;
                exit(ERR_EXIT);
            }
        }
    }

    if (bgstyle == "stretch") {
        bg->Resize(XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)), XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)));
    } else if (bgstyle == "tile") {
        bg->Tile(XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                 XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
    } else if (bgstyle == "center") {
        string hexvalue = cfg->getOption("background_color");
        hexvalue = hexvalue.substr(1,6);
        bg->Center(XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)),
                   XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)),
                   hexvalue.c_str());
    } else { // plain color or error
        string hexvalue = cfg->getOption("background_color");
        hexvalue = hexvalue.substr(1,6);
        bg->Center(XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)),
                   XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)),
                   hexvalue.c_str());
    }

    string cfgX = cfg->getOption("input_panel_x");
    string cfgY = cfg->getOption("input_panel_y");
    X = Cfg::absolutepos(cfgX, XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)), image->Width());
    Y = Cfg::absolutepos(cfgY, XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)), image->Height());
	
	input_name_x += X, input_name_y += Y, input_pass_x += X, input_pass_y += Y;
	
    // Merge image into background
    image->Merge(bg, X, Y);
    Pixmap p = image->createPixmap(Dpy, Scr, Root);
    XSetWindowBackgroundPixmap(Dpy, Root, p);
    XClearWindow(Dpy, Root);
    
    delete bg;
    //PanelPixmap = image->createPixmap(Dpy, Scr, Root);

    // Read (and substitute vars in) the welcome message
    welcome_message = cfg->getWelcomeMessage();
    intro_message = cfg->getOption("intro_msg");
    
    SetName(user);
    
    // Draw the panel for the first time
    OnExpose();
}

Panel::~Panel() {
    XftColorFree (Dpy, DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr), &inputcolor);
    XftColorFree (Dpy, DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr), &msgcolor);
    XftColorFree (Dpy, DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr), &welcomecolor);
    XftColorFree (Dpy, DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr), &entercolor);
    XFreeGC(Dpy, TextGC);
    XftFontClose(Dpy, font);
    XftFontClose(Dpy, msgfont);
    XftFontClose(Dpy, introfont);
    XftFontClose(Dpy, welcomefont);
    XftFontClose(Dpy, enterfont);
    delete image;

}

void Panel::ClosePanel() {
    XUngrabKeyboard(Dpy, CurrentTime);
    XUnmapWindow(Dpy, Root);
    XDestroyWindow(Dpy, Root);
    XFlush(Dpy);
}

void Panel::WrongPassword(int timeout) {
    string message = cfg->getOption("passwd_feedback_msg");
    string cfgX, cfgY;
    XGlyphInfo extents;
    XftDraw *draw = XftDrawCreate(Dpy, Root,
                                  DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr));
    XftTextExtents8(Dpy, msgfont, reinterpret_cast<const XftChar8*>(message.c_str()),
                    message.length(), &extents);
    cfgX = cfg->getOption("passwd_feedback_x");
    cfgY = cfg->getOption("passwd_feedback_y");
    int shadowXOffset =
        Cfg::string2int(cfg->getOption("msg_shadow_xoffset").c_str());
    int shadowYOffset =
        Cfg::string2int(cfg->getOption("msg_shadow_yoffset").c_str());

    int msg_x = Cfg::absolutepos(cfgX, XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)), extents.width);
    int msg_y = Cfg::absolutepos(cfgY, XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)), extents.height);

	// Clear the password field but draw everything else
    ResetPasswd();
    XClearWindow(Dpy, Root);
    OnExpose();
    
    SlimDrawString8(draw, &msgcolor, msgfont, msg_x, msg_y,
                    message,
                    &msgshadowcolor,
                    shadowXOffset, shadowYOffset);
    XBell(Dpy, 100);
    
    XFlush(Dpy);
    sleep(timeout);
    XSync(Dpy, True);
    XftDrawDestroy(draw);
}

void Panel::Message(const string& text) {
    string cfgX, cfgY;
    XGlyphInfo extents;
    XftDraw *draw = XftDrawCreate(Dpy, Root,
                                  DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr));
    XftTextExtents8(Dpy, msgfont, reinterpret_cast<const XftChar8*>(text.c_str()),
                    text.length(), &extents);
    cfgX = cfg->getOption("msg_x");
    cfgY = cfg->getOption("msg_y");
    int shadowXOffset =
        Cfg::string2int(cfg->getOption("msg_shadow_xoffset").c_str());
    int shadowYOffset =
        Cfg::string2int(cfg->getOption("msg_shadow_yoffset").c_str());

    int msg_x = Cfg::absolutepos(cfgX, XWidthOfScreen(ScreenOfDisplay(Dpy, Scr)), extents.width);
    int msg_y = Cfg::absolutepos(cfgY, XHeightOfScreen(ScreenOfDisplay(Dpy, Scr)), extents.height);

    SlimDrawString8(draw, &msgcolor, msgfont, msg_x, msg_y,
                    text,
                    &msgshadowcolor,
                    shadowXOffset, shadowYOffset);
    XFlush(Dpy);
    XftDrawDestroy(draw);
}

unsigned long Panel::GetColor(const char* colorname) {
    XColor color;
    XWindowAttributes attributes;

    XGetWindowAttributes(Dpy, Root, &attributes);
    color.pixel = 0;

    if(!XParseColor(Dpy, attributes.colormap, colorname, &color))
        cerr << APPNAME << ": can't parse color " << colorname << endl;
    else if(!XAllocColor(Dpy, attributes.colormap, &color))
        cerr << APPNAME << ": can't allocate color " << colorname << endl;

    return color.pixel;
}

void Panel::Cursor(int visible) {
    const char* text;
    int xx, yy, y2, cheight;
    const char* txth = "Wj"; // used to get cursor height
	
	text = HiddenPasswdBuffer.c_str();
	xx = input_pass_x;
	yy = input_pass_y;

    XGlyphInfo extents;
    XftTextExtents8(Dpy, font, (XftChar8*)txth, strlen(txth), &extents);
    cheight = extents.height;
    y2 = yy - extents.y + extents.height;
    XftTextExtents8(Dpy, font, (XftChar8*)text, strlen(text), &extents);
    xx += extents.width;

    if(visible == SHOW) {
        XSetForeground(Dpy, TextGC,
                       GetColor(cfg->getOption("input_color").c_str()));
        XDrawLine(Dpy, Root, TextGC,
                  xx+1, yy-cheight,
                  xx+1, y2);
    } else {
        XClearArea(Dpy, Root, xx+1, yy-cheight,
                   1, y2-(yy-cheight)+1, false);
    }
}

void Panel::EventHandler() {
    XEvent event;
    bool loop = true;
    //OnExpose();
    while(loop) {
        XNextEvent(Dpy, &event);
        switch(event.type) {
            case Expose:
                OnExpose();
                break;

            case KeyPress:
                loop = OnKeyPress(event);
                break;
        }
    }

    return;
}

void Panel::OnExpose(void) {
    XftDraw *draw = XftDrawCreate(Dpy, Root,
                        DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr));
    //XClearWindow(Dpy, Root);
    if (input_pass_x != input_name_x || input_pass_y != input_name_y){
        SlimDrawString8(draw, &inputcolor, font, input_name_x, input_name_y,
                        NameBuffer,
                        &inputshadowcolor,
                        inputShadowXOffset, inputShadowYOffset);
        SlimDrawString8(draw, &inputcolor, font, input_pass_x, input_pass_y,
                        HiddenPasswdBuffer,
                        &inputshadowcolor,
                        inputShadowXOffset, inputShadowYOffset);
    } else { //single input mode
		SlimDrawString8(draw, &inputcolor, font,
						input_pass_x, input_pass_y,
						HiddenPasswdBuffer,
						&inputshadowcolor,
						inputShadowXOffset, inputShadowYOffset);
    }

    XftDrawDestroy (draw);
    Cursor(SHOW);
    ShowText();
}

bool Panel::OnKeyPress(XEvent& event) {
    char ascii;
    KeySym keysym;
    XComposeStatus compstatus;
    int xx;
    int yy;
    string formerString = "";
    bool fieldTextChanged = true;
    
    XLookupString(&event.xkey, &ascii, 1, &keysym, &compstatus);
    switch(keysym){
        case XK_F11:
            // Take a screenshot
            system(cfg->getOption("screenshot_cmd").c_str());
            return true;

        case XK_Return:
        case XK_KP_Enter:
            return false;
        default:
            break;
    };

    Cursor(HIDE);
    switch(keysym){
        case XK_Delete:
        case XK_BackSpace:
            if (!PasswdBuffer.empty()){
                formerString=HiddenPasswdBuffer;
                PasswdBuffer.erase(--PasswdBuffer.end());
                HiddenPasswdBuffer.erase(--HiddenPasswdBuffer.end());
            };
            break;
        case XK_Escape:
            if (!PasswdBuffer.empty()){
                formerString = HiddenPasswdBuffer;
                PasswdBuffer.clear();
                HiddenPasswdBuffer.clear();
            };
            break;
        case XK_w:
        case XK_u:
            if (reinterpret_cast<XKeyEvent&>(event).state & ControlMask) {
                formerString = HiddenPasswdBuffer;
                HiddenPasswdBuffer.clear();
                PasswdBuffer.clear();
            };
            // Deliberate fall-through
        
        default:
            if (isprint(ascii) && (keysym < XK_Shift_L || keysym > XK_Hyper_R)){
                formerString=HiddenPasswdBuffer;
                if (PasswdBuffer.length() < INPUT_MAXLENGTH_PASSWD-1){
                    PasswdBuffer.append(&ascii,1);
                    HiddenPasswdBuffer.append("*");
                };
            } else {
                fieldTextChanged = false;
            };
            break;
    };

    XGlyphInfo extents;
    XftDraw *draw = XftDrawCreate(Dpy, Root,
                                  DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr));

    xx = input_pass_x;
    yy = input_pass_y;
    
    if (!formerString.empty()){
        const char* txth = "Wj"; // get proper maximum height ?
        XftTextExtents8(Dpy, font, reinterpret_cast<const XftChar8*>(txth), strlen(txth), &extents);
        int maxHeight = extents.height;

        XftTextExtents8(Dpy, font, reinterpret_cast<const XftChar8*>(formerString.c_str()),
                        formerString.length(), &extents);
        int maxLength = extents.width;

        XClearArea(Dpy, Root, xx-3, yy-maxHeight-3,
                   maxLength+6, maxHeight+6, false);
    }

    if (fieldTextChanged) {
        SlimDrawString8 (draw, &inputcolor, font, xx, yy,
                         HiddenPasswdBuffer,
                         &inputshadowcolor,
                         inputShadowXOffset, inputShadowYOffset);
    }

    XftDrawDestroy (draw);
    Cursor(SHOW);
    return true;
}

// Draw welcome and "enter username" message
void Panel::ShowText(){
    string cfgX, cfgY;
    string user_msg = "User: ";
    XGlyphInfo extents;

    bool singleInputMode =
    input_name_x == input_pass_x &&
    input_name_y == input_pass_y;

    XftDraw *draw = XftDrawCreate(Dpy, Root,
                                  DefaultVisual(Dpy, Scr), DefaultColormap(Dpy, Scr));
    /* welcome message */
    XftTextExtents8(Dpy, welcomefont, (XftChar8*)welcome_message.c_str(),
                    strlen(welcome_message.c_str()), &extents);
    cfgX = cfg->getOption("welcome_x");
    cfgY = cfg->getOption("welcome_y");
    int shadowXOffset =
        Cfg::string2int(cfg->getOption("welcome_shadow_xoffset").c_str());
    int shadowYOffset =
        Cfg::string2int(cfg->getOption("welcome_shadow_yoffset").c_str());
    welcome_x = Cfg::absolutepos(cfgX, image->Width(), extents.width) + X;
    welcome_y = Cfg::absolutepos(cfgY, image->Height(), extents.height) + Y;
    if (welcome_x >= 0 && welcome_y >= 0) {
        SlimDrawString8 (draw, &welcomecolor, welcomefont,
                         welcome_x, welcome_y,
                         welcome_message,
                         &welcomeshadowcolor, shadowXOffset, shadowYOffset);
    }

    /* Enter username-password message */
    string msg;
	msg = cfg->getOption("password_msg");
	XftTextExtents8(Dpy, enterfont, (XftChar8*)msg.c_str(),
					strlen(msg.c_str()), &extents);
	cfgX = cfg->getOption("password_x");
	cfgY = cfg->getOption("password_y");
	shadowXOffset =
		Cfg::string2int(cfg->getOption("username_shadow_xoffset").c_str());
	shadowYOffset =
		Cfg::string2int(cfg->getOption("username_shadow_yoffset").c_str());
	password_x = Cfg::absolutepos(cfgX, image->Width(), extents.width) + X;
	password_y = Cfg::absolutepos(cfgY, image->Height(), extents.height) + Y;
	if (password_x >= 0 && password_y >= 0){
		SlimDrawString8 (draw, &entercolor, enterfont, password_x, password_y,
						 msg, &entershadowcolor, shadowXOffset, shadowYOffset);
	}
	
    if (!singleInputMode) {
        msg = cfg->getOption("username_msg");
        XftTextExtents8(Dpy, enterfont, (XftChar8*)msg.c_str(),
                        strlen(msg.c_str()), &extents);
        cfgX = cfg->getOption("username_x");
        cfgY = cfg->getOption("username_y");
        shadowXOffset =
            Cfg::string2int(cfg->getOption("username_shadow_xoffset").c_str());
        shadowYOffset =
            Cfg::string2int(cfg->getOption("username_shadow_yoffset").c_str());
        username_x = Cfg::absolutepos(cfgX, image->Width(), extents.width) + X;
        username_y = Cfg::absolutepos(cfgY, image->Height(), extents.height) + Y;
        if (username_x >= 0 && username_y >= 0){
            SlimDrawString8 (draw, &entercolor, enterfont, username_x, username_y,
                             msg, &entershadowcolor, shadowXOffset, shadowYOffset);
        }
    }
    XftDrawDestroy(draw);
    
    // If only the password box is visible, draw the user name somewhere too
    user_msg += GetName();
    int show_username = Cfg::string2int(cfg->getOption("show_username").c_str());
    if (singleInputMode && show_username)
		Message(user_msg);
}

void Panel::SlimDrawString8(XftDraw *d, XftColor *color, XftFont *font,
                            int x, int y, const string& str,
                            XftColor* shadowColor,
                            int xOffset, int yOffset)
{
    if (xOffset && yOffset) {
        XftDrawString8(d, shadowColor, font, x+xOffset, y+yOffset,
                       reinterpret_cast<const FcChar8*>(str.c_str()), str.length());
    }
    XftDrawString8(d, color, font, x, y, reinterpret_cast<const FcChar8*>(str.c_str()), str.length());
}

void Panel::Reset(void){
    ResetName();
    ResetPasswd();
};

void Panel::ResetName(void){
    NameBuffer.clear();
};

void Panel::ResetPasswd(void){
    PasswdBuffer.clear();
    HiddenPasswdBuffer.clear();
};

void Panel::SetName(const string& name){
    NameBuffer = name;
    return;
};
const string& Panel::GetName(void) const{
    return NameBuffer;
};
const string& Panel::GetPasswd(void) const{
    return PasswdBuffer;
};
