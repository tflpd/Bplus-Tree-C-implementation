#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bf.h"
#include "file_info.h"
#include "stack.h"
#include "BoolType.h"
#include "AM.h"

extern file_info * openFiles[20];

void print_block(char * data){
  int ptrs = 0;
  int offset = 0;
  bool isleaf =0;
  int block_id = 0;
  int nextPtr=0;
  int recordsNum;
  int temp;
  memmove(&isleaf, data, sizeof(bool));
  offset+= sizeof(bool);
  memmove(&block_id, data+offset, sizeof(int));
  offset+= sizeof(int);
  memmove(&nextPtr, data+offset, sizeof(int));
  offset += sizeof(int);
  memmove(&recordsNum, data+offset, sizeof(int));
  offset += sizeof(int);
  printf("Printing block with id %d\n", block_id);
  printf("Is leaf: %d\n", isleaf);
  printf("Nexptr %d\n", nextPtr);
  printf("recordsNum %d\n", recordsNum);
  // printf("p%d ", *((int *)data+offset));
  memmove(&temp, data +offset, sizeof(int));
  printf("p%d ", temp);
  offset+= sizeof(int);
  for(int i=0; i<recordsNum; i++){
    //printf("k%d ", *((int *)data+offset));
    memmove(&temp, data +offset, sizeof(int));
    printf("k%d ", temp);
    offset += sizeof(int);
    //printf("p%d ", *((int *)data+offset));
    memmove(&temp, data +offset, sizeof(int));
    printf("p%d ", temp);
    offset += sizeof(int);
  }
  printf("\n");
}

//the declarations I will need, definitions are a the end of this file
int block_has_space(char * data, int keysize, int keys);
int find_offset(char* data, int keysize, void *key, int keytype, int keys);
//checks if we are in the root
int is_root(int block_no, int fileDesc);
//the standard initialization for an inside node
int initialize_block(char * data, int block_id, int recs);

int find_middle_key(char * data, char * mid_key, void * key, int keytype,
  int keysize, int keys_in1st);

int partition(char *ldata, char *rdata, char * mid_key, void * key,
  int keytype, int keysize, int keys_in1st, int keys_in2nd, int offset,
  int newbid, int total_size);

