# edsm
An extendable distributed shared memory system implentation

#### Compilation
In the main EDSM directory:
```
cmake .
make
```
This will create a ```build``` directory with two subdirectories, ```bin``` and ```lib```. 
Bin contains the ```edsmd``` executable which will be the main point of entry for DSM programs. 
Lib contains the shared objects that contain EDSM tasks. These tasks are linked in at runtime and the 
```up_call``` method is used as an entry point.

#### Running EDSM
```
./edsmd 5555 ../lib/ 
```
Starts edsmd and dynamically links in all of the task shared objects in ../lib
```
./edsmd 5555 ../lib/ 192.168.1.3 5555
```
Starts edsmd and joins it to the group that the peer at 192.168.1.3 is currently in. 
This is how you would join additional peers to the initial peer started using the first command above.
#### Creating a task
