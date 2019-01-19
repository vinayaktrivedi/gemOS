#include "lib.h"

struct object{
     long id;
     long size;
     int cache_index;
     char key[32];
     long indirect[8];
     long num_blocks;
};

struct pair{
  long id;
  char key[32];
}

long* data_blocks;
struct object *objs;
char* block_bitmap;
char* inode_bitmap;
char* cache_bitmap;
char* dirty_bitmap; 
struct pair** inital_ds;


#define malloc_arbit(x,size) do{\
                         (x) = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_arbit(x,size) munmap((x), size )


#ifdef CACHE         // CACHED implementation

static void remove_object_cached(struct object *obj)
{
           obj->cache_index = -1;
           obj->dirty = 0;
          return;
}
static int find_read_cached(struct objfs_state *objfs, struct object *obj, char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(user_buf, cache_ptr, size);
         return 0;
}
/*Find the object in the cache and update it*/
static int find_write_cached(struct objfs_state *objfs, struct object *obj, const char *user_buf, int size)
{
         char *cache_ptr = objfs->cache + (obj->id << 12);
         if(obj->cache_index < 0){  /*Not in cache*/
              if(read_block(objfs, obj->id, cache_ptr) < 0)
                       return -1;
              obj->cache_index = obj->id;
             
         }
         memcpy(cache_ptr, user_buf, size);
         obj->dirty = 1;
         return 0;
}
/*Sync the cached block to the disk if it is dirty*/
static int obj_sync(struct objfs_state *objfs, struct object *obj)
{
   char *cache_ptr = objfs->cache + (obj->id << 12);
   if(!obj->dirty)
      return 0;
   if(write_block(objfs, obj->id, cache_ptr) < 0)
      return -1;
    obj->dirty = 0;
    return 0;
}

static long next_block_alloc(){

  long off = (1<<15)+ 544;
  int start = off/8;
  int rem = off%8;
  for(int i=start;i<(1<<20);i++){
    char temp = block_bitmap[i];
    for(int j=7;j>=0;j--){
      if(i==start && 8-j < rem)
        continue;

      char check = (temp>>j)&0x1;
      if(check==0){
        block_bitmap[i] += 1<<j;
        return (8*i)+ 7-j;
      }
    }
  }
  return -1;
}
/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
        struct pair* first;
        char* read;
        malloc_arbit(read,BLOCK_SIZE);
        int flag = 0;
        int count = 0;

        long temp_block = 288+key[0];

        while(1){
          if(count == 0)
            read = (char*) *(inital_ds+key[0]);

          else if(read_block(objfs, temp_block, read ) < 0){
            free_arbit(read,BLOCK_SIZE);
            return -1;
          }
            

          else{

          }
          long object_id =-1;
          long* length = (long*)read;
          long* next_block = (long*)read+8;
          read = read+16;
          first = (struct object*)read;
          struct pair* temp;

          for(int k=0; k<*length; k++){
            temp = first+k;
            if(strcmp(temp->key,key) == 0 ){
              flag = 1;
              object_id = temp->id;
            }
          }

          if(*length<100){
            
            free_arbit(read, BLOCK_SIZE);
            return object_id;
          }
          else{
            if(flag == 1){
              free_arbit(read, BLOCK_SIZE);
              return object_id;   
            }

            if(*next_block == 0){
              free_arbit(read,BLOCK_SIZE);
              return -1;

            }
            else {
              temp_block = *next_block;
            }
          } 
          count +=1; 
        }
    return -1;   
}

