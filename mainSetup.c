#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_CHILDREN 50
#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

int foregroundProcess = 0;
int inputRedirection = 0;
int outputRedirection = 0;
int single_bigger_flag = 0;
int double_bigger_flag = 0;
int single_less_flag = 0;
int error_output = 0;
int sizeOfArgs = 0;
int search_without_r = 0;
int search_r = 0;
int bookmark_list = 0;
int bookmark_execute = 0;
int bookmark_execute_index = -1;
int bookmark_delete = 0;
int bookmark_delete_index = -1;
int bookmark_addition = 0;

int all_children[MAX_CHILDREN];
char inputFileName[50];
char outputFileName[50];
char searched[80];
char bookmark_key[80];
int child_count = 0;
  
/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[],int *background)
{
    int length, /* # of characters in the command line */
    i,      /* loop index for accessing inputBuffer array */
    start,  /* index where beginning of next command parameter is */
    ct;     /* index of where to place the next parameter into args[] */
    
    ct = 0;
        
    /* read what the user enters on the command line */
    length = read(STDIN_FILENO,inputBuffer,MAX_LINE);  

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ( (length < 0) && (errno != EINTR) ) {
        perror("error reading the command");
	    exit(-1);           /* terminate with error code of -1 */
    }

	printf("<<%s>>",inputBuffer);
    for (i=0;i<length;i++){ /* examine every character in the inputBuffer */

        switch (inputBuffer[i]){
	    case ' ':
	    case '\t' :               /* argument separators */
		if(start != -1){
            args[ct] = &inputBuffer[start];    /* set up pointer */
		    ct++;
		}
        inputBuffer[i] = '\0'; /* add a null char; make a C string */
		start = -1;
		break;

        case '\n':                 /* should be the final char examined */
		if (start != -1){
            args[ct] = &inputBuffer[start];     
		    ct++;
		}
        inputBuffer[i] = '\0';
        args[ct] = NULL; /* no more arguments to this command */
		break;

	    default :             /* some other character */
		if (start == -1)
		    start = i;
            if (inputBuffer[i] == '&'){
		    *background  = 1;
            inputBuffer[i-1] = '\0';
		}
	} /* end of switch */
     }    /* end of for */
     args[ct] = NULL; /* just in case the input line was > 80 */
     sizeOfArgs = ct;

	for (i = 0; i <= ct; i++)
		printf("args %d = %s\n",i,args[i]);
    
} /* end of setup routine */

void inputRedirect() {

    int file_descriptor;

    if ( inputRedirection == 1 ) {

        if ( single_less_flag == 1 ) {
            
            file_descriptor = open(inputFileName, O_RDONLY, 0777);

            if ( file_descriptor == -1 ) {
                fprintf(stderr,"There is a problem with opening inputFile\n");
                return;
            }

            if (dup2(file_descriptor,STDIN_FILENO) == -1) {
                fprintf(stderr,"There is a problem with changing file descriptors.\n");
                return;
            }

            if (close(file_descriptor) == -1)  {
                fprintf(stderr,"There is a problem with closing the opening the file.\n");
                return;                
            }
        }

    }
}

void outputRedirect() {

    int file_descriptor;

    if ( outputRedirection == 1 ) {

        if (single_bigger_flag == 1) {
            file_descriptor = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        }
        else if (double_bigger_flag == 1) {
            file_descriptor = open(outputFileName, O_WRONLY | O_CREAT | O_APPEND, 0777);
        }
        else if (error_output == 1) {
            file_descriptor = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0777);

            if (file_descriptor == -1) {
                fprintf(stderr,"There is a problem with creating file or cleaning existing content in the file.\n");
                return;
            }

            if (dup2(file_descriptor,STDERR_FILENO) == -1) {
                fprintf(stderr,"There is a problem with changing file descriptors.\n");
                return;
            }

            if (close(file_descriptor) == -1) {
                fprintf(stderr,"There is a problem with closing the opening the file.\n");
                return;
            }
        }

        if (file_descriptor == -1) {
            fprintf(stderr,"There is a problem with creating file or cleaning existing content in the file.\n");
            return;            
        }

        if (dup2(file_descriptor,STDOUT_FILENO) == -1) {
            fprintf(stderr,"There is a problem with changing file descriptors.\n");
            return;            
        }

        if(close(file_descriptor) == -1) {
            fprintf(stderr,"There is a problem with closing the opening the file.\n");
            return;            
        }

    }
}

