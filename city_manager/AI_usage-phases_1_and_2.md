# AI Usage - Phase 1

## Tool used
Claude (claude.ai)

## What I used it for

The project asked us to use AI to generate two functions for the filter command:
- parse_condition - splits "severity>=2" into field, operator and value
- match_condition - checks if a report matches a condition

I described my Report struct and what the functions should do, and Claude
generated both of them.

## What I had to fix

After reading through the code I found a few problems:

- the operator check was wrong - it checked `>` before >= so >= never worked,
I fixed the order
- timestamp comparison wasn't implemented, I added it manually
- parse_condition didn't handle the case when no operator is found, added
return -1
- permissions on reports.dat were set to 644 instead of 664, fixed that too

## What I learned

- how permission bits work in st_mode and how to check them with bitwise AND
- how lseek() and ftruncate() work to delete a record from a binary file
- difference between stat() and lstat() when working with symlinks
- that AI code isn't always correct and you need to actually read and test it

## Phase 2

### What I used AI for
I used AI as an assistant to understand how signals and processes work in Unix, and to help structure the implementation of `monitor_reports.c` and the modifications to `add_report`.