//inserts an indexing value (only for index blocks, never leaf)
int insert_index_val(void *value, int fileDesc, Stack* stack, int newbid){
  static int i =0;
  printf("inserted %d keys\n", ++i);
  printf("Entered index\n");
  printf("key to insert %d\n", *(int *)value);
  printf("Newbid %d\n", newbid);
  BF_Block *curBlock;
  BF_Block_Init(&curBlock);
  //get the number of the bf file we're in
  int bf_no = openFiles[fileDesc]->bf_desc;
  //get current block number from stack
  int block_no = get_top(stack);
  printf("BLock_no %d\n", block_no);
  //open block and get its data
  BF_GetBlock(bf_no, block_no, curBlock);
  char * data = BF_Block_GetData(curBlock);
  print_block(data);
  //get the number of keys because we may have to update the number later
  int offset = sizeof(bool) + 2*sizeof(int);
  int keys = 0;
  memmove(&keys, data+offset, sizeof(int));
  //add an int to offset to compute the total size
  offset += sizeof(int);
  //get necessary info about key
  int keytype = openFiles[fileDesc]->type1;
  int keysize =  openFiles[fileDesc]->length1;
  //the total size of the existing block
  int total_size = offset + keys*keysize + (keys+1)*sizeof(int);

  if(block_has_space(data, keysize, keys)){
    printf("Block has space \n");
    //insert in current block
    offset = find_offset(data, keysize, value, keytype, keys);
    printf("Offset is %d\n", offset);
    //move existing data to the right and insert new data
    //bytes = cur_record + static -offset
    memmove(data+offset+keysize+sizeof(int), data+offset, total_size-offset);
    memmove(data+offset, value, keysize);
    offset += keysize;
    //here I should have which block is beneath
    memmove(data+offset, &newbid, sizeof(int));
    //now set offset in the correct position to update the number of keys=
    offset = sizeof(bool) + 2*sizeof(int);
    keys++;
    memmove(data+offset, &keys, sizeof(int));
    //done :)
    BF_Block_SetDirty(curBlock);
    print_block(data);
    printf("Block has space out\n");
  }
  else if(is_root(block_no, fileDesc)){
    printf("Is root\n");
    //split and update root
    BF_Block *newBlock;
    BF_Block_Init(&newBlock);
    BF_AllocateBlock(bf_no, newBlock);
    int new_id = 0;
    BF_GetBlockCounter(bf_no, &new_id);
    new_id--;
    char * new_data = BF_Block_GetData(newBlock);
    initialize_block(new_data, new_id, 0);
    //keys in 1st is ceil(key2+1/2)
    int keys_in1st = (keys+1)%2 ? (keys+1)/2 +1 : (keys+1)/2;
    int keys_in2nd = keys - keys_in1st;
    //obviously in root there will be 1 key
    char * mid_key = malloc(keysize);
    //get key to up here
    //find middle key and keep offset in left
    int offset_inl = find_middle_key(data, mid_key, value, keytype, keysize, keys_in1st);
    //partition :)

    partition(data, new_data, mid_key, value, keytype, keysize, keys_in1st,
      keys_in2nd, offset_inl, newbid, total_size);
    //create new root_id  //split and update root
    BF_Block *newroot;
    BF_Block_Init(&newroot);
    BF_AllocateBlock(bf_no, newroot);
    int root_id = 0;
    BF_GetBlockCounter(bf_no, &root_id);
    printf("Got root_id %d\n", root_id);
    root_id--;
    char * root_data = BF_Block_GetData(newroot);
    initialize_block(root_data, root_id, 1);
    //offset is already sizeof(bool) + 3*sizeof(int) (arithmetic is ok here)
    memmove(root_data+offset, &block_no, sizeof(int));
    offset += sizeof(int);
    memmove(root_data+offset, mid_key, keysize);
    offset += keysize;
    memmove(root_data+offset, &new_id, sizeof(int));
    //update the root id in file_info
    printf("prev root %d\n", openFiles[fileDesc]->root_id);
    openFiles[fileDesc]->root_id = root_id;
    //update the root_id in openFiles
    printf("PRINTING LBLOCK\n");
    print_block(data);
    printf("PRINTING ROOT\n");
    print_block(root_data);
    printf("PRINTING RBLOCK\n");
    print_block(new_data);
    //don't forget the metadata

    //cleanup
    BF_Block_SetDirty(newBlock);
    BF_Block_SetDirty(newroot);
    BF_UnpinBlock(newBlock);
    BF_UnpinBlock(newroot);

    //now I will use newBlock to store the first block
    BF_GetBlock(bf_no, 0, newBlock);
    new_data = BF_Block_GetData(newBlock);
    //memmove the new root id now
    offset = sizeof("DIBLU$") + 4*(sizeof(int));
    memmove(new_data+offset, &root_id, sizeof(int));
    //changed root if so set Dirty
    BF_Block_SetDirty(newBlock);
    BF_UnpinBlock(newBlock);
    BF_Block_Destroy(&newBlock);
    BF_Block_Destroy(&newroot);
    free(mid_key);
    printf("Is root out\n");
  }
  else{
    printf("Split and pass\n");
    //split and pass one level up
    BF_Block *newBlock;
    BF_Block_Init(&newBlock);
    BF_AllocateBlock(bf_no, newBlock);
    int new_id = 0;
    BF_GetBlockCounter(bf_no, &new_id);
    new_id--;
    char * new_data = BF_Block_GetData(newBlock);
    initialize_block(new_data, new_id, 0);
    //keys in 1st is ceil(key2+1/2)
    int keys_in1st = (keys+1)%2 ? (keys+1)/2 +1 : (keys+1)/2;
    int keys_in2nd = keys - keys_in1st;
    //obviously in root there will be 1 key
    char * mid_key = malloc(keysize);
    //get key to up here
    //find middle key,
    int offset_inl = find_middle_key(data, mid_key, value, keytype, keysize, keys_in1st);
    //partition :)

    partition(data, new_data, mid_key, value, keytype, keysize, keys_in1st,
      keys_in2nd, offset_inl, newbid, total_size);

    //cleanup
    BF_Block_SetDirty(newBlock);
    BF_UnpinBlock(newBlock);
    BF_Block_Destroy(&newBlock);
    //pop stack and call recursion
    if(stack_pop((stack))== -1){
      printf("Invalid stack\n");
    }
    //recursive call
    insert_index_val(mid_key, fileDesc, stack, new_id);
    free(mid_key);
    printf("Split and pass out\n");
  }

  //close/unpnin block ;)
  BF_UnpinBlock(curBlock);
  BF_Block_Destroy(&curBlock);
  printf("Exit index val\n");


  return 1;
}

//pretty straight forward
int block_has_space(char * data, int keysize, int keys){
  int avail_space = 0;
  //add the first static data to offset
  int offset = sizeof(bool) + 3*sizeof(int);
  //after the static offset we have (keys)*keysize + keys+1*int
  //and we want to add a key and an int
  avail_space = BF_BLOCK_SIZE - offset - (keys+1)*keysize - (keys+2)*sizeof(int);
  return avail_space < 0 ? 0 : 1;
}

