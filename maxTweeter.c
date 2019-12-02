#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<stdbool.h>

int empty_str(const char* str) {
    while (*str != '\0') {
        if (!isspace((unsigned char)*str))
            return 0;
        str++;
    }
    return 1;
}

char* parse_str(char** str,char token) {
    char* str2 = strchr(*str,token);
	if(str2 != NULL) {
		*str2 = '\0';
		return str2+1;
	}
	return NULL;
}

int read_one_line(FILE* fp,char** str,int* str_len,int limit) {
    char c;
    int len = 0,i;
    long position = ftell(fp);
    do {
        c=fgetc(fp);
        len++;
        // the line is longer than 1024 characters, informs the caller about this error
        if(limit>=0&&len>limit)
            return 2;
    } while(c!=EOF && c!='\n' && c!='\r');
    fseek(fp,position,SEEK_SET);
    *str=(char*)malloc(sizeof(char)*len);
    for(i=0;i<len-1;i++) {
        c=fgetc(fp);
        (*str)[i]=c;
    }
    (*str)[len-1]='\0';
    *str_len = len;
    c=fgetc(fp);
    c=fgetc(fp);
    fseek(fp,-1,SEEK_CUR); // for some c magic, moving forward and backward solves the compatibility problem between \n and \r\n
    if(c == EOF)
        return 1; // EOF, code 1
    else
        return 0; // not yet EOF, code 0
}

int read_lines(const char* file,char**** lines,int* num_lines,int* num_columns,int*** str_len) {
    char c,*line,*sep;
    int i,col,len,read_success;
    //size_t charRead;
    FILE* fp = fopen(file, "r");
    // file does not exist, error code -1
    if(fp == NULL){
		return -1;
	}
	*num_columns = 0;
    // file is empty, error code -2
    if((c = fgetc(fp)) == EOF)
        return -2;
    fseek(fp,0,SEEK_SET);
    // parse first line and count number of columns
    read_success = read_one_line(fp,&line,&len,1024);
    if(read_success == 2)
        return 1; // the first line is too long, error code 1
    sep = line;
    while (sep != NULL) {
        (*num_columns)++;
        sep = parse_str(&sep,',');
    }
    // there should be at least 1 columns (name), error code -4
    if(*num_columns<1)
        return -4;
    *lines = (char***)malloc(sizeof(char**));
    *str_len = (int**)malloc(sizeof(int*));
    fseek(fp,0,SEEK_SET);
    // read line by line
    i = 0;
    while(read_success == 0) {
        // there are more than 20000 lines in the file, error code -3
        if(i>20000)
            return -3;
        // read a line
        len = 0;
        read_success = read_one_line(fp,&line,&len,1024);
        // the line is longer than 1024 characters, error code is the line number where the error occurs
        if(read_success == 2)
            return i+1;
        //printf("%s\n",line);
        // skip over the line if it is empty
        if(empty_str(line))
            continue;
        *str_len=(int**)realloc(*str_len,(i+1)*sizeof(int*));
        *lines=(char***)realloc(*lines,(i+1)*sizeof(char**));
        (*str_len)[i] = (int*)malloc(sizeof(int)*(*num_columns));
        (*lines)[i] = (char**)malloc(sizeof(char*)*(*num_columns));
        col = 0;
        while(line != NULL) {
            // extra field in this row. error code is the line number where the error occurs
            if(col >= *num_columns)
                return i+1;
            sep = parse_str(&line,',');
            len = strlen(line);
            (*lines)[i][col] = (char*)malloc(sizeof(char)*(len+1));
            strcpy((*lines)[i][col],line);
            (*str_len)[i][col] = len;
            line = sep;
            col++;
        }
        // not enough fields in this row. error code is the line number where the error occurs
        if(col<*num_columns) {
            //printf("bad %d     %d\n",col,*num_columns);
            return i+1;
        }
        i++;
        *num_lines = i;
        //printf("=====%d=====\n",*num_lines);
    }
    if (i < 2)
        return -5;
    fclose(fp);
    return 0;
}

int check_table(char**** lines,int num_lines,int num_columns,int*** str_len) {
    int i,j,quoted;
    for(i=0;i<num_columns;i++) {
        // check if the header us quoted
        if((*lines)[0][i][0]=='"')
            if((*lines)[0][i][(*str_len)[0][i]-1]=='"') {
                quoted = 1; // the field starts and ends with an quote
                if(strcmp((*lines)[0][i],"\"\"")) {
                    // remove quote if the header is not empty
                    (*str_len)[0][i] -= 2;
                    (*lines)[0][i]++;
                    (*lines)[0][i][(*str_len)[0][i]]='\0';
                }
            }
            else return 1; // the field is only partially quoted, error code is the line where this occurs
        else
            if((*lines)[0][i][(*str_len)[0][i]-1]=='"')
                return 1; // the field is only partially quoted, error code is the line where this occurs
            else {
                quoted = 0; // the field is not quoted
                if(strcmp((*lines)[0][i],"")==0) {
                    // add quote if the header is empty
                    (*lines)[0][i] = "\"\"";
                    (*str_len)[0][i] = 2;
                }
            }
        // check each column according to header
        for(j=1;j<num_lines;j++) {
            if((*lines)[j][i][0]=='"')
                if((*lines)[j][i][(*str_len)[j][i]-1]=='"')
                    if(!quoted)
                        return j+1; // the field starts and ends with an quote but its header is not, error code is where this occurs
                    else {
                        // remove quote
                        (*str_len)[j][i] -= 2;
                        (*lines)[j][i]++;
                        (*lines)[j][i][(*str_len)[j][i]]='\0';
                    }
                else return j+1; // the field is only partially quoted, error code is the line where this occurs
            else
                if((*lines)[j][i][(*str_len)[j][i]-1]=='"')
                    return j+1; // the field is only partially quoted, error code is the line where this occurs
                else
                    if(quoted)
                        return j+1; // the field is not quoted but the header is, error code is where this occurs

        }
    }
    for(i=0;i<num_columns-1;i++) {
        for(j=i+1;j<num_columns;j++) {
            if(strcmp((*lines)[0][i],(*lines)[0][j]) == 0) {
                return -1; // there are multiple of the same column header entry
            }
        }
    }
    return 0;
}

