slimlock - unholy screen locker
===============================
This is the Frankenstein's monster of screen lockers. I had the horrible
idea to graft SLiM and slock together to create the screen locker that should
have never been.

Slimlock reads your SLiM config files and uses the same interface. If you use
SLiM already, then it should work for you automatically.

Requirements and Building
-------------------------
Requires X11, libpng, libjpeg, freetype. Change CFGDIR in the makefile to the
directory containing you slim.conf. As root, install with 'make' then 'make
install'.

Running slimlock
-------------
Run with 'slimlock'.

Copyright
---------
slimlock copyright (c) 2010 Joel Burget

SLiM is copyright (c) 2004-06 by Simone Rota, Johannes Winkelmann
and is available under the GNU General Public License.
See the COPYING file for the complete license.

Image handling code adapted and extended from xplanet 1.0.1,
copyright (c) 2002-04 by Hari Nair

Login.app is copyright (c) 1997, 1998 by Per Liden and is 
licensed through the GNU General Public License. 
