#include "lib.h"
#include <pthread.h>
struct object{
     int id;
     long size;
     int cache_index;
     char key[32];
     unsigned int indirect[4];
};
 
struct cache_object{
  int block_id;
  char value[4096];
  int dirty;
  int cached;
};

pthread_mutex_t lock_block_bitmap;
pthread_mutex_t lock_inode_bitmap;
pthread_mutex_t lock_inodes;
pthread_mutex_t cache_lock;

int* eviction_index;
int* max_cache_blocks;
long* data_blocks;
int* cached_blocks;
struct object *objs;
char* block_bitmap;
char* inode_bitmap; 
char* inode_memory;


#define malloc_arbit(x,size) do{\
                         (x) = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_arbit(x,size) munmap((x), size )

static long next_block_alloc(struct objfs_state *objfs){
  

  long off = (1<<14)+ 289;
  int start = off/8;
  int rem = off%8;
  for(int i=start;i<(1<<20);i++){
    pthread_mutex_lock(&lock_block_bitmap);
    char temp = block_bitmap[i];
    for(int j=7;j>=0;j--){
      if(i==start && 8-j <= rem)
        continue;

      char check = (temp>>j)&0x1;
      if(check==0){
        block_bitmap[i] += 1<<j;
        int block_id = (8*i)+ 7-j;

        if(block_id < *data_blocks){
          dprintf("Allocated block number %d",block_id);
          pthread_mutex_unlock(&lock_block_bitmap);
          return block_id;
        }
        else{
          dprintf("error is here with block_id %d\n",block_id);
          pthread_mutex_unlock(&lock_block_bitmap);
          return -1 ;
        }
      }
    }
    pthread_mutex_unlock(&lock_block_bitmap);
  }
  dprintf("not found data page\n");
  return -1;
}

static void init_object_cached(int block_id,struct objfs_state *objfs,char* buf,int dirty)
{
          pthread_mutex_lock(&cache_lock);
          
          if(*cached_blocks < *max_cache_blocks){
            struct cache_object* start = (struct cache_object*)objfs->cache;
            struct cache_object* temp = start+*(cached_blocks);
            temp->block_id = block_id;
            memcpy(temp->value,buf,4096);
            temp->cached = 1;
            temp->dirty = dirty;
            *(cached_blocks) += 1;
          }
          else{
            struct cache_object* start = (struct cache_object*)objfs->cache;
            struct cache_object* temp = start+*(eviction_index);
            char* mem;
            malloc_arbit(mem,4096);
            memcpy(mem,temp->value,4096);
            if(temp->dirty == 1){
              write_block(objfs,temp->block_id,mem );
            }
            free_arbit(mem,4096);

            temp->block_id = block_id;
            memcpy(temp->value,buf,4096);
            temp->cached = 1;
            temp->dirty = dirty;
            *(eviction_index) = (*(eviction_index)+1)%(*max_cache_blocks);
          }
          
          pthread_mutex_unlock(&cache_lock);
          return;
          
}


#ifdef CACHE         // CACHED implementation

static int file_read_block(struct objfs_state *objfs, int block_id, char* buf  )
{
        pthread_mutex_lock(&cache_lock);
        struct cache_object* start = (struct cache_object*)objfs->cache;
          for(int i=0;i<*max_cache_blocks;i++ ){
            struct cache_object* temp = start+i;
            if(temp->cached == 1 && temp->block_id == block_id){
              memcpy(buf,temp->value,4096);
              pthread_mutex_unlock(&cache_lock);
              return 0;
            }
          }

          pthread_mutex_unlock(&cache_lock);
          read_block(objfs,block_id,buf);
          if(buf == NULL){

            return -1;
          }

          init_object_cached(block_id,objfs,(char*)buf,0);
          return 0;

}
/*Find the object in the cache and update it*/
static int file_write_block(struct objfs_state *objfs, int block_id, char* buf )
{
          pthread_mutex_lock(&cache_lock);
          struct cache_object* start = (struct cache_object*)objfs->cache;
          for(int i=0;i<*max_cache_blocks;i++ ){
            struct cache_object* temp = start+i;
            if(temp->cached == 1 && temp->block_id == block_id){
              temp->dirty = 1;
              memcpy(temp->value,buf,4096);
              pthread_mutex_unlock(&cache_lock);
              return 0;
            }
          }         
          pthread_mutex_unlock(&cache_lock);

          init_object_cached(block_id,objfs,(char*)buf,1);
          return 0;
}
/*Sync the cached block to the disk if it is dirty*/

