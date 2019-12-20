//Name: Archit Jaiswal
//      Christian Teeples




#define _GNU_SOURCE


#include <time.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdint.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments


#define BLOCK_SIZE 8192
#define NUM_BLOCKS 4226
#define NUM_FILES 128
#define MAX_INODE_BLOCKS 1250     // maximium number of blocks Inode can address
#define MAX_FILE_SIZE 10240000    //1250*8192  (blocks* BLOCK_SIZE)

int put        (char* fileName);
int get        (char* fileName, char* newFileName);
int df         ();
void list      ();
int attri      (char* fileName, char* att);
int del        (char* fileName);

FILE *fd;

uint8_t blocks[NUM_BLOCKS][BLOCK_SIZE];


struct Directory_Entry
{
  int8_t   valid;
  char     name[255];
  uint32_t inode;
};

struct Inode
{
  uint8_t   valid;
  time_t    Date;
  uint8_t   attributes;
  uint32_t  size;
  uint32_t  blocks[MAX_INODE_BLOCKS];
};

struct Directory_Entry * dir;
uint8_t                * freeBlockList;
uint8_t                * freeInodeList;
struct Inode           * inodeList;
struct stat              buf; // stat struct to hold the returns from the stat call




void initializeBlockList()
{
  int i;
  for(i =0; i< NUM_BLOCKS; i++)
  {
    freeBlockList[i] = 1;
  }
}

void initializeInodeList()
{
  int i;
  for(i = 0; i < NUM_FILES; i++)
  {
    freeInodeList[i] = 1;
  }
}

void initializeDirectory()
{
  int i;
  for(i=0 ; i<NUM_FILES ; i++)
  {
    dir[i].valid = 0; // it is not being used
    memset(dir[i].name, 0, 255);
    dir[i].inode = -1;
  }
}

void initializeInodes()
{
  int i;
  // inode goes from blocks 3 to 131 so 131 - 3 = 128.
  for(i=0 ; i<128 ; i++)
  {
    int j;
    inodeList[i].attributes = 0;
    inodeList[i].size = 0;
    inodeList[i].valid = 0;
    inodeList[i].Date = 0;

    for( j = 0 ; j < MAX_INODE_BLOCKS; j++)
    {
      inodeList[i].blocks[j] = -1;
    }
  }
}

int findFreeInode()
{
  int ret = -1;
  int i;
  // their are only 128 inodes according to the instructions in PDF
  for(i = 0 ; i < NUM_FILES; i++)
  {
    if( freeInodeList[i] == 1)
    {
      ret = i;
      freeInodeList[i] = 0;
      break;
    }
  }
  return ret;
}

int findFreeDirectory(char* fileName)
{
  int ret =-1;
  int i;

  for (i =0 ; i < NUM_FILES; i++)
  {
    if(strcmp(fileName, dir[i].name) == 0)
    {
      return i;
    }
  }

  for( i = 0 ; i < NUM_FILES; i++)
  {
    if(dir[i].valid == 0)
    {
      ret = i;
      dir[i].valid =1; // setting it to used status
      break;
    }
  }
  return ret;
}

int findFreeBlock()
{
  int ret =  -1;
  int i;

  for(i = 132 ; i < NUM_BLOCKS; i++)
  {
    if( freeBlockList[i] == 1)
    {
      ret =i;
      freeBlockList[i] = 0;
      break;
    }
  }
  return ret;
}

void createfs (char* filename)
{
  fd = fopen (filename, "w");
  memset(blocks, 0, NUM_BLOCKS * BLOCK_SIZE);
  initializeDirectory();
  initializeInodeList();
  initializeBlockList();
  initializeInodes();
  fwrite(&blocks[0], BLOCK_SIZE, NUM_BLOCKS, fd);
  fclose(fd);

}

