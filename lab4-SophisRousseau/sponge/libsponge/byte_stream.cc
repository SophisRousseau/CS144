#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity): _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    int len = 0; 
    for(auto& e: data) {
        if(_first_unassembled < _first_unread + _capacity){
            _buf.push_back(e);
            _first_unassembled++;
            len++;
        }
        else {
            break;
        }
    }
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string s;
    for(size_t i = 0; i < len && i < _first_unassembled - _first_unread; i++) {
        s += _buf[i];
    }
    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    for(size_t i = 0; i < len; i++) {
        _buf.pop_front();
    }
    _first_unread += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string s = peek_output(len);
    pop_output(s.size());
    return s;
}

void ByteStream::end_input() {
    _input_ended = true;
}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _first_unassembled - _first_unread; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _first_unassembled; }

size_t ByteStream::bytes_read() const { return _first_unread; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }