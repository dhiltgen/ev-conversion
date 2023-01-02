typedef struct {
    short int x;
    short int y;
    unsigned short int size;

    // Ranges - set div large enough to retain accuracy with integer math
    short int div; // Divide all values by this to get real unit currentValue 
    short int minValue;
    short int maxValue;
    short int lowWarn;
    short int highWarn;

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
    short int x1;
    short int y1;
    short int x2;
    short int y2;
    unsigned long color;
    short int currentValue;
    Line(
        short int x1,
        short int y1,
        short int x2,
        short int y2,
        unsigned long color,
        short int currentValue);

    virtual void Draw() = 0;
};

class points {
public:
    short int x[8];
    short int y[8];
    unsigned short int len;
};

class ValueMask {
public:
    float radians;
    points p;
    ValueMask() {radians = 0.0, p.len = 0;}
};

class BaseGauge {
public:
    BaseGauge(GaugeConfiguration cfg);
    void UpdateValue(short int currentValue);
    virtual void Draw() = 0;
    virtual Line* MakeLine(
        short int x1,
        short int y1,
        short int x2,
        short int y2,
        unsigned long color,
        short int currentValue) = 0;
    GaugeConfiguration cfg; // TODO - should be protected

protected:
    void init(); // Wrap up initialization, calls virtual functions in derived classes
    ValueMask getValueMask(float radians);

    // Current reading for the gauge
    short int currentValue;

    // General sizes and coordinates
    short int x; // Actual X after padding (upper left)
    short int y; // Actual Y after padding (upper left)
    unsigned short int size; // Actual size after padding (width and height)
    short int range; // Range of values for visual display on gauge (max - min)
    short int thickness; // How thick the gauge arc is
    short int pad; // Outer edge padding and (some internal padding)
    short int innerRadius;
    short int outerRadius;
    short int innerRadiusTick;
    short int outerRadiusTick; // TODO - maybe this just gets replaced with outerRadius

    // Font information
    short int targetValueFontHeight;
    short int targetLabelFontHeight;
    short int targetRangeFontHeight;
    char readingText[64];
    char labelText[64];
    char rangeText[4][8];
    short int rangeOrigin[4][2];
    short int maxRangeTextLengthOffset; 
    float startRadians = M_PI/180.0 * 225;
    float endRadians = M_PI/180.0 * -45;
    float currentReadingRadians;
    float lowWarnRadians;
    float highWarnRadians;


    // TODO - these go away once fully cleaned up to use circles and overlaying
    short int startAngle = 225*64; // TODO - fixup the X11 specific code to do the *64 math
    short int endAngle = -270*64; 
    short int innerX;
    short int innerY;
    unsigned short int innerSize;
    short int innerRadiusWarn;
    short int outerRadiusWarn;
    short int lowWarnAngle;
    short int highWarnAngle;
    short int currentValueAngle;

    Line *capLine;
    Line *lowWarnLine;
    Line *highWarnLine;
    Line **ticks;
    unsigned short int tickCount;

    // Mask Coordinates
    ValueMask currentReadingMask;
};
