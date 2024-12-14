#ifndef SECRET_H
#define SECRET_H


#include <sys/types.h>
#include <minix/driver.h>


// SECRET_SIZE as said in the spec
#ifndef SECRET_SIZE
#define SECRET_SIZE 8192
#endif


// the name of the state for live updates
#define SECRET_STATE "secret_state"


// will save and restore the state of the driver.
typedef struct secret_state {
   int open_counter;          // how many open file descriptors
   int secret_owned;           // the flag for if its owned
   uid_t owner;               // UID of the owner
   int cur_write_pos;              // the current writing position within the buffer
   int cur_read_pos;               // the current reading position in the buffer
   char buffer[SECRET_SIZE];  // the secret buffer
} secret_state;


#endif /* SECRET_H */