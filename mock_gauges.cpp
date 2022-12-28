// Quick mock-up of a potential set of gauges
// TODO - convert this to arduino TFT library with an abstraction layer to allow mock testing in X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdio>

Display *dis;
int screen;
Window win;
GC gc;
unsigned long fgColor,bgColor;
XColor yellow,red, blue;

typedef struct {
    // Placement
    int x;
    int y;
    unsigned int size;

    // Ranges - set div large enough to retain accuracy with integer math
    int div; // Divide all values by this to get real unit value 
    // TODO - div not yet implemented...
    int minValue;
    int maxValue;
    int lowWarn;
    int highWarn;

    unsigned long lowWarnColor;
    unsigned long highWarnColor;

    // Unit character for numeric display
    char unit;

    char label[16]; // TODO dynamic string of some sorts?
} GaugeConfiguration;

void gauge(GaugeConfiguration cfg, int value) {
    // X11 specific aspects
    char fontQuery[255];
    XFontStruct* valueFont;// TODO - these leak!
    XFontStruct* labelFont;
    XFontStruct* rangeFont; 

    char readingText[64];
    char labelText[64];
    char rangeText[4][8];
    int rangeOrigin[4][2];
    int startAngle = 225*64; // Angles defined by X11 lib
    int endAngle = -270*64; // Angles defined by X11 lib
    int width = cfg.size / 8;
    int pad = width / 4;
    int x = cfg.x + pad;
    int y = cfg.y + pad;
    unsigned int size = cfg.size - (2*pad);
    int innerX = x + width;
    int innerY = y + width;
    unsigned int innerSize = size - (2*width);

    // Range of values for visual display on gauge
    int range = cfg.maxValue - cfg.minValue;

    // Warnings to angle mappings
    int lowWarnAngle = startAngle + endAngle * (cfg.lowWarn-cfg.minValue) / range;
    int highWarnAngle = startAngle + endAngle * (cfg.highWarn-cfg.minValue) / range;

    // Range Text
    // TODO - some algo to find closest round size
    snprintf(rangeText[0], 8, "%d",cfg.minValue);
    snprintf(rangeText[1], 8, "%d",cfg.minValue + range/3);
    snprintf(rangeText[2], 8, "%d",cfg.minValue + range*2/3);
    snprintf(rangeText[3], 8, "%d",cfg.maxValue);
    int maxRange = 0;
    for (int i = 1; i < 4; i++) {
        if (strlen(rangeText[i]) > strlen(rangeText[maxRange])) {
            maxRange = i;
        }
    }

    // Determine angle of current reading relative to start
    int currentAngle = 32; // Render at least 1/2 degree
    if (value > cfg.maxValue) {
        currentAngle = endAngle;
    } else if (value > cfg.minValue) {
        currentAngle = endAngle * (value-cfg.minValue)/range;
    }

    // X11 specfic heigh/width calculations based on fonts
    // Determine target text size for center value
    int targetValueFontHeight = width + width/2;
    snprintf(fontQuery, 255, "-*-*-medium-r-s*--%d-*-*-*-*-*-iso10*-1",targetValueFontHeight);
    valueFont = XLoadQueryFont(dis, fontQuery);
    // Determine target text size for label
    int targetLabelFontHeight = width;
    snprintf(fontQuery, 255, "-*-*-medium-r-s*--%d-*-*-*-*-*-iso10*-1",targetLabelFontHeight);
    labelFont = XLoadQueryFont(dis, fontQuery);
    // Determine target text size for range
    int targetRangeFontHeight = width * 0.8;
    snprintf(fontQuery, 255, "-*-*-medium-r-s*--%d-*-*-*-*-*-iso10*-1",targetRangeFontHeight);
    rangeFont = XLoadQueryFont(dis, fontQuery);

    // Determine text value
    snprintf(readingText, 64, "%d%c",value, cfg.unit);
    int readingWidth = XTextWidth(valueFont, readingText, strlen(readingText));
    int readingHeight = valueFont->ascent + valueFont->descent ;
    int readingX = (x + size/2) - (readingWidth/2);
    int readingY = (y + size/2) + (readingHeight/2);

    // Determine text label
    int labelWidth = XTextWidth(valueFont, cfg.label, strlen(cfg.label));
    int labelHeight = labelFont->ascent + labelFont->descent ;
    int labelX = (x + size/2) - (labelWidth/2);
    int labelY = (y + size - width) + (labelHeight/2);

    // Determine text range
    int rangeWidth = XTextWidth(valueFont, rangeText[maxRange], strlen(rangeText[maxRange]));
    int rangeHeight = rangeFont->ascent + rangeFont->descent ;
    rangeOrigin[0][0] = (x);
    rangeOrigin[0][1] = (y + size - width/2 + pad/2);
    rangeOrigin[1][0] = (x);
    rangeOrigin[1][1] = (y + width/2);
    rangeOrigin[2][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[2][1] = (y + width/2);
    rangeOrigin[3][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[3][1] = (y + size - width/2 + pad/2);

    XSetForeground(dis,gc,fgColor);
    XSetFont(dis,gc,rangeFont->fid);
    XDrawString(dis,win,gc,rangeOrigin[0][0],rangeOrigin[0][1], rangeText[0], strlen(rangeText[0]));
    XDrawString(dis,win,gc,rangeOrigin[1][0],rangeOrigin[1][1], rangeText[1], strlen(rangeText[1]));
    XDrawString(dis,win,gc,rangeOrigin[2][0],rangeOrigin[2][1], rangeText[2], strlen(rangeText[2]));
    XDrawString(dis,win,gc,rangeOrigin[3][0],rangeOrigin[3][1], rangeText[3], strlen(rangeText[3]));

    // https://tronche.com/gui/x/xlib/graphics/drawing/XDrawArc.html

    // Show the bounding rectangle
    //XSetForeground(dis,gc,fgColor);
    //XDrawRectangle(dis,win,gc,labelX,labelY-labelHeight,labelWidth,labelHeight);

    // Clear out any prior readings
    // TODO - may not be necessary if the display lib is double buffered
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,x,y,size,size,startAngle,endAngle);
    XSetForeground(dis,gc,bgColor);
    XFillRectangle(dis,win,gc,readingX,readingY-readingHeight,readingWidth,readingHeight);

    // Draw the outline for the gauge
    // TODO - consider a shading band
    if (value < cfg.lowWarn && cfg.lowWarn != cfg.minValue) {
        XSetForeground(dis,gc,cfg.lowWarnColor);
    } else if (value > cfg.highWarn && cfg.highWarn != cfg.maxValue) {
        XSetForeground(dis,gc,cfg.highWarnColor);
    } else {
        XSetForeground(dis,gc,fgColor);
    }
    XDrawArc(dis,win,gc,x,y,size,size,startAngle,endAngle);
    XDrawArc(dis,win,gc,innerX,innerY,innerSize,innerSize,startAngle,endAngle);
    XFillArc(dis,win,gc,x,y,size,size,startAngle,currentAngle);
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,startAngle,currentAngle);

    // Cap the gauge
    XSetForeground(dis,gc,fgColor);
    XFillArc(dis,win,gc,x,y,size,size,startAngle+endAngle,32);
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,startAngle+endAngle,32);

    // Warning marks
    // TODO - possibly consider converting to a line instead of narrow arc
    if (cfg.lowWarn != cfg.minValue) {
        XSetForeground(dis,gc,cfg.lowWarnColor);
        XFillArc(dis,win,gc,x,y,size,size,lowWarnAngle,64);
    } 
    if (cfg.highWarn != cfg.maxValue) {
        XSetForeground(dis,gc,cfg.highWarnColor);
        XFillArc(dis,win,gc,x,y,size,size,highWarnAngle,64);
    }
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,lowWarnAngle,64);
    XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,highWarnAngle,64);

    // Place text value in the center
    XSetForeground(dis,gc,fgColor);
    XSetFont(dis,gc,valueFont->fid);
    XDrawString(dis,win,gc,readingX,readingY, readingText, strlen(readingText));
    XSetFont(dis,gc,labelFont->fid);
    XDrawString(dis,win,gc,labelX,labelY, cfg.label, strlen(cfg.label));

}

