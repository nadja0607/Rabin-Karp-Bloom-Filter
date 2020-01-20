# Rabin-Karp-Bloom-Filter
In many scenarios, we would like to know how “similar” two documents are to each other.
In this lab, I wrote a program to approximately match one input file against another file. 
In addition to speed improvement, we also make the simple algorithm more robust to spurious differences between documents. 
In particular, we "normalize" a document by doing the following
    1. Convert all upper case letters to lower case ones.
    2. Convert different white space characters (e.g. carriage return, tab,...) into the space character,
    3. Shrink any sequence of two or more space characters to exactly one space.
    4. Remove any whitespace at the beginning and end of the text
    
Our next optimization comes from using the brilliant Rabin-Karp substring matching algorithm.
RK checks if a given query string P appears as a substring in Y. At a high level, RK works by
computing a hash for the query string, hash(P ), as well as a hash for each of the n − k + 1
substrings of length k in Y, hash(Y [0...k − 1]), hash(Y [1...k]), ..., hash(Y [n − k...n − 1]). 
A hash
function turns any arbitrary string into a b-bit hash value with the property that collision (two
different strings with the same hash value) is unlikely. Therefore, by comparing hash (P) with
each of the n − k + 1 hashes from Y, we can check if P appears as a substring in Y. 

Our RK-based approximate matching algorithm can be made faster by using a Bloom filter.
Our enhanced algorithm uses a Bloom filter which is simply a bitmap of h bits initialized to zeros
in the beginning. We insert all [𝑚/𝑘] RK hash values of X that we want to match in Y into the "filter".
To insert a value v into the bitmap, we use f hash functions to map v into f positions in the bitmap
and set each position to be one. 
Using a Bloom filter, our enhanced algorithm has a total runtime of O(m + n).
