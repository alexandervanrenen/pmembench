
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)

pmem-bench
==========

A set of algorithms and benchmarks for persistent memory (PMem).
The algorithms are described in two research papers:
- There is a [short version](https://db.in.tum.de/people/sites/vanrenen/papers/nvm_stats.pdf) published at [DaMoN](https://sites.google.com/view/damon2019/home-damon-2019). 
- And an [extended version](https://link.springer.com/content/pdf/10.1007/s00778-020-00622-9.pdf) that appeared in the [VLDBJ 2020](https://link.springer.com/journal/778/volumes-and-issues/29-6).

Structure
---------
Each algorithm and experiment has a benchmark script to compile and run it in the [reproduce/](reproduce/) folder.
If, for example, you are interested in measuring the read latency of your PMem device, have a look at [reproduce/latency_read.sh](reproduce/latency_read.sh).
These file also contain instructions (already in executable bash syntax) on how to compile and use the code.

The source code of the algorithms and experiments are contained in the respective their folders on the root level of the project.
For example, the read latency experiment can be found in [latency](latency/).

The benchmark scripts (in [reproduce/](reproduce/)) print their results to `stdout` and also create a log file in [results/](results/) using `tee`.
The output is in an easy to parse format and can be used for creating plots (not included in the repository as the ones in the paper are pgfplots).

Issues & Contributions
----------------------
Note that the source code is a prototype implementation for a research paper. 
There might be bugs and other limitations.
If you find an issue or run into troubles feel free to contact me via an issue in this repository.

Licence
-------
You are free to choose any of the above licences when using the source code.
However, I encourage you in a non binding way to follow the [blessing from the SQLite folks](https://github.com/sqlite/sqlite/blob/master/LICENSE.md):

```
May you do good and not evil.
May you find forgiveness for yourself and forgive others.
May you share freely, never taking more than you give.
```

Authors
-------
Alexander van Renen
