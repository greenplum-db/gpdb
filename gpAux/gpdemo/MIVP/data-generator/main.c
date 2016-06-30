#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define INTEGER 0
#define VARCHAR 1
#define TIMESTAMP 2
#define NUMERIC 3

#define MAXLINESIZE 1024

int getyear(int date);
int getmonth(int date);
int getday(int date);

void table_gen(long long nrows, Stringlist wlist,int numcols, int *column_types, double *column_mins, double *column_maxes);

int main(int argc, char **argv) {
  
  int i,length;

  char string[MAXLINESIZE], *loc;
  Stringlist slist;
  Stringlist wlist;

  long long numrows;

  int numcols;
  int col_types[100];
  double col_mins[100],col_maxes[100];

  char punctuation[8] = { ' ', '.', '/', '\\', '(', ')', ',', '\0' };

  srand(12);

  FILE *infile;

  if ((argc-1) < 2) {
    printf("usage: %s text_seed_input_file_name number_of_rows_to_generate\n",argv[0]);
    exit(1);
  }

  if ( (infile=fopen(argv[1],"r")) == NULL ) {
    printf("error opening file %s for reading\n",argv[1]);
    exit(2);
  }  

  if (sscanf(argv[2],"%lli",&numrows) != 1) {
    printf("error scanning number of rows from command line: %s\n",argv[2]);
    exit(2);
  }  
  //printf("Number of rows to be generated: %ld\n",numrows);
  //exit(0);

  slist.list=NULL; slist.used_size=0; slist.chunksize=100; slist.alloc_size=0;

  while ( (fgets(string, MAXLINESIZE, infile)) != NULL) {

    //Find the number of characters in the string, includes the newline but not the \0
    length = strlen(string);
    //Trim off the trailing newline
    string[length-1]='\0';

    //Ensure that there's sufficient space for another string in the list
    slist.used_size++;
    alloc_more_stringlist(&slist,slist.used_size);

    //Allocate space for the length of the string
    *(slist.pos)=(char *)malloc(sizeof(char)*length);

    //Copy the string into the list
    strcpy(*(slist.pos),string);

    //Increment the string pointer to the next string in the list
    slist.pos++;

  }

  //Reset the string pointer
  slist.pos = slist.list;

  //Initialize the wlist
  wlist.list=NULL; wlist.used_size=0; wlist.chunksize=100; wlist.alloc_size=0;

  //Parse the string list into words
  for (i=0; i<slist.used_size; i++) {

    //Ensure that there's sufficient space for another word in the list
    wlist.used_size++;
    alloc_more_stringlist(&wlist,wlist.used_size);

    loc = strtok(slist.list[i],(char *)punctuation);

    if (loc != NULL) {
      //Allocate space for the length of the word
      length = strlen(loc)+1;
      *(wlist.pos)=(char *)malloc(sizeof(char)*length);
      strcpy(*(wlist.pos),loc);
      wlist.pos++;
    }

    while ( (loc = strtok(NULL,(char *)punctuation)) != NULL) {
      //Ensure that there's sufficient space for another word in the list
      wlist.used_size++;
      alloc_more_stringlist(&wlist,wlist.used_size);
      //Allocate space for the length of the word
      length = strlen(loc)+1;
      *(wlist.pos)=(char *)malloc(sizeof(char)*length);
      strcpy(*(wlist.pos),loc);
      wlist.pos++;
    }
  }

  /*
  //Print the wordlist
  for (i=0; i<wlist.used_size; i++) {
    //printf("String: \"%s\"\n",*(slist.pos));
    //slist.pos++;
    printf("String: \"%s\"\n",wlist.list[(int)randomrange(0,wlist.used_size-1)]);
    //printf("String: \"%s\"\n",wlist.list[i]);
  }
  */

  /*
   * Setup the table characteristics
   * 	numcols		= number of columns
   *	arrays of size numcols:
   *    	column_types	= type of column, mapped to integers:
   *			INTEGER
   *			VARCHAR
   *			NUMERIC
   *			TIMESTAMP
   *		column_mins 	= max value of columns
   *		column_maxes	= max value of columns
   */

  numcols = 15;
  col_types[0]  = TIMESTAMP; col_mins[0]  = 20040101; col_maxes[0]  = 20041231;
  col_types[1]  = VARCHAR;   col_mins[1]  = 1;        col_maxes[1]  = 750/100;
  col_types[2]  = VARCHAR;   col_mins[2]  = 1;        col_maxes[2]  = 100/100;
  col_types[3]  = INTEGER;   col_mins[3]  = 0;        col_maxes[3]  = 0;
  col_types[4]  = INTEGER;   col_mins[4]  = 1;        col_maxes[4]  = 40;
  col_types[5]  = INTEGER;   col_mins[5]  = 0;        col_maxes[5]  = 0;
  col_types[6]  = INTEGER;   col_mins[6]  = 1;        col_maxes[6]  = 4;
  col_types[7]  = NUMERIC;   col_mins[7]  = 0.02;     col_maxes[7]  = 0.8;
  col_types[8]  = VARCHAR;   col_mins[8]  = 1;        col_maxes[8]  = 1500/300;
  col_types[9]  = VARCHAR;   col_mins[9]  = 1;        col_maxes[9]  = 1500/300;
  col_types[10] = VARCHAR;   col_mins[10] = 1;        col_maxes[10] = 1500/300;
  col_types[11] = INTEGER;   col_mins[11] = 0;        col_maxes[11] = 2;
  col_types[12] = INTEGER;   col_mins[12] = 0;        col_maxes[12] = 4;
  col_types[13] = VARCHAR;   col_mins[13] = 1;        col_maxes[13] = 1500/300;
  col_types[14] = VARCHAR;   col_mins[14] = 1;        col_maxes[14] = 1500/300;


  table_gen(numrows, wlist,numcols,col_types,col_mins,col_maxes); 

  exit(0);

}

