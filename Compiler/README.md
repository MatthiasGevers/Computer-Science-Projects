# Compiler Project

## Overview

This project is a compiler developed as part of a university course. The project was based on a given skeleton, and several components were implemented as part of the assignment. It includes key modules that handle scanning, symbol table management, code generation, and more.

## Features

The compiler includes the following features:
- **Lexical Analysis** (Scanner): Processes the input source code and breaks it down into tokens.
- **Symbol Table Management**: A hashtable is used to manage symbols, ensuring that variables and functions are tracked correctly.
- **Code Generation**: Produces the corresponding machine or intermediate code from the parsed input.
- **Error Handling**: Implements a robust mechanism for catching and reporting errors during compilation.

## Project Structure

The project consists of the following key files:

- `scanner.c`: This file is responsible for lexical analysis, converting the source code into tokens.
- `amplc.c`: The main entry point for the compiler, coordinating the compilation process.
- `codegen.c`: Handles the generation of machine or intermediate code based on the parsed input.
- `hashtable.c`: Implements a hashtable for efficient symbol table management.
- `symboltable.c`: Manages the storage and retrieval of symbols during compilation.
- `error.c`: Provides functions to handle errors encountered during compilation.
- `token.c`: Contains the definition and handling of tokens.
- `valtypes.c`: Manages value types and type checking during compilation.



## Contribution

Since this was a course project, the base skeleton was provided, and I primarily worked on the following files:

- `scanner.c`
- `amplc.c`
- `codegen.c`
- `hashtable.c`
- `symboltable.c`









