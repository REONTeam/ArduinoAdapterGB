BGB Mobile Adapter GB
=====================

This is one of the software implementations of libmobile, interfacing with the [BGB](http://bgb.bircd.org/) emulator.  
This is very useful for quick testing when faced with the lack of a flashcart.


How to use
----------

On linux/mac, all you need is `make` and `gcc` or `clang`, then run `make` to build the program.  
On windows, you will need [msys2](https://www.msys2.org/) and the mingw compilers shipped with that, as well as the `make` program. Once installed, run `./make-mingw`.

Once you have built the program, open up the BGB emulator, select "Link-\>Listen" in its menu, and then fire up the program by running `./mobile`.