#else  //uncached implementation
static int file_read_block (struct objfs_state *objfs, int block_id, char* buf )
{

  return read_block(objfs,block_id,buf);   
}
static int file_write_block (struct objfs_state *objfs, int block_id, char* buf )
{

   return write_block(objfs,block_id,buf);
}
#endif

/*
Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs)
{
  dprintf("Entered find object id function inode_bitmap %d \n", ( int)inode_bitmap[0]);
    char temp;
   for(int i=0;i<(1<<17);i++ ){
    temp = inode_bitmap[i];
    
    for(int j=7;j>=0;j--) {
      if(i==0 && j>5)
        continue;

      char temp_temp = (temp>>j)& 0x1;
      
      if(temp_temp == 1){

        int inode_id = (i*8)+7-j;

        struct object* first_inode = (struct object*)inode_memory;
        first_inode = first_inode+inode_id;
        if(strcmp(key,first_inode->key) == 0){
          dprintf("out temp %d and in temp %d\n",(int)temp,(int)temp_temp);
          dprintf("Found inode %d for key %s\n",inode_id,key);
          return inode_id;
        }

      }
    }
   }
   dprintf("Key not found %s\n",key);
   return -1;   
}

/*
  Creates a new object with obj.key=key.
  Must check for duplicates.

  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs)
{
  dprintf("Entered create object function\n");

   char temp;
   for(int i=0;i<(1<<17);i++ ){
    pthread_mutex_lock(&lock_inodes);
    temp = inode_bitmap[i];

    for(int j=7;j>=0;j--) {
      if(i==0 && j>5)
        continue;

      char temp_temp = (temp>>j)& 0x1;
      if(temp_temp == 0){

        int inode_id = (i*8)+7-j;
        inode_bitmap[i] += (1<<j);
        struct object* first_inode = (struct object*)inode_memory;
        first_inode = first_inode+inode_id;
        first_inode->id = inode_id;
        first_inode->size = 0;
        first_inode->cache_index = -1;
        strcpy(first_inode->key,key);
        unsigned int* s = first_inode->indirect;
        for(int k=0;k<4;k++){
          *(s+k) = 0;
        }
        dprintf("returned inode id %d for key %s\n",inode_id,first_inode->key);
        pthread_mutex_unlock(&lock_inodes);    
        return inode_id;

      }
    }
    pthread_mutex_unlock(&lock_inodes);
   }
   dprintf("Could not create\n");
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
  dprintf("Entered destroy object function\n");
    int inode_number = find_object_id(key,objfs);
    int index = inode_number/8;
    int offset = 7-inode_number%8;

    pthread_mutex_lock(&lock_inode_bitmap);
    inode_bitmap[index] -= (1<<offset);
    pthread_mutex_unlock(&lock_inode_bitmap);

    struct object* temp = (struct object*)inode_memory;
    temp = temp+inode_number;
    temp->id = 0;
    temp->size = 0;
    temp->cache_index = -1;
    strcpy(temp->key,"");

    unsigned int* read ;
    malloc_arbit(read,4096);

    for(int i=0;i<4;i++){
      int indirect_block = temp->indirect[i];
      if(indirect_block != 0){
        if(file_read_block(objfs, indirect_block, (char*)read ) < 0){
          return -1;
        }

        unsigned int* iter = read;
        for(int j=0;j<1024;j++){
          unsigned int temp_block_number = *(iter+j);
          if(temp_block_number != 0){
            pthread_mutex_lock(&lock_block_bitmap);
            block_bitmap[temp_block_number/8] -= 1<<(7-(temp_block_number%8));
            pthread_mutex_unlock(&lock_block_bitmap);
          }
        }

        pthread_mutex_lock(&lock_block_bitmap);
        block_bitmap[indirect_block/8] -= 1<<(7-(indirect_block%8));
        pthread_mutex_unlock(&lock_block_bitmap);

        temp->indirect[i] = 0;  
      }
      
    } 
    return 0;
}

/*
  Renames a new object with obj.key=key. Object ID must be >=2.
  Must check for duplicates.  
  Return value: Success --> object ID of the newly created object
                Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs)
{
   dprintf("Entered rename function\n");  
   int inode_number = find_object_id(key,objfs);
   if(find_object_id(newname,objfs) != -1){
    return -1;
   }

   struct object* first_inode = (struct object*)inode_memory;
   first_inode = first_inode+inode_number;
   strcpy(first_inode->key,newname);
   return inode_number;

}

/*
  Writes the content of the buffer into the object with objid = objid.
  Return value: Success --> #of bytes written
                Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs,off_t offset)
{
  dprintf("Entered write function with offset %d size %d and objectid %d\n",(int)offset,size,objid);

    if(offset%4096 != 0 ){
      size -= (4096-(offset%4096) );
    }

    struct object* temp = (struct object*)inode_memory;
    temp = temp+objid;
    int blocks_needed;
    if(size%4096 == 0)
      blocks_needed = size/4096;
    else
      blocks_needed = (size/4096)+1;

    int indirect_needed;
    if(blocks_needed%1024 == 0 )
      indirect_needed = blocks_needed/1024;
    else
      indirect_needed = (blocks_needed/1024) +1;

    

    int offset_block = offset/4096;
    int start_indirect = offset_block/1024;
    int position = offset_block%1024;

    int bytes_written = 0;
    int blocks_written = 0;
    dprintf("blocks %d and indirect %d start_indirect %d position %d\n",blocks_needed,indirect_needed,start_indirect,position);
    for(int i=start_indirect;(i<(indirect_needed+start_indirect) && i<4) ;i++){
      int indirect_index = i;
      dprintf("%d index %d\n",temp->indirect[indirect_index],i);

      if(temp->indirect[indirect_index] == 0){
        int block_id = next_block_alloc(objfs);
        dprintf("new indirect data block %d\n",block_id);
        if(block_id == -1)
          return -1;
        else{
          
          temp->indirect[indirect_index] = block_id;
          unsigned int* temp_memory;
          malloc_arbit(temp_memory,4096);
          for(int k=0;k<1024;k++)
            *(temp_memory+k) = 0;

          int j=0;

          while(j<1024 && blocks_written < blocks_needed){
            int new_data_block = next_block_alloc(objfs);

            if(new_data_block == -1)
              return -1;
            dprintf("new data data block %d\n",new_data_block);
            *(temp_memory+j) = new_data_block;
            char* ptr;
            malloc_arbit(ptr,4096);
            int size_left = size-bytes_written;
            if(size_left > 4096)
              size_left = 4096;
            memcpy(ptr,buf+blocks_written*4096,size_left);
            file_write_block(objfs,new_data_block,(char*)ptr );
            blocks_written++;
            j++;
            bytes_written += size_left;
            free_arbit(ptr,4096);
          }

          file_write_block(objfs,block_id,(char*)temp_memory);
          free_arbit((char*)temp_memory,4096);
        }
      }
      
      else{
        int j=0;
        int flag=0;
        unsigned int* temp_memory;
        malloc_arbit(temp_memory,4096);

        if(file_read_block(objfs,temp->indirect[indirect_index],(char*)temp_memory)<0)
          return -1;

        while(j<1024 && blocks_written < blocks_needed){

          if(i==start_indirect){
            if(j<position){
              j++;
              continue;
            }
            else if(j==position){
              if(offset%4096!=0){
                int to_write = (4096-(offset%4096) );
                char* ptr;
                malloc_arbit(ptr,4096);
                file_read_block(objfs,*(temp_memory+j),ptr);
                memcpy(ptr+(offset%4096),buf,to_write);
                buf = buf+to_write;
                file_write_block(objfs,*(temp_memory+j),ptr);
                free_arbit(ptr,4096);
                bytes_written += to_write;
                j++;
                continue;
              }


            }
          }

          int size_left = size - blocks_written*4096;
          if(size_left>4096)
            size_left = 4096;

          if(*(temp_memory+j) == 0){
            flag=1;
            int block_id = next_block_alloc(objfs);
            if(block_id == -1)
              return -1;
            dprintf("new data data block %d\n",block_id);
            *(temp_memory+j) = block_id;
            char* ptr;
            malloc_arbit(ptr,4096);
            memcpy(ptr,buf+blocks_written*4096,size_left);
            file_write_block(objfs,block_id,ptr);
            free_arbit(ptr,4096);

          }
          else{
            int block_id = *(temp_memory+j);
            char* ptr;
            malloc_arbit(ptr,4096);
            memcpy(ptr,buf+blocks_written*4096,size_left);
            file_write_block(objfs,block_id,ptr);
            free_arbit(ptr,4096);
          }
          j++;
          blocks_written++;
          bytes_written += size_left;

        }
        if(flag==1){
            file_write_block(objfs,temp->indirect[indirect_index],(char*)temp_memory);
        }
        free_arbit((char*)temp_memory,4096);
      }
    }

    if(bytes_written == 0){
      return -1;
    }
     temp->size += bytes_written;
     return bytes_written;
      
      
}

/*
  Reads the content of the object onto the buffer with objid = objid.
  Return value: Success --> #of bytes read
                Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs,off_t offset)
{
  dprintf("Entered read function with offset %d size %d and objectid %d\n",(int)offset,size,objid);
   if(offset%4096 != 0 ){
      size -= (4096-(offset%4096) );
    }

    struct object* temp = (struct object*)inode_memory;
    temp = temp+objid;
    int blocks_needed;
    if(size%4096 == 0)
      blocks_needed = size/4096;
    else
      blocks_needed = (size/4096)+1;

    int indirect_needed;
    if(blocks_needed%1024 == 0 )
      indirect_needed = blocks_needed/1024;
    else
      indirect_needed = blocks_needed/1024 +1;

    int offset_block = offset/4096;
    int start_indirect = offset_block/1024;
    int position = offset_block%1024;

    int bytes_read = 0;
    int blocks_read = 0;

    for(int i=start_indirect;i<(indirect_needed+start_indirect) && i<4;i++){
      int indirect_index = i;

      if(temp->indirect[indirect_index] == 0){
        return bytes_read;
      }
      
      else{
        int j=0;
        unsigned int* temp_memory;
        malloc_arbit(temp_memory,4096);

        if(file_read_block(objfs,temp->indirect[indirect_index],(char*)temp_memory)<0){
          dprintf("indirect data block read error\n");
          return -1;
        }

        while(j<1024 && blocks_read < blocks_needed){

          if(i==start_indirect){
            if(j<position){
              j++;
              continue;
            }
            else if(j==position){
              if(offset%4096!=0){
                if(*(temp_memory+j) == 0)
                  return bytes_read;

                int to_read = (4096-(offset%4096) );
                char* ptr;
                malloc_arbit(ptr,4096);
                file_read_block(objfs,*(temp_memory+j),ptr);
                memcpy(buf ,ptr+(offset%4096),to_read);
                buf = buf+to_read;
                free_arbit(ptr,4096);
                bytes_read += to_read;
                j++;
                continue;
              }


            }
          }

          int size_left = size - blocks_read*4096;
          if(size_left>4096)
            size_left = 4096;

          if(*(temp_memory+j) == 0){
            dprintf("Not found mapping read error\n");
            return bytes_read;
          }
          else{
            int block_id = *(temp_memory+j);
            char* ptr;
            malloc_arbit(ptr,4096);
            file_read_block(objfs,block_id,ptr);
            memcpy(buf+bytes_read,ptr,size_left);
            free_arbit(ptr,4096);
          }
          j++;
          blocks_read++;
          bytes_read += size_left;
        }
        free_arbit(temp_memory,4096);
      }
    }

      if(bytes_read == 0)
        return -1;
      return bytes_read;

}

/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf,struct objfs_state *objfs)
{
  int inode_number = buf->st_ino;
  struct object* temp = (struct object*)inode_memory;
  temp = temp + inode_number;
  buf->st_size = temp->size;
  buf->st_blocks = (temp->size)/4096;
  if((temp->size)%4096 != 0)
    (buf->st_blocks)++;

   return 0;
}

/*
   Set your private pointeri, anyway you like.
*/
int objstore_init(struct objfs_state *objfs)
{
  
   malloc_arbit(block_bitmap,1<<20);
   malloc_arbit(inode_bitmap, 1<<17);
   malloc_arbit(eviction_index,4);
   malloc_arbit(max_cache_blocks,4);
   malloc_arbit(data_blocks,8);
   malloc_arbit(cached_blocks,4);
   malloc_arbit(inode_memory,sizeof(struct object)*(1<<20));
   malloc_arbit(objs,4096);

   if(!block_bitmap || !inode_bitmap || !eviction_index || !max_cache_blocks || !data_blocks || !objs || !inode_memory || !cached_blocks){
       dprintf("%s: malloc\n", __func__);
       return -1;
   }

   if(read_block(objfs,0,(char*)objs) < 0)
    return -1;

   for(int i=0;i<256;i++){
    if(read_block(objfs, i+1, (char*)(block_bitmap+i*4096) ) < 0){
      dprintf("Error block bitmap\n");
      return -1;
    }
   }

   for(int i=0;i<32;i++){
    if(read_block(objfs, 257+i, (char *)(inode_bitmap+i*4096) ) < 0){
      dprintf("Error inode bitmap\n");
      return -1;
    }
   }

  for(int i=0;i<(1<<14);i++){
    if(read_block(objfs, 289+i, (char*)(inode_memory+i*4096) ) < 0){
      dprintf("Error inode memory\n");
      return -1;
    }

   }   

   *data_blocks = objfs->disksize;
   *cached_blocks = 0;
   *max_cache_blocks = (128*1024*1024)/sizeof(struct cache_object);
   *eviction_index = 0;

     struct cache_object* start = (struct cache_object*)objfs->cache;
     for(int i=0;i<*max_cache_blocks;i++){
      struct cache_object* temp = start+i;
      temp->cached = 0;
      temp->dirty = 0;
      temp->block_id = 0;
     }

   objfs->objstore_data = objs;
   dprintf("Done objstore init hahaha inode_bitmap %d\n",(int)inode_bitmap[0]);
   
   return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs)
{

  struct cache_object* start = (struct cache_object*)objfs->cache;
  for(int i=0;i<*max_cache_blocks;i++){
    struct cache_object* temp = start+i;
    if(temp->cached == 1 && temp->dirty == 1){
        char* val;
        malloc_arbit(val,4096);
        memcpy(val,temp->value,4096);
        write_block(objfs,temp->block_id,val);
        free_arbit(val,4096);
    }
  }

   if(write_block(objfs,0,(char*)objs) < 0)
    return -1;

   for(int i=0;i<256;i++){
    if(write_block(objfs, i+1, (char*)(block_bitmap+i*4096) ) < 0)
      return -1;

   }

   for(int i=0;i<32;i++){
    if(write_block(objfs, 257+i, (char *)(inode_bitmap+i*4096) ) < 0)
      return -1;
   }

  for(int i=0;i<(1<<14);i++){
    if(write_block(objfs, 289+i, (char*)(inode_memory+i*4096) ) < 0)
      return -1;

   }

   free_arbit(block_bitmap,1<<20);
   free_arbit(inode_bitmap, 1<<17);
   free_arbit(data_blocks,8);
   free_arbit(inode_memory,sizeof(struct object)*(1<<20));
   free_arbit(cached_blocks,4);
   free_arbit(objs,4096);
   free_arbit(eviction_index,4);
   free_arbit(max_cache_blocks,4);

   objfs->objstore_data = NULL;
   dprintf("Done objstore destroy\n");
   return 0;
}
