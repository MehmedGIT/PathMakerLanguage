# Basic path maker interpreter

Task: Path_maker is a basic scripting language for creating 
directory trees. This is a basic interpreter for Path_maker.(Rules of the language are below).

### Basic Commands:
#### go		<path_expression>;
#### make	<path_expression>;
#### delete 	<path_expression>;
	
### Control Structures:
#### if		<path_expression> command
#### ifnot 	<path_expression> command

Command could be any Basic Command or Block
	
### Blocks: Between {}, they can be nested
### Comments: Between [], they cannot be nested
	
## Note: 
Nested blocks are not fully functioning currently!
Also the delete <path_expression> should be fixed.

### Note 2
The code was developed on Windows with the aid of MinGW. So, there might be some portability problems running the code on Linux. One problem you are going to face is the "mkdir" function. On Windows the function takes single parameter - the path value, on Linux it takes two parameters - the path value and the mode. I will make it runnable on both OS in the future.