bool checkRedirection(char* args[]) {

    int index;
    for(index = 0 ; index < sizeOfArgs ; index++) {

        // myprog [args] > file.out NULL
        if (strcmp(args[index],">") == 0) {
            if(index != 0 && index == sizeOfArgs - 2 && args[index + 1] != NULL && args[index + 2] == NULL) {
                strcpy(outputFileName,args[index + 1]);
                outputRedirection = 1;
                single_bigger_flag = 1;
                break;
            }
            else {
                fprintf(stderr,"Your entered command doesn't match pattern of myprog [args] > file.out \n");
                return false;
            }
        }

        // myshell:  myprog [args] >> file.out NULL
        else if (strcmp(args[index],">>") == 0) {
            if(index != 0 && index == sizeOfArgs - 2 && args[index + 1] != NULL && args[index + 2] == NULL) {
                strcpy(outputFileName,args[index + 1]);
                outputRedirection = 1;
                double_bigger_flag = 1;
                break;
            }
            else {
                fprintf(stderr,"Your entered command doesn't match pattern of myprog [args] >> file.out\n");
                return false;
            }
        }
        // myshell:  myprog [args] < file.in NULL
        else if (strcmp(args[index],"<") == 0) {
            if (index != 0 && index == sizeOfArgs - 2 && args[index + 1] != NULL && args[index + 2] == NULL) {
                strcpy(inputFileName,args[index + 1]);
                inputRedirection = 1;
                single_less_flag = 1;
                break;
            }
            else {
                fprintf(stderr,"Your entered command doesn't match pattern of myprog [args] < file.in \n");
                return false;
            }
        }

        // myprog [args] 2> file.out NULL
        else if (strcmp(args[index],"2>") == 0) {
            if(index != 0 && index == sizeOfArgs - 2 && args[index + 1] != NULL && args[index + 2] == NULL) {
                strcpy(outputFileName,args[index + 1]);
                outputRedirection = 1;
                error_output = 1;
                break;
            }
            else {
                fprintf(stderr,"You entered command doesn't match pattern of myprog [args] 2> file.out\n");
                return false;
            }
        }
    }
    
    if (index != sizeOfArgs) {
        if (strcmp(args[index],">") == 0 ||strcmp(args[index],">>") == 0 || strcmp(args[index],"<") == 0 || strcmp(args[index],"2>") == 0 ) {
            int totalSize = sizeOfArgs;
            for(int j = index ; j < totalSize ; j++) {
                if(args[j] != NULL) {
                    args[j] = '\0';
                    sizeOfArgs -= 1;
                }
            }
        }
    }
    return true;
}

bool checkExecutableOrNot(char *path) {


    struct stat fd_info; 
    int result = stat(path,&fd_info);
    if ( result >= 0 && !S_ISDIR(fd_info.st_mode) && (S_IXUSR & fd_info.st_mode ||  fd_info.st_mode & 010 || fd_info.st_mode & 001) ) {
        return true;
    }
    else {
        return false;
    } 
}

bool findPath(char* currentP, char* exeFile) {

    char* all_paths;
    char* path;
    char currentPath[PATH_MAX + 1];
    bool Is_Executable = false;

    if ( strchr(exeFile, '/') != NULL ) {  
        if ( access(exeFile, F_OK) != -1 ) {
            Is_Executable = checkExecutableOrNot(exeFile);
            sprintf(currentP, "%s", exeFile);
        }
        return Is_Executable;        
    }


    all_paths = strdup(getenv("PATH"));



    path = strtok(all_paths,":");
    while (path != NULL && Is_Executable == false) {
        if(path[strlen(path) - 1] == '/') {
            sprintf(currentPath, "%s%s", path,exeFile);
        }
        else {
           sprintf(currentPath, "%s/%s", path,exeFile); 
        }
        Is_Executable = checkExecutableOrNot(currentPath);
        path = strtok(NULL,":");
    }

    sprintf(currentP, "%s", currentPath);
    free(all_paths);

    return Is_Executable;
    
}

struct Node {
    char data[100];
    struct Node* nextPtr;
};

typedef struct Node Bookmark;
typedef Bookmark* BookmarkPtr;

