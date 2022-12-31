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

Display *dis;
int screen;
Window win;
GC gc;
unsigned long fgColor,bgColor;
XColor yellow,red, blue, darkGrey;

typedef struct {
    int x;
    int y;
    unsigned int size;

    // Ranges - set div large enough to retain accuracy with integer math
    int div; // Divide all values by this to get real unit value 
    int minValue;
    int maxValue;
    int lowWarn;
    int highWarn;

    // TODO refactor warnings to be a list, possibly with callback function when crossed
    unsigned long lowWarnColor;
    unsigned long highWarnColor;
    unsigned long tickColor;

    // Unit character for numeric display
    char unit;

    const char *label;
} GaugeConfiguration;

class Line {
public:
    int x1;
    int y1;
    int x2;
    int y2;
    unsigned long color;
    int value;
    Line(
        int x1,
        int y1,
        int x2,
        int y2,
        unsigned long color,
        int value);

    virtual void Draw() = 0;
};

Line::Line(
        int x1,
        int y1,
        int x2,
        int y2,
        unsigned long color,
        int value){
    this->x1 = x1;
    this->y1 = y1;
    this->x2 = x2;
    this->y2 = y2;
    this->color = color;
    this->value = value;
}

class XLine: public Line {
public:
    XLine(
        int x1,
        int y1,
        int x2,
        int y2,
        unsigned long color,
        int value) : Line(x1,y1,x2,y2,color,value){}
    virtual void Draw();
};


void XLine::Draw() {
    XSetForeground(dis,gc,color);
    XDrawLine(dis, win, gc, x1,y1,x2,y2);
}


class BaseGauge {
public:
    BaseGauge(GaugeConfiguration cfg);
    void UpdateValue(int value);
    virtual void Draw() = 0;
    virtual Line* MakeLine(
        int x1,
        int y1,
        int x2,
        int y2,
        unsigned long color,
        int value) = 0;
    GaugeConfiguration cfg; // TODO - should be protected

protected:
    void init(); // Wrap up initialization
    int x; // Actual X after padding
    int y; // Actual Y after padding
    unsigned int size; // Actual size after padding
    int value;
    int width;
    int pad;
    char readingText[64];
    char labelText[64];
    char rangeText[4][8];
    int rangeOrigin[4][2];
    int startAngle = 225*64; // TODO - fixup the X11 specific code to do the *64 math
    int endAngle = -270*64; 
    int innerX;
    int innerY;
    unsigned int innerSize;
    int innerRadius;
    int outerRadius;
    int innerRadiusWarn;
    int outerRadiusWarn;
    int innerRadiusTick;
    int outerRadiusTick;
    int range; // Range of values for visual display on gauge
    int lowWarnAngle;
    int highWarnAngle;
    int currentAngle;
    int maxRange;

    Line *capLine;
    Line *lowWarnLine;
    Line *highWarnLine;
    Line **ticks;
    unsigned int tickCount;
};

BaseGauge::BaseGauge(GaugeConfiguration cfg) {
    printf("In BaseGauge default ctor %s\n", cfg.label);
    if (cfg.div == 0) {
        cfg.div = 1;
    }
    capLine = nullptr;
    lowWarnLine = nullptr;
    highWarnLine = nullptr;
    this->cfg = cfg;
    width = cfg.size / 7;
    pad = this->width / 4;
    x = cfg.x + this->pad;
    y = cfg.y + this->pad;
    size = cfg.size - (2*this->pad);
    innerX = x + this->width;
    innerY = y + this->width;
    innerSize = size - (2*width);
    innerRadius = size/2-width;
    outerRadius = size/2;
    innerRadiusWarn = size/2-width;
    outerRadiusWarn = size/2;
    innerRadiusTick = size/2-width/3;
    outerRadiusTick = size/2;

    this->range = cfg.maxValue - cfg.minValue;

    // Warnings to angle mappings
    lowWarnAngle = startAngle + endAngle * (cfg.lowWarn-cfg.minValue) / range;
    highWarnAngle = startAngle + endAngle * (cfg.highWarn-cfg.minValue) / range;

}