// list function
void list()
{
 int i;
 int flag =0;
 for(i=0; i< NUM_FILES; i++)
 {
  if(dir[i].valid == 1)
  {
    flag =1;
    break;
  }
  else
  {
    printf("List: No file found\n" );
    return;
  }
 }

 if (flag == 1)
 {
   for(i=0; i< NUM_FILES; i++)
   {
  	 //if valid and if not hidden
  	 if(dir[i].valid ==1 && inodeList[dir[i].inode].attributes != 1 ) // and add the attribute checking here
  	 {
  		 int inode_idx = dir[i].inode;
  		 printf("%d %s\n", inodeList[inode_idx].size, dir[i].name);
  	 }
   }
 }
}


void open( char* filename)
{
  fd = fopen(filename, "r");
  fread(blocks, BLOCK_SIZE, NUM_BLOCKS, fd);
  fclose(fd);
}

void close(char* fileName)
{
  fd = fopen(fileName, "w");
  fwrite(blocks, BLOCK_SIZE, NUM_BLOCKS, fd);
  fclose(fd);
}

int main()
{
  //directory is the pointer which points to block zero
  dir           = (struct Directory_Entry*) &blocks[0];
  freeInodeList = (uint8_t*)                &blocks[7];
  freeBlockList = (uint8_t*)                &blocks[8];
  inodeList     = (struct Inode*)           &blocks[9]; // it keeps track of inodes in files

  initializeDirectory();
  initializeInodeList();
  initializeBlockList();
  initializeInodes();

// code from msh.c file, it was bash shell assignment
// just using string handling from it
// starts from here

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  while( 1 )
  {
    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );


    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;
    int childPID;
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;
    char *working_str;


    working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
    (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );

      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }

      token_count++;
    }

    if(token[0] != '\0') // if the input is empty space then re-prompt for the input
    {

      int status;

      // first check if the command is exit
      if( (strcmp( token[0], "exit" ) == 0) || (strcmp( token[0], "quit") == 0) )
      {
        exit(0);
      }
      else if (strcmp(token[0], "put") == 0)
      {
        put(token[1]);
      }
      else if(strcmp(token[0], "list")==0)
      {
        list();
      }
      else if(strcmp(token[0], "createfs") == 0)
      {
        createfs(token[1]);
      }
      else if(strcmp(token[0], "get") == 0 )
      {
        get(token[1], token[2]);
      }
      else if(strcmp(token[0], "attrib") == 0)
      {
        attrib(token[1], token[2]);
      }
      else if(strcmp(token[0], "del") == 0 )
      {
        del(token[1]);
      }
      else if(strcmp(token[0], "df") == 0 )
      {
        printf("%d bytes free\n",df() );
      }

    }

    free( working_root );
  }

  free(cmd_str);


  return 0;
}


// df command
int df()
{
  int j = 130;
  int counting = 0;
  for( j = 130; j < NUM_BLOCKS; j++)
  {
    if(freeBlockList[j] == 1)
    {
      counting++;
    }
  }
  return counting * BLOCK_SIZE;
}

