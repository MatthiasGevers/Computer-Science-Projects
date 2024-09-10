# **Gomoku with Minimax and Alpha-Beta Pruning**

This C program is a parallel implementation of a **Gomoku (Connect 5)** game using the **Minimax algorithm with Alpha-Beta pruning**. It leverages **MPI (Message Passing Interface)** for parallelism, allowing multiple processes to evaluate potential game moves and improve the decision-making.

## **How it Works**

- **Master Process**:  
  Handles the main game loop, assigns tasks to worker processes, and decides the best move.

- **Worker Processes**:  
  Evaluate possible game states using the minimax algorithm and return the best move to the master process.

- **Board Evaluation**:  
  Moves are evaluated based on their ability to contribute to a line of 5 consecutive pieces.
