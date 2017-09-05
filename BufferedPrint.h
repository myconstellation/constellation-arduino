/**************************************************************************/
/*! 
    @file     BufferedPrint.h
    @author   Benoît Blanchon (https://github.com/bblanchon/ArduinoJson)

    @section LICENSE

    Copyright Benoit Blanchon 2014-2016
    MIT License
    
    Arduino JSON library
    https://github.com/bblanchon/ArduinoJson
*/
/**************************************************************************/

#ifndef _BUFFERED_PRINT_
#define _BUFFERED_PRINT_

#include <Client.h>

template <size_t CAPACITY>
class BufferedPrint : public Print {
  public:
    BufferedPrint(Print& destination) : _destination(destination), _size(0) {}

    ~BufferedPrint() { flush(); }

    virtual size_t write(uint8_t c) {
        _buffer[_size++] = c;

        if (_size + 1 == CAPACITY) {
            flush();
        }

        return 1;
    }

    void flush() {
        _buffer[_size] = '\0';
        _destination.print(_buffer);
        _size = 0;
        if(_debug && Serial) {
            Serial.print(_buffer);
        }
    }

    void setDebug(bool debug) {
        _debug = debug;
    }

  private:
    Print& _destination;
    size_t _size;
    char _buffer[CAPACITY];
    bool _debug;
};

#endif