//find offset works fine!!!
//find_offset takes a key and finds the byte position where it should be put
int find_offset(char* data, int keysize, void *key, int keytype, int keys){
 /*AM 1400192 STRONGLY OPPOSES the use of "bool"*/

 int offset = 0;
 int keys_no = 0;

 //add the first static data to offset
 offset += sizeof(bool) + 2*sizeof(int);
 //get the number of
 memmove(&keys_no, data+offset, sizeof(int));
 //add to the offset 4 bytes for keys_no and 4 for the first block pointer
 offset += 2*sizeof(int);
 if(keys!=keys_no)
  printf("Wrong keys wtf\n");
 for(int i=0; i<keys; i++){
   if(keysComparer(key, data+offset, LESS_THAN, keytype))
    break;
  offset += keysize + sizeof(int);
 }
 //if the correct position was not found then it is the next one
 return offset;
}

//checks if we are in the root
int is_root(int block_no, int fileDesc){
  return openFiles[fileDesc]->root_id == block_no;
}

//the standard initialization for an inside node
int initialize_block(char * data, int block_id, int recs){
  bool isLeaf = false;
  int nextPtr = -2;
  int offset = 0;
  memmove(data, &isLeaf, sizeof(bool));
  offset += sizeof(bool);
  memmove(data+offset, &block_id, sizeof(int));
  offset += sizeof(int);
  memmove(data+offset, &nextPtr, sizeof(int));
  offset += sizeof(int);
  memmove(data+offset, &recs, sizeof(int));
  return 1;
}

//
int find_middle_key(char * data, char * mid_key, void * key, int keytype,
  int keysize, int keys_in1st){

  int offset = sizeof(bool) + 4*sizeof(int);
  int past_key = 0;
  for(int i=0; i<keys_in1st; i++){
    if(past_key || keysComparer(data+offset, key, LESS_THAN, keytype))
      offset += keysize +sizeof(int);
    else
      past_key = 1;
  }
  //copy the correct middle key to the variable
  if(past_key || keysComparer(data+offset, key, LESS_THAN, keytype)){
    memmove(mid_key, data+offset, keysize);
    offset += keysize +sizeof(int);
  }
  else{
    memmove(mid_key, key, keysize);
  }
  offset -= sizeof(int);
  //past this offset are the greater values
  return offset;
}


int partition(char *ldata, char *rdata, char * mid_key, void * key,
  int keytype, int keysize, int keys_in1st, int keys_in2nd, int offset,
  int newbid, int total_size){

  int roffset = sizeof(bool) + 2*sizeof(int);
  int loffset = offset;

  //move keys_in2nd no of keys in rblock and possibly insert the new key
  //first copy the number of keys
  memmove(rdata+roffset, &keys_in2nd, sizeof(int));
  roffset += sizeof(int);
  //then copy the first block pointer
  memmove(rdata+roffset, ldata+loffset, sizeof(int));
  loffset += sizeof(int);
  roffset += sizeof(int);
  //copy all the key/pointer pairs
  for(int i=0; i<keys_in2nd; i++){
    //copy the key
    if(keysComparer(key, ldata+loffset, LESS_THAN, keytype)){
      memmove(rdata+roffset, key, keysize);
      roffset += keysize;
      memmove(rdata+roffset, &newbid, sizeof(int));
      roffset += sizeof(int);
    }
    else{
      memmove(rdata+roffset, ldata+loffset, keysize);
      loffset += keysize;
      roffset += keysize;
      //copy the block pointer
      memmove(rdata+roffset, ldata+loffset, sizeof(int));
      loffset += sizeof(int);
      roffset += sizeof(int);
    }
  }

  //keep keys_in1st no of keys in lblock and possibly insert the new key
  //first update the number of keys the left block will have
  loffset = sizeof(bool) + 2*sizeof(int);
  memmove(ldata+loffset, &keys_in1st, sizeof(int));
  loffset += 2*sizeof(int);

  //then start checking if we are ok or we should insert the new key
  for(int i=0; i<keys_in1st; i++){
    if(keysComparer(key, ldata+loffset, LESS_THAN, keytype)){
      //make newspace to put the key and the block pointer
      int newspace = keysize+sizeof(int);
      memmove(ldata+loffset+newspace, ldata+loffset, total_size-offset-newspace);
      //copy the key
      memmove(ldata+loffset, key, keysize);
      loffset += keysize;
      memmove(ldata+loffset, &newbid, sizeof(int));
      break;
    }
  }
  return 1;
  //done
}
