/* Match every k-character snippet of the query_doc document
	 among a collection of documents doc1, doc2, ....

	 ./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include "bloom.h"

enum algotype { SIMPLE = 0, RK, RKBATCH};

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437; 

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;

/* modulo addition */
long long
madd(long long a, long long b)
{
	return ((a+b)>BIG_PRIME?(a+b-BIG_PRIME):(a+b));
}

/* modulo substraction */
long long
mdel(long long a, long long b)
{
	return ((a>b)?(a-b):(a+BIG_PRIME-b));
}



/* modulo multiplication*/
long long
mmul(long long a, long long b)
{
	return ((a*b) % BIG_PRIME);
}

/* read the entire content of the file 'fname' into a 
	 character array allocated by this procedure.
	 Upon return, *doc contains the address of the character array
	 *doc_len contains the length of the array
	 */
void
read_file(const char *fname, char **doc, int *doc_len) 
{
	struct stat st;
	int fd;
	int n = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_file: open ");
		exit(1);
	}

	if (fstat(fd, &st) != 0) {
		perror("read_file: fstat ");
		exit(1);
	}

	*doc = (char *)malloc(st.st_size);
	if (!(*doc)) {
		fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
		exit(1);
	}

	n = read(fd, *doc, st.st_size);
	if (n < 0) {
		perror("read_file: read ");
		exit(1);
	}else if (n != st.st_size) {
		fprintf(stderr,"read_file: short read!\n");
		exit(1);
	}
	
	close(fd);
	*doc_len = n;
}


/* The normalize procedure examines a character array of size len 
	 in ONE PASS and does the following:
	 1) turn all upper case letters into lower case ones
	 2) turn any white-space character into a space character and, 
	    shrink any n>1 consecutive spaces into exactly 1 space only
			Hint: use C library function isspace() 
	 You must do the normalization IN PLACE so that when the procedure
	 returns, the character array buf contains the normalized string and 
	 the return value is the length of the normalized string.
*/
int normalize(char *buf,	/* The character array containing the string to be normalized*/
					int len			/* the size of the original character array */)
{
	int i; //keeps us moving through the string
	int j=0; //used to overwrite the buf
	bool follows_the_space=true; //helps to determine if there are more consecutive spaces

	for (i=0; i<len; i++)
	{
		if (isspace(buf[i]))
			buf[i]=' '; //changhing all whitespace characters into space
		if (isspace(buf[i]) && (j==0 || isspace(buf[j-1])))
		{
			continue;
		}

		else //the letter is found
		{
			follows_the_space=false;
			buf[j]=tolower(buf[i]); //change to lower case letter
			j++; //move on
		}

	}
	if (isspace(buf[j]))
	return j-1;
	else return j;
}
/* check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring 
	 If so, return 1. Else return 0
	 You may want to use the library function strncmp
	 */
int
simple_match(const char *ps,	/* the query string */
						 int k, 					/* the length of the query string */
						 const char *ts,	/* the document string (Y) */ 
						 int n						/* the length of the document Y */)
{
	int i;
	for (i=0; i<n; i++)
	{
		if (strncmp(ps, ts+i, k)==0) 
		{
			return 1;
		}
	}
	return 0;
}

/* Check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring using the rabin-karp algorithm
	 If so, return 1. Else return 0
	 In addition, print the first 'PRINT_RK_HASH' hash values of ts
	 Example:
	 $ ./rkmatch -t 1 -k 20 X Y
	 605818861882592 812687061542252 1113263531943837 1168659952685767 4992125708617222 
	 0.01 matched: 1 out of 148
	 */
int
rabin_karp_match(const char *ps,	/* the query string */
								 int k, 					/* the length of the query string */
								 const char *ts,	/* the document string (Y) */ 
								 int n						/* the length of the document Y */ )
{ 
	int i;
	int cnt=0;
	long long query_hash = 0;
	long long string_hash =0;
	long long temp=1; 
	bool hash_match=false;
	
	for (i=0 ;i<k-1; i++)
			{
				temp=mmul(temp,256);
			}
	for (i=0;i<k;i++)
	{
		query_hash= madd(mmul(query_hash,256),(int)ps[i]);
	}

	for (i=0;i<n;i++)
	{
		if (i<k)
		{
			string_hash = madd(mmul(string_hash,256),(int)ts[i]);
			if (i==k-1)
			{
				if (query_hash==string_hash)
					hash_match=true;
			}
		}
		else
		{
			string_hash= madd(mmul(mdel(string_hash,mmul(ts[i-k],temp)),256),(int)ts[i]);
			if (query_hash==string_hash)
				hash_match=true;
		}

		if (i>=k-1 && cnt < PRINT_RK_HASH)
			{
				printf("%lld ",string_hash);
				cnt++;
			}

		if (hash_match==true)
		{
			printf("\n");
			return 1;
		}
	}
	printf("\n");

	return 0;
}