void BaseGauge::init() {
    // Precalculate the cartesian cords for the various overlay lines
    // to save on cpu cycles when updating
    double endRadians = M_PI/180.0 * (startAngle+endAngle)/64;
    capLine = MakeLine(
        x + size/2 + (int)(innerRadius * cos(endRadians)),
        y + size/2 - (int)(innerRadius * sin(endRadians)),
        x + size/2 + (int)(outerRadius * cos(endRadians)),
        y + size/2 - (int)(outerRadius * sin(endRadians)),
        cfg.tickColor,
        cfg.maxValue
    );
    if (cfg.lowWarn != cfg.minValue) {
        double lowWarnRadians = M_PI/180.0 * lowWarnAngle/64;
        lowWarnLine = MakeLine(
            x + size/2 + (int)(innerRadiusWarn * cos(lowWarnRadians)),
            y + size/2 - (int)(innerRadiusWarn * sin(lowWarnRadians)),
            x + size/2 + (int)(outerRadiusWarn * cos(lowWarnRadians)),
            y + size/2 - (int)(outerRadiusWarn * sin(lowWarnRadians)),
            cfg.lowWarnColor,
            cfg.lowWarn
        );
    }
    if (cfg.highWarn != cfg.maxValue) {
        double highWarnRadians = M_PI/180.0 * highWarnAngle/64;
        highWarnLine = MakeLine(
            x + size/2 + (int)(innerRadiusWarn * cos(highWarnRadians)),
            y + size/2 - (int)(innerRadiusWarn * sin(highWarnRadians)),
            x + size/2 + (int)(outerRadiusWarn * cos(highWarnRadians)),
            y + size/2 - (int)(outerRadiusWarn * sin(highWarnRadians)),
            cfg.highWarnColor,
            cfg.highWarn
        );
    }

    // Tick marks
    // TODO - is there a more clever way to break down the tick spacing?
    int tickSpacing = 0;
    if (range-2 <= 10) {
        tickSpacing = 1;
    } else if (range-2 <= 100) {
        tickSpacing = 10;
    } else if (range-2 <= 1000) {
        tickSpacing = 100;
    } else {
        tickSpacing = 1000;
    }
    tickCount = (range-2) / tickSpacing;
    //printf("Ticks: %d %d\n", tickCount, tickSpacing);
    ticks = new Line*[tickCount];
    for (int i = 0; i < tickCount; i++) {
        int val = cfg.minValue + (i+1) * tickSpacing;
        int tickAngle = startAngle + endAngle * (val-cfg.minValue)/range;
        //printf("Tick %d = %d @ %d\n", i, val, tickAngle/64);
        double tickRadians = M_PI/180.0 * tickAngle/64;
        ticks[i] = MakeLine(
            x + size/2 + (int)(innerRadiusTick * cos(tickRadians)),
            y + size/2 - (int)(innerRadiusTick * sin(tickRadians)),
            x + size/2 + (int)(outerRadiusTick * cos(tickRadians)),
            y + size/2 - (int)(outerRadiusTick * sin(tickRadians)),
            cfg.tickColor,
            val
        );
    }

    // Range Text   
    int corner = cfg.minValue + range*2/3;
    double tmp = (double)corner/((double)tickSpacing);
    printf("Reference %d %f %f\n", corner, tmp, nearbyint((double)corner/(double)tickSpacing)*((double)tickSpacing));
    //printf("Reference %f\n", nearbyint((cfg.minValue/(double)cfg.div + (double)range*2.0/3.0)/(double)tickSpacing)*tickSpacing);
    snprintf(rangeText[0], 8, "%d",cfg.minValue/cfg.div);
    snprintf(rangeText[1], 8, "%d",cfg.minValue/cfg.div + range/3/cfg.div);
    snprintf(rangeText[2], 8, "%d",cfg.minValue/cfg.div + range*2/3/cfg.div);
    snprintf(rangeText[3], 8, "%d",cfg.maxValue/cfg.div);
    maxRange = 0;
    for (int i = 1; i < 4; i++) {
        if (strlen(rangeText[i]) > strlen(rangeText[maxRange])) {
            maxRange = i;
        }
    }
}

void BaseGauge::UpdateValue(int value) {
    //printf("%s new value %d\n", cfg.label, value);
    this->value = value;
    if (cfg.div == 10) {
        snprintf(readingText, 64, "%d.%d%c",value/cfg.div,value%10, cfg.unit);
    } else {
        snprintf(readingText, 64, "%d%c",value/cfg.div, cfg.unit);
    }
    // Determine angle of current reading relative to start
    currentAngle = 32; // Render at least 1/2 degree
    if (value > cfg.maxValue) {
        currentAngle = endAngle;
    } else if (value > cfg.minValue) {
        currentAngle = endAngle * (value-cfg.minValue)/range;
    }

    // TODO - more to do here
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
        int x1,
        int y1,
        int x2,
        int y2,
        unsigned long color,
        int value) {return new XLine(x1,y1,x2,y2,color,value);}

