#include <unistd.h>
#include <stdlib.h>
#include <cstdio>
#include <math.h>

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
    //double tmp = (double)corner/((double)tickSpacing);
    snprintf(rangeText[0], 8, "%d",cfg.minValue/cfg.div);
    snprintf(rangeText[1], 8, "%d",(int)(nearbyint((double)(cfg.minValue + range/3)/(double)tickSpacing)*((double)tickSpacing))/cfg.div);
    snprintf(rangeText[2], 8, "%d",(int)(nearbyint((double)(cfg.minValue + range*2/3)/(double)tickSpacing)*((double)tickSpacing))/cfg.div);
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
