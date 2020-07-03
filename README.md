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