void init_x() {
        printf("Starting\n");

	dis=XOpenDisplay((char *)0);
   	screen=DefaultScreen(dis);
	fgColor=BlackPixel(dis,screen),
	bgColor=WhitePixel(dis, screen);
   	win=XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,	
		800, 600, 5,fgColor, bgColor);
	XSetStandardProperties(dis,win,"Howdy","Hi",None,NULL,0,NULL);
	XSelectInput(dis, win, ExposureMask|ButtonPressMask|KeyPressMask);
        gc=XCreateGC(dis, win, 0,0);        
	XSetBackground(dis,gc,bgColor);
	XSetForeground(dis,gc,fgColor);
	XClearWindow(dis, win);
	XMapRaised(dis, win);
        XParseColor(dis, DefaultColormap(dis,screen), "gold", &yellow);
        XAllocColor(dis, DefaultColormap(dis,screen),&yellow);
        XParseColor(dis, DefaultColormap(dis,screen), "crimson", &red);
        XAllocColor(dis, DefaultColormap(dis,screen),&red);
        XParseColor(dis, DefaultColormap(dis,screen), "blue", &blue);
        XAllocColor(dis, DefaultColormap(dis,screen),&blue);
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

	XClearWindow(dis, win);
        //gauge();
        GaugeConfiguration battTemp = GaugeConfiguration{
            x:5,
            y:5,
            size:180,
            minValue:10,
            maxValue:100,
            lowWarn:20,
            highWarn:70,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            unit:'c',
            label: {'t','e', 'm','p',0}
        };

        GaugeConfiguration soc = GaugeConfiguration{
            x:190,
            y:5,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:10,
            highWarn:95,
            lowWarnColor:red.pixel,
            highWarnColor:yellow.pixel,
            unit:'%',
            label: {'S','O','C',0}
        };

        GaugeConfiguration motor = GaugeConfiguration{
            x:5,
            y:190,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            unit:'c',
            label: {'m','o','t','o','r', 0}
        };
        GaugeConfiguration inverter = GaugeConfiguration{
            x:190,
            y:190,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            unit:'c',
            label: {'i','n','v','e','r','t','e','r', 0}
        };

        GaugeConfiguration dcwatts = GaugeConfiguration{
            x:5,
            y:380,
            size:145,
            minValue:0,
            maxValue:1000,
            lowWarn:0,
            highWarn:800,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            unit:'w',
            label: {'D','C',' ','w','a', 't', 't', 's', 0}
        };
        GaugeConfiguration dcamps = GaugeConfiguration{
            x:150,
            y:380,
            size:145,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:80,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            unit:'a',
            label: {'D','C',' ','a','m', 'p', 's', 0}
        };
        GaugeConfiguration lowvolts = GaugeConfiguration{
            x:300,
            y:380,
            size:145,
            minValue:9,
            maxValue:16,
            lowWarn:10,
            highWarn:15,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            unit:'v',
            label: {'1','2','v', 0}
        };
        GaugeConfiguration speed = GaugeConfiguration{
            x:390,
            y:5,
            size:370,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:100,
            lowWarnColor:yellow.pixel,
            highWarnColor:yellow.pixel,
            unit:'m',
            label: {'M','P','H', 0}
        };

        GaugeConfiguration gauges[] = {
            battTemp,
            soc,
            motor,
            inverter,
            dcwatts,
            dcamps,
            lowvolts,
            speed,
        };


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
			int x=event.xbutton.x,
			    y=event.xbutton.y;

			strcpy(text,"X is FUN!");
                        //int col = rand()%event.xbutton.x%255;
			//XSetForeground(dis,gc,col);
			//printf("You clicked %d,%d => %d\n",x, y, col);
			//XDrawString(dis,win,gc,x,y, text, strlen(text));

                        for (int i = 0; i < sizeof(gauges)/sizeof(gauges[0]); i++) {
                            gauge(gauges[i], rand()%(gauges[i].maxValue-gauges[i].minValue) + gauges[i].minValue);
                        }

		}
	}
    return 0;
}