protected:
    int readingWidth;
    int readingHeight;
    int readingX;
    int readingY;
    int labelWidth;
    int labelHeight;
    int labelX;
    int labelY;
    int rangeWidth;
    int rangeHeight;
};

XGauge::XGauge(GaugeConfiguration cfg) : BaseGauge(cfg){
    printf("In XGauge default ctor %s\n", cfg.label);
    init();
    // X11 specfic heigh/width calculations based on fonts
    char fontQuery[64];
    // Determine target text size for center value
    int targetValueFontHeight = this->width + this->width/2;
    this->visual = DefaultVisual(dis, screen);
    this->cmap = DefaultColormap(dis, screen);
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetValueFontHeight);
    this->valueFont = XftFontOpenName(dis, screen, fontQuery);
    // Determine target text size for label
    int targetLabelFontHeight = this->width;
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetLabelFontHeight);
    this->labelFont = XftFontOpenName(dis, screen, fontQuery);
    // Determine target text size for range
    int targetRangeFontHeight = this->width * 0.8;
    snprintf(fontQuery, 64, "times:pixelsize=%d",targetRangeFontHeight);
    this->rangeFont = XftFontOpenName(dis, screen, fontQuery);
    XftColorAllocName(dis, visual, cmap, "#0000ee", &textColor);
    this->draw = XftDrawCreate(dis, win, this->visual, this->cmap);

    // Determine text value using max value 
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
    XftTextExtents8(dis, valueFont, (const FcChar8*) this->cfg.label, strlen(this->cfg.label), &extents);
    labelWidth = extents.width;
    labelHeight = extents.height;
    labelX = (x + size/2) - (labelWidth/2) + extents.width/strlen(this->cfg.label)/2;
    labelY = (y + size - width) + (labelHeight/2);

    // Determine text range
    XftTextExtents8(dis, valueFont, (const FcChar8*) rangeText[maxRange], strlen(rangeText[maxRange]), &extents);
    rangeWidth = extents.width;
    rangeHeight = extents.height;
    rangeOrigin[0][0] = (x);
    rangeOrigin[0][1] = (y + size - width/2 + pad/2);
    rangeOrigin[1][0] = (x);
    rangeOrigin[1][1] = (y + width/2);
    rangeOrigin[2][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[2][1] = (y + width/2);
    rangeOrigin[3][0] = (x + size - rangeWidth/2); // XXX width's don't quite seem rght
    rangeOrigin[3][1] = (y + size - width/2 + pad/2);
}

void XGauge::Draw() {
    XSetLineAttributes(dis,gc,1,LineSolid,CapButt,JoinMiter);
    //XSetForeground(dis,gc,fgColor);
    //XSetFont(dis,gc,rangeFont->fid);
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[0][0],rangeOrigin[0][1], (const FcChar8*) rangeText[0], strlen(rangeText[0]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[1][0],rangeOrigin[1][1], (const FcChar8*) rangeText[1], strlen(rangeText[1]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[2][0],rangeOrigin[2][1], (const FcChar8*) rangeText[2], strlen(rangeText[2]));
    XftDrawStringUtf8(draw,&textColor,rangeFont,rangeOrigin[3][0],rangeOrigin[3][1], (const FcChar8*) rangeText[3], strlen(rangeText[3]));

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


    // Draw current reading
    if (value < cfg.lowWarn && cfg.lowWarn != cfg.minValue) {
        XSetForeground(dis,gc,cfg.lowWarnColor);
    } else if (value > cfg.highWarn && cfg.highWarn != cfg.maxValue) {
        XSetForeground(dis,gc,cfg.highWarnColor);
    } else {
        XSetForeground(dis,gc,fgColor);
    }
    XFillArc(dis,win,gc,x,y,size,size,startAngle,currentAngle);
    XSetForeground(dis,gc,bgColor);
    XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,startAngle,currentAngle);

    // Draw the outline for the gauge
    XSetForeground(dis,gc,cfg.tickColor);
    XSetLineAttributes(dis,gc,3,LineSolid,CapButt,JoinMiter);
    XDrawArc(dis,win,gc,x,y,size,size,startAngle,endAngle);
    XDrawArc(dis,win,gc,innerX,innerY,innerSize,innerSize,startAngle,endAngle);

    // Cap the gauge
    capLine->Draw();

    // Tick marks
    for (int i = 0; i < tickCount; i++) {
        ticks[i]->Draw();
    }

    // Warning marks
    if (lowWarnLine) {
        lowWarnLine->Draw();
    } 
    if (highWarnLine) {
        highWarnLine->Draw();
    }

    //XSetForeground(dis,gc,bgColor);
    //XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,lowWarnAngle,64);
    //XFillArc(dis,win,gc,innerX,innerY,innerSize,innerSize,highWarnAngle,64);

    // Place text value in the center
    //XSetForeground(dis,gc,fgColor);
    //XSetFont(dis,gc,valueFont->fid);
    XftDrawStringUtf8(draw,&textColor,valueFont,readingX,readingY, (const FcChar8*) readingText, strlen(readingText));
    //XSetFont(dis,gc,labelFont->fid);
    XftDrawStringUtf8(draw,&textColor,labelFont,labelX,labelY, (const FcChar8*) cfg.label, strlen(cfg.label));

    XFlush(dis);
}

