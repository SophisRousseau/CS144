Lab 1 Writeup
=============

My name: Zuo, Qikun

My Student number : 201830013

This lab took me about 3 hours to do. I did attend the lab session.

My secret code from section 2.1 was: ALOHA, NJUer! Welcome to computer networking!

#### 1. Program Structure and Design:
1.use connect() to build connection between my computer and the server.
2.use write() to send the http request to the server.
3.use read() to receive the response, i.e. a web page, from the server.

To implement it:
First, I constructed an object of the class Address, using one of the constructors of the class to include the information of the name of the server and the "http" service. Then I constructed an object of the class TCPSocket, and used the function connect() of the class to connect to the server.
Second, I referred to the steps in the Section 2.1 to put all the texts, i.e. http request, into a string, and used the function write() to send it to the server.
Last, I used the function read() to receive the response which the server sends back, and used the function eof() to guarantee that the socket has read the entire byte stream coming from the server.

#### 2. Implementation Challenges:
First, I spent a lot of time on configuring the experimental environment. 
Second, because of the unfamiliarity with the classes in the headers, I spent much of my time thinking about which classes should be used in the experiment until I saw the hint in the tutorial which says 'Use the TCPSocket and Address classes'.
Last, when I put the http request into the string, I forgot to add another '\r\n' to the end of the string. Thus, it is obviously that the server could not send back any response to me. Then I began to suspect that whether I had used the wrong functions of the class TCPSocket, and replaced them with other functions, such as replace the function connect() with bind(). But the effort did not work. After hours of various sorts of trials, when I scrutinized the Section 2.1, I realized that I did not send an empty line to the server to tell it that I had finish my http request, and that was the problem. Finally, the bug was solved.


#### 3. Remaining Bugs:
![Screenshot 2022-09-28 051028](../Screenshot%202022-09-28%20051028.png)
Until now, no new bugs in the code are found.