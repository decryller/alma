### alma
a linux memory analyzer.\
alma provides:
* Memory reading and writing functions.
* A wildcard-supporting pattern scanning function.
* Easy memory page retrieval based on access permissions.

Requires root permissions and `/proc` to be mounted.\
The `rvmt` folder and its contents are necessary just for the example.
### Running the example
```
g++ -lX11 rvmt/rvmt.cpp alma.cpp example.cpp -o alma && sudo ./alma
```
Latest version: alma v1.1
### Contact
E-mail: decryller@gmail.com\
Discord: decryller
