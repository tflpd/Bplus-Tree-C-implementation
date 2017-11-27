#include "AM.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      printf("Error\n");      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }                           \



typedef enum AM_ErrorCode {
  OK,
  OPEN_SCANS_FULL
}AM_ErrorCode;

//openFiles holds the names of open files in the appropriate index
//TODO not char * but some other struct?
char * openFiles[20];

//insert_file takes the name of a file and inserts it into openFiles
//if openFiles is fulll then -1 is returned
int insert_file(char * fileName){
  for(int i=0; i<20; i++){
    //if a spot is free put the fileName in it and return
    //its index
    if(openFiles[i] == NULL ){
      openFiles[i] = malloc(strlen(fileName) +1);
      strcpy(openFiles[i], fileName);
      return i;
    }
  }
  return -1;
}

//close_file removes a file with index i from openFiles
void close_file(int i){
  free(openFiles[i]);
  openFiles[i] == NULL;
}

/************************************************
**************Scan*******************************
*************************************************/

typedef struct Scan {
	int fileDesc;			//the file that te scan refers to
	int block_num;		//last block that was checked
	int record_num;		//last record that was checked
	int op;			//the operation
	void *value;			//the target value
}Scan;

#define MAX_SCANS 20
Scan* openScans[MAX_SCANS];  //This is where open Scans are saved. The array is initialized with NULL's.

int openScansInsert(Scan* scan);  //inserts a Scan in openScans[] if possible and returns the position
int openScansFindEmptySlot();     //finds the first empty slot in openScans[]
bool openScansFull();             //checks


int openScansInsert(Scan* scan){
	int pos = openScansFindEmptySlot();
  if(openScansFull() != true)
    openScans[pos] = scan;
  else{
    fprintf(stderr, "openScans[] can't fit more Scans! Exiting...\n");
    exit(0);
  }
  return pos;
}

int openScansFindEmptySlot(){
	int i;
	for(i=0; i<MAX_SCANS; i++)
		if(openScans[i] == NULL)
			return i;
	return i;
}

bool openScansFull(){
	if(openScansFindEmptySlot() == MAX_SCANS) //if you cant find empty slot in [0-19] then its full
		return true;
	return false;
}

/************************************************
**************Create*******************************
*************************************************/

int typeChecker(char attrType, int attrLength, int *type, int *len){
  if (attrType == INTEGER)
  {
    *type = 1; //If type is equal to 1 the attribute is int
  }else if (attrType == FLOAT)
  {
    *type = 2; //If type is equal to 2 the attribute is float
  }else if (attrType == STRING)
  {
    *type = 3; //If type is equal to 3 the attribute is string
  }else{
    return AME_WRONGARGS;
  }

  *len = attrLength;

  if (*type == 1 || *type == 2) // Checking if the argument type given matches the argument size given
  {
    if (*len != 4)
    {
      return AME_WRONGARGS;
    }
  }else{
    if (*len < 1 || *len > 255)
    {
      return AME_WRONGARGS;
    }
  }

  return AME_OK;
}

/************************************************
**************Insert*******************************
*************************************************/

int findLeaf(int fd, int key){
  BF_Block *tmpBlock;
  BF_Block_Init(&tmpBlock);

  int keyType, keyLength, rootId;
  void *data;
  char isLeaf = 0;

  //Getting the first block to have access in the first attrr abd the root block id 
  CALL_OR_DIE(BF_GetBlock(fd, 0, tmpBlock));
  data = BF_Block_GetData(tmpBlock);

  data += sizeof(char)*15; //move further from the keyword
  
  //Get the type and the length of this file's key and its root Id
  memcpy(&keyType, data, sizeof(int));
  data += sizeof(int);
  memcpy(&keyLength, data, sizeof(int));
  data += (sizeof(int)*3);
  memcpy(&rootId, data,sizeof(int));

  CALL_OR_DIE(BF_UnpinBlock(tmpBlock));

  CALL_OR_DIE(BF_GetBlock(fd, rootId, tmpBlock));
  data = BF_Block_GetData(tmpBlock);

  memcpy(&isLeaf, data, sizeof(char));

  if (isLeaf == 1) //If the root is a leaf we are on the only leaf so the key should be here
  {
    BF_Block_Destroy(&tmpBlock);
    return rootId;
  }

  
  BF_Block_Destroy(&tmpBlock);
  return 0;
}

/***************************************************
***************AM_Epipedo***************************
****************************************************/

int AM_errno = AME_OK;

void AM_Init() {
  BF_Init(MRU);
	return;
}


