#include <minix/drivers.h>
#include <minix/driver.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include <sys/ioc_secret.h>
#include "secret.h"


/*
* Function prototypes for the secret driver.
*/
FORWARD _PROTOTYPE( char * secret_name,   (void) );
FORWARD _PROTOTYPE( int secret_open,      (struct driver *d, message *m) );
FORWARD _PROTOTYPE( int secret_close,     (struct driver *d, message *m) );
FORWARD _PROTOTYPE( struct device * secret_prepare, (int device) );
FORWARD _PROTOTYPE( int secret_transfer,  (int procnr, int opcode,
                                         u64_t position, iovec_t *iov,
                                         unsigned nr_req) );
FORWARD _PROTOTYPE( void secret_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( int secret_ioctl,     (struct driver *d, message *m) );


/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( int sef_cb_lu_state_save, (int) );
FORWARD _PROTOTYPE( int lu_state_restore, (void) );


/* Entry points to the secret driver. */
/* need an ioctl for this that the hello struct had as a nop*/
PRIVATE struct driver secret_tab =
{
   secret_name,
   secret_open,
   secret_close,
   secret_ioctl,
   secret_prepare,
   secret_transfer,
   nop_cleanup,
   secret_geometry,
   nop_alarm,
   nop_cancel,
   nop_select,
   do_nop,
};


//represents the device
PRIVATE struct device secret_device;


//keeps count of how many times it has been opened
PRIVATE int open_counter;


//acts as a flag for if the buffer needs to be read is currently being read
PRIVATE int needs_reading;


//the flag for if the device is full or not
PRIVATE int secret_owned;


//the owner of the secret
PRIVATE uid_t secret_owner;


//the current reading position in the buffer
PRIVATE int cur_read_pos;


//the current writing position in buffer
PRIVATE int cur_write_pos;


//the secret buffer
PRIVATE char buffer[SECRET_SIZE];


//returns the secret name
PRIVATE char * secret_name(void)
{
   printf("secret_name()\n");
   return "secret";
}


//will open the file and assign the owner if empty
PRIVATE int secret_open(d, m)
   struct driver *d;
   message *m;
{
   struct ucred calling_process;
   getnucred(m->IO_ENDPT, &calling_process);


   //if there is no owner yet
   if (secret_owned == 0) {
       // if they try to read and write at the same time, return error
       if ((m->COUNT & R_BIT) && (m->COUNT & W_BIT)) {
           return EACCES;
       }
       //if currently reading
       else if (m->COUNT & R_BIT) {
           needs_reading = 0;
       }
       //if currently writing
       else if (m->COUNT & W_BIT) {
           needs_reading = 1;
       }
       //assign the first caller to the owner
       secret_owner = calling_process.uid;
       secret_owned = 1;
   }
   //if the secret is already owned
   else if (secret_owned == 1){
       if (m->COUNT & W_BIT) {
           //if its owned, it is full and can't be written to
           return ENOSPC;
       }
       if (m->COUNT & R_BIT) {
           //if its full and you are tring to read, check if reading process is not the owner
           if (calling_process.uid != secret_owner) {
               return EACCES;
           }
           needs_reading = 0;
       }
   }
   open_counter++;
   return OK;
}


//closes the file, decrements the counter
PRIVATE int secret_close(d, m)
   struct driver *d;
   message *m;
{
   open_counter--;
   //once there is no one opening it, reset the state
   if (open_counter == 0 && needs_reading == 0) {
       secret_owner = -1;
       secret_owned = 0;
       memset(buffer, 0, sizeof(buffer));
       cur_write_pos = 0;
       cur_read_pos = 0;
   }
   return OK;
}


//switch the ownership
PRIVATE int secret_ioctl(d, m)
   struct driver *d;
   message *m;
{v
   uid_t grantee;
   int result;
   //check for if the command wasnt SSGRANT
   if (m->REQUEST != SSGRANT) {
       return ENOTTY;
   }
   //must cast to virtual bytes
   result = sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)m->IO_GRANT, 0,
                          (vir_bytes)&grantee, sizeof(grantee), D);
   secret_owner = grantee;                    
   return result;
}


//same as hello driver
PRIVATE struct device * secret_prepare(dev)
   int dev;
{
   secret_device.dv_base.lo = 0;
   secret_device.dv_base.hi = 0;
   secret_device.dv_size.lo = 0;
   secret_device.dv_size.hi = 0;
   return &secret_device;
}


