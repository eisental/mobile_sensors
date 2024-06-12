#ifndef CHART_H
#define CHART_H

#include <Adafruit_SSD1306.h>

class Chart {
private:
  int _bufferSize;
  int _maxHeight;
  int _curIndex;
  float _minValue;
  float _maxValue;
  bool _firstUpdate;
  Adafruit_SSD1306 *_display;
  float *_buffer;

  void drawLine(uint8_t x, uint8_t height);

public:
  Chart(Adafruit_SSD1306 *display, int bufferSize, int maxHeight);
  ~Chart();

  void draw();
  void updateChart(float newValue);
};

#endif // CHART_H