int AM_CreateIndex(char *fileName, char attrType1, int attrLength1, char attrType2, int attrLength2) {

  int type1,type2,len1,len2;

  if (typeChecker(attrType1, attrLength1, &type1, &len1) != AME_OK)
  {
    return AME_WRONGARGS;
  }

  if (typeChecker(attrType2, attrLength2, &type2, &len2) != AME_OK)
  {
    return AME_WRONGARGS;
  }

  /*attrMeta.type1 = type1;
  attrMeta.len1 = len1;
  attrMeta.type2 = type2;
  attrMeta.len2 = len2;*/


  BF_Block *tmpBlock;
  BF_Block_Init(&tmpBlock);

  int fd;
  //temporarily insert the file in openFiles
  //int file_index = insert_file(fileName);
  //if(file_index == -1)
  //  return AME_MAXFILES;

  CALL_OR_DIE(BF_CreateFile(fileName));
  CALL_OR_DIE(BF_OpenFile(fileName, &fd));

  void *data;
  char keyWord[15];
  //BF_AllocateBlock(fd, tmpBlock);
  CALL_OR_DIE(BF_AllocateBlock(fd, tmpBlock));//Allocating the first block that will host the metadaτa

  data = BF_Block_GetData(tmpBlock);
  strcpy(keyWord,"DIBLU$");
  memcpy(data, keyWord, sizeof(char)*15);//Copying the key-phrase DIBLU$ that shows us that this is a B+ file
  data += sizeof(char)*15;
  //Writing the attr1 and attr2 type and length right after the keyWord in the metadata block
  memcpy(data, &type1, sizeof(int));
  data += sizeof(int);
  memcpy(data, &len1, sizeof(int));
  data += sizeof(int);
  memcpy(data, &type2, sizeof(int));
  data += sizeof(int);
  memcpy(data, &len2, sizeof(int));

  BF_Block_SetDirty(tmpBlock);
  CALL_OR_DIE(BF_UnpinBlock(tmpBlock));

  //Allocating the root block
  CALL_OR_DIE(BF_AllocateBlock(fd, tmpBlock));
  int blockNum, nextPtr, recordsNum;
  CALL_OR_DIE(BF_GetBlockCounter(fd, &blockNum));
  blockNum--;

  data = BF_Block_GetData(tmpBlock);

  char c = 1; //It is leaf so the first byte of the block is going to be 1
  memcpy(data, &c, sizeof(char));
  data += sizeof(char);

  memcpy(data, &blockNum, sizeof(int)); //Writing the block's id to it
  data += sizeof(int);

  nextPtr = -1; //It is a leaf and the last one
  memcpy(data, &nextPtr, sizeof(int));
  data += sizeof(int);

  recordsNum = 0; //No records yet inserted
  memcpy(data, &recordsNum, sizeof(int));

  BF_Block_SetDirty(tmpBlock);
  CALL_OR_DIE(BF_UnpinBlock(tmpBlock));

  //Getting again the first (the metadata) block to write after the attributes info the root block id
  CALL_OR_DIE(BF_GetBlock(fd, 0, tmpBlock));
  data = BF_Block_GetData(tmpBlock);
  data += (sizeof(char)*15 + sizeof(int)*4);
  printf("GRAFW %d\n", blockNum);
  memcpy(data, &blockNum, sizeof(int));
  BF_Block_SetDirty(tmpBlock);
  CALL_OR_DIE(BF_UnpinBlock(tmpBlock));

  BF_Block_Destroy(&tmpBlock);
  CALL_OR_DIE(BF_CloseFile(fd));
  //remove the file from openFiles array
  //close_file(file_index);

  return AME_OK;
}


int AM_DestroyIndex(char *fileName) {
  return AME_OK;
}


int AM_OpenIndex (char *fileName) {
  BF_Block *tmpBlock;
  int fileDesc, type1;
  BF_Block_Init(&tmpBlock);
  //int file_index = insert_file(fileName);
  //check if we have reached the maximum number of files
  //if(file_index == -1)
  //  return AME_MAXFILES;

  CALL_OR_DIE(BF_OpenFile(fileName, &fileDesc));

  //here should be the error checking

  char *data = NULL;
  CALL_OR_DIE(BF_GetBlock(fileDesc, 0, tmpBlock));//Getting the first block
  data = BF_Block_GetData(tmpBlock);//and its data

  if (data == NULL || strcmp(data, "DIBLU$"))//to check if this new opened file is a B+ tree file
  {
    printf("File: %s is not a B+ tree file. Exiting..\n", fileName);
    exit(-1);
  }

  /*data += sizeof(char)*15;
  int type, len;
  memcpy(&type, data, sizeof(int));
  data += sizeof(int);
  memcpy(&len, data, sizeof(int));

  printf("%d %d\n", type, len);*/

  CALL_OR_DIE(BF_UnpinBlock(tmpBlock));
  BF_Block_Destroy(&tmpBlock);

  findLeaf(fileDesc,1);
  return AME_OK;
}


int AM_CloseIndex (int fileDesc) {
  //TODO other stuff?

  //remove the file from the openFiles array
  //close_file(fileDesc);
  return AME_OK;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
  return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op, void *value) {
  /*Scan* scan = malloc(sizeof(Scan));
  scan->fileDesc = fileDesc;
	scan->op = op;
	scan->value = value;
	scan->block_num = -1;
	scan->record_num = -1;
//=======
  Scan scan;
  ScanInit(&scan, fileDesc, op, value);
//>>>>>>> 2f294059a66ca65a6b4924db49a0238933bf79f7*/

	//return openScansInsert(scan);
  return AME_OK;
}


void *AM_FindNextEntry(int scanDesc) {

}


int AM_CloseIndexScan(int scanDesc) {
  return AME_OK;
}


void AM_PrintError(char *errString) {
  printf("%s\n", errString);
}

void AM_Close() {

}
