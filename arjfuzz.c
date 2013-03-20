/*
ARJFUZZ - URL Fuzzer dictionnary based, multiple thread based.

Uses 10 threads to scan website (NUM_THREADS_MAX)
Uses 3 transversals (TRANSVERSALS_MAX) - can be set from command line. Recommends one to start with...

v0.1 - G - Original : 18.03.2013
v0.2 - G - fixed 2 segmentation faults -u / -p : 19.03.13
v0.3 - G - fixed 1 mutex segfault + changed parameter from -p to -o in usage : 20.03.13

How to use :

make
make install
Add word into dictionnary       -w word
Add file into dictionnary       -f file
Scan                            -u url [-t <number of transversals> -o <false-positive-string>]
                                -u https://www.testserver.com
                                -u https://www.testserver.com -t 3
                                -u https://www.testserver.com -t 3 -o Home

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>

#define DEBUG 1
#define NUM_THREADS_MAX 10
#define TRANSVERSALS_MAX 3
#define DICT "dictionnary/dict.txt"

/* name of process */
char *name;

/* number of defined transversals and info needed to work out */
int transversals = 1;
int indext[TRANSVERSALS_MAX];
int maxcount = 0;

/* url */
char *url;

/* positive */
char *positive=NULL;


/* threads information data */
struct thread_data
{
  int thread_id;
  char *message;
};

/* Dynamic buffer for curl */
struct BufferStruct
{
  char *buffer;
  size_t size;
};

/* creating enough structure for max threads */
struct thread_data thread_data_array[NUM_THREADS_MAX];
/* lock system */
pthread_mutex_t mutexsum;
pthread_mutex_t mutexsum1;


/* get word from dictionarry */
char *
get_word (int count)
{
  int i;
  static char word[255];
  FILE *fp;

  fp = fopen (DICT, "r");
  if (fp == NULL)
    {
      perror (name);
      exit (1);
    }

  for (i = 0; i < count; i++)
    {
      bzero (word, 255);
      fscanf (fp, "%s", word);
    }

  fclose (fp);


  return word;
}


// This is the function we pass to LC, which writes the output to a BufferStruct
static size_t WriteMemoryCallback
  (void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;

  struct BufferStruct *mem = (struct BufferStruct *) data;

  mem->buffer = realloc (mem->buffer, mem->size + realsize + 1);

  if (mem->buffer)
    {
      memcpy (&(mem->buffer[mem->size]), ptr, realsize);
      mem->size += realsize;
      mem->buffer[mem->size] = 0;
    }
  return realsize;
}


/* open http URL and return page */
char *
openhttp (char *url)
{
  CURL *curl;
  CURLcode res;
  struct BufferStruct output;


  output.buffer = NULL;
  output.size = 0;

  curl = curl_easy_init ();
  if (curl)
    {
      curl_easy_setopt (curl, CURLOPT_URL, url);
      curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
      curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &output);
      curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);    
  /* Perform the request, res will get the return code */
      res = curl_easy_perform (curl);
      /* Check for errors */
      if (res != CURLE_OK)
	fprintf (stderr, "curl_easy_perform() failed: %s\n",
		 curl_easy_strerror (res));
	
      /* always cleanup */
      curl_easy_cleanup (curl);

    }

  if (res != CURLE_OK)
    return 0;
  else
    return output.buffer;
}

