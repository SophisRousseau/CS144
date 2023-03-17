#include "wrapping_integers.hh"
#include<iostream>
// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2_2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn + n;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t d1 = (isn + checkpoint) - isn;
    uint32_t d2 = n - isn;
    if(d1 < d2) {
       uint32_t d = d2 - d1;
       if(d <= (1ul << 31) || checkpoint + d < (1ull << 32)) {
            return checkpoint + d;
       }
       else {
            return checkpoint + d - (1ull << 32);
       }
    }
    else {
        uint32_t d = d1 - d2;
        if(d <= (1ul << 31) || checkpoint - d >= uint64_t(-(1ll << 32))) {
            return checkpoint - d;
        }
        else {
            return checkpoint - d + (1ull << 32);
        }
    }
}