/*
  Creates a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs)
{

   char temp;
   struct  object *load_temp;
   for(int i=0;i<(1<<17);i++ ){
    temp = inode_bitmap[i];

    for(int j=7;j>=0;j--) {
      temp = (temp>>j)& 0x1;
      if(temp==0){
        long inode_id = (i*8)+7-j;
        long inode_block = inode_id/32;
        int offset = inode_id%32;
        if(read_block(objfs, 544+inode_block, load_temp ) < 0){
          return -1;
        }
        load = load_temp+offset;
        load->id = inode_id;
        load->size = 0;
        load->cache_index = -1;
        strcpy(load->key,key);
        load->num_blocks = 0;
        for(int j=0;j<8;j++){
          load->indirect[j] = 0;
        }
        write_block(objfs,544+inode_block,load_temp);
        inode_bitmap[i] += 1<<j;

        struct pair* first;
        char* read;
        malloc_arbit(read,BLOCK_SIZE);

        int count = 0;

        long temp_block = 288+key[0];

        while(1){
          if(count == 0)
            read = (char*) *(inital_ds+key[0]);

          else if(read_block(objfs, temp_block, read ) < 0){
            free_arbit(read,BLOCK_SIZE);
            return -1;
          }
          else{

          }

          long* length = (long*)read;
          long* next_block = (long*)read+8;
          read = read+16;
          first = (struct object*)read;
          if(*length<100){
            
            struct pair* temp;
            for(int k=0; k<=*length; k++){
              temp = first+k;
              if(temp->id == 0){
                temp->id = inode_id;
                strcpy(temp->key,key);
                break;
              }
            }
            *length = *length+1;
            if(count>0)
              write_block(objfs,temp_block,read );

            free_arbit(read,BLOCK_SIZE);
            return 0;
          }
          else{
            if(*next_block == 0){
              long block_number  = next_block_alloc();
              if(block_number == -1){
                free_arbit(read,BLOCK_SIZE);
                return -1;
              }

              *next_block = block_number;
              write_block(objfs,temp_block,read);
              if(read_block(objfs,block_number,read) < 0){
                free_arbit(read,BLOCK_SIZE);
                return -1;
              }

              length = (long*)read;
              *length =1;
              next_block = (long*)read+8;
              *next_block = 0;
              read = read+16;
              first = (struct object*)read;
              first->id = inode_id;
              strcpy(first->key,key);
              write_block(objfs,block_number,read);

            }
            else{
              temp_block = *next_block;
            }
          } 
          count +=1; 
        }
        
      }
    }
   }
   return -1;
}
/*
  One of the users of the object has dropped a reference
  Can be useful to implement caching.
  Return value: Success --> 0
                Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs)
{
    return 0;
}

/*
  Destroys an object with obj.key=key. Object ID is ensured to be >=2.

  Return value: Success --> 0
                Failure --> -1
*/
long destroy_object(const char *key, struct objfs_state *objfs)
{
    return -1;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.  
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
   
   inode_number = find_object_id(key,objfs);
   
}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs)
{
   return -1;
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs)
{
   return -1;
}

/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf)
{
   return -1;
}

/*
   Set your private pointeri, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
   malloc_arbit(block_bitmap,1<<20);
   malloc_arbit(inode_bitmap, 1<<17);
   malloc_arbit(cache_bitmap,i<<20);
   malloc_arbit(dirty_bitmap,i<<20);
   malloc_arbit(data_blocks,8);
   malloc_arbit(inital_ds,1<<20);

   if(!block_bitmap || !inode_bitmap || !cache_bitmap || !dirty_bitmap || data_blocks || inital_ds){
       dprintf("%s: malloc\n", __func__);
       return -1;
   }

   for(int i=0;i<256;i++){
    if(read_block(objfs, i, (char*)block_bitmap+i*4096 ) < 0)
      return -1;

   }

   for(int i=0;i<32;i++){
    if(read_block(objfs, 256+i, (char *)inode_bitmap+i*4096 ) < 0)
      return -1;
   }

  for(int i=0;i<256;i++){
    if(read_block(objfs, 288+i, *(inital_ds+i) ) < 0)
      return -1;

   }   

   *data_blocks = 1<<23;

   dprintf("Done objstore init\n");
   return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{
   dprintf("Done objstore destroy\n");
   return 0;
}
