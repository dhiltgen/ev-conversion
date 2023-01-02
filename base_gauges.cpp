#include <unistd.h>
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include <math.h>
#include "base_gauges.h"

Line::Line(
        short int x1,
        short int y1,
        short int x2,
        short int y2,
        unsigned long color,
        short int currentValue){
    this->x1 = x1;
    this->y1 = y1;
    this->x2 = x2;
    this->y2 = y2;
    this->color = color;
    this->currentValue = currentValue;
}

short int getFontTarget(short int height) {
    if (height <= 16) {
        return 16;
    } else if (height <= 24) {
        return 24;
    } else if (height <= 32) {
        return 32;
    } else if (height <= 24*2) {
        return 24*2;
    } else if (height <= 32*2) {
        return 32*2;
    } else if (height <= 24*3) {
        return 24*3;
    }
    return 32*3;
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
    thickness = cfg.size / 7;
    pad = this->thickness / 4;
    x = cfg.x + this->pad;
    y = cfg.y + this->pad;
    size = cfg.size - (2*pad);
    innerX = x + this->thickness;
    innerY = y + this->thickness;
    innerSize = size - (2*thickness);
    innerRadius = size/2-thickness;
    outerRadius = size/2;
    innerRadiusWarn = size/2-thickness;
    outerRadiusWarn = size/2;
    innerRadiusTick = size/2-thickness/3;
    outerRadiusTick = size/2;

    this->range = cfg.maxValue - cfg.minValue;

    // Warnings to angle mappings
    lowWarnAngle = startAngle + endAngle * (cfg.lowWarn-cfg.minValue) / range;
    highWarnAngle = startAngle + endAngle * (cfg.highWarn-cfg.minValue) / range;

    targetValueFontHeight = getFontTarget(thickness * 1.2);
    targetLabelFontHeight = getFontTarget(thickness * 0.8);
    targetRangeFontHeight = getFontTarget(thickness * 0.8);

    // Calculate Masks
    endRadians = M_PI/180.0 * (startAngle+endAngle)/64;
    if (cfg.lowWarn != cfg.minValue) {
        lowWarnRadians = M_PI/180.0 * lowWarnAngle/64;
    }
    if (cfg.highWarn != cfg.maxValue) {
        highWarnRadians = M_PI/180.0 * highWarnAngle/64;
    }
    //printf("Start Radians: %f -- end radians: %f\n", startRadians, endRadians);
    // endMask = mask(endRadians, true);
    // startMask = mask(startRadians, false);
    // triangle t = startMask.getMask();


}

ValueMask BaseGauge::getValueMask(float radians) {
    ValueMask m = ValueMask();
    m.radians = radians;

    // TODO delete these notes...
    // return ValueMask(
    //     x + size/2 + (short int)((outerRadius+pad) * cos(radians)),
    //     y + size/2 - (short int)((outerRadius+pad) * sin(radians)),
    //     radians,
    //     x - pad,
    //     y - pad,
    //     size + pad*2,
    //     cw
    // );
    // this->x=x;
    // this->y=y;
    // this->radians=radians;
    // this->originX=originX;
    // this->originY=originY;
    // this->size=size;
    // this->cw=cw;
    //printf("%d,%d @ %f\n", x, y, radians);
    // t = triangle{
    //     x1: originX+size/2,
    //     y1: originY+size/2,
    //     x2: x,
    //     y2: y
    // };

    // First mask point set to center
    m.p.x[0] = cfg.x + cfg.size/2;
    m.p.y[0] = cfg.y + cfg.size/2;
    // Second point based on reading
    m.p.x[1] = x + size/2 + (short int)((outerRadius+pad) * cos(radians));
    m.p.y[1] = y + size/2 - (short int)((outerRadius+pad) * sin(radians));

    // Third through N based on what quadrant we're in
    short int quadrant = (short int)floorf(radians/(M_PI/4.0));
    m.p.len = 2;
    switch (quadrant){
        case 5: // ~7:00
        case 4: // ~8:00
            m.p.x[m.p.len] = cfg.x;
            m.p.y[m.p.len++] = cfg.y + cfg.size/2;
        case 3: // ~10:00
            m.p.x[m.p.len] = cfg.x;
            m.p.y[m.p.len++] = cfg.y;
        case 2: // ~11:00
            m.p.x[m.p.len] = cfg.x + cfg.size/2;
            m.p.y[m.p.len++] = cfg.y;
        case 1: // ~1:00
            m.p.x[m.p.len] = cfg.x + cfg.size;
            m.p.y[m.p.len++] = cfg.y;
        case 0: // ~2:00
            m.p.x[m.p.len] = cfg.x + cfg.size;
            m.p.y[m.p.len++] = cfg.y + cfg.size/2;
        case -1: // ~ 4:00 on the clock
            m.p.x[m.p.len] = cfg.x + cfg.size;
            m.p.y[m.p.len++] = cfg.y + cfg.size;
            break;
        default:
            printf("Unexpected radian quadrant: %d\n", quadrant);
    }
    // printf("Quad: %d Radians %f len %d\n", quadrant, radians, m.p.len);
    return m;
}