void deleteBookmark( int index, BookmarkPtr* startPtr) {
    if (*startPtr == NULL) {
        printf("List is empty.\n");
        return;
    }

    if (index == 0) {
        BookmarkPtr temp = *startPtr;
        *startPtr = (*startPtr)->nextPtr;
        free(temp);
        return;
    }

    int count = 0;
    BookmarkPtr current = *startPtr;
    while (count < index - 1 && current != NULL) {
        count++;
        current = current->nextPtr;
    }

    if (current == NULL || current->nextPtr == NULL) {
        printf("There is no bookmark at the specified index.\n");
        return;
    }

    BookmarkPtr temp = current->nextPtr;
    current->nextPtr = temp->nextPtr;
    free(temp);
}

void addBookmark(BookmarkPtr* startPtr, char progName[]) {
    BookmarkPtr newNode = (BookmarkPtr)malloc(sizeof(Bookmark));

    if (newNode == NULL) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
    }

    strcpy(newNode->data, progName);
    newNode->nextPtr = NULL;

    if (*startPtr == NULL) {
        *startPtr = newNode;
    } else {
        BookmarkPtr current = *startPtr;
        while (current->nextPtr != NULL) {
            current = current->nextPtr;
        }
        current->nextPtr = newNode;
    }
}

void printListBookmark(BookmarkPtr* startPtr) {
    if (*startPtr == NULL) {
        printf("List is empty.\n");
        return;
    }

    int i = 0;
    BookmarkPtr current = *startPtr;
    while (current != NULL) {
        printf("%d \"%s\"\n",i, current->data);
        current = current->nextPtr;
        i++;
    }
}

void checkAndAddBookmark(BookmarkPtr* startPtr,char * exe) {

    char path[PATH_MAX];
    if (findPath(path,exe)) {
        addBookmark(&(*startPtr),exe);
    }
    else {
        fprintf(stderr,"There is no available executable file \n");
    }

}

void runBookmarkIndex(int index, BookmarkPtr *startPtrBookmark) {
    int count = 0;


    BookmarkPtr current = *startPtrBookmark;
    while (current != NULL && count < index) {
        current = current->nextPtr;
        count++;
    }


    if (count == index && current != NULL) {

        char command[MAX_LINE];

        strcpy(command,current->data);


        int result = system(command);

        if (result == 0) {
            printf("Command executed successfully: %s\n", command);
        } else {
            fprintf(stderr, "Error executing command: %s\n", command);
        }

    } else {
        fprintf(stderr, "Invalid index or bookmark not found.\n");
    }
}

struct backgroundProcess {
    
    struct backgroundProcess* nextPtr;
    pid_t processId;
    struct backgroundProcess* previousPtr;

};

typedef struct backgroundProcess BackgroundProcess;
typedef BackgroundProcess* BackGroundProcessPtr;


void addProcess(pid_t childpid, BackGroundProcessPtr *startPtr) {

    BackGroundProcessPtr newProcess = malloc(sizeof(BackgroundProcess));

    if(newProcess != NULL) {
        
        newProcess->previousPtr = NULL;
        newProcess->processId = childpid;
        newProcess->nextPtr = NULL;

        // If the list is empty
        if( *startPtr == NULL) {
            *startPtr = newProcess;
        }
        // If the list is not empty
        else {
            BackGroundProcessPtr currentPtr = *startPtr;
            while (currentPtr->nextPtr != NULL) {
                currentPtr = currentPtr->nextPtr;
            }
            currentPtr->nextPtr = newProcess;
            newProcess->previousPtr = currentPtr;
        }

    }
    else {
        fprintf(stderr,"There is a problem with allocation of memory");
    }
}

bool isBackGroundProcessEmpty(BackGroundProcessPtr* startPtr) {

    if (*startPtr == NULL) {
        return true;
    }
    else {
        return false;
    }
}

void deleteProcess(BackGroundProcessPtr* startPtr) {
    
    BackGroundProcessPtr currentPtr = *startPtr;
    BackGroundProcessPtr previousPtr = NULL;

    while (currentPtr != NULL) {
        if (currentPtr == *startPtr && waitpid(currentPtr->processId,NULL,WNOHANG) != 0) {
            *startPtr = (*startPtr)->nextPtr;
            free(currentPtr);
            currentPtr = *startPtr;
        } else {
            while (currentPtr != NULL && waitpid(currentPtr->processId,NULL,WNOHANG) == 0) {
                previousPtr = currentPtr;
                currentPtr = currentPtr->nextPtr;
            }

            if (currentPtr != NULL) {
                if (currentPtr->nextPtr == NULL) {
                    previousPtr->nextPtr = NULL;
                } else {
                    previousPtr->nextPtr = currentPtr->nextPtr;
                    currentPtr->nextPtr->previousPtr = previousPtr;
                }
                free(currentPtr); 
                currentPtr = previousPtr->nextPtr; 
            }
        }
    }
}


