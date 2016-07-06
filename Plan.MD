# Epoch Language Plans
This file is a running stream-of-consciousness log of plans and thoughts surrounding the Epoch programming language. It is not intended to be an authoritative or exhaustive resource; it exists primarily for the curious and for recording various ideas.

  
  
### MikeL 2016-07-06
I'd like to quickly address the general question of overall plan for the language. I see the project progressing in roughly the following order (more or less) over the next few months/years/etc.:

 * Finish GC implementation for strings on Windows x64
 * Assess what's left for getting self-hosting on x64
 * Build out functionality needed to self-host
 * Flesh out test suites as necessary
 * Self-host on x64
 * Take time to analyze state of the project
 * Probably tackle arrays around this timeframe
 * Massive cleanup, documentation, and test-hardening passes
 * Introduction of tasks
 * Explore BGFX integration further during this time
 * Refactor Era to use new language features, x64 port
 * Continuing development on Era
 * Debugging infrastructure work
 * Version 0.1?

Each bullet point represents anywhere from a few weeks to a couple of months worth of work. Obviously this is not slowing down any time soon, and there's plenty left to be done. However, I'm fairly confident that this is the right direction for things and that with sufficient investment the final candidate for 0.1 will land gracefully.
  
  
### MikeL 2016-07-05
Garbage collection of strings is now loosely prototyped and running on Windows x64 binaries. There are still many challenges to solve, such as marking intermediate values in expressions as GC roots, and interacting with callstacks that contain external/non-Epoch code. However, given that I had reliable solutions for these in previous compilers on 32-bit Windows, I'm confident that I can rebuild the GC implementation fairly soon.

There is a strong chance that I will be exploring more of the resource allocation mechanisms of Epoch during this time. I've gained a renewed interest in supporting optional GC as a first-class principle of the language - i.e. it should be possible to totally opt-out of the GC and write manually resource-controlled programs without much extra pain. I also want to have first-class support for things like stack allocators, pool allocators, freelists, etc. etc. although most of that will have to wait until after array support lands.

Speaking of arrays... I need to think up a good syntactical approach to expressing "array of type T". I also have to decide if I want to build fixed-size arrays or go hog wild and support resizing in native array objects. It's looking like the former for simplicity and elegance; we can always build standard library containers atop fixed arrays that support growth patterns. It might even be nice to allow programmers to supply growth policies for their "vector" containers.
