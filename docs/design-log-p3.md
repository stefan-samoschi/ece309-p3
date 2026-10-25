# Project 3 Design Log
**Author:** Stefan · **P2 code used:** instructor reference  
**Tools used:** VS Code file tools, CMake, WinLibs MinGW GCC for compilation validation, and the project spec as the design reference.

## 1. Hash table
I chose separate chaining with a power-of-two array of buckets and FNV-1a hashing. Separate chaining is simpler and safer than open addressing for this project because we never remove entries, the bucket lists stay short, and the code is easier to reason about during resize. A collision in one bucket just adds another `std::unique_ptr<Tool>` to that list; lookup stays effectively O(1) on average, and the longest chain stays small in practice.

My hash function is FNV-1a over the full string, with the final index computed as `hash(key) & (bucket_count - 1)`. This uses every character of the key and keeps the hash order-sensitive, so `"ab"` and `"ba"` hash differently. I used a load threshold of 0.75 and resized by doubling the bucket count before insertion when the next insert would exceed that threshold. This keeps the table sparse enough to avoid long chains while still avoiding excessive rehashes. The policy is amortized O(1): the occasional resize is spread across many cheap insertions.

For the 10,000-tool stress test, I measured: `bucket_count = 16384`, `load_factor = 0.610`, `max_probe_length = 6`. That is well below the project bound of 64. A weak hash like `s[0] + s.size()` would create much worse clustering and a much longer longest chain, which is exactly the failure mode the spec is trying to prevent.

## 2. Pointer stability
Pointer stability matters because the dispatcher obtains a `Tool*` from `find()` and then calls `invoke()` on it right away. If the registry rehashed and moved the underlying data without preserving the object identity, the pointer would become dangling.

I avoided that by storing `std::unique_ptr<Tool>` in each bucket. During a resize, I allocate a new bucket array, move each `unique_ptr` into the correct bucket in the new table, and discard the old array. The `Tool` object itself is never copied or moved out of its heap allocation; only the ownership pointer moves. That means the `Tool*` returned by `find()` stays valid for the registry’s lifetime, even after several later `add()` calls trigger rehashes.

## 3. Calculator trace
For the expression `2 * (3 + 4) - 5`, the tokenizer produces a well-formed sequence: `[2, *, (, 3, +, 4, ), -, 5]`.

- Start with `values = []`, `ops = []`.
- Read `2`: push `2` onto `values`.
- Read `*`: because `*` has higher precedence than the next operator `-`, it is pushed onto `ops`.
- Read `(`: push `(` onto `ops`.
- Read `3`: push `3`.
- Read `+`: push `+` onto `ops`.
- Read `4`: push `4`.
- Read `)`: apply operators until `(`. Apply `+` to `3` and `4`, producing `7`; pop `(`.
- Read `-`: `*` is still on top of `ops` and has higher precedence, so apply `*` to `2` and `7`, producing `14`; then push `-`.
- Read `5`: push `5`.
- End of tokens: apply the remaining `-` to `14` and `5`, producing `9`.

The important rule is that operators are applied when a new operator of higher-or-equal precedence appears or when a closing parenthesis forces a flush.

## 4. Errors as values
I followed the project’s design: tools, `Workspace::resolve()`, and `calc_evaluate()` all report errors as ordinary values instead of throwing exceptions. That is the right fit for an agent loop because failures are expected: a file may be missing, a path may escape the workspace, or division by zero may occur in a valid expression. The model should receive an explicit error message and decide what to do next.

Exceptions are better reserved for programmer errors or impossible states, such as a violated invariant or a logic bug that should not happen in normal use. The harness is designed so that the tool boundary is data-driven: a file tool returns `ToolResult::failure("...")`, and the model reads that result like any other message from the environment. This makes the conversation robust and keeps the tool loop safe even when the model tries something invalid.

## 5. What I would design differently
If I had more time, I would add more registry instrumentation for automatic hash-quality reporting and trace the resize pattern directly in the log. I would also factor the path-validation logic into a few small helper functions so the acceptance and rejection rules are easier to test and explain. The current design is simple, fast, and correct for the assignment, but those extras would make future tuning and debugging easier.
