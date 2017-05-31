#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifdef WIN32
	#include <windows.h>
	#include <windowsx.h>
#endif

#ifdef __linux__
	#include <unistd.h>
	#include <sys/time.h>
	#include <sys/ipc.h>
	#include <sys/shm.h>

	#include <X11/Xlib.h>
	#include <X11/Xatom.h>
	#include <X11/Xutil.h>
	#include <X11/extensions/XShm.h>
	
	#define VK_LEFT		113
	#define VK_UP		111
	#define	VK_RIGHT	114
	#define	VK_DOWN		116
	#define	VK_BACK		22
#endif

typedef unsigned int Color;

Color toColor(unsigned int x) {
	if (sizeof(Color) == 4)
		return x;
	if (sizeof(Color) == 2)
		return (x & 0xF80000) >> 8 | (x & 0xFC00) >> 5 | (x & 0xFF) >> 3;
}

#define COLOR_CLEAR	toColor(0xFF000000)

struct Point {
	int x, y;
	Point() {}
	Point(int x, int y) : x(x), y(y) {}
};

struct Rect {
	int l, t, r, b;
	Rect() {}
	Rect(int l, int t, int r, int b) : l(l), t(t), r(r), b(b) {}
};

struct Canvas {
	int		width, height;
	int		stride;
	Color	*pixels;
	Rect	rect;
	
	XShmSegmentInfo	shminfo;
	GC		gc;
	Display	*display;
	XImage	*image;

	Canvas(Display *display) : width(0), height(0), pixels(NULL), display(display), image(NULL) {
		int i;
		if (!XQueryExtension(display, "MIT-SHM", &i, &i, &i))
			printf("SHM is not supported\n");
		gc = DefaultGC(display, DefaultScreen(display));
	}

	~Canvas() {
		resize(0, 0);
	}

	void resize(int width, int height)
	{
		if (this->width != width || this->height != height) 
		{
			printf("resize %d %d\n", width, height);

			this->width = width;
			this->height = height;
		
			if(image){
				XShmDetach(display, &shminfo);
				XDestroyImage(image);
				shmdt(shminfo.shmaddr);
			}
			
			if(!width||!height)return;
				
			int depth = DefaultDepth(display, DefaultScreen(display));
			printf("color depth:# %d\n", depth);

			image = XShmCreateImage(display, NULL, depth, ZPixmap, NULL, &shminfo, width, height);

			shminfo.shmid		= shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT|0777);
			shminfo.shmaddr		= image->data = (char*)shmat(shminfo.shmid, 0, 0);
			shminfo.readOnly	= false;

			XShmAttach(display, &shminfo);
			XSync(display, false);
			shmctl(shminfo.shmid, IPC_RMID, 0);

			pixels = (Color*)image->data;
			stride = image->bytes_per_line / sizeof(Color);
		}
	}
	bool is_crazy(int w){return w<0||w>1024*16;}
	void present(const Window &window) {
		if(!image){return;}
		if(is_crazy(width)||is_crazy(height))return;
		XShmPutImage(display,window,gc,image,0,0,0,0,width,height,false);
		XFlush(display);
		XSync(display,false);
	}

	void scrollX(int delta) {

	}

	void scrollY(int offset) {
		int size = width * (height - abs(offset)) * 4;
		if (size <= 0) return;

		if (offset > 0)
			memmove(&pixels[offset * width], &pixels[0], size);
		else
			memmove(&pixels[0], &pixels[-offset * width], size);
	}

	void fill(int x, int y, int w, int h, Color color) {
		for (int j = y; j < y + h; j++)
			for (int i = x; i < x + w; i++)
				pixels[i + j * stride] = color;
	};
	static inline double hypot(double x,double y){return sqrt(x*x+y*y);}
	void circle(int x, int y, int r, Color color) {
		x+=r;y+=r;int d=r*2;
		for (int j = 0; j < d; j++)
			for (int i = 0; i < d; i++){
				int px=i+x-d;int py=j+y-d;if(px<0||px>=width)continue;if(py<0||py>=height)continue;
				if(hypot(i-r,j-r)<r)pixels[px + py * stride] = color;
			}
	};
};

struct Application {
	int		width, height;
	Canvas	*canvas;

	Display	*display;
	Window	window;
	Atom	WM_DELETE_WINDOW;
	long event_mask;
	bool down;

	Application(int width, int height) : width(width), height(height),down(false) {
		display = XOpenDisplay(NULL);
		if (!display) {
			printf("can't connect to X server\n");
			return;
		}		
		Window root = DefaultRootWindow(display);
		int screen = DefaultScreen(display);
		
		XSetWindowAttributes attr;
		//attr.override_redirect = True;   
		attr.event_mask = ExposureMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask| KeyReleaseMask | StructureNotifyMask | FocusChangeMask;		
		event_mask=attr.event_mask;

		window = XCreateWindow(display, root, 0, 0, width, height, 0, 0, InputOutput, NULL, CWEventMask, &attr);

		XMapWindow(display, window);
		XSync(display, false);
		
		XStoreName(display, window, "xedit");
		WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", false);
		XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1);
		
