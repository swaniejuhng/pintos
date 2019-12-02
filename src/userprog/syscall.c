#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"

typedef int pid_t;
static void syscall_handler (struct intr_frame *);

struct lock my_lock;

void
syscall_init (void) 
{
    lock_init(&my_lock);
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_valid_addr(void *vaddr);
pid_t exec(char *filename);
void halt(void);
int wait(pid_t pid);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
void exit(int status);
int fibonacci(int n);
int sum_of_four_int(int a, int b, int c, int d);

bool create (const char* file, unsigned initial_size);
bool remove (const char* file);
int open(const char* file);
void close(int fd);
int filesize(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* my_esp = (uint32_t*)f->esp;
  //hex_dump((uint32_t)f->esp,f->esp,200,1);
  //printf("%d\n",*(my_esp+1) );
  //printf("%d\n", *(int*)(f->esp));

  int syscall_num = *(int*)(f->esp) ; 
      switch(syscall_num) {
      /* document p. 43~ */
        case SYS_HALT:
            halt();
            break;
        case SYS_EXIT:
            // Terminates the current userprogram,
            // returning status to the kernel
            check_valid_addr(my_esp+1);
            exit(*(my_esp+1));
            // in lib/user/syscall.c
            break;
        case SYS_EXEC:
            // Create child process
            // Refer to process_execute() in userprog/process.c
            check_valid_addr(my_esp+1);
            f->eax = exec((char*)(*(my_esp+1)));
            break;
        case SYS_WAIT:
            check_valid_addr(my_esp+1);
            f->eax = wait(*(my_esp+1));
            break;

        case SYS_CREATE:
            check_valid_addr(my_esp+1);
            check_valid_addr(my_esp+2);          
            f->eax=create((char*)*(my_esp+1), (unsigned)*(my_esp+2));
            break;
        case SYS_REMOVE:
            check_valid_addr(my_esp+1);
            f->eax = remove((char*)(*(my_esp+1)));
            break;
        case SYS_OPEN:
            lock_acquire(&my_lock);
            check_valid_addr(my_esp+1);
            f->eax = open((char*)(*(my_esp+1)));
            lock_release(&my_lock);
            break;
        case SYS_FILESIZE:
            check_valid_addr(my_esp+1);
            f->eax = filesize((int)(*(my_esp+1)));
            break;

        case SYS_READ:
            lock_acquire(&my_lock);
            check_valid_addr(my_esp+1);
            check_valid_addr(my_esp+2);
            check_valid_addr(my_esp+3);
            f->eax=read((int)*(my_esp+1), (void*)*(my_esp+2), (unsigned)*(my_esp+3));
            lock_release(&my_lock);
            break;
        case SYS_WRITE:
            lock_acquire(&my_lock);
            check_valid_addr(my_esp+6);
            check_valid_addr(my_esp+7);
            check_valid_addr(my_esp+8);            
            f->eax=write((int)*(my_esp+6), (void*)*(my_esp+7), (unsigned)*(my_esp+8));
            lock_release(&my_lock);
            break;
        
        case SYS_SEEK:
            check_valid_addr(my_esp+1);
            check_valid_addr(my_esp+2);
            seek((int)(*(my_esp+1)),(unsigned)(*(my_esp+2)));
            break;

        case SYS_TELL:
            check_valid_addr(my_esp+1);
            f->eax = tell((int)(*(my_esp+1)));
            break;

        case SYS_CLOSE:
            check_valid_addr(my_esp+1);
            close((int)(*(my_esp+1)));
            break;
        case SYS_FIBONACCI:
            check_valid_addr(my_esp+1);
            f->eax = fibonacci((int)*(my_esp+1));
            break;
        case SYS_SUMOFFOURINT:
            check_valid_addr(my_esp+1);
            check_valid_addr(my_esp+2);
            check_valid_addr(my_esp+3);
            check_valid_addr(my_esp+4);
            f->eax = sum_of_four_int((int)*(my_esp+1), (int)*(my_esp+2), (int)*(my_esp+3), (int)*(my_esp+4));
            break;

  }
}

void check_valid_addr(void *vaddr) {
    struct thread *cur = thread_current();
    /* if null pointer */
    if(!vaddr) exit(-1);
    /* pointer to kernel virtual address space (above PHYS_BASE)? */
    if(is_kernel_vaddr(vaddr)) exit(-1);
    /* pointer to unmapped virtual memory? */
    if(!pagedir_get_page(cur->pagedir, vaddr)) exit(-1);
    
    return;
}

pid_t exec(char *cmd_line) {
    /* create child process by calling process_execute() */
    pid_t cpid = process_execute(cmd_line);
        
    return cpid;
}

void halt(void) {
    shutdown_power_off();
    return;
}

int wait(pid_t pid){
    /* wait until child process expires using process_wait() */
    return process_wait(pid);
}

int read(int fd, void* buffer, unsigned size) {
    int i;
    int ans= -1;
    if(fd == 0) {
        for(i = 0; i < size; i++) {
            *(char*)(buffer + i) = input_getc();
        }
        ans = size;        
    }
    else if (fd >=3 && fd < 128) {
                if(fd <2 || fd>= 128) return -1;
        struct file* f = (thread_current()->fd)[fd];
        if(f==NULL) exit(-1);
        check_valid_addr(buffer);
        
        int res = file_read(f,buffer,(off_t)size);
        ans = res;        
    }
    else
        return -1;
    return ans;
}

int write(int fd, const void* buffer, unsigned size){
    /* STDIN: 0, STDOUT: 1 */
    int ans = -1;
    if(fd==1){
        putbuf(buffer, size);
        ans = size;        
    }
    else{
        if(fd <3 || fd>= 128) return -1;
        struct file* f = (thread_current()->fd)[fd];
        if(f==NULL) exit(-1);
        
        int res =file_write(f,(const void*)buffer,(off_t)size);
       
        ans = res;
    }

        return ans;
}

void exit(int status){
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_current()->exit_status = status;
    int cnt = thread_current()->file_cnt;
    int i;
    
    for(i=3;i<128;i++){
        struct file* f = thread_current()->fd[i];
        if(f==NULL)continue;
        close(i);
        thread_current()->fd[i] = NULL;
    }
    thread_exit();
}

int fibonacci(int n) {
    int n1 = 1, n2 = 1, n3 = 0, i, result;

    if(n == 0) result = 0;
    else if(n < 3) result = 1;
    else {
        for(i = 2; i < n; i++) {
            n3 = n2 + n1;
            n1 = n2;
            n2 = n3;
        }
        result = n3;
    }
    return result;
}

int sum_of_four_int(int a, int b, int c, int d) {
    int result = a + b + c + d;
    return result;
}

bool create (const char* file, unsigned initial_size){
    if(file==NULL) exit(-1);
    
    if(filesys_create(file,initial_size)){
        return true;
    }
    else
        return false;
}
bool remove (const char* file){
    if(file==NULL) exit(-1);
    if(filesys_remove(file)){
        return true;
    }
    else
        return false;
}
int open(const char* file){
    if(file==NULL) return -1;
    int fd = -1;
    //printf("%s\n", file);
    
    
    struct file* f = filesys_open(file);
    if(strcmp(file, thread_current()->name)==0) file_deny_write(f);
    if(f==NULL) return -1;
    fd = thread_current()->file_cnt;
    (thread_current()->fd)[fd] = f;
    (thread_current()->file_cnt)++;
    
    return fd;
}

void close(int fd) {
    struct file* f = (thread_current()->fd)[fd];
    if(f==NULL) exit(-1);
    file_close(f);
    (thread_current()->fd)[fd] = NULL;
}

int filesize(int fd){
    int siz=-1;
    struct file* f = (thread_current()->fd)[fd];
    if(f==NULL) return -1;
    siz = (int)file_length(f);
    //printf("%d\n",siz);
    
    return siz;
}

void seek(int fd, unsigned position) {

    struct file* f = (thread_current()->fd)[fd];
    if(f==NULL) exit(-1);
    file_seek(f, (off_t)position);
}

unsigned tell(int fd) {

    struct file* f = (thread_current()->fd)[fd];
    if(f==NULL) exit(-1);
    file_tell(f);
}
