# AI Usage Documentation — Phase 1

## Tool Used
Claude (claude.ai) — used as an assistant for generating two specific functions
as required by the project specification.

## What I Asked the AI to Generate

### 1. `parse_condition`
I described my `Report` structure and the condition format (`field:operator:value`)
and asked the AI to generate a parsing function with the following signature:

```c
int parse_condition(const char *input, char *field, char *op, char *value);
```

**Prompt given:**
"I have a condition string in the format field:operator:value (e.g. severity>=2,
category=road). Write a C function that splits this string into its three
components: field, operator, and value."

### 2. `match_condition`
I described the fields and their types and asked the AI to generate a matching
function:

```c
int match_condition(Report *r, const char *field, const char *op, const char *value);
```

**Prompt given:**
"Given a Report struct with fields id (int), inspector (char[]), latitude and
longitude (double), category (char[]), severity (int), timestamp (time_t), and
description (char[]), write a function that checks if a report satisfies a
condition given as field, operator, and value strings."

## What Was Generated

The AI generated both functions. `parse_condition` searched for operators in the
correct order (>=, <=, !=, >, <, =) to avoid partial matches, then extracted the
field and value using pointer arithmetic. `match_condition` handled numeric fields
(severity, id) with `atoi()` and string fields (category, inspector) with `strcmp()`.

## What I Changed and Why

After reviewing the generated code line by line, I identified several issues:

1. **Operator order** — the initial version checked `>` before `>=`, which caused
`>=` to never match. I fixed the order so longer operators are checked first.

2. **Type conversion** — the generated `match_condition` did not convert the value
string to `time_t` for timestamp comparisons. I added the conversion manually.

3. **Error handling** — the generated `parse_condition` did not return an error code
when no valid operator was found. I added a `return -1` for that case.

4. **Permission checks** — the AI-generated code set `reports.dat` permissions to
`644` instead of the required `664`. I corrected this with `chmod(path, 0664)` and
verified using `stat()`.

## What I Learned

- How Unix permission bits are structured in `st_mode` and how to extract them
using bitwise AND with constants like `S_IRUSR`, `S_IWGRP` etc.
- How `lseek()` combined with `ftruncate()` can be used to remove a record from
a binary file by shifting subsequent records one position forward.
- How `lstat()` differs from `stat()` — `lstat()` inspects the symlink itself
while `stat()` follows it to the target, which is essential for detecting dangling
links.
- How `O_APPEND` makes each `write()` atomic at the end of the file, avoiding
race conditions in the operation log.
- The importance of reviewing AI-generated code carefully — the functions worked
in simple cases but had edge case bugs that required manual correction.

## Conclusion

The AI was useful for generating a working draft of the two required functions,
but the output required careful review and several corrections before it was
correct and complete. All other code in the project was written independently.