		canvas = new Canvas(display);
		resize(1024, 768);
		fullscreen();
	}

	~Application() {
		delete canvas;
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}

	void close() {
	}

	void invalidate() {
		XClearArea(display, window, 0, 0, width, height, True);
	}

	void resize(int width, int height) {
		canvas->resize(width, height);
	}

	class QapClock
	{
	public:
	  double beg,tmp;
	  bool run;
	public:
	  QapClock(){run=false;Start();}
	  double em_perf_now(){
	    timeval t;
	    gettimeofday(&t,NULL);
	    return t.tv_sec*1e6+t.tv_usec;
	  }
	  void Start(){beg=em_perf_now();run=true;}
	  void Stop(){tmp=em_perf_now();run=false;tmp-=beg;}
	  double Time(){if(run)tmp=em_perf_now();return double(run?(tmp-beg):tmp)/1000.0;}
	  double MS()
	  {
	    double d1000=1000.0;
	    if(run)tmp=em_perf_now();
	    if(run)return tmp-beg;
	    if(!run)return tmp;
	    return 0;
	  }
	};

	void loop() {
		XEvent e;
		bool quit = false;int iter=0;QapClock clock;clock.Start();
		while (!quit) {
			//if(!XCheckMaskEvent(display,event_mask, &e)){
			//	canvas->present(window);
			//	XNextEvent(display,&e);
			//}
			if(!XPending(display)){

				canvas->circle((iter*2)%canvas->width, 200, 8, 0x00000000);
				iter++;
				canvas->circle((iter*2)%canvas->width, 200, 8, 0xffff0000);
				canvas->present(window);
				double ups=60;
				double ut=1000.0/ups;
				double ms=clock.MS()/1000;double dt=iter*ut-ms;double dtold=dt;
				/*printf("dt = %i\n",int(dt));*/
				if(dt<0)dt=0;if(dt>500)dt=500;
				//printf("dt = %4.2f             %4.2f\n",float(dt),float(dtold));
				usleep(abs(dt<0?0:dt)*1000);
				//printf("iter = %i\n",iter++);
				continue;
			}
			XNextEvent(display,&e);
			switch (e.type) {
				case FocusIn:
					invalidate();
					paint();
				break;
				case ButtonPress :{
					int b=e.xbutton.button;
					if(b==1) down=1;
					invalidate();
					break;		
				}
				case ButtonRelease:{
					int b=e.xbutton.button;
					printf("button: %d \n", e.xbutton.button);
					if(b==1) down=0;
					break;	
				}
				case MotionNotify :
					//printf("mouse: %d %d\n", e.xmotion.x, e.xmotion.y);
					if(down)canvas->circle(e.xmotion.x, e.xmotion.y, 8, 0xff80ff80);
					//offset = e.xmotion.y;
					//draw(display, window, DefaultGC(display, screen));
					break;
				case KeyPress: {
					//	printf("key: %d %d\n", e.xkey.state, e.xkey.keycode);
						char c;
						if (e.xkey.keycode != VK_BACK &&
							XLookupString(&e.xkey, &c, 1, NULL, NULL)) {
							//editor->onChar(c);
						//	printf("char %d %d\n", len, (int));
						} else
							//editor->onKey(e.xkey.keycode);
						invalidate();
					}
					break;
				case KeyRelease:{ 
						printf("key: %d %d\n", e.xkey.state, e.xkey.keycode);
						int VK_ESCAPE=9;
						int VK_RETURN=36;
						if(e.xkey.keycode==VK_ESCAPE)quit=true;
						if(e.xkey.keycode==VK_RETURN)fullscreen();
					break;
				}
				case ConfigureNotify : {
						width	= e.xconfigure.width;
						height	= e.xconfigure.height;
						resize(width, height);
					}
					break;
				case Expose :
					paint();
					break;
				case ClientMessage :
					if ((Atom)e.xclient.data.l[0] == WM_DELETE_WINDOW)
						quit = true;
					break;					
			}
			
		}
	}
	void fullscreen(){
		if(canvas&&canvas->image)fullscreen(display,window);
	}
	void fullscreen(Display* dpy, Window win) {
		#define _NET_WM_STATE_REMOVE        0
		#define _NET_WM_STATE_ADD           1
		#define _NET_WM_STATE_TOGGLE        2

		XEvent xev;
		Atom wm_state  =  XInternAtom(dpy, "_NET_WM_STATE", False);
		Atom max_horz  =  XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
		Atom max_vert  =  XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
		Atom scr_full  =  XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

		memset(&xev, 0, sizeof(xev));
		xev.type = ClientMessage;
		xev.xclient.window = win;
		xev.xclient.message_type = wm_state;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
		xev.xclient.data.l[1] = max_horz;
		//xev.xclient.data.l[2] = max_vert;
		xev.xclient.data.l[1] = scr_full;

		XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, &xev);
	}

	void paint() {
		canvas->present(window);
	}
};

int main() {
//	convertFont("font.tga", "font.dat");
	Application app(800, 600);
	app.loop();
	return 0;
};
