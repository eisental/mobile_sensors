#include "chart.h"

void Chart::drawTitle() {
  int16_t x1, y1;
  uint16_t w, h;

  _display->setTextSize(2);
  _display->setTextColor(SSD1306_INVERSE);
  _display->getTextBounds(String(_title), 0, 0, &x1, &y1, &w, &h);
  _display->setCursor(_display->width() - w, _display->height() - h);
  _display->println(_title);
}

void Chart::drawLine(uint8_t x, uint8_t height) {
  _display->drawFastVLine(x, _display->height() - height, height,
                          SSD1306_WHITE);
}

Chart::Chart(Adafruit_SSD1306 *display, char title[], int bufferSize,
             int maxHeight)
    : _bufferSize(bufferSize), _title(title), _maxHeight(maxHeight),
      _curIndex(0), _minValue(0), _maxValue(0), _firstUpdate(true),
      _inInfoMode(true) {
  _display = display;
  _buffer = new float[bufferSize];
  for (int i = 0; i < _bufferSize; i++) {
    _buffer[i] = -1;
  }
}

Chart::~Chart() { delete[] _buffer; }

void Chart::start() { setInfoMode(true); }

void Chart::draw() {
  uint8_t x = 0;
  for (int i = _curIndex; i < _bufferSize; ++i) {
    if (_buffer[i] >= 0) {
      int v = (int)((_buffer[i] - _minValue) / (_maxValue - _minValue) *
                    _maxHeight);
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

  if (_curIndex > 0) {
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_INVERSE);

    if (_inInfoMode) {
      // min - max
      _display->setCursor(2, 2);
      _display->print(_minValue);
      _display->print("-");
      _display->println(_maxValue);
    }

    // last
    _display->setCursor(2, 23);
    _display->println(_buffer[_curIndex - 1]);
  }

  unsigned long now = millis();
  if (_inInfoMode) {
    if (now - _infoModeStartTime < 2000) {
      drawTitle();
    } else if (now - _infoModeStartTime >= 10000) {
      _inInfoMode = false;
    }
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

void Chart::reset() {
  for (int i = 0; i < _bufferSize; ++i) {
    _buffer[i] = -1;
  }
  _curIndex = 0;
  _firstUpdate = true;
  _inInfoMode = true;
}

void Chart::setInfoMode(bool inInfoMode) {
  if (inInfoMode) {
    _infoModeStartTime = millis();
  } else {
    _infoModeStartTime = 0;
  }
  _inInfoMode = inInfoMode;
}

bool Chart::getInfoMode() { return _inInfoMode; }
