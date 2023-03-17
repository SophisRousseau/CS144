Lab 2 Writeup
=============

My name: Zuo, Qikun

My Student number : 201830013

This lab took me about 6 hours to do. I did attend the lab session.

#### 1. Program Structure and Design:

In byte_stream.hh, I define a deque '_buf' to the byte stream which is unread but is assembled. Thus, in the function 'write' in byte_stream.cc, the bytes are pushed to the back of '_buf', and in the function 'read', the bytes are popped from the front of '_buf' after being read. Something which deserves special attention is that the total number of bytes being written to '_buf' can never exceed the value 'first_unaccepted - first_unassembled'. And the total number of bytes being read from '_buf' can never exceed the value 'first_unassembled - first_unread'.

In stream_reassembler.hh, I define an unordered map '_stream' which serves as the auxiliary storage to accommodate the unassembled bytes. And in the function 'push_substring' in stream_reassembler.cc, several different cases should be taken into consideration. 

The first case is that, when the 'index' of the substring equals to 'first_assembled', the reassembler needs to call the function 'write' of '_output' to write the valid part (not exceed 'first_unaccepted') of 'data' to '_buf'. And then the reassembler needs to visit the unordered map '_stream' to check whether there are other bytes which could be assembled and written to the '_buf'. That is, they need to form a consecutive sequence and follow the the valid part of 'data' which has been written to '_buf'. And finally, the consecutive sequence (if exists) should also be written to '_buf' and then be erased from '_stream'. 

The second case is that, when the 'index' of the substring is bigger than 'first_assembled', then the reassembler only need to insert each byte of the valid part (not exceed 'first_unaccepted') of the substring to '_stream'.

The final case is that, when the 'index' of the substring is smaller than 'first_assembled', there are two sub-cases needs to the be considered. When the index of the final byte of the substring is smaller than 'first_assembled', then the whole substring could be ignored. Otherwise, the valid part (exceed 'first_unassembled') of the substring should be taken into consideration (I use the recursion to implement this sub-case). 

The byte stream will end input when receiving 'eof' from the substring and no unassembled bytes are left in '_buf'.

#### 2. Implementation Challenges:

The biggest challenges are designing the data structure to accommodate the unassembled bytes and implementing the function 'push_substring' in stream_reassembler.cc. For the data structure, I first designed a string '_stream' of size '_capacity' and a bool vector '_valid' of size '_capacity' to implement this. The unassembled byte whose index is 'i' is stored at '_stream[i % _capacity]' and its corresponding valid byte '_valid[i % _capacity]' is set to 'true'. When the unassembled bytes are assembled and written to the byte stream, their valid bytes are set to 'false'. Thus it only costs O(1) to store each unassembled byte and check whether an unassembled byte exists according to the index 'i' provided, i.e, if '_valid[i % _capacity]' is true. This data structure makes the function 'push_substring' run fast but its shortcoming is that when '_capacity' is very large, the string and the vector will consume a large amount of storage space while there may exist only a small number of unassembled bytes. For the implementation of the function 'push_substring', because there are several different cases and edge conditions, it takes me quite a long time to consider all the cases in a right way. The picture presented in the tutorial helps me a lot in the implementation of the function.

#### 3. Remaining Bugs:
<img width = '300' height ='300' src ="../Screenshot%202022-10-20%20182023.png"/>
<img width = '300' height ='300' src ="../Screenshot%202022-10-20%20182102.png"/>

Until now, no bugs are found in the code submitted.