void printAllBackgroundProcess(BackGroundProcessPtr *startPtr) {

    BackGroundProcessPtr currentPtr = *startPtr;
    while (currentPtr != NULL) {
        printf("The process ID of BackgroundProcess is : %d\n",currentPtr->processId);
        currentPtr = currentPtr -> nextPtr;
    }
}

void findAllChildren(int pid) {
    char filename[256];
    sprintf(filename, "/proc/%d/task", pid);


    DIR *dir = opendir(filename);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (isdigit(entry->d_name[0])) {
                int tid = atoi(entry->d_name);
                sprintf(filename, "/proc/%d/task/%d/children", pid, tid);
                FILE *file = fopen(filename, "r");
                if (file != NULL) {
                    int child_pid;
                    while (fscanf(file, "%d", &child_pid) == 1) {
                        all_children[child_count++] = child_pid;

                        findAllChildren(child_pid);
                    }
                    fclose(file);
                }
            }
        }
        closedir(dir);
    }
}

void stopAllChildProcess() {

    for(int i = 0; i < child_count ; i++) {
        printf("Giren Process ID'si : %d\n",all_children[i]);
        kill(all_children[i],SIGTSTP);
    }

    child_count = 0;
}

void parentProcess(int* background, pid_t childProcess, BackGroundProcessPtr* startPtr) {
    
    // background-Process
    if (*background == 1) {
        addProcess(childProcess,&(*startPtr));
        sleep(5);

    }
    // foreground-Process
    else { 
        foregroundProcess = childProcess;
        if (childProcess != waitpid(childProcess, NULL, WUNTRACED)) {
            fprintf(stderr,"There is a error that child process don't wait the finish of the execution of child process.\n");
        } 
    }
}

void createProcess(char* currentPath, char* args[], int* background,BackGroundProcessPtr* startPtr) {

    pid_t childProcess = fork();

    if(childProcess > 0) { // Parent Part
        parentProcess(&(*background),childProcess,&(*startPtr));
    }

    else if (childProcess == 0) { // Child Part
        
        if(inputRedirection == 1) {
            inputRedirect();
        }
        else if (outputRedirection == 1) {
            outputRedirect();
        }        
        execv(currentPath,args);
    }

    else { // Error Part
        fprintf(stderr, "There is an error with creation of child-process.\n");
    }

}

void handle_TSTP_Signal(int signal) {
    
    if(foregroundProcess == 0 || waitpid(foregroundProcess,NULL,WNOHANG) == -1) {
        printf("\nmyshell: ");
		fflush(stdout);
		return;
    }

    findAllChildren(foregroundProcess);
    stopAllChildProcess();
    kill(foregroundProcess,SIGTSTP);
    foregroundProcess = 0;

}


char *trimwhitespace(char *str) {

    while(isspace((unsigned char)*str)) {
        str++;
    } 

    if(*str == 0) 
        return str;

    return str;
}

void searchKeywordInFiles(char* keyword) {
   
    DIR *directory;
    struct dirent *entry;

    directory = opendir(".");
    if (directory == NULL) {
        fprintf(stderr,"Unable to open directory\n");
    }
    else {

        while ((entry = readdir(directory)) != NULL) {

            size_t len = strlen(entry->d_name);

            if (len > 2 && (
                (strcmp(entry->d_name + len - 2, ".c") == 0) ||
                (strcmp(entry->d_name + len - 2, ".C") == 0) ||
                (strcmp(entry->d_name + len - 2, ".h") == 0) ||
                (strcmp(entry->d_name + len - 2, ".H") == 0)
            )) {

                FILE *file = fopen(entry->d_name, "r");
                if (file == NULL) {
                    fprintf(stderr,"Unable to open file");
                    continue;
                }

                char line[MAX_LINE];
                int lineNumber = 1;

                while (fgets(line, sizeof(line), file) != NULL) {
                    if (strstr(line, keyword) != NULL) {
                        strcpy(line,trimwhitespace(line));
                        printf("%d: %s -> %s", lineNumber, entry->d_name, line);
                    }
                    lineNumber++;
                }
                fclose(file);
            }
        }
    }
    closedir(directory);
}

    
void searchInDirectory(char *Path,char *searchString) {
    
    
    DIR *dir = opendir(Path);
    if (dir == NULL) {
        fprintf(stderr,"Unable to open directory\n");
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        char filePath[PATH_MAX];
        snprintf(filePath, sizeof(filePath), "%s/%s", Path, entry->d_name);
        struct stat statbuf;
        stat(filePath,&statbuf);

        if (S_ISREG(statbuf.st_mode)) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL && (strcmp(ext, ".c") == 0 || strcmp(ext, ".C") == 0 || 
                                strcmp(ext, ".h") == 0 || strcmp(ext, ".H") == 0)) {
                    FILE *file = fopen(filePath, "r");
                    if (file == NULL) {
                        fprintf(stderr,"Unable to open file");
                        return;
                    }
                    char line[MAX_LINE];
                    int lineNumber = 0;

                    while (fgets(line, sizeof(line), file) != NULL) {
                        lineNumber++;

                    if (strstr(line, searchString) != NULL) {
                        strcpy(line,trimwhitespace(line));
                        printf("%d: %s -> %s", lineNumber, filePath , line);
                    }
                }
                fclose(file);
            }
        } 
        else if (S_ISDIR(statbuf.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            searchInDirectory(filePath, searchString);
        }
    }
    closedir(dir);
}