/* run on thread*/
void *
run_thread (void *threadarg)
{
  CURLcode code;
  int taskid, i, local_maxcount,local_transversals;
  int tmpindex[transversals];
  char fullurl[255],local_url[255];
  char local_positive[255];
  char *msg;

  struct thread_data *my_data;

/* more argument passing will be needed */
  my_data = (struct thread_data *) threadarg;
  taskid = my_data->thread_id;
/* busy work */

  do
    {

/* lock things */

      bzero (local_positive, 255);
      bzero (local_url, 255);

        pthread_mutex_lock (&mutexsum);						// LOCK 1
        bzero (local_url, 255);
	strncpy(local_url,url,strlen(url));
	local_maxcount = maxcount;
	local_transversals = transversals;

	if(positive!=NULL) {
      		strncpy (local_positive, positive, strlen (positive));
	} 
          indext[0]++;

	  pthread_mutex_unlock (&mutexsum); 					// UNLOCK 1

      if (tmpindex[local_transversals - 1] > local_maxcount);
      else
	{

		pthread_mutex_lock (&mutexsum);					// LOCK 2

	  for (i = 0; i < local_transversals; i++)
	    {
	      if (indext[i] > (local_maxcount))
		if ((i + 1) < local_transversals)
		  {
		    indext[i + 1]++;
		    indext[i] = 0;
		  }
		tmpindex[i] = indext[i];
	    }

		pthread_mutex_unlock (&mutexsum);				// UNLOCK 2

	  bzero (fullurl, 255);
	  strncat (fullurl, local_url, strlen (local_url));
	
	  for (i = 0; i < local_transversals; i++)
	    {

	      if (tmpindex[i] > -1)
		{
		  if (strlen (get_word (tmpindex[i])) > 0)
		    {
		      strncat (fullurl, "/ ", 1);
		      strncat (fullurl, get_word (tmpindex[i]),
			       strlen (get_word (tmpindex[i])));
		    }
		}
	    }
	//	printf ("%d : check positive (%s)\n", taskid, fullurl);
	usleep(300);

	  if (strlen (fullurl) > strlen (local_url))
	    {
	
	        pthread_mutex_lock (&mutexsum1);				// LOCK A
	      		msg = openhttp (fullurl);
	        pthread_mutex_unlock (&mutexsum1);				// UNLOCK A

		if(msg==0) pthread_exit ((void *) threadarg);

		if(local_positive!=NULL) {
	      if (strstr (msg, local_positive) == NULL
		  && strstr (msg, "404") == NULL) 
		printf ("%d : positive (%s)\n", taskid, fullurl);
	}
	else{
		if(strstr (msg, "404") == NULL) 
                printf ("%d : positive (%s)\n", taskid, fullurl);


	}

	      free (msg);


	    }

	}

      if (tmpindex[local_transversals - 1] > local_maxcount)
	{

	  pthread_exit ((void *) threadarg);

	}


/* busy work until the end */

    }
  while (1);			// will never die !

/* end of this thread */


}

/* add file to dictionnary */
int
add_file (char *file)
{
  char word[255], word1[255];
  FILE *fp;
  FILE *fp1;

  fp = fopen (file, "r");
  if (fp == NULL)
    {
      perror (name);
      exit (1);
    }

  do
    {

      bzero (word, 255);
      fscanf (fp, "%s\n", word);


/* parsing to see if word inside (or not) */

      fp1 = fopen (DICT, "r");
      if (fp == NULL)
	{
	  perror (name);
	  exit (1);
	}


      do
	{

	  bzero (word1, 255);
	  fscanf (fp1, "%s\n", word1);
	  if (strlen (word) == strlen (word1))
	    if (strncmp (word, word1, strlen (word)) == 0)
	      {
		bzero (word, 255);
	      }

	}
      while (!feof (fp1));

      fclose (fp1);

/* Is word found ? */

      if (strlen (word) > 0)
	{

/* we insert then */

	  fp1 = fopen (DICT, "a");
	  if (fp == NULL)
	    {
	      perror (name);
	      exit (1);
	    }
	  fprintf (fp1, "%s\n", word);
	  fclose (fp1);

	}


    }
  while (!feof (fp));



  /* We finished */

  fclose (fp);


}


/* add word to dictionnary */
int
add_word (char *word)
{
  FILE *fp;
  char word1[255];

/* parsing to see if word inside (or not) */

  fp = fopen (DICT, "r");
  if (fp == NULL)
    {
      perror (name);
      exit (1);
    }

  do
    {

      bzero (word1, 255);
      fscanf (fp, "%s\n", word1);
      if (strlen (word) == strlen (word1))
	if (strncmp (word, word1, strlen (word)) == 0)
	  {
	    fprintf (stderr, "%s : word %s already in file.\n", name, word);
	    exit (1);
	  }

    }
  while (!feof (fp));

  fclose (fp);


/* if all good, adding word */

  fp = fopen (DICT, "a");
  if (fp == NULL)
    {
      perror (name);
      exit (1);
    }
  fprintf (fp, "%s\n", word);
  fclose (fp);

}

/* read dictionnary file */
int
read_dictionnary ()
{

}

/* usage */
void
usage (char *argv[0])
{

  fprintf (stderr,
	   "Usage for %s :\nAdd word into dictionnary\t-w word\nAdd file into dictionnary\t-f file\nscan\t\t\t\t-u url [-t <number of transversals> -o <false-positive-string>] \n",
	   argv[0]);

  exit (1);

}

