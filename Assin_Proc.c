#include <linux/kernel.h> /* Needed by all modules */
#include <linux/module.h> /* Needed for KERN_ALERT */
#include <linux/kprobes.h>
#include <linux/time.h>
#include<linux/init.h>
#include<linux/syscalls.h>
#include<linux/kernel.h>
#include<linux/slab.h>        
#include <linux/proc_fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define _XOPEN_SOURCE 700
#define BUFF_SIZE 500

int init_module(void)
{
   printk("<1>Hello CORONA WORLD 1.\n");
					// A non 0 return means init_module failed
   return 0;
}


void cleanup_module(void)
{
  printk(KERN_ALERT "Goodbye CORONA 1.\n");
}  

static int my_pid;

typedef struct probe_struct{
    unsigned long addr;
    long time;
}probe_struct;

probe_struct array[BUFF_SIZE];
static int head = -1;

ssize_t 
my_read(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
    int val;
    int uncopied = copy_to_user(buf, &array, sizeof(array));
    if(uncopied)
    {
        printk(KERN_ALERT "%d bytes could not be copied!\n", uncopied);
        val = -EFAULT;
    }
    else{
   	val = (int) sizeof(array);
	}
    return val;
}

struct file_operations my_fops  = {
    read : my_read
};

int 
page_fault_handler(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address, unsigned int flags)
{
    struct timespec curr_tm;
    getnstimeofday(&curr_tm);

    if(current->pid == my_pid)
    {
        if(head == BUFF_SIZE -1)
            head = 0;
        else
            head++;

        array[head].time = curr_tm.tv_nsec;
        array[head].addr = address;

        printk(KERN_INFO "Page fault at %lx\n", address);
    }

    jprobe_return();
    return 0;
}

static struct jprobe my_jprobe = {
    .entry = page_fault_handler,
    .kp = {
        .symbol_name = "handle_mm_fault",
    },
};

static int 
__init jprobe_init(void)
{
    struct proc_dir_entry *entry;

    if (register_jprobe(&my_jprobe) < 0) {
        printk(KERN_INFO "registering probe failed.\n");
        return -1;
    }
    printk(KERN_INFO "Jprobe : %p, handler address : %p\n", my_jprobe.kp.addr, my_jprobe.entry);


    entry = proc_create("proc_device", 0, NULL, &my_fops);

    if(entry == NULL)
        return -ENOMEM;

    return 0;
}


static void 
__exit jprobe_exit(void)
{
    unregister_jprobe(&my_jprobe);
    printk(KERN_INFO "jprobe at %p unregistered\n", my_jprobe.kp.addr);
    remove_proc_entry("proc_device", NULL);
}

module_param(my_pid, int, 0);
MODULE_LICENSE("GPL");
module_init(jprobe_init)
module_exit(jprobe_exit)