PRIVATE int secret_transfer(proc_nr, opcode, position, iov, nr_req)
   //process number
   int proc_nr;
   //gather or scatter
   int opcode;
   u64_t position;
   iovec_t *iov;
   unsigned nr_req;
{
   int bytes;
   int result;


   if (opcode == DEV_GATHER_S) {
       //reading
       if (cur_write_pos - cur_read_pos < iov->iov_size) {
           bytes = cur_write_pos - cur_read_pos;
       } else {
           bytes = iov->iov_size;
       }


       if (bytes <= 0) {
           return OK;
       }


       result = sys_safecopyto(proc_nr, iov->iov_addr, 0,
                           (vir_bytes)(buffer + cur_read_pos),
                           bytes, D);
       iov->iov_size -= bytes;
       cur_read_pos += bytes;


   } else if (opcode == DEV_SCATTER_S) {
       //writing
       if (SECRET_SIZE - cur_write_pos < iov->iov_size) {
           bytes = SECRET_SIZE - cur_write_pos;
       } else {
           bytes = iov->iov_size;
       }


       if (bytes <= 0) {
           return ENOSPC;
       }


       result = sys_safecopyfrom(proc_nr, iov->iov_addr, 0,
                              (vir_bytes)(buffer + cur_write_pos),
                              bytes, D);
       iov->iov_size -= bytes;
       cur_write_pos += bytes;


   } else {
       //its an invalid opcode not read or write
       return EINVAL;
   }


   return result;
}




//same as hello driver
PRIVATE void secret_geometry(entry)
   struct partition *entry;
{
   printf("hello_geometry()\n");
   entry->cylinders = 0;
   entry->heads     = 0;
   entry->sectors   = 0;
}


//save all the variables in the curState instance of the struct
PRIVATE int sef_cb_lu_state_save(int state) {
   secret_state curState;
   curState.open_counter = open_counter;
   curState.owner = secret_owner;
   curState.secret_owned = secret_owned;
   curState.cur_write_pos = cur_write_pos;
   curState.cur_read_pos = cur_read_pos;
   memcpy(curState.buffer, buffer, sizeof(buffer));
   ds_publish_mem(SECRET_STATE, (char*)&curState,
                  sizeof(curState), DSF_OVERWRITE);


   return OK;
}


PRIVATE int lu_state_restore() {
//restore all the variables from the curState struct
   secret_state curState;
   size_t curStateSize = sizeof(curState);
   ds_retrieve_mem(SECRET_STATE, (char*)&curState, &curStateSize);
   open_counter = curState.open_counter;
   secret_owner = curState.owner;
   secret_owned = curState.secret_owned;
   cur_write_pos = curState.cur_write_pos;
   cur_read_pos = curState.cur_read_pos;
   memcpy(buffer, curState.buffer, sizeof(curState.buffer));
   ds_delete_mem(SECRET_STATE);
   return OK;
}


//same as hello driver
PRIVATE void sef_local_startup()
{
   /*
    * Register init callbacks. Use the same function for all event types
    */
   sef_setcb_init_fresh(sef_cb_init);
   sef_setcb_init_lu(sef_cb_init);
   sef_setcb_init_restart(sef_cb_init);


   /*
    * Register live update callbacks.
    */
   /* - Agree to update immediately when LU is requested in a valid state. */
   sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
   /* - Support live update starting from any standard state. */
   sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
   /* - Register a custom routine to save the state. */
   sef_setcb_lu_state_save(sef_cb_lu_state_save);


   /* Let SEF perform startup. */
   sef_startup();
}


PRIVATE int sef_cb_init(int type, sef_init_info_t *info)
{
   //initialize the driver
   int do_announce_driver = TRUE;


   if (type == SEF_INIT_FRESH) {
       open_counter = 0;
       cur_write_pos = 0;
       cur_read_pos = 0;
       secret_owned = 0;
       secret_owner = -1;


   } else if (type == SEF_INIT_LU) {
       //need to restore the state
       lu_state_restore();
       do_announce_driver = FALSE;
       printf("%snew version\n");


   } else if (type == SEF_INIT_RESTART) {
       printf("%just restarted\n");
   }


   //announce when up
   if (do_announce_driver) {
       driver_announce();
   }


   //been initialized properly
   return OK;
}


//same as hello driver
PUBLIC int main(int argc, char **argv)
{
   /*
    * Perform initialization.
    */
   sef_local_startup();


   /*
    * Run the main loop.
    */
   driver_task(&secret_tab, DRIVER_STD);
   return OK;
}