// put command
int put(char* fileName)
{


  int status =  stat( fileName, &buf ); //check if the file exists or Not

  if( status != -1 )
  {
    if(buf.st_size > MAX_FILE_SIZE)
    {
      printf("Error: file size too big\n" );
      return;
    }
    else if ( buf.st_size > df() )
    {
      printf("Error: Not enought free space in file system \n");
      return;
    }

    // if file is not too big then find the free directory entry
    int freeDirectoryIndex = findFreeDirectory(fileName);

    // checking the file name length
    if(strlen(fileName) <= 255)
    {
      strcpy(dir[freeDirectoryIndex].name , fileName);
    }
    else
    {
      printf("Error: file name is TOO BIG! \n");
      return;
    }

    // check if all the directory entries are filled
    // till the maximum capacity of 128 files as per PDF
    if (freeDirectoryIndex == -1 )
    {
      printf("Error: No more than 128 files \n");
      return;
    }

    // check if their is any inode available
    int freeInodeIndex = findFreeInode();

    // if no free inodes then error
    if (freeInodeIndex == -1 )
    {
      printf("Error: No free Inodes are available \n");
      return;
    }

    dir[freeDirectoryIndex].valid = 1;
    dir[freeDirectoryIndex].inode = freeInodeIndex;
    inodeList[freeInodeIndex].size = buf.st_size;
    inodeList[freeInodeIndex].Date = time(NULL);
    strcpy(dir[freeDirectoryIndex].name, fileName);
    inodeList[freeInodeIndex].valid = 1;
    inodeList[freeInodeIndex].Date = buf.st_ctime;
    freeInodeList[freeInodeIndex] = 0;

    // Open the input file read-only
    FILE *ifp = fopen ( fileName, "r" );
    printf("Reading %d bytes from %s\n", (int) buf.st_size, fileName );

    // Save off the size of the input file since we'll use it in a couple of places and
    // also initialize our index variables to zero.
    int copy_size   = buf.st_size;

    // We want to copy and write in chunks of BLOCK_SIZE. So to do this
    // we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
    // We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
    int offset      = 0;

    int blockIdx =0 ; // iterates through inodeList.blocks

    // copy_size is initialized to the size of the input file so each loop iteration we
    // will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
    // BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
    // we have copied all the data from the input file.
    while( copy_size > 0 )
    {
      // We are going to copy and store our file in BLOCK_SIZE chunks instead of one big
      // memory pool. Why? We are simulating the way the file system stores file data in
      // blocks of space on the disk. block_index will keep us pointing to the area of
      // the area that we will read from or write to.
      int block_index = findFreeBlock();

      // check if their is a free block available
      // if not then exit
      if (findFreeBlock() == -1)
      {
        printf("Error: No free Inodes are available \n");
        return;
      }

      // Index into the input file by offset number of bytes.  Initially offset is set to
      // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We
      // then increase the offset by BLOCK_SIZE and continue the process.  This will
      // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
      fseek( ifp, offset, SEEK_SET );

      // Read BLOCK_SIZE number of bytes from the input file and store them in our
      // data array.
      int bytes  = fread( &blocks[block_index], BLOCK_SIZE, 1, ifp );

      // If bytes == 0 and we haven't reached the end of the file then something is
      // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
      // It means we've reached the end of our input file.
      if( bytes == 0 && !feof( ifp ) )
      {
        printf("An error occured reading from the input file.\n");
        return -1;
      }

      // Clear the EOF file flag.
      clearerr( ifp );
      freeBlockList[block_index] = 0;

      // Reduce copy_size by the BLOCK_SIZE bytes.
      copy_size -= BLOCK_SIZE;

      // Increase the offset into our input file by BLOCK_SIZE.  This will allow
      // the fseek at the top of the loop to position us to the correct spot.
      offset    += BLOCK_SIZE;

      inodeList[freeInodeIndex].blocks[blockIdx++] = block_index;

    }

    // We are done copying from the input file so close it out.
    fclose( ifp );

  }
  else
  {
        printf("File does not exist!\n" );
  }
}