bool checkValid(char* args[]) {


    if (strcmp(args[0],"search") == 0) {

        if (sizeOfArgs == 1) {
            fprintf(stderr, "You enter just search argument, type other parameters.\n");
            return false;
        }

        if (sizeOfArgs > 1 && strcmp(args[1],"-r") != 0) {
            if(sizeOfArgs == 2) {
                char* secondParameter = args[1];
                int length = strlen(secondParameter);
                if(length >= 3 && secondParameter[0] == '\"' && secondParameter[length - 1] == '\"') {
                    strcpy(searched,secondParameter);
                    search_without_r = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as search \"searched_keyword\".\n");
                    return false;
                }
            }
            else {
                if(args[1][0] == '\"' && args[sizeOfArgs - 1][strlen(args[sizeOfArgs - 1]) - 1] == '\"') {
                    strcat(searched,args[1]);
                    for(int j = 2 ; j < sizeOfArgs ; j++) {
                        strcat(searched, " ");
                        strcat(searched,args[j]);
                    }
                    search_without_r = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as search \"searched_keyword\".\n");
                    return false;
                }

            }
        }

        if (sizeOfArgs > 1 && strcmp(args[1],"-r") == 0) {

            if (sizeOfArgs == 2) {
                fprintf(stderr,"You enter just search -r argumentü type other arguments.\n");
                return false;
            }

            if (sizeOfArgs == 3) {
                char* thirdParameter = args[2];
                int length = strlen(thirdParameter);
                if(length >= 3 && thirdParameter[0] == '\"' && thirdParameter[length - 1] == '\"') {
                    strcpy(searched,thirdParameter);
                    search_r = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as search -r \"searched_keyword\".\n");
                    return false;
                }
            }

            else {
                
                if(args[2][0] == '\"' && args[sizeOfArgs - 1][strlen(args[sizeOfArgs - 1]) - 1] == '\"') {
                    strcat(searched,args[2]);
                    for(int j = 3 ; j < sizeOfArgs ; j++) {
                        strcat(searched, " ");
                        strcat(searched,args[j]);
                    }
                    search_r = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as search -r \"searched_keyword\".\n");
                    return false;
                }                
            }
        }
    }

    // For bookmark part
    else {

        if (sizeOfArgs == 1) {
            fprintf(stderr, "You enter just bookmark argument, type other parameters.\n");
            return false;
        }

        if (sizeOfArgs == 2 && strcmp(args[1],"-l") == 0) {
            // to list
            bookmark_list = 1;
            return true;
        }

        if (sizeOfArgs == 3) {
            int index = atoi(args[2]);

            if (strcmp(args[1],"-i") == 0) {
                bookmark_execute_index = index;
                bookmark_execute = 1;
                return true;
            }

            if (strcmp(args[1],"-d") == 0) {
                bookmark_delete_index = index;
                bookmark_delete = 1;
                return true;
            }
            
        }

        if(args[1][0] == '\"' && args[sizeOfArgs - 1][strlen(args[sizeOfArgs - 1]) - 1] == '\"') {
            if(sizeOfArgs == 2) {
                char* secondParameter = args[1];
                int length = strlen(secondParameter);
                if(length >= 3 && secondParameter[0] == '\"' && secondParameter[length - 1] == '\"') {
                    strcpy(bookmark_key,secondParameter);
                    bookmark_addition = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as bookmark \"keyword\".\n");
                    return false;
                }
            }
            else {
                if(args[1][0] == '\"' && args[sizeOfArgs - 1][strlen(args[sizeOfArgs - 1]) - 1] == '\"') {
                    strcat(bookmark_key,args[1]);
                    for(int j = 2 ; j < sizeOfArgs ; j++) {
                        strcat(bookmark_key, " ");
                        strcat(bookmark_key,args[j]);
                    }
                    bookmark_addition = 1;
                    return true;
                }
                else {
                    fprintf(stderr,"Please enter a valid format as bookmark \"keyword\".\n");
                    return false;
                }

            }
        }
        else {
        printf("Please enter a valid format for bookmark\n");
            return false;
        }
    }

}

