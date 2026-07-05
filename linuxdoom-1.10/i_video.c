// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <X11/extensions/XShm.h>
// Had to dig up XShm.c for this one.
// It is in the libXext, but not in the XFree86 headers.
#ifdef LINUX
int XShmGetEventBase( Display* dpy ); // problems with g++?
#endif

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"

#include "doomdef.h"

#define POINTER_WARP_COUNTDOWN	1

Display*	X_display=0;
Window		X_mainWindow;
Colormap	X_cmap;
Visual*		X_visual;
GC		X_gc;
XEvent		X_event;
int		X_screen;
XVisualInfo	X_visualinfo;
XImage*		image;
int		X_width;
int		X_height;

// MIT SHared Memory extension.
boolean		doShm;

XShmSegmentInfo	X_shminfo;
int		X_shmeventtype;

// Fake mouse handling.
// This cannot work properly w/o DGA.
// Needs an invisible mouse cursor at least.
boolean		grabMouse;
int		doPointerWarp = POINTER_WARP_COUNTDOWN;

// window size multiplier (-2/-3/-4)
static int	multiply=1;

// palette, or truecolor we convert to
typedef enum
{
    VMODE_PALETTE8,
    VMODE_TRUECOLOR
} vmode_t;

static vmode_t		X_mode;

// palette index -> native pixel value
static int		red_shift,   red_bits;
static int		green_shift, green_bits;
static int		blue_shift,  blue_bits;
static unsigned long	truecolor_lut[256];

// source-column lookup for scaling
static int*		scale_xlut;

static Atom		X_wmState;
static Atom		X_wmStateFullscreen;
static boolean		X_fullscreen;

static boolean		needResize;
static int		pendingWidth, pendingHeight;

// auto-press a key after a few seconds to skip the demo loop
#define AUTOADVANCE_TICS	(3*TICRATE)
static boolean		realInputSeen;
static boolean		autoAdvanceSent;
static int		startupTic = -1;

static void SetFullscreen(boolean enable);
static void I_ResizeXImage(int width, int height);
static void BlitScaled(void);


//
//  Translates the key currently in X_event
//

int xlatekey(void)
{

    int rc;

    switch(rc = XKeycodeToKeysym(X_display, X_event.xkey.keycode, 0))
    {
      case XK_Left:	rc = KEY_LEFTARROW;	break;
      case XK_Right:	rc = KEY_RIGHTARROW;	break;
      case XK_Down:	rc = KEY_DOWNARROW;	break;
      case XK_Up:	rc = KEY_UPARROW;	break;
      case XK_Escape:	rc = KEY_ESCAPE;	break;
      case XK_Return:	rc = KEY_ENTER;		break;
      case XK_Tab:	rc = KEY_TAB;		break;
      case XK_F1:	rc = KEY_F1;		break;
      case XK_F2:	rc = KEY_F2;		break;
      case XK_F3:	rc = KEY_F3;		break;
      case XK_F4:	rc = KEY_F4;		break;
      case XK_F5:	rc = KEY_F5;		break;
      case XK_F6:	rc = KEY_F6;		break;
      case XK_F7:	rc = KEY_F7;		break;
      case XK_F8:	rc = KEY_F8;		break;
      case XK_F9:	rc = KEY_F9;		break;
      case XK_F10:	rc = KEY_F10;		break;
      case XK_F11:	rc = KEY_F11;		break;
      case XK_F12:	rc = KEY_F12;		break;
	
      case XK_BackSpace:
      case XK_Delete:	rc = KEY_BACKSPACE;	break;

      case XK_Pause:	rc = KEY_PAUSE;		break;

      case XK_KP_Equal:
      case XK_equal:	rc = KEY_EQUALS;	break;

      case XK_KP_Subtract:
      case XK_minus:	rc = KEY_MINUS;		break;

      case XK_Shift_L:
      case XK_Shift_R:
	rc = KEY_RSHIFT;
	break;
	
      case XK_Control_L:
      case XK_Control_R:
	rc = KEY_RCTRL;
	break;
	
      case XK_Alt_L:
      case XK_Meta_L:
      case XK_Alt_R:
      case XK_Meta_R:
	rc = KEY_RALT;
	break;
	
      default:
	if (rc >= XK_space && rc <= XK_asciitilde)
	    rc = rc - XK_space + ' ';
	if (rc >= 'A' && rc <= 'Z')
	    rc = rc - 'A' + 'a';
	break;
    }

    return rc;

}

