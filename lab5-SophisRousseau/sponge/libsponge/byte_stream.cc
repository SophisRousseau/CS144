#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity): _capacity(capacity) {}
// 从 first_unassembled 处开始写字节
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
// 从 _first_unread 处开始读数据，实则从 0 开始读 _buf 中的数据
string ByteStream::peek_output(const size_t len) const {
    string s;
    for(size_t i = 0; i < len && i < _buf.size(); i++) {
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
// _buf.size(), the reassembled bytes are pushed into _buf, and the read (by application) bytes are removed from _buf
// for sender, `_first_unread` represents bytes sent to the receiver, `_first_unassembled` represents bytes read from the application
// for receiver, `_first_unread` represents ordered bytes received from the sender, `_first_unread` represents bytes sent to the application
bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); } 
// for receiver: all the bytes sent from the sender are reassembled and read by the application; for sender: all the bytes are read from the application and sent to the receiver
// i.e., all the bytes are pushed into the stream and then popped out from the stream
size_t ByteStream::bytes_written() const { return _first_unassembled; }

size_t ByteStream::bytes_read() const { return _first_unread; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }