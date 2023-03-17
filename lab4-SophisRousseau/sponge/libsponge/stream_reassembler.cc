#include "stream_reassembler.hh"
#include<iostream>
// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them size_to the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t first_unassembled = _output.bytes_written();
    size_t first_unaccepted = _output.bytes_read() + _capacity;

    if(index == first_unassembled) {
        size_t len = _output.write(data);

        for(size_t i = first_unassembled; i < first_unassembled + len; i++) {
            _stream.erase(i);
        }
        
        string s;
        for(size_t i = first_unassembled + len; i < first_unaccepted && _stream.find(i) != _stream.end(); i++) {
            s += _stream[i];
            _stream.erase(i);
        }
        _output.write(s);
    }
    else if(index > first_unassembled) {
        for(size_t i = index; i < index + data.size() && i < first_unaccepted; i++) {
            _stream[i] = data[i - index];
        }
    }
    else if(index + data.size() > first_unassembled) {
        return push_substring(data.substr(first_unassembled - index),  first_unassembled, eof);
    }

    if(eof) {
        _eof = true;
        _eof_index = index + data.size();
    }
    if(_eof && _output.bytes_written() == _eof_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _stream.size();
}

bool StreamReassembler::empty() const { 
    return unassembled_bytes() == 0;
}