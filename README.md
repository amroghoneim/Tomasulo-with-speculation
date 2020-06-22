# Tomasulo-with-speculation
Tomasulo's algorithm with speculation implementation in C++

# Implementation

Tomasulo’s algorithm with speculation has five main stages where these stages are: fetch, issue, execute, writeback, commit. I implemented five main driver functions for each stage handling each aspect in this dual issue pipeline. The input for the whole program is received from an input file in a specific format. After that, I created a parsing function to get all the main components of the instructions, that is the opcode, the register numbers and the immediate values. I input these results to a vector of instructions in which I use to start the program itself. After getting the instructions from the file, I ask the user to provide the required data and its address to memory.

I also have a main driver function that combines all these functions, initializes all data types, where it initializes the data structures that represent the instruction buffer, reorder buffer, reservation stations , memory and register file. This function is also responsible for a cycle by cycle output of results where it shows all changes happening in the mentioned most data structures above to allow for a better analysis of the results. It then outputs the total number of clock cycles, IPC and branch prediction if there is any.

# User Guide
The program begins by asking the user for the text file name. The user must add the complete data path if the text file is not in the project folder.
Ex: “filname.txt” if included in project folder.
Ex: “Users:/project/filename.txt” if not included in project folder.

The input instructions must follow the following format:  

*The Opcode is lowercase*  

*There must be a space between the opcode and rd*  

*No spaces should be present in the rest of the instruction*  

The user is then asked to input the starting address of the program.  
The program then asks the user to  input the address and data respectively.  
Please note that the user must input -1 when the program asks for an address to stop receiving input.  
The program then executes!  
