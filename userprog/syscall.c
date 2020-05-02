#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
void syscall_entry (void);
void syscall_handler (struct intr_frame *);

int write (int , const void *, unsigned);
int read (int , void *, unsigned);
bool create (const char *, unsigned);
void check_arg(void *);
bool remove (const char *);
int open (const char *);
int filesize (int );
void close (int );

//static int get_user (const uint8_t *);
//static int get_user (const uint8_t *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f ) {
	
    uint64_t arg[6];
    arg[0] = f->R.rax;
    arg[1] = f->R.rdi;
    arg[2] = f->R.rsi;
    arg[3] = f->R.rdx;
    arg[4] = f->R.r10;
    arg[5] = f->R.r8;
    arg[6] = f->R.r9;
    /*
    for(int i=0; i<6; i++){
        arg[i] =  f->rsp + 8*i;
        //strlcpy( arg[i], ((char *)(f->rsp + 8*i)), strlen(argv[i])+1);
    }*/
/*
    check_arg(arg[0]);
    printf("syscall: %d\n",*(int *)arg[0]);
*/
    switch(arg[0]){
        case SYS_HALT :
            halt();
            break;
        case SYS_EXIT :
            //check_arg(arg[1]);
            f->R.rax = arg[1];
            exit(arg[1]);
            break;
        case SYS_FORK:
            //thread_current() -> tf = f;
            //f->R.rax = fork (arg[1]);
            break;
        case SYS_EXEC:
            break;
        case SYS_WAIT:
            break;
        case SYS_CREATE:
            f->R.rax = create(arg[1],arg[2]);
            break;
        case SYS_REMOVE:
            f->R.rax = remove(arg[1]);
            break;
        case SYS_OPEN:
            f->R.rax = open(arg[1]);
            break;
        case SYS_FILESIZE:
            f->R.rax = filesize(arg[1]);
            break;
        case SYS_READ:
            f->R.rax = read(arg[1],arg[2],arg[3]);
            break;
        case SYS_WRITE:
            f->R.rax = write (arg[1], arg[2], arg[3]);
            break;
        case SYS_SEEK:
            break;
        case SYS_TELL:
            break;
        case SYS_CLOSE:
            close(arg[1]);
            break;

        
        default :
            thread_exit ();


    }
    //printf("syscall: %d\n",(f->R).rax);
	//printf ("system call!\n");
	
}
/*
static int
get_user (const uint8_t *uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
         : "=&a" (result) : "m" (*uaddr));
    return result;
}
*/
void
check_arg(void *addr){
    if ((!is_user_vaddr(addr))||(addr == NULL)){
        exit(-1);
    }
    // exception에서 page fault 시 exit(-1) 코드 추가
}

void
halt(void){
    for(int i=3; i<128; i++){
        if(thread_current()->files[i] != NULL){
            close(i);
        }
    }
    power_off ();
}

void
exit (int status){

    for(int i=3; i<128; i++){
        if(thread_current()->files[i] != NULL){
            close(i);
        }
    }
    
    printf("%s: exit(%d)\n", thread_current()->name, status);
    thread_exit();
}

int 
write (int fd, const void *buffer, unsigned size){
    check_arg(buffer);
    check_arg(buffer+size-1);
    
    struct file * _file;
    _file = thread_current() -> files[fd];

    if (fd == 1){ //stdout
        putbuf(buffer, size);
        return size;
    }
    else if(fd > 2 && fd < 128){
        
        if (_file == NULL ){
            return 0;
        }
        
        return file_write(_file, buffer, size); //아직 file 확장은 구현안됨
    }
    else{
        //printf("1\n");
        return 0;
    }
    
}

int 
read (int fd, void *buffer, unsigned size){
    check_arg(buffer);
    check_arg(buffer+size-1);

    struct file * _file;
    _file = thread_current() -> files[fd];

    if (fd==0){ //stdin
        input_getc();
        return size;
    }
    else if(fd > 2 && fd < 128){
        
        if (_file == NULL ){
            return -1;
        }
        return file_read(_file, buffer, size);
    }
    else{
        return -1;
    }
}

/*
pid_t 
fork (const char *thread_name){
    return process_fork(thread_name);
}*/

bool 
create (const char *file, unsigned initial_size){
    
    check_arg(file);
    return filesys_create (file, initial_size);
}

bool 
remove (const char *file){
    
    check_arg(file);
    return filesys_remove (file);
}

int 
open (const char *file){
    
    int i = 3; // 0: stdin, 1: stdout, 2: stderr
    struct file *_file;
    check_arg(file);

    _file = filesys_open(file);

    if (_file == NULL){
        //printf("1\n");
        return -1;
    }
    else{
        while((thread_current() -> files[i] != NULL) && (i<128)){
            i += 1;
        }
        if(i==128) return -1; //all of files are full.

        thread_current() -> files[i] = _file;

        return i;
    }

}

int 
filesize (int fd){
    struct file *_file;
    _file = thread_current()->files[fd];

    if(fd<0 && fd>127){
        return -1;
    }

    if(_file == NULL){
        exit(-1);
    }

    int l = file_length(_file);
    return l;
}

void 
close (int fd){
    struct file *_file;
    _file = thread_current()->files[fd];

    if(fd<0 && fd>127){
        return;
    }

    if(_file == NULL){
        return;
    }

    file_close (_file);
    thread_current()->files[fd] = NULL; //나중에 exit에서 중복 방지
}