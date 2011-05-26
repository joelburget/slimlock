#include <Imlib2.h>
#include <X11/Xlib.h>

void init_imlib_contexts(Display *dpy);

Imlib_Image image_read(const char *file);
void image_tile(Imlib_Image image, Display *dpy, Window win);
