# s2n 

s2n is an implementation of the TLS/SSL protocols. It is designed for servers, and supports SSLv3 (disabled by default), TLS1.0, TLS1.1 and TLS1.2. s2n is released and licensed under the Apache License 2.0. 

s2n features include:

* **Systematic C safety**<br/>
s2n is written in C, but makes light use of standard C library functions and wraps all buffer handling and serialization in boundary-enforcing checks. 
* **Erase on read**<br/> s2n’s copies of decrypted data buffers are erased as they are read by the application. 
* **No locking overhead**<br/> There are no mutexes or locks in s2n. 
* **Small code base**<br/> Ignoring tests, blank lines and comments, s2n is about 3,000 lines of code. 
* **Minimalist feature adoption**<br/> s2n targets servers and aims to satisfy the common use cases, while avoiding little used features. Additionally; features with a history of triggering protocol-level vulnerabilities are not implemented. For example there is no support for session renegotiation or DTLS, and SSLv3 is disabled by default. 
* **Table based state-machines**<br/> s2n uses simple tables to drive the TLS/SSL state machines, making it difficult for invalid out-of-order states to arise. 
* **Built-in memory protection**<br/> On Linux; data buffers may not be swapped to disk or appear in core dumps.

s2n handles the protocol validation, state machine and buffer handling, while
encryption and decryption are handled by passing simple and opaque data “blobs”
to cryptographic libraries for processing.  

At present s2n uses OpenSSL’s libcrypto for the underlying cryptographic operations.
Cryptographic routines have been written in a modular way so that it is also
possible to use BoringSSL, LibreSSL or other cryptographic libraries for these
operations. 

s2n has support for the AES256-CBC, AES128-CBC, 3DES-CBC and RC4
ciphers, and the RSA and DHE-RSA (a form of perfect forward secrecy) key
exchange algorithms.  For more detail about s2n, see the [API Reference](https://github.com/awslabs/s2n/blob/master/docs/USAGE-GUIDE.md),
[Example server](https://github.com/awslabs/s2n/tree/master/bin), and [Backlog](https://github.com/awslabs/s2n/issues).

## Security and vulnerability notifications
If you discover a security vulnerability or issue in s2n we ask that you notify
it to AWS Security via our [vulnerability reporting page](http://aws.amazon.com/security/vulnerability-reporting/). 
