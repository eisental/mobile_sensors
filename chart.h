#ifndef CHART_H
#define CHART_H

#include <Adafruit_SSD1306.h>

class Chart {
private:
  int _bufferSize;
  char *_title;
  int _maxHeight;
  int _curIndex;
  float _minValue;
  float _maxValue;
  bool _firstUpdate;
  Adafruit_SSD1306 *_display;
  float *_buffer;
  bool _inInfoMode;
  unsigned long _infoModeStartTime;

  void drawLine(uint8_t x, uint8_t height);
  void drawTitle();

public:
  Chart(Adafruit_SSD1306 *display, char title[], int bufferSize, int maxHeight);
  ~Chart();

  void start();
  void draw();
  void updateChart(float newValue);
  void setInfoMode(bool newValue);
  bool getInfoMode();
  void reset();
};

#endif // CHART_H