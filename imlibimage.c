#include <stdio.h>
#include "imlibimage.h"

void init_imlib_contexts(Display *dpy) {
	int scr = DefaultScreen(dpy);

	imlib_context_set_display(dpy);
	imlib_context_set_visual(DefaultVisual(dpy, scr));
	imlib_context_set_colormap(DefaultColormap(dpy, scr));
}

Imlib_Image image_read(const char *file) {
	Imlib_Image buf = imlib_load_image(file);
	if (buf)
		printf("loaded %s", file);
	return buf;
}

void image_tile(Imlib_Image image, Display *dpy, Window win) {
	int im_width, im_height;
	
	Pixmap pmap;
	int scr = DefaultScreen(dpy);
	int depth = DefaultDepth(dpy, scr);
	GC gc_im;
	XGCValues gc_val;
	unsigned long gc_mask;

	imlib_context_set_display(dpy);
	imlib_context_set_visual(DefaultVisual(dpy, scr));
	imlib_context_set_colormap(DefaultColormap(dpy, scr));
	
	imlib_context_set_image(image);
	im_width = imlib_image_get_width();
	im_height = imlib_image_get_height();

	pmap = XCreatePixmap(dpy, win, im_width, im_height, depth);

	init_imlib_contexts(dpy);
	
	imlib_context_set_drawable(pmap);
	imlib_render_image_on_drawable(0, 0);

	gc_val.tile = pmap;
	gc_val.fill_style = FillTiled;
	gc_mask = GCTile | GCFillStyle;
	gc_im = XCreateGC(dpy, pmap, gc_mask, &gc_val);
	XFillRectangle(dpy, win, gc_im, 0, 0, XWidthOfScreen(ScreenOfDisplay(dpy, scr)),
										  XHeightOfScreen(ScreenOfDisplay(dpy, scr)));
	
}

