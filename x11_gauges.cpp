// Quick mock-up of a potential set of gauges
// TODO - convert this to arduino TFT library with an abstraction layer to allow mock testing in X11
//
// g++ mock_gauges.cpp -g -I/usr/include/freetype2 -o mock_gauges -lX11 -lXft

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xft/Xft.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdio>
#include <math.h>
#include <map>
#include "base_gauges.h"


Display *dis;
int screen;
Window win;
GC gc;
unsigned long fgColor,bgColor;
// XColor yellow,red, blue, grey, darkGrey;

std::map<unsigned int, XColor> colorMap;
unsigned long convertColor(unsigned int color) {
    // printf("Convert Color called %06X ", color);
    XColor c;
    if (colorMap.find(color) == colorMap.end()) {
        char tmp[8];
        snprintf(tmp, 8, "#%06X", color);
        XParseColor(dis, DefaultColormap(dis,screen), tmp, &c);
        XAllocColor(dis, DefaultColormap(dis,screen),&c);
        colorMap[color] = c;
        // printf("Storing color - pixel %lx\n", c.pixel);
    } else {
        c = colorMap[color];
        // printf("exists - pixel %lx\n", c.pixel);
    }
    return c.pixel;
}


class XLine: public Line {
public:
    XLine(
        short int x1,
        short int y1,
        short int x2,
        short int y2,
        unsigned long color,
        short int currentValue) : Line(x1,y1,x2,y2,color,currentValue){}
    virtual void Draw();
};

void XLine::Draw() {
    XSetForeground(dis,gc,convertColor(color));
    XDrawLine(dis, win, gc, x1,y1,x2,y2);
}

class XGauge: public BaseGauge {
private:
    XftFont* valueFont;// TODO - these leak!
    XftFont* labelFont;
    XftFont* rangeFont; 
    XGlyphInfo extents;
    XftDraw *draw;
    XftColor textColor;
    Visual *visual;
    Colormap cmap;
public:
    XGauge(GaugeConfiguration cfg);
    virtual void Draw();
    virtual Line* MakeLine(
        short int x1,
        short int y1,
        short int x2,
        short int y2,
        unsigned int color,
        short int currentValue) {return new XLine(x1,y1,x2,y2,color,currentValue);}

protected:
    short int readingWidth;
    short int readingHeight;
    short int readingX;
    short int readingY;
    short int labelWidth;
    short int labelHeight;
    short int labelX;
    short int labelY;
    short int rangeWidth;
    short int rangeHeight;
};


