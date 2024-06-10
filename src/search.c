#include <stdbool.h>
#include "search.h"
#include "threads.h"
#include "globals.h"
#include "tree.h"
#include "tables.h"
#include "types.h"
#include "util.h"

#ifdef DEBUG
#include <stdio.h>
#endif

volatile u8 is_searching; // Flag for if search loop is running

// Search Parameters
volatile u32 search_depth;
volatile u8 print_on_depth;

/*
* Starts the search threads
* Passed the max time and the max depth for the search
* Called from the IO Thread
*/
void startSearch(u32 time, u32 depth){
    is_searching = FALSE; // Set up new search
    print_on_depth = FALSE;
    clearKillerMoves();
    search_depth = depth;
    
    startSearchThreads(); // Launch Threads

    if(time){ // If a time has been set setup the timer
        #ifdef DEBUG
        printf("info string Search time is: %d\n", time);
        #endif
        while(!is_searching); // Wait for search to begin in at least one thread
        startTimerThread(time);
    }
    else if (depth != MAX_DEPTH){ // If we are in a depth based search we want to setup to print move 
        print_on_depth = TRUE;
    }
}

/*
 * Stops the search and prints the best move
 * Called from the IO Thread
 */
void stopSearch(){
    stopTimerThread();
    stopSearchThreads();
    is_searching = FALSE;
}

/*
 * Loop Function for Search Threads
 */
i32 searchLoop(){
    // Set up local thread info
    Move *pvArray = calloc((MAX_DEPTH*MAX_DEPTH + MAX_DEPTH)/2, sizeof(Move));

    Position searchPosition = get_global_position();

    // Begin Search
    is_searching = TRUE;

    u32 cur_depth = 1;
    i32 eval = 0, eval_prev = 0;

    while(run_get_best_move && cur_depth <= search_depth){
        SearchStats stats;
        eval = searchTree(searchPosition, cur_depth, pvArray, eval_prev, &stats);
        update_global_pv(cur_depth, pvArray, eval, stats);

        // Update for next iteration
        eval_prev = eval;
        cur_depth++;
    }

    if(cur_depth > search_depth && print_on_depth){ // Print best move in the case we reached max depth
        print_on_depth = FALSE;
        print_best_move = TRUE;
    }

    // Free Thread Data
    free(pvArray);
    return 0;
}