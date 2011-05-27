/* SLiM - Simple Login Manager
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>
      
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   The following code has been adapted and extended from
   xplanet 1.0.1, Copyright (C) 2002-04 Hari Nair <hari@alumni.caltech.edu>
*/

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

#include "image.h"

Image::Image(Display *dpy) {
    int scr = DefaultScreen(dpy);

	imlib_context_set_display(dpy);
	imlib_context_set_visual(DefaultVisual(dpy, scr));
	imlib_context_set_colormap(DefaultColormap(dpy, scr));

}

Image::~Image() {
    imlib_context_set_image(image);
    imlib_free_image();
}

bool
Image::Read(const char *filename) {
    image = imlib_load_image(filename);
    return(image); 
}

void
Image::Resize(const int w, const int h) {
    image = imlib_create_cropped_scaled_image(0, 0, width, height, w, h);
    width = w;
    height = h;
    imlib_context_set_image(image);
}

/* Merge the image with a background, taking care of the
 * image Alpha transparency. (background alpha is ignored).
 * The images is merged on position (x, y) on the
 * background, the background must contain the image.
 */
void Image::Merge(Image* background, const int x, const int y) {
    if (x + width > background->Width()|| y + height > background->Height()) {
        return;
    }

    if (background->Width()*background->Height() != width*height)
        background->Crop(x, y, width, height);

    imlib_context_set_image(background);
    imlib_blend_image_onto_image(image, 1, 0, 0, width, height, x, y, width, height);

    imlib_context_set_image(image);
    imlib_free_image();
    imlib_context_set_image(background);
}

/* Tile the image growing its size to the minimum entire
 * multiple of w * h.
 * The new dimensions should be > of the current ones.
 * Note that this flattens image (alpha removed)
 */
void Image::Tile(const int w, const int h) {
    
    

}

/* Crop the image
 */
void Image::Crop(const int x, const int y, const int w, const int h) {
    imlib_context_set_image(image);
    image = imlib_create_cropped_image(x, y, w, h);
    width = w;
    height = h;
    imlib_context_set_image(image);
}

/* Center the image in a rectangle of given width and height.
 * Fills the remaining space (if any) with the hex color
 */
void Image::Center(const int w, const int h, const char *hex) {
    
    Imlib_Image bg = imlib_create_image(w, h);
    int pos_x, pos_y;
    unsigned long packed_rgb;
    sscanf(hex, "%lx", &packed_rgb);  

    unsigned long r = packed_rgb>>16;
    unsigned long g = packed_rgb>>8 & 0xff;
    unsigned long b = packed_rgb & 0xff;

    imlib_context_set_image(bg);
    imlib_context_set_color(r, g, b, 255);
    imlib_image_fill_rectangle(0, 0, w, h);
    pos_x = (w - width) / 2;
    pos_y = (h - height) / 2;

    imlib_blend_image_onto_image(image, 1, 0, 0, width, height, pos_x, pos_y,
                                 width, height);

    image = imlib_clone_image();
    
    
}

Pixmap
Image::createPixmap(Display* dpy, int scr, Window win, bool tiled) {

    const int depth = DefaultDepth(dpy, scr);
    Visual *visual = DefaultVisual(dpy, scr);
    Colormap colormap = DefaultColormap(dpy, scr);
    Pixmap tmp = XCreatePixmap(dpy, win, XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                               XHeightOfScreen(ScreenOfDisplay(dpy, scr)),
                               depth);

    imlib_context_set_image(image);

    if (tiled) {
        XGCValues gc_val;
        unsigned long gc_mask;


        Pixmap pmap = XCreatePixmap(dpy, win, width, height, depth);
        imlib_context_set_drawable(pmap);
        imlib_render_image_on_drawable(0, 0);

        gc_val.tile = tmp;
        gc_val.fill_style = FillTiled;
        gc_mask = GCTile | GCFillStyle;
    
        GC gc = XCreateGC(dpy, tmp, gc_mask, &gc_val);

        XFillRectangle(dpy, tmp, gc, 0, 0,
                       XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
                       XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
        XFreePixmap(dpy, pmap);
        XFreeGC(dpy, gc);
    } else {
        imlib_context_set_drawable(tmp);
        imlib_render_image_on_drawable(0, 0);
    }
    

    return(tmp);
}