void table_gen(long long nrows, Stringlist wlist,int numcols, int *column_types, double *column_mins, double *column_maxes) {

  int itemp;
  int i,j,ii, nwords;
  int year, month, day, hour, minute, second;
  int minyear, minmonth, minday;
  int maxyear, maxmonth, maxday;

  int leaps[] = LEAPS;
  int nonleaps[] = NONLEAPS;

  //printf("Number of rows to be generated: %ld\n",nrows);
  //exit(0);

  for (i=0; i<nrows; i++) {

   for (j=0; j<numcols; j++) {
    switch (column_types[j]) {
      case INTEGER:
        //printf("column_type[%d] = INTEGER\n",j+1);
	printf("%d",(int)randomrange(column_mins[j],column_maxes[j]));
	break;;
      case VARCHAR:
        //printf("column_type[%d] = VARCHAR\n",j+1);
	nwords = (int)randomrange(column_mins[j],column_maxes[j]);
	for (ii=0; ii<nwords ; ii++) {
	  itemp = (int)randomrange(0,wlist.used_size-1);
	  printf("%s",wlist.list[itemp]);
	  if (ii != (nwords-1)) printf(" ");
        }
	break;;
      case TIMESTAMP:
        //printf("column_type[%d] = TIMESTAMP\n",j+1);
	minyear = getyear(column_mins[j]); maxyear = getyear(column_maxes[j]);
	minmonth = getmonth(column_mins[j]); maxmonth = getmonth(column_maxes[j]);
	minday = getday(column_mins[j]); maxday = getday(column_maxes[j]);

	year 	= (int)randomrange(minyear,maxyear);
	month 	= (int)randomrange(minmonth,maxmonth);
	day 	= (int)randomrange(minday,maxday);
	if (ISLEAP(year)) {
	  day = MIN(leaps[month-1],day);
        } else {
	  day = MIN(nonleaps[month-1],day);
        }
	hour 	= (int)randomrange(0,23);
	minute 	= (int)randomrange(0,59);
	second 	= (int)randomrange(0,59);

	printf("%.4d-%.2d-%.2d %.2d:%.2d:%.2d",year,month,day,hour,minute,second);
	break;;
      case NUMERIC:
        //printf("column_type[%d] = NUMERIC\n",j+1);
	printf("%.6f",(float)randomrange(column_mins[j],column_maxes[j]));
	break;;
    }
    if (j != (numcols-1)) printf("|");

   } //End of numcolumns
   printf("\n");

  }

}

int getyear(int date) {
  return date/10000;
}

int getmonth(int date) {
  return (date-10000*getyear(date))/100;
}

int getday(int date) {
  return (date-10000*getyear(date)-100*getmonth(date));
}