void I_ShutdownGraphics(void)
{
  if (doShm)
  {
    // Detach from X server
    if (!XShmDetach(X_display, &X_shminfo))
	    I_Error("XShmDetach() failed in I_ShutdownGraphics()");

    // Release shared memory.
    shmdt(X_shminfo.shmaddr);
    shmctl(X_shminfo.shmid, IPC_RMID, 0);
  }

  // Paranoia.
  if (image)
      image->data = NULL;
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?

}

static int	lastmousex = 0;
static int	lastmousey = 0;
boolean		mousemoved = false;
boolean		shmFinished;

static void MarkRealInput(const char *why)
{
    if (!realInputSeen)
	fprintf(stderr, "Real input seen (%s), auto-advance cancelled.\n", why);
    realInputSeen = true;
}

void I_GetEvent(void)
{

    event_t event;

    // put event-grabbing stuff in here
    XNextEvent(X_display, &X_event);
    switch (X_event.type)
    {
      case KeyPress:
	MarkRealInput("keypress");
	// alt+enter toggles fullscreen
	if (XKeycodeToKeysym(X_display, X_event.xkey.keycode, 0) == XK_Return
	    && (X_event.xkey.state & Mod1Mask))
	{
	    X_fullscreen = !X_fullscreen;
	    SetFullscreen(X_fullscreen);
	    break;
	}
	event.type = ev_keydown;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "k");
	break;
      case KeyRelease:
	MarkRealInput("keyrelease");
	event.type = ev_keyup;
	event.data1 = xlatekey();
	D_PostEvent(&event);
	// fprintf(stderr, "ku");
	break;
      case ButtonPress:
	MarkRealInput("buttonpress");
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0)
	    | (X_event.xbutton.button == Button1)
	    | (X_event.xbutton.button == Button2 ? 2 : 0)
	    | (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "b");
	break;
      case ButtonRelease:
	MarkRealInput("buttonrelease");
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xbutton.state & Button1Mask)
	    | (X_event.xbutton.state & Button2Mask ? 2 : 0)
	    | (X_event.xbutton.state & Button3Mask ? 4 : 0);
	// suggest parentheses around arithmetic in operand of |
	event.data1 =
	    event.data1
	    ^ (X_event.xbutton.button == Button1 ? 1 : 0)
	    ^ (X_event.xbutton.button == Button2 ? 2 : 0)
	    ^ (X_event.xbutton.button == Button3 ? 4 : 0);
	event.data2 = event.data3 = 0;
	D_PostEvent(&event);
	// fprintf(stderr, "bu");
	break;
      case MotionNotify:
	event.type = ev_mouse;
	event.data1 =
	    (X_event.xmotion.state & Button1Mask)
	    | (X_event.xmotion.state & Button2Mask ? 2 : 0)
	    | (X_event.xmotion.state & Button3Mask ? 4 : 0);
	event.data2 = (X_event.xmotion.x - lastmousex) << 2;
	event.data3 = (lastmousey - X_event.xmotion.y) << 2;

	if (event.data2 || event.data3)
	{
	    lastmousex = X_event.xmotion.x;
	    lastmousey = X_event.xmotion.y;
	    if (X_event.xmotion.x != X_width/2 &&
		X_event.xmotion.y != X_height/2)
	    {
		D_PostEvent(&event);
		// fprintf(stderr, "m");
		mousemoved = false;
	    } else
	    {
		mousemoved = true;
	    }
	}
	break;
	
      case Expose:
	break;

      case ConfigureNotify:
	if (X_event.xconfigure.width > 0 && X_event.xconfigure.height > 0
	    && (X_event.xconfigure.width != X_width
		|| X_event.xconfigure.height != X_height))
	{
	    pendingWidth = X_event.xconfigure.width;
	    pendingHeight = X_event.xconfigure.height;
	    needResize = true;
	}
	break;

      default:
	if (doShm && X_event.type == X_shmeventtype) shmFinished = true;
	break;
    }

}