void tweet_processor(char*** csvArr, int numRows, int numCols) {
	int i;
	int tweeterColumn = 0;
	int topTenTweeters[10];
	int tweetCount[numRows - 1];		// number of rows - 1 since the header is not a tweet
	char tweeters[numRows - 1][1024];	// 1024 since that is the maximum line length
	int tweetersWorkingSize = 0;
	int topTenWorkingSize = 0;
	int pos = 0;
    bool namePresent = false;
	bool nameMatch = false;
	bool replaced = false;

	// finds the "name" column in the header
	for (i = 0; i < numCols; i++) {
		if (strcmp(csvArr[0][i], "name") == 0) {
			tweeterColumn = i;
            namePresent = true;
		}
	}
    if (!namePresent) {
        printf("Invalid Input Format\n");
        printf("No name column found\n");
        return;
    }

    // compiles a list of tweeters and their respective tweet count
	for (i = 1; i < numRows; i++) {
		int j;
		pos = tweetersWorkingSize;
		nameMatch = false;

		// looks for previously recorded tweeter names
		for (j = 0; j < tweetersWorkingSize; j++) {
			if (strcmp(tweeters[j], csvArr[i][tweeterColumn]) == 0) {
				pos = j;
				nameMatch = true;
			}
		}

		// add new tweeter if tweeter name was not recorded before
		if (nameMatch) {
			tweetCount[pos]++;
		}
		else {
			strcpy(tweeters[pos], csvArr[i][tweeterColumn]);
			tweetersWorkingSize++;
			tweetCount[pos] = 1;
		}
	}

    // place the top 10 tweeters in a sorted list in descending order
	for (i = 0; i < tweetersWorkingSize; i++) {
		int j = 0;
		replaced = false;

		// makes a working list of the top 10 tweeters
		while (!replaced && j < topTenWorkingSize) {
			// places a tweeter in its position
			if (tweetCount[i] > tweetCount[topTenTweeters[j]]) {
				replaced = true;

				// shuffle down
				int k;
				for (k = 9; k > j; k--) {
					topTenTweeters[k] = topTenTweeters[k - 1];
				}

				topTenTweeters[j] = i;

				// working size updated
				if (topTenWorkingSize < 10) {
					topTenWorkingSize++;
				}
			}
			j++;
		}

		// tweet count is smaller than any existing tweeters, but there's still space
		if (!replaced && topTenWorkingSize < 10) {
			topTenTweeters[topTenWorkingSize] = i;
            topTenWorkingSize++;
		}
	}

    // prints results
	for (i = 0; i < topTenWorkingSize; i++) {
		printf("%s: %d\n", tweeters[topTenTweeters[i]], tweetCount[topTenTweeters[i]]);
	}
}

int main(int argc,const char* argv[]) {
	int result,num_lines,num_columns;
	//int i,j;
	int** table_str_length;
	char*** table;
	// debug
	/*argc = 2;
	argv[1] = "cl-tweets-short-ten.csv";*/
	if(argc<2) {
        printf("Input file not specified\n");
        return -5;
	}
	else if(argc>2)
        printf("Warning: only one parameter is accepted\n");
	char* extension = strrchr(argv[1],'.');
    if(extension) {
        if(strcmp(extension,".csv")) {
            printf("File must be a .csv file\n");
            return 0;
        }
    }
    else {
        printf("Argument must be a file\n");
        return 0;
    }
    char* extension = strchr(argv[1],'.');
    if(extension) {
        if(strcmp(extension,".csv")) {
            printf("File must be a .csv file\n");
            return 0;
        }
    }
    result = read_lines(argv[1],&table,&num_lines,&num_columns,&table_str_length);
    if(result) {
        if(result==-1)
            printf("Cannot find file: %s\n",argv[1]);
        else if(result==-3)
            printf("File exceeds 20000 lines\n");
        else if(result==-5)
            return 0;
        else {
            printf("Invalid Input Format\n");
            if(result == -2)
                printf("File is empty\n");
            else if(result == -4)
                printf("File should have at least one column\n");
            else
                printf("Read Error at line %d\n",result);
        }
        return result;
	}
	result = check_table(&table,num_lines,num_columns,&table_str_length);
    if(result > 0) {
        printf("Invalid Input Format\n");
        printf("Read Error at line %d\n",result);
        return 0;
	}
    else if(result == -1) {
        printf("Invalid Input Format\n");
        printf("Multiple column headers share the same name\n");
        return 0;
    }
	// debug, prints out first 10 rows of the char*** table
    /*for(i=0;i<5;i++) {
        for(j=0;j<num_columns;j++) {
            printf("%s\t",table[i][j]);
        }
        printf("\n");
    }*/

    tweet_processor(table,num_lines,num_columns);
	return 0;
}
