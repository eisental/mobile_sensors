#include "chart.h"

void Chart::drawLine(uint8_t x, uint8_t height) {
  _display->drawFastVLine(x, _display->height() - height, height,
                          SSD1306_WHITE);
}

Chart::Chart(Adafruit_SSD1306 *display, int bufferSize, int maxHeight)
    : _bufferSize(bufferSize), _curIndex(0), _maxHeight(maxHeight),
      _minValue(0), _maxValue(0), _firstUpdate(true) {
  _display = display;
  _buffer = new float[bufferSize];
  for (int i = 0; i < _bufferSize; i++) {
    _buffer[i] = -1;
  }
}

Chart::~Chart() { delete[] _buffer; }

void Chart::draw() {
  uint8_t x = 0;
  for (int i = _curIndex; i < _bufferSize; ++i) {
    if (_buffer[i] >= 0) {
      int v = (int)((_buffer[i] - _minValue) / (_maxValue - _minValue) *
                    _maxHeight);

      // int v = map(_buffer[i], _minValue, _maxValue, 0, _maxHeight);
      drawLine(x, v);
    }

    x += 1;
  }

  for (int i = 0; i < _curIndex; ++i) {
    if (_buffer[i] >= 0) {
      int v = (int)((_buffer[i] - _minValue) / (_maxValue - _minValue) *
                    _maxHeight);
      drawLine(x, v);
    }

    x += 1;
  }

  _display->setTextSize(1);
  _display->setTextColor(SSD1306_INVERSE);
  _display->setCursor(2, 23);
  if (_curIndex > 0) {
    _display->println(_buffer[_curIndex - 1]);
  }
}

void Chart::updateChart(float newValue) {
  if (newValue < 0) {
    return;
  }

  // Keep a symmetric distance from the bounds to center initial chart
  if (_firstUpdate) {
    _firstUpdate = false;
    _minValue = newValue > 0 ? newValue - 1 : 0;
    _maxValue = newValue + 1;
  }

  if (newValue < _minValue) {
    _minValue = newValue;
  }
  if (newValue > _maxValue) {
    _maxValue = newValue;
  }

  _buffer[_curIndex] = newValue;
  _curIndex = (_curIndex + 1) % _bufferSize;
}