Cursor
createnullcursor
( Display*	display,
  Window	root )
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
				 &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

//
// I_StartTic
//
void I_StartTic (void)
{

    if (!X_display)
	return;

    while (XPending(X_display))
	I_GetEvent();

    if (!realInputSeen && !autoAdvanceSent)
    {
	if (startupTic < 0)
	    startupTic = I_GetTime();
	else if (I_GetTime() - startupTic >= AUTOADVANCE_TICS)
	{
	    event_t autoEvent;

	    autoAdvanceSent = true;
	    fprintf(stderr, "No input seen in %d seconds, auto-advancing.\n",
		    AUTOADVANCE_TICS / TICRATE);

	    autoEvent.type = ev_keydown;
	    autoEvent.data1 = KEY_ENTER;
	    D_PostEvent(&autoEvent);

	    autoEvent.type = ev_keyup;
	    D_PostEvent(&autoEvent);
	}
    }

    // Warp the pointer back to the middle of the window
    //  or it will wander off - that is, the game will
    //  loose input focus within X11.
    if (grabMouse)
    {
	if (!--doPointerWarp)
	{
	    XWarpPointer( X_display,
			  None,
			  X_mainWindow,
			  0, 0,
			  0, 0,
			  X_width/2, X_height/2);

	    doPointerWarp = POINTER_WARP_COUNTDOWN;
	}
    }

    mousemoved = false;

}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{

    static int	lasttic;
    int		tics;
    int		i;
    // UNUSED static unsigned char *bigscreen=0;

    // draws little dots on the bottom of the screen
    if (devparm)
    {

	i = I_GetTime();
	tics = i - lasttic;
	lasttic = i;
	if (tics > 20) tics = 20;

	for (i=0 ; i<tics*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
	for ( ; i<20*2 ; i+=2)
	    screens[0][ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    
    }

    if (needResize)
    {
	needResize = false;
	I_ResizeXImage(pendingWidth, pendingHeight);
    }

    BlitScaled();

    if (doShm)
    {

	if (!XShmPutImage(	X_display,
				X_mainWindow,
				X_gc,
				image,
				0, 0,
				0, 0,
				X_width, X_height,
				True ))
	    I_Error("XShmPutImage() failed\n");

	// wait for it to finish and processes all input events
	shmFinished = false;
	do
	{
	    I_GetEvent();
	} while (!shmFinished);

    }
    else
    {

	// draw the image
	XPutImage(	X_display,
			X_mainWindow,
			X_gc,
			image,
			0, 0,
			0, 0,
			X_width, X_height );

	// sync up with server
	XSync(X_display, False);

    }

}


//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy (scr, screens[0], SCREENWIDTH*SCREENHEIGHT);
}


//
// Palette stuff.
//
static XColor	colors[256];

// shift and width of a channel mask
static void ComputeChannelShift(unsigned long mask, int *shift, int *bits)
{
    int s = 0;
    int b = 0;

    if (!mask)
    {
	*shift = 0;
	*bits = 0;
	return;
    }

    while (!(mask & 1))
    {
	mask >>= 1;
	s++;
    }
    while (mask & 1)
    {
	mask >>= 1;
	b++;
    }

    *shift = s;
    *bits = b;
}

static void UploadTruecolorPalette(byte *palette)
{
    int		i;
    int		r, g, b;

    for (i=0 ; i<256 ; i++)
    {
	r = gammatable[usegamma][*palette++];
	g = gammatable[usegamma][*palette++];
	b = gammatable[usegamma][*palette++];

	truecolor_lut[i] =
	    ((unsigned long)(r >> (8-red_bits))   << red_shift)
	  | ((unsigned long)(g >> (8-green_bits)) << green_shift)
	  | ((unsigned long)(b >> (8-blue_bits))  << blue_shift);
    }
}

void UploadNewPalette(Colormap cmap, byte *palette)
{

    register int	i;
    register int	c;
    static boolean	firstcall = true;

    if (X_mode == VMODE_TRUECOLOR)
    {
	UploadTruecolorPalette(palette);
	return;
    }

    {
	    // initialize the colormap
	    if (firstcall)
	    {
		firstcall = false;
		for (i=0 ; i<256 ; i++)
		{
		    colors[i].pixel = i;
		    colors[i].flags = DoRed|DoGreen|DoBlue;
		}
	    }

	    // set the X colormap entries
	    for (i=0 ; i<256 ; i++)
	    {
		c = gammatable[usegamma][*palette++];
		colors[i].red = (c<<8) + c;
		c = gammatable[usegamma][*palette++];
		colors[i].green = (c<<8) + c;
		c = gammatable[usegamma][*palette++];
		colors[i].blue = (c<<8) + c;
	    }

	    // store the colors to the current colormap
	    XStoreColors(X_display, cmap, colors, 256);

	}
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
{
    UploadNewPalette(X_cmap, palette);
}


//
// This function is probably redundant,
//  if XShmDetach works properly.
// ddt never detached the XShm memory,
//  thus there might have been stale
//  handles accumulating.
//
void grabsharedmemory(int size)
{

  int			key = ('d'<<24) | ('o'<<16) | ('o'<<8) | 'm';
  struct shmid_ds	shminfo;
  int			minsize = 320*200;
  int			id;
  int			rc;
  // UNUSED int done=0;
  int			pollution=5;
  
  // try to use what was here before
  do
  {
    id = shmget((key_t) key, minsize, 0777); // just get the id
    if (id != -1)
    {
      rc=shmctl(id, IPC_STAT, &shminfo); // get stats on it
      if (!rc) 
      {
	if (shminfo.shm_nattch)
	{
	  fprintf(stderr, "User %d appears to be running "
		  "DOOM.  Is that wise?\n", shminfo.shm_cpid);
	  key++;
	}
	else
	{
	  if (getuid() == shminfo.shm_perm.cuid)
	  {
	    rc = shmctl(id, IPC_RMID, 0);
	    if (!rc)
	      fprintf(stderr,
		      "Was able to kill my old shared memory\n");
	    else
	      I_Error("Was NOT able to kill my old shared memory");
	    
	    id = shmget((key_t)key, size, IPC_CREAT|0777);
	    if (id==-1)
	      I_Error("Could not get shared memory");
	    
	    rc=shmctl(id, IPC_STAT, &shminfo);
	    
	    break;
	    
	  }
	  if (size >= shminfo.shm_segsz)
	  {
	    fprintf(stderr,
		    "will use %d's stale shared memory\n",
		    shminfo.shm_cpid);
	    break;
	  }
	  else
	  {
	    fprintf(stderr,
		    "warning: can't use stale "
		    "shared memory belonging to id %d, "
		    "key=0x%x\n",
		    shminfo.shm_cpid, key);
	    key++;
	  }
	}
      }
      else
      {
	I_Error("could not get stats on key=%d", key);
      }
    }
    else
    {
      id = shmget((key_t)key, size, IPC_CREAT|0777);
      if (id==-1)
      {
	fprintf(stderr, "errno=%d\n", errno);
	I_Error("Could not get any shared memory");
      }
      break;
    }
  } while (--pollution);
  
  if (!pollution)
  {
    I_Error("Sorry, system too polluted with stale "
	    "shared memory segments.\n");
    }	
  
  X_shminfo.shmid = id;
  
  // attach to the shared memory segment
  image->data = X_shminfo.shmaddr = shmat(id, 0, 0);
  
  fprintf(stderr, "shared memory id=%d, addr=%p\n", id,
	  image->data);
}

//
// EWMH fullscreen support.
//
static void InitEWMHAtoms(void)
{
    X_wmState = XInternAtom(X_display, "_NET_WM_STATE", False);
    X_wmStateFullscreen = XInternAtom(X_display, "_NET_WM_STATE_FULLSCREEN", False);
}

static void SetFullscreen(boolean enable)
{
    XEvent xev;

    if (!X_display || !X_wmState || !X_wmStateFullscreen)
	return;

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = X_mainWindow;
    xev.xclient.message_type = X_wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = enable ? 1 : 0;	// add / remove
    xev.xclient.data.l[1] = (long) X_wmStateFullscreen;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1;			// source: normal app

    XSendEvent(X_display, RootWindow(X_display, X_screen), False,
	       SubstructureNotifyMask | SubstructureRedirectMask, &xev);
    XFlush(X_display);
}

//
// rebuilds the backing image buffer at the given size
//
static void I_ResizeXImage(int width, int height)
{
    int	i;
    int	sx;

    if (width < 32) width = 32;
    if (height < 32) height = 32;

    if (image)
    {
	if (doShm)
	{
	    XShmDetach(X_display, &X_shminfo);
	    shmdt(X_shminfo.shmaddr);
	    shmctl(X_shminfo.shmid, IPC_RMID, 0);
	    image->data = NULL;	// shm memory, not ours to free()
	    XDestroyImage(image);
	}
	else
	{
	    XDestroyImage(image);	// also frees image->data
	}
	image = NULL;
    }

    X_width = width;
    X_height = height;

    if (doShm)
    {
	image = XShmCreateImage(	X_display,
					X_visual,
					X_visualinfo.depth,
					ZPixmap,
					0,
					&X_shminfo,
					X_width,
					X_height );

	grabsharedmemory(image->bytes_per_line * image->height);

	if (!image->data)
	{
	    perror("");
	    I_Error("shmat() failed in I_ResizeXImage()");
	}

	if (!XShmAttach(X_display, &X_shminfo))
	    I_Error("XShmAttach() failed in I_ResizeXImage()");
    }
    else
    {
	image = XCreateImage(	X_display,
				X_visual,
				X_visualinfo.depth,
				ZPixmap,
				0,
				NULL,
				X_width, X_height,
				32,
				0 );
	image->data = (char *) malloc(image->bytes_per_line * X_height);
    }

    free(scale_xlut);
    scale_xlut = (int *) malloc(X_width * sizeof(*scale_xlut));
    for (i=0 ; i<X_width ; i++)
    {
	sx = i * SCREENWIDTH / X_width;
	if (sx >= SCREENWIDTH)
	    sx = SCREENWIDTH - 1;
	scale_xlut[i] = sx;
    }
}

//
// scales/colour-converts into the output image
//
static void BlitScaled(void)
{
    int			x, y;
    int			srcy;
    byte*		srcrow;
    byte*		dstrow;

    if (!scale_xlut || !image)
	return;

    if (X_mode == VMODE_PALETTE8)
    {
	for (y=0 ; y<X_height ; y++)
	{
	    srcy = y * SCREENHEIGHT / X_height;
	    srcrow = screens[0] + srcy*SCREENWIDTH;
	    dstrow = (byte *) image->data + y*image->bytes_per_line;
	    for (x=0 ; x<X_width ; x++)
		dstrow[x] = srcrow[scale_xlut[x]];
	}
	return;
    }

    // truecolor / directcolor
    if (image->bits_per_pixel == 32)
    {
	static int	checkedOrder;
	static boolean	swapBytes;
	unsigned int*	dst;
	unsigned long	pixel;

	if (!checkedOrder)
	{
	    union { unsigned int i; unsigned char c[4]; } u;
	    u.i = 1;
	    swapBytes = (u.c[0] == 1) != (image->byte_order == LSBFirst);
	    checkedOrder = 1;
	}

	for (y=0 ; y<X_height ; y++)
	{
	    srcy = y * SCREENHEIGHT / X_height;
	    srcrow = screens[0] + srcy*SCREENWIDTH;
	    dst = (unsigned int *) ((byte *) image->data + y*image->bytes_per_line);
	    for (x=0 ; x<X_width ; x++)
	    {
		pixel = truecolor_lut[srcrow[scale_xlut[x]]];
		if (swapBytes)
		    pixel =   ((pixel & 0x000000ff) << 24)
			    | ((pixel & 0x0000ff00) << 8)
			    | ((pixel & 0x00ff0000) >> 8)
			    | ((pixel & 0xff000000) >> 24);
		dst[x] = (unsigned int) pixel;
	    }
	}
    }
    else
    {
	// rare depths: fall back to XPutPixel
	for (y=0 ; y<X_height ; y++)
	{
	    srcy = y * SCREENHEIGHT / X_height;
	    srcrow = screens[0] + srcy*SCREENWIDTH;
	    for (x=0 ; x<X_width ; x++)
		XPutPixel(image, x, y, truecolor_lut[srcrow[scale_xlut[x]]]);
	}
    }
}

void I_InitGraphics(void)
{

    char*		displayname;
    char*		d;
    int			n;
    int			pnum;
    int			x=0;
    int			y=0;
    
    // warning: char format, different type arg
    char		xsign=' ';
    char		ysign=' ';
    
    int			oktodraw;
    unsigned long	attribmask;
    XSetWindowAttributes attribs;
    XGCValues		xgcvalues;
    int			valuemask;
    static int		firsttime=1;

    if (!firsttime)
	return;
    firsttime = 0;

    signal(SIGINT, (void (*)(int)) I_Quit);

    if (M_CheckParm("-2"))
	multiply = 2;

    if (M_CheckParm("-3"))
	multiply = 3;

    if (M_CheckParm("-4"))
	multiply = 4;

    X_width = SCREENWIDTH * multiply;
    X_height = SCREENHEIGHT * multiply;

    // check for command-line display name
    if ( (pnum=M_CheckParm("-disp")) ) // suggest parentheses around assignment
	displayname = myargv[pnum+1];
    else
	displayname = 0;

    // check if the user wants to grab the mouse (quite unnice)
    grabMouse = !!M_CheckParm("-grabmouse");

    // check for command-line geometry
    if ( (pnum=M_CheckParm("-geom")) ) // suggest parentheses around assignment
    {
	// warning: char format, different type arg 3,5
	n = sscanf(myargv[pnum+1], "%c%d%c%d", &xsign, &x, &ysign, &y);
	
	if (n==2)
	    x = y = 0;
	else if (n==4)
	{
	    if (xsign == '-')
		x = -x;
	    if (ysign == '-')
		y = -y;
	}
	else
	    I_Error("bad -geom parameter");
    }

    // open the display
    X_display = XOpenDisplay(displayname);
    if (!X_display)
    {
	if (displayname)
	    I_Error("Could not open display [%s]", displayname);
	else
	    I_Error("Could not open display (DISPLAY=[%s])", getenv("DISPLAY"));
    }

    // prefer 8-bit palette, fall back to truecolor
    X_screen = DefaultScreen(X_display);
    if (XMatchVisualInfo(X_display, X_screen, 8, PseudoColor, &X_visualinfo))
    {
	X_mode = VMODE_PALETTE8;
    }
    else
    {
	int depth = DefaultDepth(X_display, X_screen);
	if (!XMatchVisualInfo(X_display, X_screen, depth, TrueColor, &X_visualinfo)
	    && !XMatchVisualInfo(X_display, X_screen, depth, DirectColor, &X_visualinfo))
	{
	    I_Error("I_InitGraphics: no usable 8-bit PseudoColor or "
		    "TrueColor/DirectColor visual found");
	}
	X_mode = VMODE_TRUECOLOR;
	ComputeChannelShift(X_visualinfo.red_mask,   &red_shift,   &red_bits);
	ComputeChannelShift(X_visualinfo.green_mask, &green_shift, &green_bits);
	ComputeChannelShift(X_visualinfo.blue_mask,  &blue_shift,  &blue_bits);
    }
    X_visual = X_visualinfo.visual;

    // check for the MITSHM extension
    doShm = XShmQueryExtension(X_display);

    // even if it's available, make sure it's a local connection
    if (doShm)
    {
	if (!displayname) displayname = (char *) getenv("DISPLAY");
	if (displayname)
	{
	    d = displayname;
	    while (*d && (*d != ':')) d++;
	    if (*d) *d = 0;
	    if (!(!strcasecmp(displayname, "unix") || !*displayname)) doShm = false;
	}
    }

    fprintf(stderr, "Using MITSHM extension\n");

    // create the colormap
    X_cmap = XCreateColormap(X_display, RootWindow(X_display,
						   X_screen), X_visual,
			      X_mode == VMODE_PALETTE8 ? AllocAll : AllocNone);

    // setup attributes for main window
    attribmask = CWEventMask | CWColormap | CWBorderPixel;
    attribs.event_mask =
	KeyPressMask
	| KeyReleaseMask
	| StructureNotifyMask	// so we hear about resizes
	// | PointerMotionMask | ButtonPressMask | ButtonReleaseMask
	| ExposureMask;

    attribs.colormap = X_cmap;
    attribs.border_pixel = 0;

    // create the main window
    X_mainWindow = XCreateWindow(	X_display,
					RootWindow(X_display, X_screen),
					x, y,
					X_width, X_height,
					0, // borderwidth
					X_visualinfo.depth,
					InputOutput,
					X_visual,
					attribmask,
					&attribs );

    XStoreName(X_display, X_mainWindow, "DOOM");

    XDefineCursor(X_display, X_mainWindow,
		  createnullcursor( X_display, X_mainWindow ) );

    // create the GC
    valuemask = GCGraphicsExposures;
    xgcvalues.graphics_exposures = False;
    X_gc = XCreateGC(	X_display,
  			X_mainWindow,
  			valuemask,
  			&xgcvalues );

    // map the window
    XMapWindow(X_display, X_mainWindow);

    if (X_mode == VMODE_PALETTE8)
    {
	// install the colormap ourselves
	XInstallColormap(X_display, X_cmap);
    }

    InitEWMHAtoms();
    if (M_CheckParm("-fullscreen"))
    {
	X_fullscreen = true;
	SetFullscreen(true);
    }
    fprintf(stderr, "Press Alt+Enter to toggle fullscreen; drag the "
		     "window edge to resize.\n");

    // wait until it is OK to draw
    oktodraw = 0;
    while (!oktodraw)
    {
	XNextEvent(X_display, &X_event);
	if (X_event.type == Expose
	    && !X_event.xexpose.count)
	{
	    oktodraw = 1;
	}
    }

    // grab focus so keypresses aren't swallowed
    XSetInputFocus(X_display, X_mainWindow, RevertToParent, CurrentTime);
    XRaiseWindow(X_display, X_mainWindow);

    // grabs the pointer so it is restricted to this window
    if (grabMouse)
	XGrabPointer(X_display, X_mainWindow, True,
		     ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
		     GrabModeAsync, GrabModeAsync,
		     X_mainWindow, None, CurrentTime);

    if (doShm)
	X_shmeventtype = XShmGetEventBase(X_display) + ShmCompletion;

    // read the window's actual current size
    {
	XWindowAttributes attr;
	if (XGetWindowAttributes(X_display, X_mainWindow, &attr))
	{
	    X_width = attr.width;
	    X_height = attr.height;
	}
    }

    I_ResizeXImage(X_width, X_height);

    screens[0] = (unsigned char *) malloc (SCREENWIDTH * SCREENHEIGHT);

}



