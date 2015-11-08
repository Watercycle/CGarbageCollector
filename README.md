CGarbageCollector
===================
A simple, straightforward, __unoptimized__, garbage collector for C!

This collector is inspired by Matthew Plant's short paper, "Writing a Simple Garbage Collector in C".  It is intended to be a simplification of his original idea (with less pointer magic) in order to demystify garbage collectors for an even wider audience.

This garbage collector essentially reimplements malloc, uses a linked list to track memory, and then follows a 'mark and sweep' system to detect when memory is no longer in use.  This part tends to be abstracted because most languages implement a virtual machine which gives them full access to allocated objects.  If anything this project teaches you to appreciate the simplicity of virtual machines.  It could be a good optimization exercise!

General Procedure
====================
1. Allocation tries to reuse memory, or asks the system for more.
2. Extra meta data is squirreled away beneath the requested memory, and saved to a linked list.
3. Occasionally the garbage collector will - collect.
  1. scan the heap (our linked list), the stack, and initialized variables
  2. mark pieces of memory in our heap that have been referenced in (a) as being 'actively used'
  3. make memory in the heap once again usable by marking it as "free" if it is not being 'actively used'

Issues / Todo
====================
- Variables are not being properly marked as 'active' from the stack scan
- Does not support 64-bit
- A few things could be made more portable
- Silence warnings without cluttering the code with lots of casts

Suggestions
====================
- The malloc implementation is horrible.  Ideally you should request large chunks and manage them yourself
- Scanning the heap for self-referencing objects is an O(n^3) operation, that's pretty bad
- Many heaps scan automatically after so many allocations have occured