/* sanitize argvz */
int
sanitize_argv (int argc, char **argv)
{

  if (argc < 3)
    usage (&argv[0]);

/* clearing argument 1 */
  if (strlen (argv[1]) != 2)
    usage (&argv[0]);

/* clearing argument 2 */
  if (strlen (argv[2]) > 255)
    usage (&argv[0]);

  if (argc == 3)
    return 0;
  else
    {

      if (argc < 5)
	usage (&argv[0]);

/* clearing argument 3 */
      if (strlen (argv[3]) != 2)
	usage (&argv[0]);

/* clearing argument 4 */
      if (strlen (argv[4]) > 100)
	usage (&argv[0]);

      if (argc == 5)
	return 0;
      else
	{

	  if (argc < 7)
	    usage (&argv[0]);

/* clearing argument 5 */
	  if (strlen (argv[5]) != 2)
	    usage (&argv[0]);

/* clearing argument 6 */
	  if (strlen (argv[6]) > 100)
	    usage (&argv[0]);

	}

    }

  return 0;

}

/* main program */
int
main (int argc, char **argv)
{

/* TYPE OF USES :

Add word into dictionnary 	-w word 
Add file into dictionnary 	-f file 
Scan 				-u url [-t <number of transversals> -o <false-positive-string>]
				-u https://www.testserver.com 
				-u https://www.testserver.com -t 3
				-u https://www.testserver.com -t 3 -o Home

*/

  int rc, i, change = 0;
  char word[255];
  int err;
  long t;
  pthread_t threads[NUM_THREADS_MAX];
  pthread_attr_t attr;
  FILE *fp;
  char *msg;

/* Stay sane */
  err = sanitize_argv (argc, argv);
/* fill in name */
  name = (char *) malloc (strlen (argv[0]) + 1);
  bzero (name, strlen (argv[0]) + 1);
  strncpy (name, argv[0], strlen (argv[0]));

/* add word */
  if (strncmp (argv[1], "-w", 2) == 0)
    {
      err = add_word (argv[2]);
      exit (1);
    }

/* add file */
  if (strncmp (argv[1], "-f", 2) == 0)
    {
      err = add_file (argv[2]);
      exit (1);
    }

/* However, if we are in scan mode, we need to parse all arguments */

  for (i = 1; i < argc; i = i + 2)
    {

// If transversals

      if (strncmp (argv[i], "-t", 2) == 0)
	{

	  sscanf (argv[i + 1], "%d", &transversals);

	  if (transversals > TRANSVERSALS_MAX)
	    {
	      fprintf (stderr,
		       "%s : too many transversals, please change #define TRANSVERSALS_MAX if ou are sure.\n",
		       argv[0]);
	      exit (1);

	    }
	  else if (transversals == 0)
	    usage (&argv[0]);

	  printf ("Transversals number set to %d\n", transversals);

	}


// If URL

      else if (strncmp (argv[i], "-u", 2) == 0)
	{

	  url = (char *) malloc (strlen (argv[i + 1]) + 1);
	  bzero (url, strlen (argv[i + 1]) + 1);
	  strncpy (url, argv[i + 1], strlen (argv[i + 1]));

	  printf ("URL set to %s\n", url);

	}


// If false positive detection

      else if (strncmp (argv[i], "-o", 2) == 0)
	{

	  positive = (char *) malloc (strlen (argv[i + 1]) + 2);
	  bzero (positive, strlen (argv[i + 1]) + 2);
	  strncpy (positive, argv[i + 1], strlen (argv[i + 1]));

	  printf ("Positive set to %s\n", positive);

	}
      else
	usage (&argv[0]);

    }


/* setting up max count in lines */

  fp = fopen (DICT, "r");
  if (fp == NULL)
    {
      perror (name);
      exit (1);
    }
  maxcount = 0;
  do
    {

      bzero (word, 255);
      fscanf (fp, "%s\n", word);
/* set max count */
      if (strlen (word) > 0)
	{
	  maxcount++;

	}
    }
  while (!feof (fp));

  fclose (fp);


/* initializing transversals */
  for (i = 0; i < transversals; i++)
    indext[i] = -1;

// algorythme for transversals





/* 

/* readying lock mechanism */
  pthread_mutex_init (&mutexsum, NULL);
  pthread_mutex_init (&mutexsum1, NULL);

/* creating threads */
  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

/* running threads */

  for (t = 0; t < NUM_THREADS_MAX; t++)
    {
      thread_data_array[t].thread_id = t;
      thread_data_array[t].message = "";

      rc =
	pthread_create (&threads[t], NULL, run_thread,
			(void *) &thread_data_array[t]);
    }

/* freeing data (cleanup) */
  pthread_mutex_destroy (&mutexsum);
  pthread_exit (NULL);

}