void trim(char *str) {
    if (str == NULL) {
        fprintf(stderr,"The given input is NULL\n");
        return;
    }


    int start = 1, end = strlen(str) - 1;

    while (str[start] == '"') {
        start++;
    }


    while (end > start && str[end] == '"') {
        end--;
    }

    int length = end - start + 1;
    memmove(str, str + start, length);
    str[length] = '\0';
}

int main(void) {
    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2 + 1]; /*command line arguments */
    char currentPath[PATH_MAX + 1];
    char* path ;
    char* executionFile;

    signal(SIGTSTP,handle_TSTP_Signal);
    BackGroundProcessPtr startPtr = NULL;
    BookmarkPtr start = NULL;

        while (1) {
            background = 0;
            printf("myshell: ");
            fflush(0);
            /*setup() calls exit() when Control-D is entered */
            setup(inputBuffer, args, &background);


            if (args[0] == NULL) {
                continue;
            }

            executionFile = args[0];

            // Checks the given input args whether they are valid or not.
            if ( checkRedirection(args) ) {
                // do search operation
                if (strcmp(args[0],"search") == 0) {                    
                    if (checkValid(args)) {
                        trim(searched);
                        if(search_without_r == 1) {
                            searchKeywordInFiles(searched);
                        }
                        else if (search_r == 1) {
                            char cwd[PATH_MAX];
                            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                                searchInDirectory(cwd,searched);
                            }
                        }
                    }
                }
                // do bookmark operation
                else if (strcmp(args[0],"bookmark") == 0) {
                    if (checkValid(args)) {

                        if (bookmark_addition == 1) {
                            trim(bookmark_key);
                            checkAndAddBookmark(&start,bookmark_key);
                        }

                        else if (bookmark_list == 1) {
                            printListBookmark(&start);
                        }

                        else if (bookmark_execute == 1) {
                            runBookmarkIndex(bookmark_execute_index,&start);
                        }

                        else if (bookmark_delete == 1) {
                            deleteBookmark(bookmark_delete_index,&start);
                        }
                    }
                }

                // do exit operation
                else if (strcmp(args[0],"exit") == 0) {
                    if( isBackGroundProcessEmpty(&startPtr) ) {
                        printf("Exit the Program succesffuly!\n");
                        break;
                    }
                    else {
                        deleteProcess(&startPtr);
                        if(isBackGroundProcessEmpty(&startPtr)) {
                            printf("Exit the Program succesffuly!\n");
                            break;                            
                        }
                        else {
                            fprintf(stderr,"There are some programs running.\n");
                        }
                    }
                }
                else if (findPath(currentPath,executionFile)) {

                    if(strcmp(args[sizeOfArgs - 1],"&") == 0) {
                        args[sizeOfArgs - 1] = '\0';
                        sizeOfArgs -= 1;
                    }
                    createProcess(currentPath,args,&background,&startPtr);

                }

                else {
                    fprintf(stderr,"There is no executable file.\n");
                }

                inputRedirection = 0;
                outputRedirection = 0;
                single_bigger_flag = 0;
                double_bigger_flag = 0;
                single_less_flag = 0;
                error_output = 0;
                inputFileName[0] = '\0';
                outputFileName[0] = '\0';
                currentPath[0] = '\0';
                strcpy(searched,"");
                strcpy(bookmark_key,"");
                background = 0;
                search_r = 0;
                search_without_r = 0;
                bookmark_list = 0;
                bookmark_execute = 0;
                bookmark_execute_index = -1;
                bookmark_delete = 0;
                bookmark_delete_index = -1;
                bookmark_addition = 0;

            }
            else {
                continue;
            }            
        }
}