// triangle ValueMask::getMask() {
//     printf("Mask: radians: %f --  %d,%d:%d,%d:%d,%d\n", radians, t.x1,t.y1,t.x2,t.y2,t.x3,t.y3);
//     return t;
// }

void BaseGauge::init() {
    // Precalculate the cartesian cords for the various overlay lines
    // to save on cpu cycles when updating
    capLine = MakeLine(
        x + size/2 + (short int)(innerRadius * cos(endRadians)),
        y + size/2 - (short int)(innerRadius * sin(endRadians)),
        x + size/2 + (short int)(outerRadius * cos(endRadians)),
        y + size/2 - (short int)(outerRadius * sin(endRadians)),
        cfg.tickColor,
        cfg.maxValue
    );
    if (cfg.lowWarn != cfg.minValue) {
        lowWarnLine = MakeLine(
            x + size/2 + (short int)(innerRadiusWarn * cos(lowWarnRadians)),
            y + size/2 - (short int)(innerRadiusWarn * sin(lowWarnRadians)),
            x + size/2 + (short int)(outerRadiusWarn * cos(lowWarnRadians)),
            y + size/2 - (short int)(outerRadiusWarn * sin(lowWarnRadians)),
            cfg.lowWarnColor,
            cfg.lowWarn
        );
    }
    if (cfg.highWarn != cfg.maxValue) {
        highWarnLine = MakeLine(
            x + size/2 + (short int)(innerRadiusWarn * cos(highWarnRadians)),
            y + size/2 - (short int)(innerRadiusWarn * sin(highWarnRadians)),
            x + size/2 + (short int)(outerRadiusWarn * cos(highWarnRadians)),
            y + size/2 - (short int)(outerRadiusWarn * sin(highWarnRadians)),
            cfg.highWarnColor,
            cfg.highWarn
        );
    }

    // Tick marks
    // TODO - is there a more clever way to break down the tick spacing?
    short int tickSpacing = 0;
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
    for (short int i = 0; i < tickCount; i++) {
        short int val = cfg.minValue + (i+1) * tickSpacing;
        short int tickAngle = startAngle + endAngle * (val-cfg.minValue)/range;
        //printf("Tick %d = %d @ %d\n", i, val, tickAngle/64);
        float tickRadians = M_PI/180.0 * tickAngle/64;
        ticks[i] = MakeLine(
            x + size/2 + (short int)(innerRadiusTick * cos(tickRadians)),
            y + size/2 - (short int)(innerRadiusTick * sin(tickRadians)),
            x + size/2 + (short int)(outerRadiusTick * cos(tickRadians)),
            y + size/2 - (short int)(outerRadiusTick * sin(tickRadians)),
            cfg.tickColor,
            val
        );
    }

    // Range Text   
    //double tmp = (double)corner/((double)tickSpacing);
    snprintf(rangeText[0], 8, "%d",cfg.minValue/cfg.div);
    snprintf(rangeText[1], 8, "%d",(short int)(nearbyint((float)(cfg.minValue + range/3)/(float)tickSpacing)*((float)tickSpacing))/cfg.div);
    snprintf(rangeText[2], 8, "%d",(short int)(nearbyint((float)(cfg.minValue + range*2/3)/(float)tickSpacing)*((float)tickSpacing))/cfg.div);
    snprintf(rangeText[3], 8, "%d",cfg.maxValue/cfg.div);
    maxRangeTextLengthOffset = 0;
    for (short int i = 1; i < 4; i++) {
        if (strlen(rangeText[i]) > strlen(rangeText[maxRangeTextLengthOffset])) {
            maxRangeTextLengthOffset = i;
        }
    }
}

void BaseGauge::UpdateValue(short int value) {
    //printf("%s new currentValue %d\n", cfg.label, currentValue);
    this->currentValue = value;
    if (cfg.div == 10) {
        snprintf(readingText, 64, "%d.%d%c",currentValue/cfg.div,currentValue%10, cfg.unit);
    } else {
        snprintf(readingText, 64, "%d%c",currentValue/cfg.div, cfg.unit);
    }
    // Determine angle of current reading relative to start
    currentValueAngle = 32; // Render at least 1/2 degree
    if (currentValue > cfg.maxValue) {
        currentValueAngle = endAngle;
    } else if (currentValue > cfg.minValue) {
        currentValueAngle = endAngle * (currentValue-cfg.minValue)/range;
    }
    // TODO this can be simplified once the angle usage is cleaned up
    short int tmp = startAngle + endAngle * (value-cfg.minValue)/range;
    currentReadingRadians = M_PI/180.0 * tmp/64;

    currentReadingMask = getValueMask(currentReadingRadians);
    //printf("Reading Radians: %f\n", currentReadingRadians);
    // TODO - more to do here
    //printf("Current Reading Mask: %f\n", floorf(currentReadingRadians/(M_PI/4.0)));

}
