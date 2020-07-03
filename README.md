# Basic path maker interpreter


Task: Path_maker is a basic scripting language for creating 
directory trees. (Rules of the language are below). This is 
a basic interpreter for Path_maker.

Basic Commands:
go		<path_expression>;
make		<path_expression>;
delete 	<path_expression>;
	
Control Structures:
if		<path_expression> command
ifnot 	<path_expression> command

command could be any Basic Command or Block
	
Blocks: Between {}, they can be nested
Comments: Between [], they cannot be nested
	
Note: Nested blocks are not fully functioning!