// get command
int get (char* fileName, char* newFileName)
{
  int i;
  int flag =0 ;
  int inode_index =0;


  for ( i = 0; i< NUM_FILES; i++)
  {
    if(strcmp(dir[i].name, fileName) ==0 )
    {
      flag = 1;
      inode_index = dir[i].inode;
    }
  }

  if(flag = 0)
  {
    printf("Error: file not found\n" );
    return -1;
  }

  FILE *ofp;
// after the file exists in the directory
  if(newFileName == NULL)
  {
    ofp = fopen( fileName, "w");
    strcpy(newFileName, fileName);
  }
  else
  {
    ofp = fopen( newFileName, "w");
  }
/////this is the code from course github//////////////////

    //*********************************************************************************
    //
    // The following chunk of code demonstrates similar functionality to your get command
    //

    // Initialize our offsets and pointers just we did above when reading from the file.
    int block_index = 0;
    int copy_size   = inodeList[inode_index].size;
    int offset      = 0;

    printf("Writing %d bytes to %s\n", (int) buf.st_size, newFileName );

    // Using copy_size as a count to determine when we've copied enough bytes to the output file.
    // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
    // our stored data to the file fp, then we will increment the offset into the file we are writing to.
    // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
    // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
    // last iteration we'd end up with gibberish at the end of our file.
    while( copy_size > 0 )
    {

      int num_bytes;

      // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
      // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
      // end up with garbage at the end of the file.
      if( copy_size < BLOCK_SIZE )
      {
        num_bytes = copy_size;
      }
      else
      {
        num_bytes = BLOCK_SIZE;
      }

      // Write num_bytes number of bytes from our data array into our output file.
      fwrite( &blocks[inodeList[inode_index].blocks[block_index]], num_bytes, 1, ofp );

      // Reduce the amount of bytes remaining to copy, increase the offset into the file
      // and increment the block_index to move us to the next data block.
      copy_size -= BLOCK_SIZE;
      offset    += BLOCK_SIZE;
      block_index ++;

      // Since we've copied from the point pointed to by our current file pointer, increment
      // offset number of bytes so we will be ready to copy to the next area of our output file.
      fseek( ofp, offset, SEEK_SET );
    }

    // Close the output file, we're done.
    fclose( ofp );
/////////////////////////////////here github code ends //////////////////////
}

int attrib (  char* att, char* fileName)
{
  // 0 - none
  // 1 - hidden
  // 2 - read only
  // 3 - hidden and readonly
  int i;
  int flag =0;
  int inodeIndex = -1;

  for(i=0 ; i < NUM_FILES; i++)
  {
    if(strcmp(dir[i].name, fileName) == 0)
    {
      flag = 1;
      inodeIndex = dir[i].inode;
      break;
    }
    else
    {
      printf("Error: filename does not exits\n" );
      return -1;
    }
  }

  if(att[0] == '+')
  {
    if(att[1] == 'h')
    {
      if(inodeList[inodeIndex].attributes == 2)
      {
        inodeList[inodeIndex].attributes = 3;
      }
      else
      {
        inodeList[inodeIndex].attributes = 1;
      }
    }
    else if(att[1] == 'r')
    {
      if(inodeList[inodeIndex].attributes == 1)
      {
        inodeList[inodeIndex].attributes = 3;
      }
      else
      {
        inodeList[inodeIndex].attributes = 2;
      }
    }
  }
  else
  {
    if(att[1] == 'h')
    {
      if(inodeList[inodeIndex].attributes == 3)
      {
        inodeList[inodeIndex].attributes = 2;
      }
      else
      {
        inodeList[inodeIndex].attributes = 0;
      }
    }
    else if(att[1] == 'r')
    {
      if(inodeList[inodeIndex].attributes == 3)
      {
        inodeList[inodeIndex].attributes = 1;
      }
      else
      {
        inodeList[inodeIndex].attributes = 0;
      }
    }
  }
}

int del ( char* fileName)
{
  int i, j, inodeIndex;
  int flag = 1;

  for( i = 0 ; i < NUM_FILES ; i++)
  {
    if(strcmp(dir[i].name, fileName) == 0 )
    {
      flag = 0;

      //setting the code of initialize directory
      dir[i].valid = 0; // it is not being used
      memset(dir[i].name, 0, 255);
      inodeIndex = dir[i].inode; // storing the inodeIndex before clearing it
      dir[i].inode = -1;

      // setting the inode to free
      freeInodeList[i] = 1;

      int size = inodeList[inodeIndex].size;
      int blockIdx= inodeList[inodeIndex].blocks[0];

      while(size > 0)
      {
        freeBlockList[blockIdx] = 1;
        size -= BLOCK_SIZE;
        blockIdx++;
      }

      break;
    }
  }

  if(flag)
  {
    printf("Error: file not found\n" );
  }
}