XGauge::XGauge(GaugeConfiguration cfg) : BaseGauge(cfg){
    printf("In XGauge default ctor %s\n", cfg.label);
    init();
    // X11 specfic heigh/width calculations based on fonts
    char fontQuery[64];
    // Determine target text size for center currentValue
    this->visual = DefaultVisual(dis, screen);
    this->cmap = DefaultColormap(dis, screen);
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetValueFontHeight);
    this->valueFont = XftFontOpenName(dis, screen, fontQuery);
    // Determine target text size for label
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetLabelFontHeight);
    this->labelFont = XftFontOpenName(dis, screen, fontQuery);
    // Determine target text size for range
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetRangeFontHeight);
    this->rangeFont = XftFontOpenName(dis, screen, fontQuery);
    XftColorAllocName(dis, visual, cmap, "#B0E0E6", &textColor);
    this->draw = XftDrawCreate(dis, win, this->visual, this->cmap);
    //printf("Font Heights: V:%d L:%d R:%d\n", targetValueFontHeight, targetLabelFontHeight, targetRangeFontHeight);

    // Determine text currentValue using max currentValue 
    if (cfg.div == 10) {
    snprintf(readingText, 64, "%d.0%c",this->cfg.maxValue/this->cfg.div, this->cfg.unit);
    } else {
    snprintf(readingText, 64, "%d%c",this->cfg.maxValue/this->cfg.div, this->cfg.unit);
    }
    XftTextExtents8(dis, valueFont, (const FcChar8*) readingText, strlen(readingText), &extents);
    readingWidth = extents.width;
    readingHeight = extents.height;
    readingX = (x + size/2) - (readingWidth/2);
    readingY = (y + size/2) + (readingHeight/2);

    // Determine text label
    XftTextExtents8(dis, labelFont, (const FcChar8*) this->cfg.label, strlen(this->cfg.label), &extents);
    labelWidth = extents.width;
    labelHeight = extents.height;
    labelX = (x + size/2) - (labelWidth/2);
    labelY = (y + size - thickness ) + (labelHeight/2) + pad;

    // Determine text range
    XftTextExtents8(dis, rangeFont, (const FcChar8*) rangeText[maxRangeTextLengthOffset], strlen(rangeText[maxRangeTextLengthOffset]), &extents);
    rangeWidth = extents.width;
    rangeHeight = extents.height;
    rangeOrigin[0][0] = (x);
    rangeOrigin[0][1] = (y + size - thickness/2 + pad/2);
    rangeOrigin[1][0] = (x);
    rangeOrigin[1][1] = (y + thickness/2);
    rangeOrigin[2][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[2][1] = (y + thickness/2);
    rangeOrigin[3][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[3][1] = (y + size - thickness/2 + pad/2);
}

void XGauge::Draw() {
    // XSetLineAttributes(dis,gc,1,LineSolid,CapButt,JoinMiter);

    // Draw current reading
    if (currentValue < cfg.lowWarn && cfg.lowWarn != cfg.minValue) {
        XSetForeground(dis,gc,convertColor(cfg.lowWarnColor));
    } else if (currentValue > cfg.highWarn && cfg.highWarn != cfg.maxValue) {
        XSetForeground(dis,gc,convertColor(cfg.highWarnColor));
    } else {
        XSetForeground(dis,gc,fgColor);
    }
    // Bounding box for debugging
    // XDrawRectangle(dis,win,gc,cfg.x,cfg.y,cfg.size,cfg.size);
    // printf("Cfg Bounds: %d,%d x %d,%d size %d\n", cfg.x, cfg.y, cfg.x+cfg.size, cfg.y+cfg.size, cfg.size);
    // printf("Effective Bounds: %d,%d x %d,%d with size %d pad %d\n", x, y, x+size, y+size, size, pad);
    // XDrawRectangle(dis,win,gc,x,y,size,size);


    XFillArc(dis,win,gc,x,y,size,size,0,360*64);
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,x+thickness,y+thickness,size - (2*thickness),size - (2*thickness),0,360*64);
    points p = currentReadingMask.p;
    XPoint pts[8];
    for (short int i=0; i < p.len; i++) {
        pts[i].x = p.x[i];
        pts[i].y = p.y[i];
    }
    XFillPolygon(dis,win,gc,pts,p.len,Complex,CoordModeOrigin);

    // Draw the outline for the gauge
    XSetForeground(dis,gc,convertColor(cfg.tickColor));
    // XSetLineAttributes(dis,gc,3,LineSolid,CapButt,JoinMiter);
    XDrawArc(dis,win,gc,x,y,size,size,0,360*64);
    XDrawArc(dis,win,gc,x+thickness,y+thickness,size - (2*thickness),size - (2*thickness),0,360*64);

    // Clear the bottom of the gauge
    pts[0].x = cfg.x+cfg.size/2;
    pts[0].y = cfg.y+cfg.size/2;
    pts[1].x = cfg.x;
    pts[1].y = cfg.y+cfg.size;
    pts[2].x = cfg.x+cfg.size;
    pts[2].y = cfg.y+cfg.size;
    XSetForeground(dis,gc,bgColor);
    XFillPolygon(dis,win,gc,pts,3,Convex,CoordModeOrigin);

    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[0][0],rangeOrigin[0][1], (const FcChar8*) rangeText[0], strlen(rangeText[0]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[1][0],rangeOrigin[1][1], (const FcChar8*) rangeText[1], strlen(rangeText[1]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[2][0],rangeOrigin[2][1], (const FcChar8*) rangeText[2], strlen(rangeText[2]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[3][0],rangeOrigin[3][1], (const FcChar8*) rangeText[3], strlen(rangeText[3]));

    // https://tronche.com/gui/x/xlib/graphics/drawing/XDrawArc.html

    // Show the bounding rectangle
    //XSetForeground(dis,gc,fgColor);
    //XDrawRectangle(dis,win,gc,labelX,labelY-labelHeight,labelWidth,labelHeight);

    // Clear out any prior readings
    XSetForeground(dis,gc,bgColor);
    XFillRectangle(dis,win,gc,readingX,readingY-readingHeight,readingWidth,readingHeight);


    // Cap the gauge
    capLine->Draw();

    // Tick marks
    for (short int i = 0; i < tickCount; i++) {
        ticks[i]->Draw();
    }

    // Warning marks
    if (lowWarnLine) {
        lowWarnLine->Draw();
    } 
    if (highWarnLine) {
        highWarnLine->Draw();
    }

    // Place text currentValue in the center
    XftDrawStringUtf8(draw,&textColor,valueFont,readingX,readingY, (const FcChar8*) readingText, strlen(readingText));
    XftDrawStringUtf8(draw,&textColor,labelFont,labelX,labelY, (const FcChar8*) cfg.label, strlen(cfg.label));

    XFlush(dis);
}

void init_x() {
    //printf("Starting\n");

	dis=XOpenDisplay((char *)0);
   	screen=DefaultScreen(dis);
	bgColor=convertColor(BLACK);
    fgColor=convertColor(GREY);
   	win=XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,	800, 480, 5,fgColor, bgColor);
	XSetStandardProperties(dis,win,"Howdy","Hi",None,NULL,0,NULL);
	XSelectInput(dis, win, ExposureMask|ButtonPressMask);
    gc=XCreateGC(dis, win, 0,0);        
	XSetBackground(dis,gc,bgColor);
	XSetForeground(dis,gc,fgColor);
	XClearWindow(dis, win);
	XMapRaised(dis, win);
    // XParseColor(dis, DefaultColormap(dis,screen), "gold", &yellow);
    // XAllocColor(dis, DefaultColormap(dis,screen),&yellow);
    // XParseColor(dis, DefaultColormap(dis,screen), "crimson", &red);
    // XAllocColor(dis, DefaultColormap(dis,screen),&red);
    // XParseColor(dis, DefaultColormap(dis,screen), "royal blue", &blue);
    // XAllocColor(dis, DefaultColormap(dis,screen),&blue);
    // XParseColor(dis, DefaultColormap(dis,screen), "dim grey", &darkGrey);
    // XAllocColor(dis, DefaultColormap(dis,screen),&darkGrey);
    printf("finished\n");
};

void close_x() {
/* it is good programming practice to return system resources to the 
   system...
*/
        printf("cleaning up\n");
	XFreeGC(dis, gc);
	XDestroyWindow(dis,win);
	XCloseDisplay(dis);	
// TODO - other leaks addressed here..
        printf("bye\n");
	exit(1);				
}

void redraw() {
	XClearWindow(dis, win);
};


int main() {
	XEvent event;		/* the XEvent declaration !!! */
	KeySym key;		/* a dealie-bob to handle KeyPress Events */	
	char text[255];		/* a char buffer for KeyPress Events */
        init_x();

    // TODO refactor this to be common for both types of UIs
	XClearWindow(dis, win);
    int len;
    GaugeConfiguration *cfgs = BaseGauge::GetFullConfiguration(&len);
    printf("Got %d cfgs\n", len);
    XGauge *gauges[len];
    for (int i = 0; i < len; i++) {
        gauges[i] = new XGauge(cfgs[i]);
    }

	/* look for events forever... */
	while(1) {		
		/* get the next event and stuff it into our event variable.
		   Note:  only events we set the mask for are detected!
		*/
		XNextEvent(dis, &event);
	
		if (event.type==Expose && event.xexpose.count==0) {
		/* the window was exposed redraw it! */
            printf("Redrawing\n");
            redraw();
            for (float v = 0; v <= 1; v += 0.01) {
                for (short int i = 0; i < sizeof(gauges)/sizeof(gauges[0]); i++) {
                    gauges[i]->UpdateValue((short int)(gauges[i]->cfg.maxValue-gauges[i]->cfg.minValue)*v + gauges[i]->cfg.minValue);
                    gauges[i]->Draw();
                }
            //redraw();
                usleep(50000);
            }
		}
		if (event.type==KeyPress&&
		    XLookupString(&event.xkey,text,255,&key,0)==1) {
		/* use the XLookupString routine to convert the invent
		   KeyPress data into regular text.  Weird but necessary...
		*/
			if (text[0]=='q') {
				close_x();
			}
			printf("You pressed the %c key!\n",text[0]);
		}
		if (event.type==ButtonPress) {
		/* tell where the mouse Button was Pressed */
			short int x=event.xbutton.x,
			    y=event.xbutton.y;

			//strcpy(text,"X is FUN!");
                        double relative = y/480.0;
                        for (short int i = 0; i < sizeof(gauges)/sizeof(gauges[0]); i++) {
                            short int currentValue = (short int)(gauges[i]->cfg.maxValue-gauges[i]->cfg.minValue)*relative + gauges[i]->cfg.minValue;
                            gauges[i]->UpdateValue(currentValue);
                            gauges[i]->Draw();
                        }

		}
	}
    return 0;
}







