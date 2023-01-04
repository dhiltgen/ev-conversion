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
        unsigned int color,
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
    innerRadius = size/2-thickness;
    outerRadius = size/2;
    innerRadiusTick = size/2-thickness/3;
    outerRadiusTick = size/2;

    this->range = cfg.maxValue - cfg.minValue;

    targetValueFontHeight = getFontTarget(thickness * 1.2);
    targetLabelFontHeight = getFontTarget(thickness * 0.8);
    targetRangeFontHeight = getFontTarget(thickness * 0.8);

    // Calculate Masks
    if (cfg.lowWarn != cfg.minValue) {
        lowWarnRadians = startRadians+(endRadians-startRadians)*(float)(cfg.lowWarn-cfg.minValue)/range;

        printf("%f %f\n", startRadians - endRadians, (float)(cfg.lowWarn-cfg.minValue)/range);
    }
    if (cfg.highWarn != cfg.maxValue) {
        highWarnRadians = startRadians+(endRadians-startRadians)*(float)(cfg.highWarn-cfg.minValue)/range;
    }
}

ValueMask BaseGauge::getValueMask(float radians) {
    ValueMask m = ValueMask();
    m.radians = radians;

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
            x + size/2 + (short int)(innerRadius * cos(lowWarnRadians)),
            y + size/2 - (short int)(innerRadius * sin(lowWarnRadians)),
            x + size/2 + (short int)(outerRadius * cos(lowWarnRadians)),
            y + size/2 - (short int)(outerRadius * sin(lowWarnRadians)),
            cfg.lowWarnColor,
            cfg.lowWarn
        );
    }
    if (cfg.highWarn != cfg.maxValue) {
        highWarnLine = MakeLine(
            x + size/2 + (short int)(innerRadius * cos(highWarnRadians)),
            y + size/2 - (short int)(innerRadius * sin(highWarnRadians)),
            x + size/2 + (short int)(outerRadius * cos(highWarnRadians)),
            y + size/2 - (short int)(outerRadius * sin(highWarnRadians)),
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
        float tickRadians = startRadians+(endRadians-startRadians)*(float)(val-cfg.minValue)/range;
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
    currentReadingRadians = startRadians+(endRadians-startRadians)*(float)(value-cfg.minValue)/range;
    currentReadingMask = getValueMask(currentReadingRadians);
}

GaugeConfiguration* BaseGauge::GetFullConfiguration(int *len) {
    GaugeConfiguration *cfgs = new GaugeConfiguration[8];
    int i = 0;
    cfgs[i++] = GaugeConfiguration{
            x:0,
            y:0,
            size:170,
            minValue:10,
            maxValue:100,
            lowWarn:20,
            highWarn:70,
            lowWarnColor:BLUE,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'c',
            label: "temp",
        };

    cfgs[i++] = GaugeConfiguration{
            x:190,
            y:0,
            size:170,
            minValue:0,
            maxValue:100,
            lowWarn:10,
            highWarn:95,
            lowWarnColor:RED,
            highWarnColor:YELLOW,
            tickColor:DARK_GREY,
            unit:'%',
            label: "SOC",
        };

    cfgs[i++] = GaugeConfiguration{
            x:0,
            y:170,
            size:170,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:BLUE,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'c',
            label: "motor",
        };
    cfgs[i++] = GaugeConfiguration{
            x:190,
            y:170,
            size:170,
            minValue:0,
            maxValue:100,
            lowWarn:20,
            highWarn:60,
            lowWarnColor:BLUE,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'c',
            label: "inverter",
        };

    cfgs[i++] = GaugeConfiguration{
            x:0,
            y:340,
            size:145,
            minValue:0,
            maxValue:1000,
            lowWarn:0,
            highWarn:800,
            lowWarnColor:YELLOW,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'w',
            label: "DC Watts",
        };
    cfgs[i++] = GaugeConfiguration{
            x:160,
            y:340,
            size:145,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:80,
            lowWarnColor:YELLOW,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'a',
            label: "DC Amps",
        };
    cfgs[i++] = GaugeConfiguration{
            x:320,
            y:340,
            size:145,
            div:10,
            minValue:90,
            maxValue:160,
            lowWarn:100,
            highWarn:150,
            lowWarnColor:YELLOW,
            highWarnColor:RED,
            tickColor:DARK_GREY,
            unit:'v',
            label: "12v",
        };
    cfgs[i++] = GaugeConfiguration{
            x:410,
            y:0,
            size:340,
            minValue:0,
            maxValue:100,
            lowWarn:0,
            highWarn:100,
            lowWarnColor:YELLOW,
            highWarnColor:YELLOW,
            tickColor:DARK_GREY,
            unit:'\0',
            label: "MPH",
        };
    *len = i;
    return cfgs;
}
