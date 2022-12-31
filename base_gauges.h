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