/* Initialize the bitmap for the bloom filter using bloom_init().
	 Insert all m/k RK hashes of qs into the bloom filter using bloom_add().
	 Then, compute each of the n-k+1 RK hashes of ts and check if it's in the filter using bloom_query().
	 Use the given procedure, hash_i(i, p), to compute the i-th bloom filter hash value for the RK value p.

	 Return the number of matched chunks. 
	 Additionally, print out the first PRINT_BLOOM_BITS of the bloom filter using the given bloom_print 
	 after inserting m/k substrings from qs.
*/
int
rabin_karp_batchmatch(int bsz,        /* size of bitmap (in bits) to be used */
                      int k,          /* chunk length to be matched */
                      const char *qs, /* query docoument (X)*/
                      int m,          /* query document length */ 
                      const char *ts, /* to-be-matched document (Y) */
                      int n           /* to-be-matched document length*/)
{
	//if the lebgth of to-be-matched document is less than k, return false
  	if (n < k)
    return 0;
	int i;
	int j;
	int matched_chunks = 0;
	long long query_hash = 0;
	long long string_hash = 0;
	long long temp = 1;
  	//initializing the bitmap
	bloom_filter my_bitmap = bloom_init(bsz);
	//computing hash values of qs
	for (i=0; i<k-1; i++)
	{
		temp = mmul(temp,256);
	}
	//adding m/k chunks of qs into the bloom filter
	for (i=0 ; i<m/k; i++)
  	{
  		query_hash = 0;
	  	for (j=0; j<k; j++)
		{
			query_hash = madd(mmul(query_hash,256),(int)qs[i*k+j]);
		}
    	bloom_add(my_bitmap, query_hash);
	}
	bloom_print (my_bitmap, PRINT_BLOOM_BITS);
	for (i=0; i<k; i++)
	{
		string_hash = madd(mmul(string_hash,256),(int)ts[i]);
	}
	//printf("%d\n",n-k+1 );
	for (i=0; i<n-k+1; i++)
	{	//if we get a possible match from bloom filter, go through each chunk and compare the strings
		if (bloom_query (my_bitmap,string_hash))
		{
			for (j=0; j<m/k; j++)
			{	//if the match is found, the counter is increased
				//comparing addresses
				if (strncmp(qs+j*k, ts+i, k)==0)
				{
					matched_chunks++;
					break;
				}
			}
		}
		//rolling hash
		string_hash= madd(mmul(mdel(string_hash,mmul(ts[i],temp)),256),(int)ts[i+k]);
	}
	return matched_chunks;
}

int 
main(int argc, char **argv)
{
	int k = 100; /* default match size is 100*/
	int which_algo = SIMPLE; /* default match algorithm is simple */

	char *qdoc, *doc; 
	int qdoc_len, doc_len;
	int i;
	int num_matched = 0;
	int to_be_matched;
	int c;

	/* Refuse to run on platform with a different size for long long*/
	assert(sizeof(long long) == 8);

	/*getopt is a C library function to parse command line options */
	while (( c = getopt(argc, argv, "t:k:q:")) != -1) {
		switch (c) 
		{
			case 't':
				/*optarg is a global variable set by getopt() 
					it now points to the text following the '-t' */
				which_algo = atoi(optarg);
				break;
			case 'k':
				k = atoi(optarg);
				break;
			case 'q':
				BIG_PRIME = atoi(optarg);
				break;
			default:
				fprintf(stderr,
						"Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
				exit(1);
			}
	}

	/* optind is a global variable set by getopt() 
		 it now contains the index of the first argv-element 
		 that is not an option*/
	if (argc - optind < 1) {
		printf("Usage: ./rkmatch query_doc doc\n");
		exit(1);
	}

	/* argv[optind] contains the query_doc argument */
	read_file(argv[optind], &qdoc, &qdoc_len); 
	qdoc_len = normalize(qdoc, qdoc_len);

	/* argv[optind+1] contains the doc argument */
	read_file(argv[optind+1], &doc, &doc_len);
	doc_len = normalize(doc, doc_len);

	switch (which_algo) 
		{
			case SIMPLE:
				/* for each of the qdoc_len/k chunks of qdoc, 
					 check if it appears in doc as a substring*/
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (simple_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
				break;
			case RK:
				/* for each of the qdoc_len/k chunks of qdoc, 
					 check if it appears in doc as a substring using 
				   the rabin-karp substring matching algorithm */
				for (i = 0; (i+k) <= qdoc_len; i += k) {
					if (rabin_karp_match(qdoc+i, k, doc, doc_len)) {
						num_matched++;
					}
				}
				break;
			case RKBATCH:
				/* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
				num_matched = rabin_karp_batchmatch(((qdoc_len*10/k)>>3)<<3, k, qdoc, qdoc_len, doc, doc_len);
				break;
			default :
				fprintf(stderr,"Wrong algorithm type, choose from 0 1 2\n");
				exit(1);
		}
	
	to_be_matched = qdoc_len / k;
	printf("%.2f matched: %d out of %d\n", (double)num_matched/to_be_matched, 
			num_matched, to_be_matched);

	free(qdoc);
	free(doc);

	return 0;
}
