Overload overview

The overload directory allows the project to include local copies of system headers or source files. To use
this feature, the file to be overloaded should be placed in this folder under a directory structure that
matches the file's location relative to the NNDK install directory.

Example:

To overload the predef.h header, create the following folder structure in the project:

Project Root
	-> overload
		-> nbrtos
			-> include
				-> predef.h
				
After adding the file to be overloaded, clean and rebuild the NetBurner Archive by selecting "Clean NetBurner
Archive" under the Build Targets pull-down in the project. From that point on, modifying predef.h (or any
overloaded source/header) will automatically rebuild the necessary files in the NetBurner Archive when
then project builds.

If a system include folder is overloaded, this folder should be added to your project include paths. Right click
on the project and select project properties. Under C/C++ Build->Settings, select GNU C++ Compiler->Includes and
add the overload include folder. If utilizing C code, then GNU C Compiler->Includes should also be added. In the
above example, add overload/nbrtos/include to the C/C++ Build settings. This will allow the NBEclipse error parser
 to find the changes made to predef.h