void init_x() {
        //printf("Starting\n");

	dis=XOpenDisplay((char *)0);
   	screen=DefaultScreen(dis);
	fgColor=BlackPixel(dis,screen),
	bgColor=WhitePixel(dis, screen);
   	win=XCreateSimpleWindow(dis,DefaultRootWindow(dis),0,0,	
		800, 600, 5,fgColor, bgColor);
	XSetStandardProperties(dis,win,"Howdy","Hi",None,NULL,0,NULL);
	XSelectInput(dis, win, ExposureMask|ButtonPressMask);
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
        XParseColor(dis, DefaultColormap(dis,screen), "dark grey", &darkGrey);
        XAllocColor(dis, DefaultColormap(dis,screen),&darkGrey);
        //printf("finished\n");
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

	XClearWindow(dis, win);
        XGauge battTemp = XGauge(GaugeConfiguration{
            x:5,
            y:5,
            size:180,
            minValue:10,
            maxValue:100,
            lowWarn:20,
            highWarn:70,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'c',
            label: "temp",
        });

        XGauge soc = XGauge(GaugeConfiguration{
            x:190,
            y:5,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:10,
            highWarn:95,
            lowWarnColor:red.pixel,
            highWarnColor:yellow.pixel,
            tickColor:darkGrey.pixel,
            unit:'%',
            label: "SOC",
        });

        XGauge motor = XGauge(GaugeConfiguration{
            x:5,
            y:190,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'c',
            label: "motor",
        });
        XGauge inverter = XGauge(GaugeConfiguration{
            x:190,
            y:190,
            size:180,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:blue.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'c',
            label: "inverter",
        });

        XGauge dcwatts = XGauge(GaugeConfiguration{
            x:5,
            y:380,
            size:145,
            minValue:0,
            maxValue:1000,
            lowWarn:0,
            highWarn:800,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'w',
            label: "DC Watts",
        });
        XGauge dcamps = XGauge(GaugeConfiguration{
            x:150,
            y:380,
            size:145,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:80,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'a',
            label: "DC Amps",
        });
        XGauge lowvolts = XGauge(GaugeConfiguration{
            x:300,
            y:380,
            size:145,
            div:10,
            minValue:90,
            maxValue:160,
            lowWarn:100,
            highWarn:150,
            lowWarnColor:yellow.pixel,
            highWarnColor:red.pixel,
            tickColor:darkGrey.pixel,
            unit:'v',
            label: "12v",
        });
        XGauge speed = XGauge(GaugeConfiguration{
            x:390,
            y:5,
            size:370,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:100,
            lowWarnColor:yellow.pixel,
            highWarnColor:yellow.pixel,
            tickColor:darkGrey.pixel,
            unit:'\0',
            label: "MPH",
        });

        XGauge *gauges[] = {
            &battTemp,
            &soc,
            &motor,
            &inverter,
            &dcwatts,
            &dcamps,
            &lowvolts,
            &speed,
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
                        for (float v = 0; v <= 1; v += 0.01) {
                            for (int i = 0; i < sizeof(gauges)/sizeof(gauges[0]); i++) {
                                gauges[i]->UpdateValue((int)(gauges[i]->cfg.maxValue-gauges[i]->cfg.minValue)*v + gauges[i]->cfg.minValue);
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
			int x=event.xbutton.x,
			    y=event.xbutton.y;

			//strcpy(text,"X is FUN!");
                        double relative = y/600.0;
                        for (int i = 0; i < sizeof(gauges)/sizeof(gauges[0]); i++) {
                            int value = (int)(gauges[i]->cfg.maxValue-gauges[i]->cfg.minValue)*relative + gauges[i]->cfg.minValue;
                            gauges[i]->UpdateValue(value);
                            gauges[i]->Draw();
                        }

		}
	}
    return 0;
}
