/*
 * This file is part of Connect4 Game Solver <http://connect4.gamesolver.org>
 * Copyright (C) 2007 Pascal Pons <contact@gamesolver.org>
 *
 * Connect4 Game Solver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Connect4 Game Solver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Connect4 Game Solver. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include "position.hpp"
#include "TranspositionTable.hpp"
#include "MoveSorter.hpp"

using namespace GameSolver::Connect4;

namespace GameSolver { namespace Connect4 {


  // log2(1) = 0; log2(2) = 1; log2(3) = 1; log2(4) = 2; log2(8) = 3
  constexpr unsigned int log2(unsigned int n) 
  {
    return n <= 1 ? 0 : log2(n/2)+1;
  }

  /*
   * A class to solve Connect 4 position using Negamax variant of alpha-beta algorithm.
   */
  class Solver {
  private:
    unsigned long long nodeCount; // counter of explored nodes.

    int columnOrder[Position::WIDTH]; // column exploration order

    TranspositionTable<Position::WIDTH*(Position::HEIGHT+1),
                      log2(Position::MAX_SCORE - Position::MIN_SCORE + 1) + 1,
                      23> transTable;

    /**
     * Reccursively score connect 4 position using negamax variant of alpha-beta algorithm.
     * @param: position to evaluate, this function assumes nobody already won and 
     *         current player cannot win next move. This has to be checked before
     * @param: alpha < beta, a score window within which we are evaluating the position.
     *
     * @return the exact score, an upper or lower bound score depending of the case:
     * - if actual score of position <= alpha then actual score <= return value <= alpha
     * - if actual score of position >= beta then beta <= return value <= actual score
     * - if alpha <= actual score <= beta then return value = actual score
     */
    int negamax(const Position &P, int alpha, int beta) {
      assert(alpha < beta);
      assert(!P.canWinNext());

      nodeCount++; // increment counter of explored nodes

      uint64_t next = P.possibleNonLosingMoves();
      if(next == 0)     // if no possible non losing move, opponent wins next move
        return -(Position::WIDTH*Position::HEIGHT - P.nbMoves())/2;

      if(P.nbMoves() >= Position::WIDTH*Position::HEIGHT - 2) // check for draw game
        return 0; 

      int min = -(Position::WIDTH*Position::HEIGHT-2 - P.nbMoves())/2;	// lower bound of score as opponent cannot win next move
      if(alpha < min) {
        alpha = min;                     // there is no need to keep beta above our max possible score.
        if(alpha >= beta) return alpha;  // prune the exploration if the [alpha;beta] window is empty.
      }

      int max = (Position::WIDTH*Position::HEIGHT-1 - P.nbMoves())/2;	// upper bound of our score as we cannot win immediately
      if(int val = transTable.get(P.key()))
        max = val + Position::MIN_SCORE - 1;

      if(beta > max) {
        beta = max;                     // there is no need to keep beta above our max possible score.
        if(alpha >= beta) return beta;  // prune the exploration if the [alpha;beta] window is empty.
      }


      MoveSorter moves;

      for(int i = Position::WIDTH; i--; )
         if(uint64_t move = next & Position::column_mask(columnOrder[i]))
           moves.add(move, P.moveScore(move));

      while(uint64_t next = moves.getNext()) {
          Position P2(P);
          P2.play(next);  // It's opponent turn in P2 position after current player plays x column.
          int score = -negamax(P2, -beta, -alpha); // explore opponent's score within [-beta;-alpha] windows:
          // no need to have good precision for score better than beta (opponent's score worse than -beta)
          // no need to check for score worse than alpha (opponent's score worse better than -alpha)

          if(score >= beta) return score;  // prune the exploration if we find a possible move better than what we were looking for.
          if(score > alpha) alpha = score; // reduce the [alpha;beta] window for next exploration, as we only 
          // need to search for a position that is better than the best so far.
        }

      transTable.put(P.key(), alpha - Position::MIN_SCORE + 1); // save the upper bound of the position
      return alpha;
    }

  public:

    int solve(const Position &P, bool weak = false) 
    {
      if(P.canWinNext()) // check if win in one move as the Negamax function does not support this case.
        return (Position::WIDTH*Position::HEIGHT+1 - P.nbMoves())/2;
      int min = -(Position::WIDTH*Position::HEIGHT - P.nbMoves())/2;
      int max = (Position::WIDTH*Position::HEIGHT+1 - P.nbMoves())/2;
      if(weak) {
        min = -1;
        max = 1;
      }

      while(min < max) {                    // iteratively narrow the min-max exploration window
        int med = min + (max - min)/2;
        if(med <= 0 && min/2 < med) med = min/2;
        else if(med >= 0 && max/2 > med) med = max/2;
        int r = negamax(P, med, med + 1);   // use a null depth window to know if the actual score is greater or smaller than med
        if(r <= med) max = r;
        else min = r;
      }
      return min;
    }

    unsigned long long getNodeCount() 
    {
      return nodeCount;
    }

    void reset() 
    {
      nodeCount = 0;
      transTable.reset();
    }

    // Constructor
    Solver() : nodeCount{0} {
      reset();
      for(int i = 0; i < Position::WIDTH; i++)
        columnOrder[i] = Position::WIDTH/2 + (1-2*(i%2))*(i+1)/2;   
      // initialize the column exploration order, starting with center columns
      // example for WIDTH=7: columnOrder = {3, 4, 2, 5, 1, 6, 0}
    }

  };
}} // namespace GameSolver::Connect4


/*
 * Get micro-second precision timestamp
 * uses unix gettimeofday function
 */
#include <sys/time.h>
unsigned long long getTimeMicrosec() {
  timeval NOW;
  gettimeofday(&NOW, NULL);
  return NOW.tv_sec*1000000LL + NOW.tv_usec;    
}

/*
 * Main function.
 * Reads Connect 4 positions, line by line, from standard input 
 * and writes one line per position to standard output containing:
 *  - score of the position
 *  - number of nodes explored
 *  - time spent in microsecond to solve the position.
 *
 *  Any invalid position (invalid sequence of move, or already won game) 
 *  will generate an error message to standard error and an empty line to standard output.
 */
#include <iostream>
int main(int argc, char** argv) {

  Solver solver;

  bool weak = false;
  if(argc > 1 && argv[1][0] == '-' && argv[1][1] == 'w') weak = true;

  std::string line;

  for(int l = 1; std::getline(std::cin, line); l++) {
    Position P;
    if(P.play(line) != line.size())
    {
      std::cerr << "Line " << l << ": Invalid move " << (P.nbMoves()+1) << " \"" << line << "\"" << std::endl;
    }
    else
    {
      solver.reset();
      unsigned long long start_time = getTimeMicrosec();
      int score = solver.solve(P, weak);
      unsigned long long end_time = getTimeMicrosec();
      std::cout << line << " " << score << " " << solver.getNodeCount() << " " << (end_time - start_time);
    }
    std::cout << std::endl;
  }
}


