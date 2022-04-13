/**************************************************************************

Copyright (c) 2005-2015, Silicom
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Silicom nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17))
    #include <linux/config.h>
#endif
#if defined(CONFIG_SMP) && ! defined(__SMP__)
    #define __SMP__
#endif



#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/module.h> /* Specifically, a module */
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/rcupdate.h>
#ifdef BP_SELF_TEST
    #include <linux/etherdevice.h>
#endif

#include <asm/uaccess.h> /* for get_user and put_user */
#include <linux/sched.h>
#include <linux/ethtool.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/hardirq.h>

#include <linux/types.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>


#include "bp_ioctl.h"
#include "bp_mod.h"
#include "bypass.h"
#include "libbp_sd.h"
#include "bp_cmd.h"


#define SUCCESS 0
#define BP_MOD_VER  VER_STR_SET
#define BP_MOD_DESCR "Silicom Bypass-SD Control driver"

static int Device_Open = 0;
static int major_num=0;
spinlock_t bypass_wr_lock;
static struct i2c_client *client = NULL;
static unsigned int na_base = 0;

MODULE_AUTHOR("Anna Lukin, annal@silicom.co.il");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(BP_MOD_DESCR);
MODULE_VERSION(BP_MOD_VER);
/*spinlock_t bpvm_lock;*/



#define lock_bpctl() 					\
if (down_interruptible(&bpctl_sema)) {			\
	return -ERESTARTSYS;				\
}							\

#define unlock_bpctl() 					\
	up(&bpctl_sema);

int bp_shutdown = 0;
/* Media Types */
typedef enum {
    bp_copper = 0,
    bp_fiber,
    bp_cx4,
    bp_none,
} bp_media_type;

struct pfs_unit_sd {
    struct proc_dir_entry *proc_entry;
    char proc_name[32];
} ; 

struct bypass_pfs_sd {
    char dir_name[32];
    struct proc_dir_entry *bypass_entry;
    struct pfs_unit_sd bypass_info; 
    struct pfs_unit_sd bypass_slave;  
    struct pfs_unit_sd bypass_caps;   
    struct pfs_unit_sd wd_set_caps;   
    struct pfs_unit_sd bypass;     
    struct pfs_unit_sd bypass_change; 
    struct pfs_unit_sd bypass_wd;     
    struct pfs_unit_sd wd_expire_time;
    struct pfs_unit_sd reset_bypass_wd;   
    struct pfs_unit_sd dis_bypass; 
    struct pfs_unit_sd bypass_pwup; 
    struct pfs_unit_sd bypass_pwoff; 
    struct pfs_unit_sd std_nic;
    struct pfs_unit_sd tap;
    struct pfs_unit_sd dis_tap;
    struct pfs_unit_sd tap_pwup;
    struct pfs_unit_sd tap_change;
    struct pfs_unit_sd wd_exp_mode; 
    struct pfs_unit_sd wd_autoreset;
    struct pfs_unit_sd tpl;

}    ; 


typedef struct _bpctl_dev {
    char *name;
    char *desc;
    struct pci_dev *pdev;  /* PCI device */
    struct net_device *ndev; /* net device */
    unsigned long mem_map;
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
	uint8_t  bus_p;
    uint8_t  slot_p;
    uint8_t  func_p;
    u_int32_t  device;
    u_int32_t  vendor;
    u_int32_t  subvendor;
    u_int32_t  subdevice;
    int      ifindex;
    uint32_t bp_caps;
    uint32_t bp_caps_ex;
    uint8_t  bp_fw_ver;
    int  bp_ext_ver;
    int wdt_status;
    unsigned long bypass_wdt_on_time;
    uint32_t bypass_timer_interval;
    struct timer_list bp_timer;
    uint32_t reset_time;
    uint8_t   bp_status_un;
    atomic_t  wdt_busy;
    bp_media_type media_type;
    int   bp_tpl_flag;
    struct timer_list bp_tpl_timer;
    int   tpl_wait;	
    spinlock_t bypass_wr_lock;
    int   bp_10g;
    int   bp_10gb;
    int   bp_fiber5;
    int   bp_10g9;
    int   bp_i80;
    int   bp_540;
    int   bp_40g;
    int   bp_rang;
/* #ifdef BP_SELF_TEST */
    int (*hard_start_xmit_save) (struct sk_buff *skb,
                                 struct net_device *dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31))
    const struct net_device_ops *old_ops;
    struct net_device_ops new_ops;
#endif
    int   bp_self_test_flag;
    char *bp_tx_data;
        uint8_t rb_oem;
    uint8_t oem_data[20];
    uint32_t ledctl_default;
    uint32_t ledctl_mode1;
    uint32_t ledctl_mode2;
    uint32_t  led_status;
//#endif
    struct timer_list blink_timer;
    struct timer_list wait_timer; 
/* #endif */
    struct bypass_pfs_sd bypass_pfs_set; 
    char proc_dir[128];

} bpctl_dev_t;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,15,0))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))

#ifndef do_gettimeofday

#define do_gettimeofday(x) _kc_do_gettimeofday(x)
static inline void _kc_do_gettimeofday(struct timeval *tv)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec/1000;
}

#endif
#endif
#endif

int usec_delay_bp1(unsigned long x) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
	struct timeval tv, tv_end;
    do_gettimeofday(&tv);
#endif
    udelay(x);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
	do_gettimeofday(&tv_end);
    if (tv_end.tv_usec>tv.tv_usec) {
        if ((tv_end.tv_usec-tv.tv_usec)<(x))
            return 0;
    } else if ((~tv.tv_usec+tv_end.tv_usec)<(x))
        return 0;
#endif
    return 1;

}
void usec_delay_bp(unsigned long x) {
#if 1
    int i=2;
    while (i--) {
        if (usec_delay_bp1(x))
            return;
	   /* printk("bpmod: udelay failed!\n");*/
    }
    /*printk("bpmod: udelay failed!!\n");*/
#else
	udelay(x);
#endif
}


void msec_delay_bp(unsigned long x){
#ifdef BP_NDELAY_MODE
    return;
#else
    int  i; 
    if (in_interrupt()) {
        for (i = 0; i < 1000; i++) {
            usec_delay_bp(x) ;       
        }                     
    } else {
        msleep(x); 
    }
#endif    
}




/*static bpctl_dev_t *bpctl_dev_a;
static bpctl_dev_t *bpctl_dev_b;*/
static bpctl_dev_t *bpctl_dev_arr;

static struct semaphore bpctl_sema;
static int device_num = 0;

#ifdef ADI_RANGELEY_SUPPORT
static int na_device_num = 0;
#endif


static int get_dev_idx(int ifindex);
static bpctl_dev_t *get_master_port_fn(bpctl_dev_t *pbpctl_dev);
static int disc_status(bpctl_dev_t *pbpctl_dev);
static int bypass_status(bpctl_dev_t *pbpctl_dev);
static int wdt_timer(bpctl_dev_t *pbpctl_dev, int *time_left);
static bpctl_dev_t *get_status_port_fn(bpctl_dev_t *pbpctl_dev);
static void if_scan_init(void);

static int tx_status (bpctl_dev_t *pbpctl_dev);
static int set_tx (bpctl_dev_t *pbpctl_dev, int tx_state);

int bypass_proc_create_dev_sd(bpctl_dev_t * pbp_device_block);
int bypass_proc_remove_dev_sd(bpctl_dev_t * pbp_device_block);
int bp_proc_create(void);


int is_bypass_fn(bpctl_dev_t *pbpctl_dev);
int get_dev_idx_bsf(int bus, int slot, int func);
int get_bypass_caps_fn(bpctl_dev_t *pbpctl_dev);
int get_wd_expire_fn (bpctl_dev_t *pbpctl_dev);

int wdt_on(bpctl_dev_t *pbpctl_dev, unsigned int timeout);


static int bp_get_dev_idx_bsf(struct net_device *dev, int *index)
{
	struct ethtool_drvinfo drvinfo = {0};
	char *buf;
	int bus, slot, func;

	if (dev->ethtool_ops && dev->ethtool_ops->get_drvinfo)
		dev->ethtool_ops->get_drvinfo(dev, &drvinfo);
	else
		return -EOPNOTSUPP;

	if (!drvinfo.bus_info)
		return -ENODATA;
	if (!strcmp(drvinfo.bus_info, "N/A"))
		return -ENODATA;

	buf = strchr(drvinfo.bus_info, ':');
	if (!buf)
		return -EINVAL;
	buf++;
	if (sscanf(buf, "%x:%x.%x", &bus, &slot, &func) != 3)
		return -EINVAL;

	*index = get_dev_idx_bsf(bus, slot, func);
	return 0;
}
static int
bp_reboot_event(struct notifier_block *nb,
		unsigned long event,
		void *ptr)
{
    bp_shutdown = 1;
    return NOTIFY_DONE;
}	
static int bp_device_event(struct notifier_block *unused,
			   unsigned long event, void *ptr)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0))
	struct net_device *dev = ptr;
#else
	struct net_device *dev = netdev_notifier_info_to_dev(ptr);
#endif
	static bpctl_dev_t *pbpctl_dev, *pbpctl_dev_m, *pbpctl_dev2;
	int dev_num = 0, ret = 0, ret_d = 0, time_left = 0;
	 /*printk("BP_PROC_SUPPORT event =%d %s %d\n", event,dev->name, dev->ifindex ); 
	 return NOTIFY_DONE;*/ 
	if (!dev)
		return NOTIFY_DONE;

	if (event == NETDEV_REGISTER) {
	    	int idx_dev;

		if (bp_get_dev_idx_bsf(dev, &idx_dev))
			return NOTIFY_DONE;

		if (idx_dev == -1)
			return NOTIFY_DONE;

		bpctl_dev_arr[idx_dev].ifindex = dev->ifindex;
		bpctl_dev_arr[idx_dev].ndev = dev;

		bypass_proc_remove_dev_sd(&bpctl_dev_arr[idx_dev]);
		bypass_proc_create_dev_sd(&bpctl_dev_arr[idx_dev]);
		return NOTIFY_DONE;
	}
	if (event == NETDEV_UNREGISTER) {
		int idx_dev = 0;
		for (idx_dev = 0;
		     ((idx_dev < device_num)
		      && (bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
			if (bpctl_dev_arr[idx_dev].ndev == dev) {
				bypass_proc_remove_dev_sd(&bpctl_dev_arr
							  [idx_dev]);
				bpctl_dev_arr[idx_dev].ndev = NULL;

				return NOTIFY_DONE;

			}

		}
		return NOTIFY_DONE;
	}
	if (event == NETDEV_CHANGENAME) {
		int idx_dev = 0;
		for (idx_dev = 0;
		     ((idx_dev < device_num)
		      && (bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
			if (bpctl_dev_arr[idx_dev].ndev == dev) {
				bypass_proc_remove_dev_sd(&bpctl_dev_arr
							  [idx_dev]);
				bypass_proc_create_dev_sd(&bpctl_dev_arr
							  [idx_dev]);

				return NOTIFY_DONE;

			}

		}
		return NOTIFY_DONE;

	}

	switch (event) {

	case NETDEV_CHANGE:{

			
			if (((dev_num = get_dev_idx(dev->ifindex)) == -1) ||
			    (!(pbpctl_dev = &bpctl_dev_arr[dev_num])))
				return NOTIFY_DONE;

			if(bp_shutdown == 1)
			    return NOTIFY_DONE;
			    
			if ((is_bypass_fn(pbpctl_dev)) == 1){
				pbpctl_dev_m = pbpctl_dev;
				pbpctl_dev2 = get_status_port_fn(pbpctl_dev);
			}
			else {
				pbpctl_dev2 = pbpctl_dev_m = get_master_port_fn(pbpctl_dev);
			}
				
			if ((!pbpctl_dev_m)||(!pbpctl_dev2))
				return NOTIFY_DONE;
							
			if (netif_carrier_ok(dev)) {
				if(pbpctl_dev_m->bp_tpl_flag){
					if (tx_status(pbpctl_dev)) {
						pbpctl_dev->tpl_wait = 0;
						if (!tx_status(pbpctl_dev2)) {
							pbpctl_dev2->tpl_wait = 1;
            				set_tx(pbpctl_dev2, 1);
						}
					}
				}
				return NOTIFY_DONE;
			}	

			if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
			ret = get_wd_expire_fn(pbpctl_dev_m);
			if (ret == 1)
				printk("bpmod: %s WDT has expired\n", dev->name);	
			} else {
				ret = bypass_status(pbpctl_dev_m);
				if (ret == 1)
					printk("bpmod: %s is in the Bypass mode now",
				       dev->name);
				ret_d = disc_status(pbpctl_dev_m);
				if (ret_d == 1)
					printk("bpmod: %s is in the Disconnect mode now",
				     dev->name);
				if (ret || ret_d) {
					wdt_timer(pbpctl_dev_m, &time_left);
					if (time_left == -1)
						printk("; WDT has expired");
					printk(".\n");

				}
			}
			if(pbpctl_dev_m->bp_tpl_flag){
				if(pbpctl_dev_m->bp_tpl_flag){
					if (tx_status(pbpctl_dev)) {
						if (tx_status(pbpctl_dev2)) 
            				set_tx(pbpctl_dev2, 0);
        			} else
						printk("bpmod: %s NIC Link is Down forse by TPL mode.\n",
										dev->name); 
							
				}
			}
			return NOTIFY_DONE;

		}

	default:
		return NOTIFY_DONE;

	}
	return NOTIFY_DONE;

}

static struct notifier_block bp_notifier_block = {
    .notifier_call = bp_device_event,
};
static struct notifier_block bp_reboot_block = {
    .notifier_call = bp_reboot_event,
};



static int device_open(struct inode *inode, struct file *file)
{
#ifdef DEBUG
    printk("device_open(%p)\n", file);
#endif
    Device_Open++;
/*
* Initialize the message
*/
    return SUCCESS;
}
static int device_release(struct inode *inode, struct file *file)
{
#ifdef DEBUG
    printk("device_release(%p,%p)\n", inode, file);
#endif
    Device_Open--;
    return SUCCESS;
}

/******************* I2C INTERFACE *****************************************************/
static inline void setsda(bpctl_dev_t *pslcm_dev, unsigned char bit){
	writel(0x3f80010, (void *)((pslcm_dev->mem_map) + 0x88100));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	if(bit)
		writel(0x60, (void *)((pslcm_dev->mem_map) + 0x88184));
	else
		writel(0x40, (void *)((pslcm_dev->mem_map) + 0x88184));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
}

static inline void setscl(bpctl_dev_t *pslcm_dev, unsigned char bit){
	writel(0x3f80010, (void *)((pslcm_dev->mem_map) + 0x88104));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	if(bit)
		writel(0x61, (void *)((pslcm_dev->mem_map) + 0x88184));
	else
		writel(0x41, (void *)((pslcm_dev->mem_map) + 0x88184));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
}


static inline void sdalo(bpctl_dev_t *pslcm_dev)
{
	
	setsda(pslcm_dev, 0);
	udelay((TIME_CLK + 1) / 2);
}

static inline void sdahi(bpctl_dev_t *pslcm_dev)
{
	setsda(pslcm_dev, 1);
	udelay((TIME_CLK + 1) / 2);
}

static inline unsigned char getscl(bpctl_dev_t *pslcm_dev)
{
	unsigned char ctrl_ext;
	writel(0x3f00000, (void *)((pslcm_dev->mem_map) + 0x88104));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	 ctrl_ext = BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	return ( (ctrl_ext&BIT_1)? 1:0);
}

static inline unsigned char getsda(bpctl_dev_t *pslcm_dev)
{
	unsigned char ctrl_ext;
	writel(0x3f00000, (void *)((pslcm_dev->mem_map) + 0x88100));
	BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	ctrl_ext = BP40G_RD_REG(pslcm_dev, GPIO_STAT);
	return ( (ctrl_ext&BIT_0)? 1:0);
}


/*
 * Raise scl line, and do checking for delays. This is necessary for slower
 * devices.
 */
static int sclhi(bpctl_dev_t *pslcm_dev)
{
	unsigned long start;

	setscl(pslcm_dev, 1);

	/* Not all adapters have scl sense line... !!!*/

	start = jiffies;
	while (!getscl(pslcm_dev)) {
		/* This hw knows how to read the clock line, so we wait
		 * until it actually gets high.  This is safer as some
		 * chips may hold it low ("clock stretching") while they
		 * are processing data internally.
		 */
		if (time_after(jiffies, start + 5)) {
			/* Test one last time, as we may have been preempted
			 * between last check and timeout test.
			 */
			if (getscl(pslcm_dev))
				break;
			return -1;
		}
		cpu_relax();
	}

	udelay(TIME_CLK);
	return 0;
}

static inline void scllo(bpctl_dev_t *pslcm_dev)
{
	setscl(pslcm_dev, 0);
	udelay(TIME_CLK / 2);
}



/* --- other auxiliary functions --------------------------------------	*/
static void i2c_start(bpctl_dev_t *pslcm_dev)
{
	/* assert: scl, sda are high */
	sdahi(pslcm_dev);
	sclhi(pslcm_dev);
	setsda(pslcm_dev, 0);
	udelay(TIME_CLK);
	scllo(pslcm_dev);
}

static void i2c_repstart(bpctl_dev_t *pslcm_dev)
{
	/* assert: scl is low */
	sdahi(pslcm_dev);
	sclhi(pslcm_dev);
	setsda(pslcm_dev, 0);
	udelay(TIME_CLK);
	scllo(pslcm_dev);
}


static void i2c_stop(bpctl_dev_t *pslcm_dev)
{
	/* assert: scl is low */
	sdalo(pslcm_dev);
	sclhi(pslcm_dev);
	setsda(pslcm_dev, 1);
	udelay(TIME_CLK);
}



/* send a byte without start cond., look for arbitration,
   check ackn. from slave */
/* returns:
 * 1 if the device acknowledged
 * 0 if the device did not ack
 * -ETIMEDOUT if an error occurred (while raising the scl line)
 */
static int i2c_outb(bpctl_dev_t *pslcm_dev, unsigned char c)
{
	int i;
	int sb;
	int ack;

	/* assert: scl is low */
	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 1;
		setsda(pslcm_dev, sb);
		udelay((TIME_CLK + 1) / 2);
		if (sclhi(pslcm_dev) < 0) { /* timed out */
			printk("bpmod: timed out\n");
			return -1;
		}
		/* FIXME do arbitration here:
		 * if (sb && !getsda(adap)) -> ouch! Get out of here.
		 *
		 * Report a unique code, so higher level code can retry
		 * the whole (combined) message and *NOT* issue STOP.
		 */
		scllo(pslcm_dev);
	}
	sdahi(pslcm_dev);
	if (sclhi(pslcm_dev) < 0) { /* timeout */
		/*printk("bpmod: i2c_outb: 0x%02x, "
		    "timeout at ack\n", (int)c);*/
		return -1;
	}

	/* read ack: SDA should be pulled down by slave, or it may
	 * NAK (usually to report problems with the data we wrote).
	 */
	ack = !getsda(pslcm_dev);    /* ack: sda is pulled low -> success */
	/*printk("bpmod: i2c_outb: 0x%02x %s\n", (int)c,
		ack ? "A" : "NA");*/

	scllo(pslcm_dev);
	return ack;
	/* assert: scl is low (sda undef) */
}


static int i2c_inb(bpctl_dev_t *pslcm_dev)
{
	/* read byte via i2c port, without start/stop sequence	*/
	/* acknowledge is sent in i2c_read.			*/
	int i;
	unsigned char indata = 0;

	/* assert: scl is low */
	sdahi(pslcm_dev);
	for (i = 0; i < 8; i++) {
		if (sclhi(pslcm_dev) < 0) { /* timeout */
			/*printk("bpmod: i2c_inb: timeout at bit "
				"#%d\n", 7 - i);*/
			return -1;
		}
		indata *= 2;
		if (getsda(pslcm_dev))
			indata |= 0x01;
		setscl(pslcm_dev, 0);
		udelay(i == 7 ? TIME_CLK / 2 : TIME_CLK);
	}
	/* assert: scl is low */
	return indata;
}

/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
#if 0
static int test_bus(bpctl_dev_t *pslcm_dev)
{

	int scl, sda;

	sda = getsda(pslcm_dev);
	scl = getscl(pslcm_dev);
	if (!scl || !sda) {
		printk("bpmod: bus seems to be busy (scl=%d, sda=%d)\n",
		       scl, sda);
		goto bailout;
	}

	sdalo(pslcm_dev);
	sda = getsda(pslcm_dev);
	scl = getscl(pslcm_dev);
	if (sda) {
		printk("bpmod: SDA stuck high!\n");
		goto bailout;
	}
	if (!scl) {
		printk("bpmod: SCL unexpected low "
		       "while pulling SDA low!\n");
		goto bailout;
	}

	sdahi(pslcm_dev);
	sda = getsda(pslcm_dev);
	scl = getscl(pslcm_dev);
	if (!sda) {
		printk("bpmod: SDA stuck low!\n");
		goto bailout;
	}
	if (!scl) {
		printk("bpmod: SCL unexpected low "
		       "while pulling SDA high!\n");
		goto bailout;
	}

	scllo(pslcm_dev);
	sda = getsda(pslcm_dev);
	scl = getscl(pslcm_dev);
	if (scl) {
		printk("bpmod:  SCL stuck high!\n");
		goto bailout;
	}
	if (!sda) {
		printk("bpmod:  SDA unexpected low "
		       "while pulling SCL low!\n");
		goto bailout;
	}

	sclhi(pslcm_dev);
	sda = getsda(pslcm_dev);
	scl = getscl(pslcm_dev);
	if (!scl) {
		printk(KERN_WARNING "bpmod: SCL stuck low!\n");
		goto bailout;
	}
	if (!sda) {
		printk("bpmod: SDA unexpected low "
		       "while pulling SCL high!\n");
		goto bailout;
	}

	/*printk("bpmod: Test OK\n");*/
	return 0;
bailout:
	sdahi(pslcm_dev);
	sclhi(pslcm_dev);

	return -ENODEV;
}
#endif

/* ----- Utility functions
 */

/* try_address tries to contact a chip for a number of
 * times before it gives up.
 * return values:
 * 1 chip answered
 * 0 chip did not answer
 * -x transmission error
 */
static int try_address(bpctl_dev_t *pslcm_dev,
		       unsigned char addr, int retries)
{
	int i, ret = 0;

	for (i = 0; i <= retries; i++) {
		ret = i2c_outb(pslcm_dev, addr);
		if (ret == 1 || i == retries)
			break;
		/*printk("bpmod: emitting stop condition\n");*/
		i2c_stop(pslcm_dev);
		udelay(TIME_CLK);
		yield();
		/*printk("bpmod: emitting start condition\n");*/
		i2c_start(pslcm_dev);
	}
	/*if (i && ret)
		printk("bpmod: Used %d tries to %s client at "
			"0x%02x: %s\n", i + 1,
			addr & 1 ? "read from" : "write to", addr >> 1,
			ret == 1 ? "success" : "failed, timeout?");*/
	return ret;
}

static int sendbytes(bpctl_dev_t *pslcm_dev, unsigned char *value,
			unsigned int num_byte)
{
	int count =0;
	int retval;
	int wrcount = 0;

	do {
		retval = i2c_outb(pslcm_dev, value[count]);

		/* OK/ACK; or ignored NAK */
		if (retval > 0) {
			count++;
			wrcount++;

		/* A slave NAKing the master means the slave didn't like
		 * something about the data it saw.  For example, maybe
		 * the SMBus PEC was wrong.
		 */
		} else if (retval == 0) {
			printk("bpmod: sendbytes: NAK bailout.\n");
			return -1;

		/* Timeout; or (someday) lost arbitration
		 *
		 * FIXME Lost ARB implies retrying the transaction from
		 * the first message, after the "winning" master issues
		 * its STOP.  As a rule, upper layer code has no reason
		 * to know or care about this ... it is *NOT* an error.
		 */
		} else {
			printk("bpmod: sendbytes: error %d\n",
					retval);
			return retval;
		}
	}while(count < num_byte);
	return wrcount;
}

static int acknak(bpctl_dev_t *pslcm_dev, int is_ack)
{

	/* assert: sda is high */
	if (is_ack)		/* send ack */
		setsda(pslcm_dev, 0);
	udelay((TIME_CLK + 1) / 2);
	if (sclhi(pslcm_dev) < 0) {	/* timeout */
		printk("bpmod: readbytes: ack/nak timeout\n");
		return -1;
	}
	scllo(pslcm_dev);
	return 0;
}

static int readbytes(bpctl_dev_t *pslcm_dev, unsigned char *value,
			unsigned int num_byte)
{
	int inval;
	int rdcount = 0;	/* counts bytes read */
	int count = 0;

	do {
		inval = i2c_inb(pslcm_dev);
		if (inval >= 0) {
			value[count] = inval;
			rdcount++;
		} else {   /* read timed out */
			break;
		}

		count++;


		/*printk("bpmod: readbytes: 0x%02x %s\n",
			inval,
			(count ? "A" : "NA"));*/

		if(count == num_byte)
			inval = acknak(pslcm_dev, 0);
		else 
			inval = acknak(pslcm_dev, count);
		if (inval < 0)
			return inval;
	} while(count < num_byte);
	return rdcount;
}

/* doAddress initiates the transfer by generating the start condition (in
 * try_address) and transmits the address in the necessary format to handle
 * reads, writes as well as 10bit-addresses.
 * returns:
 *  0 everything went okay, the chip ack'ed, or IGNORE_NAK flag was set
 * -x an error occurred (like: -ENXIO if the device did not answer, or
 *	-ETIMEDOUT, for example if the lines are stuck...)
 */
static int bit_doAddress(bpctl_dev_t *pslcm_dev, unsigned char rd, unsigned char addr)
{
	int ret, retries;

	retries = 1;

	 {		/* normal 7bit address	*/
		if (rd)
			addr |= 1;
		ret = try_address(pslcm_dev, addr, retries);
		if (ret != 1)
			return -1;
	}

	return 0;
}

static int bit_xfer(bpctl_dev_t *pslcm_dev,
			unsigned char rd,
			unsigned char addr,
		    unsigned char *value,
			unsigned int num_byte)
{
	int i, ret, num =1;

	/*printk("bpmod: emitting start condition\n");*/
	i2c_start(pslcm_dev);
	for (i = 0; i < num; i++) {
		if (i) {
				printk("bpmod: emitting "
					"repeated start condition\n");
				i2c_repstart(pslcm_dev);
			}
		ret = bit_doAddress(pslcm_dev, rd, addr);
		if (ret != 0) {
			printk("bpmod: NAK from "
				"device addr 0x%02x\n",
				addr);
			goto bailout;
		}
		if (rd) {
			/* read bytes into buffer*/
			ret = readbytes(pslcm_dev, value, num_byte);
			/*if (ret >= 1)
				printk("bpmod:read %d byte%s\n",
					ret, ret == 1 ? "" : "s");*/
			if (ret < num_byte) {
				if (ret >= 0)
					ret = -1;
				goto bailout;
			}
		} else {
			/* write bytes from buffer */
			ret = sendbytes(pslcm_dev, value, num_byte);
			/*if (ret >= 1)
				printk("bpmod:wrote %d byte%s\n",
					ret, ret == 1 ? "" : "s");*/
			if (ret < num_byte) {
				if (ret >= 0)
					ret = -1;
				goto bailout;
			}
		}
	}
	ret = i;

bailout:
	/*printk("bpmod:emitting stop condition\n");*/
	i2c_stop(pslcm_dev);
	return ret;
}




s32 bp_write_i2c_bytes(bpctl_dev_t *pslcm_dev, unsigned int num_byte, unsigned char *value,
                    unsigned char dev_addr)
{
	if (pslcm_dev->bp_rang) {
		int ret;
		unsigned char *tmp_value = value;


		tmp_value++;
		ret = i2c_smbus_write_i2c_block_data(client, *value, (u8) (num_byte - 1), tmp_value);
		return num_byte;

	} else 
		return (bit_xfer(pslcm_dev,
					0,
					dev_addr,
					value,
					num_byte));

}




s32 bp_read_i2c_bytes(bpctl_dev_t *pslcm_dev, unsigned char *value, 
                                        unsigned int num_byte,
                                        unsigned int dev_addr)
{
	if (pslcm_dev->bp_rang) {
		int ret;
		char tmp = *value;

		value[0] = 0;

		ret  = i2c_smbus_read_i2c_block_data(client,
			      tmp, (u8) (num_byte), value);

#if 0
		printk("bpmod: i2c_smbus_read_block_data ret = %d\n", ret);
		if(ret > 0) { 
			unsigned char *tmp_value;

			tmp_value = value;
			for(i = 0; i < ret; i++){
				printk("bpmod val = %d\n", *tmp_value);
				tmp_value++;
			}
		}
#endif
		return ret;

	} else
		return (bit_xfer(pslcm_dev,
					1,
					dev_addr,
					value,
					num_byte));

}



#if 1 //Pavelk
int set_bp_bytes_fn(bpctl_dev_t *pbpctl_dev, char *add_param){
    unsigned int ret=0;

	 /*test_bus(pbpctl_dev);*/
    ret=bp_write_i2c_bytes(pbpctl_dev, 7, add_param, 0xae);

    return ret;
}

int get_bp_bytes_fn(bpctl_dev_t *pbpctl_dev, char *add_param){
    char value[6];
    int ret=0;

	memset(value, 0, 6);

	value[0] = add_param[0];

    ret=bp_read_i2c_bytes(pbpctl_dev, value, 6, 0xae);

    if(pbpctl_dev->bp_rang) {
        if ((ret!=0) && (ret == 6)) {
          memcpy(&add_param[0], value, 6);
          return 0;
       }
    } else if (ret!=0) {
        memcpy(&add_param[0], value, 6);

        return 0;
    }

    return -1;
}

#endif  
 

int bp_cmd_request(bpctl_dev_t *pbpctl_dev, bp_cmd_t *bp_cmd_buf, bp_cmd_rsp_t *bp_rsp_buf) {
    int ret_val;
	byte data[120]; 
	int try_num = 10;

	atomic_set(&pbpctl_dev->wdt_busy,1); 
	/*
	 * Send command
	 */
     while (try_num--) {
		memset(bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));

    	memcpy(&data[0], bp_cmd_buf, sizeof(bp_cmd_t));
		usec_delay_bp(CMND_INTERVAL);


		if ((ret_val = set_bp_bytes_fn(pbpctl_dev, (char *)data))<0) {
		    printk("bp_cmd_request(write): Not supported!\n"); 
		    continue;
		   // return 0;
		}
		usec_delay_bp(CMND_INTERVAL);
#if 1
  	/*
	 * Receive replay
	 */
		memcpy(&data[0], bp_cmd_buf, sizeof(bp_cmd_t));
		if ((ret_val = get_bp_bytes_fn(pbpctl_dev, (char *)data))<0) {
			printk("bp_cmd_request(read): Not supported!\n");
			continue;
			//return 0;	
		}
#endif

	   // if (bp_rsp_buf->rsp.rsp_id != BP_ERR_OK)
		//	continue;


		memcpy(bp_rsp_buf, &data[0], sizeof(bp_cmd_rsp_t));

#if 0 
	if (bp_rsp_buf->rsp.rsp_id != BP_ERR_OK) 
		printk("bp_cmd_request(got err code or corrupted data!) (%x %x %x %x %x %x)\n",
			               bp_rsp_buf->cmd_bytes[0],bp_rsp_buf->cmd_bytes[1],bp_rsp_buf->cmd_bytes[2],
			               bp_rsp_buf->cmd_bytes[3],bp_rsp_buf->cmd_bytes[4],bp_rsp_buf->cmd_bytes[5]);
		
#endif
		if(pbpctl_dev->bp_rang) {
			if((bp_rsp_buf->rsp.rsp_id == 8) || 			
				(bp_rsp_buf->rsp.rsp_id == 2))
				bp_rsp_buf->rsp.rsp_id = BP_ERR_OK;
		}

		if (bp_rsp_buf->rsp.rsp_id != BP_ERR_OK)
			continue;

		
		break;
	}
	atomic_set(&pbpctl_dev->wdt_busy,0);

    if(!try_num)
  		return 0;
	return 1;
}

/*******************I2C INTERFACE END*****************************************************/

int wdt_time_left (bpctl_dev_t *pbpctl_dev);





static void write_pulse(bpctl_dev_t *pbpctl_dev, 
                        unsigned int ctrl_ext ,
                        unsigned char value, 
                        unsigned char len){
    unsigned char ctrl_val=0;
    unsigned int i=len;
    unsigned int ctrl= 0;
    bpctl_dev_t *pbpctl_dev_c=NULL;

    if (pbpctl_dev->bp_i80)
        ctrl= BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    if (pbpctl_dev->bp_540)
        ctrl= BP10G_READ_REG(pbpctl_dev, ESDP);

    if (pbpctl_dev->bp_10g9) {
        if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
            return;
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);
    }




    while (i--) {
        ctrl_val=(value>>i) & 0x1;
        if (ctrl_val) {
            if (pbpctl_dev->bp_10g9) {

                /* To start management : MCLK 1, MDIO 1, output*/
                /* DATA 1 CLK 1*/
                /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext|BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9));*/
                BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ctrl_ext|BP10G_MDIO_DATA_OUT9);
                BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DATA_OUT9|BP10G_MCLK_DIR_OUT9));

            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext | 
                                                      BPCTLI_CTRL_EXT_MCLK_DIR5|
                                                      BPCTLI_CTRL_EXT_MDIO_DIR5|
                                                      BPCTLI_CTRL_EXT_MDIO_DATA5 | BPCTLI_CTRL_EXT_MCLK_DATA5));


            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext | 
                                                      BPCTLI_CTRL_EXT_MDIO_DIR80|
                                                      BPCTLI_CTRL_EXT_MDIO_DATA80 ));


                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl | 
                                                          BPCTLI_CTRL_EXT_MCLK_DIR80|
                                                          BPCTLI_CTRL_EXT_MCLK_DATA80));


            } else if (pbpctl_dev->bp_540) {
                BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl | 
                                                   BP540_MDIO_DIR|
                                                   BP540_MDIO_DATA| BP540_MCLK_DIR|BP540_MCLK_DATA));




            } else if (pbpctl_dev->bp_10gb) {
                BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_SET|
                                                             BP10GB_MCLK_SET)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_DIR|BP10GB_MDIO_CLR|BP10GB_MCLK_CLR)); 

            } else if (!pbpctl_dev->bp_10g)
                /* To start management : MCLK 1, MDIO 1, output*/
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl_ext | 
                                                          BPCTLI_CTRL_EXT_MCLK_DIR|
                                                          BPCTLI_CTRL_EXT_MDIO_DIR|
                                                          BPCTLI_CTRL_EXT_MDIO_DATA | BPCTLI_CTRL_EXT_MCLK_DATA));
            else {

/* To start management : MCLK 1, MDIO 1, output*/
                //writel((0x2|0x8), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
                BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext|BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT));
                //BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl | BP10G_MDIO_DATA | BP10G_MDIO_DIR));  

            }


            usec_delay_bp(PULSE_TIME);
            if (pbpctl_dev->bp_10g9) {

                /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ((ctrl_ext|BP10G_MDIO_DATA_OUT9)&~(BP10G_MCLK_DATA_OUT9)));*/
                /* DATA 1 CLK 0*/
                BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ctrl_ext|BP10G_MDIO_DATA_OUT9);
                BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DIR_OUT9)&~BP10G_MCLK_DATA_OUT9);


            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR5|
                                                       BPCTLI_CTRL_EXT_MDIO_DIR5|BPCTLI_CTRL_EXT_MDIO_DATA5)&~(BPCTLI_CTRL_EXT_MCLK_DATA5)));

            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext | 
                                                      BPCTLI_CTRL_EXT_MDIO_DIR80|BPCTLI_CTRL_EXT_MDIO_DATA80));
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                           BPCTLI_CTRL_EXT_MCLK_DIR80)&~(BPCTLI_CTRL_EXT_MCLK_DATA80)));

            } else if (pbpctl_dev->bp_540) {
                BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl | BP540_MDIO_DIR|BP540_MDIO_DATA|BP540_MCLK_DIR)&~(BP540_MCLK_DATA));

            } else if (pbpctl_dev->bp_10gb) {

                BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_SET|
                                                             BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_DIR|BP10GB_MDIO_CLR|BP10GB_MCLK_SET));

            } else if (!pbpctl_dev->bp_10g)

                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                           BPCTLI_CTRL_EXT_MCLK_DIR|
                                                           BPCTLI_CTRL_EXT_MDIO_DIR|BPCTLI_CTRL_EXT_MDIO_DATA)&~(BPCTLI_CTRL_EXT_MCLK_DATA)));
            else {

                //writel((0x2), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
                BP10G_WRITE_REG(pbpctl_dev, EODSDP, ((ctrl_ext|BP10G_MDIO_DATA_OUT)&~(BP10G_MCLK_DATA_OUT)));
                //  BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl |BP10G_MDIO_DIR|BP10G_MDIO_DATA));
            }

            usec_delay_bp(PULSE_TIME);

        } else {
            if (pbpctl_dev->bp_10g9) {
                /* DATA 0 CLK 1*/
                /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ((ctrl_ext|BP10G_MCLK_DATA_OUT9)&~BP10G_MDIO_DATA_OUT9));*/
                BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
                BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DATA_OUT9|BP10G_MCLK_DIR_OUT9));

            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR5|
                                                       BPCTLI_CTRL_EXT_MDIO_DIR5|BPCTLI_CTRL_EXT_MCLK_DATA5)&~(BPCTLI_CTRL_EXT_MDIO_DATA5)));

            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MDIO_DIR80)&~(BPCTLI_CTRL_EXT_MDIO_DATA80)));
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl | 
                                                          BPCTLI_CTRL_EXT_MCLK_DIR80|
                                                          BPCTLI_CTRL_EXT_MCLK_DATA80));

            } else if (pbpctl_dev->bp_540) {
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP540_MCLK_DIR| BP540_MCLK_DATA|BP540_MDIO_DIR)&~(BP540_MDIO_DATA)));


            } else if (pbpctl_dev->bp_10gb) {
                BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_CLR|
                                                             BP10GB_MCLK_SET)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_DIR|BP10GB_MDIO_SET|BP10GB_MCLK_CLR));

            } else if (!pbpctl_dev->bp_10g)

                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                           BPCTLI_CTRL_EXT_MCLK_DIR|
                                                           BPCTLI_CTRL_EXT_MDIO_DIR|BPCTLI_CTRL_EXT_MCLK_DATA)&~(BPCTLI_CTRL_EXT_MDIO_DATA)));
            else {

                //    writel((0x8), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
                BP10G_WRITE_REG(pbpctl_dev, EODSDP, ((ctrl_ext|BP10G_MCLK_DATA_OUT)&~BP10G_MDIO_DATA_OUT));
                //  BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));

            }
            usec_delay_bp(PULSE_TIME);
            if (pbpctl_dev->bp_10g9) {
                /* DATA 0 CLK 0 */
                /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
                BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
                BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9)));


            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR5|
                                                       BPCTLI_CTRL_EXT_MDIO_DIR5)&~(BPCTLI_CTRL_EXT_MCLK_DATA5|BPCTLI_CTRL_EXT_MDIO_DATA5)));

            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MDIO_DIR80)&~BPCTLI_CTRL_EXT_MDIO_DATA80));
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                           BPCTLI_CTRL_EXT_MCLK_DIR80)&~(BPCTLI_CTRL_EXT_MCLK_DATA80)));

            } else if (pbpctl_dev->bp_540) {
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | BP540_MCLK_DIR|
                                                    BP540_MDIO_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));
            } else if (pbpctl_dev->bp_10gb) {

                BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_CLR|
                                                             BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_DIR|BP10GB_MDIO_SET|BP10GB_MCLK_SET));

            } else if (!pbpctl_dev->bp_10g)
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                           BPCTLI_CTRL_EXT_MCLK_DIR|
                                                           BPCTLI_CTRL_EXT_MDIO_DIR)&~(BPCTLI_CTRL_EXT_MCLK_DATA|BPCTLI_CTRL_EXT_MDIO_DATA)));
            else {

                //writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
                BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
                //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));
            }

            usec_delay_bp(PULSE_TIME);
        } 

    }
}

static int read_pulse(bpctl_dev_t *pbpctl_dev, unsigned int ctrl_ext ,unsigned char len){
    unsigned char ctrl_val=0;
    unsigned int i=len;
    unsigned int ctrl= 0;
    bpctl_dev_t *pbpctl_dev_c=NULL;

    if (pbpctl_dev->bp_i80)
        ctrl= BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    if (pbpctl_dev->bp_540)
        ctrl= BP10G_READ_REG(pbpctl_dev, ESDP);
    if (pbpctl_dev->bp_10g9) {
        if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
            return -1;
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);
    }


    //ctrl_ext=BP10G_READ_REG(pbpctl_dev,EODSDP);    

    while (i--) {
        if (pbpctl_dev->bp_10g9) {
            /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ((ctrl_ext|BP10G_MDIO_DATA_OUT9)&~BP10G_MCLK_DATA_OUT9));*/
            /* DATA ? CLK 0*/
            BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9)));

        } else if (pbpctl_dev->bp_fiber5) {
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR5)&~(BPCTLI_CTRL_EXT_MDIO_DIR5 | BPCTLI_CTRL_EXT_MCLK_DATA5)));


        } else if (pbpctl_dev->bp_i80) {
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext &~BPCTLI_CTRL_EXT_MDIO_DIR80 ));
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR80)&~( BPCTLI_CTRL_EXT_MCLK_DATA80)));



        } else if (pbpctl_dev->bp_540) {
            BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP540_MCLK_DIR) &~(BP540_MDIO_DIR|BP540_MCLK_DATA) ));



        } else if (pbpctl_dev->bp_10gb) {


            BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_DIR|
                                                         BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_CLR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));

        } else if (!pbpctl_dev->bp_10g)
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR)&~(BPCTLI_CTRL_EXT_MDIO_DIR | BPCTLI_CTRL_EXT_MCLK_DATA)));
        else {

            // writel(( 0/*0x1*/), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
            BP10G_WRITE_REG(pbpctl_dev, EODSDP, ((ctrl_ext|BP10G_MDIO_DATA_OUT)&~BP10G_MCLK_DATA_OUT));  /* ? */
            //    printk("0x28=0x%x\n",BP10G_READ_REG(pbpctl_dev,EODSDP););
            //BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl &~BP10G_MDIO_DIR));

        }

        usec_delay_bp(PULSE_TIME);
        if (pbpctl_dev->bp_10g9) {
            /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext|BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9));*/
            /* DATA ? CLK 1*/
            BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DATA_OUT9|BP10G_MCLK_DIR_OUT9));



        } else if (pbpctl_dev->bp_fiber5) {
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR5|
                                                   BPCTLI_CTRL_EXT_MCLK_DATA5)&~(BPCTLI_CTRL_EXT_MDIO_DIR5)));



        } else if (pbpctl_dev->bp_i80) {
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext&~(BPCTLI_CTRL_EXT_MDIO_DIR80)));
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl | 
                                                      BPCTLI_CTRL_EXT_MCLK_DIR80|
                                                      BPCTLI_CTRL_EXT_MCLK_DATA80));




        } else if (pbpctl_dev->bp_540) {
            BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP540_MCLK_DIR|BP540_MCLK_DATA)&~(BP540_MDIO_DIR)));




        } else if (pbpctl_dev->bp_10gb) {
            BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MDIO_DIR|
                                                         BP10GB_MCLK_SET)&~(BP10GB_MCLK_DIR|BP10GB_MDIO_CLR| BP10GB_MDIO_SET|BP10GB_MCLK_CLR));


        } else if (!pbpctl_dev->bp_10g)
            BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                       BPCTLI_CTRL_EXT_MCLK_DIR|
                                                       BPCTLI_CTRL_EXT_MCLK_DATA)&~(BPCTLI_CTRL_EXT_MDIO_DIR)));
        else {

            // writel((0x8 /*|0x1*/ ), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
            BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext|BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT));
            //BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl &~BP10G_MDIO_DIR));

        }
        if (pbpctl_dev->bp_10g9) {
            ctrl_ext= BP10G_READ_REG(pbpctl_dev,I2CCTL);

        } else if ((pbpctl_dev->bp_fiber5)||(pbpctl_dev->bp_i80)) {
            ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        } else if (pbpctl_dev->bp_540) {
            ctrl_ext = BP10G_READ_REG(pbpctl_dev, ESDP);
        } else if (pbpctl_dev->bp_10gb)
            ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);

        else if (!pbpctl_dev->bp_10g)
            ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        else
            ctrl_ext= BP10G_READ_REG(pbpctl_dev,EODSDP);
        //ctrl_ext =readl((void *)((pbpctl_dev)->mem_map) + 0x28);


        usec_delay_bp(PULSE_TIME);
        if (pbpctl_dev->bp_10g9) {
            if (ctrl_ext & BP10G_MDIO_DATA_IN9)
                ctrl_val |= 1<<i;

        } else if (pbpctl_dev->bp_fiber5) {
            if (ctrl_ext & BPCTLI_CTRL_EXT_MDIO_DATA5)
                ctrl_val |= 1<<i;
        } else if (pbpctl_dev->bp_i80) {
            if (ctrl_ext & BPCTLI_CTRL_EXT_MDIO_DATA80)
                ctrl_val |= 1<<i;
        } else if (pbpctl_dev->bp_540) {
            if (ctrl_ext & BP540_MDIO_DATA)
                ctrl_val |= 1<<i;
        } else if (pbpctl_dev->bp_10gb) {
            if (ctrl_ext & BP10GB_MDIO_DATA)
                ctrl_val |= 1<<i;

        } else if (!pbpctl_dev->bp_10g) {

            if (ctrl_ext & BPCTLI_CTRL_EXT_MDIO_DATA)
                ctrl_val |= 1<<i;
        } else {

            if (ctrl_ext & BP10G_MDIO_DATA_IN)
                ctrl_val |= 1<<i;
        }

    }

    return ctrl_val;
}

static void write_reg(bpctl_dev_t *pbpctl_dev, unsigned char value, unsigned char addr){
    uint32_t ctrl_ext=0, ctrl=0;
    bpctl_dev_t *pbpctl_dev_c=NULL;
#ifdef BP_SYNC_FLAG
    unsigned long flags;
#endif  
    if (pbpctl_dev->bp_10g9) {
        if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
            return;
    }
    if ((pbpctl_dev->wdt_status==WDT_STATUS_EN)&&
        (pbpctl_dev->bp_ext_ver<PXG4BPFI_VER))
        wdt_time_left(pbpctl_dev);

#ifdef BP_SYNC_FLAG
    spin_lock_irqsave(&pbpctl_dev->bypass_wr_lock, flags);
#else
    atomic_set(&pbpctl_dev->wdt_busy,1);
#endif
    if (pbpctl_dev->bp_10g9) {

        ctrl_ext=BP10G_READ_REG(pbpctl_dev,I2CCTL);
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);
        /* DATA 0 CLK 0*/
        /* BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9))); */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9)));

    } else if (pbpctl_dev->bp_fiber5) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5 |
                                               BPCTLI_CTRL_EXT_MDIO_DIR5 )&~(BPCTLI_CTRL_EXT_MDIO_DATA5|BPCTLI_CTRL_EXT_MCLK_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80 )&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80 )&~BPCTLI_CTRL_EXT_MCLK_DATA80));

    } else if (pbpctl_dev->bp_540) {
        ctrl=ctrl_ext = BP10G_READ_REG(pbpctl_dev, ESDP);
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | 
                                            BP540_MDIO_DIR| BP540_MCLK_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));

    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);

        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));


    } else if (!pbpctl_dev->bp_10g) {


        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR |
                                                   BPCTLI_CTRL_EXT_MDIO_DIR )&~(BPCTLI_CTRL_EXT_MDIO_DATA|BPCTLI_CTRL_EXT_MCLK_DATA)));
    } else {
        ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,EODSDP);
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));
        //writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
    }
    usec_delay_bp(CMND_INTERVAL);

    /*send sync cmd*/
    write_pulse(pbpctl_dev,ctrl_ext,SYNC_CMD_VAL,SYNC_CMD_LEN);
    /*send wr cmd*/
    write_pulse(pbpctl_dev,ctrl_ext,WR_CMD_VAL,WR_CMD_LEN);
    write_pulse(pbpctl_dev,ctrl_ext,addr,ADDR_CMD_LEN);

    /*write data*/
    write_pulse(pbpctl_dev,ctrl_ext,value,WR_DATA_LEN);
    if (pbpctl_dev->bp_10g9) {
        /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
        /* DATA 0 CLK 0 */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9)));


    } else if (pbpctl_dev->bp_fiber5) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5 |
                                               BPCTLI_CTRL_EXT_MDIO_DIR5 )&~(BPCTLI_CTRL_EXT_MDIO_DATA5|BPCTLI_CTRL_EXT_MCLK_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80 )&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80 )&~BPCTLI_CTRL_EXT_MCLK_DATA80));
    } else if (pbpctl_dev->bp_540) {
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | 
                                            BP540_MDIO_DIR| BP540_MCLK_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));
    } else if (pbpctl_dev->bp_10gb) {
        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));



    } else if (!pbpctl_dev->bp_10g)

        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR |
                                                   BPCTLI_CTRL_EXT_MDIO_DIR )&~(BPCTLI_CTRL_EXT_MDIO_DATA|BPCTLI_CTRL_EXT_MCLK_DATA)));
    else {
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        // BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));


        //   writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
    }

    usec_delay_bp(CMND_INTERVAL);


    if ((pbpctl_dev->wdt_status==WDT_STATUS_EN)&&
        (pbpctl_dev->bp_ext_ver<PXG4BPFI_VER)&&
        (addr==CMND_REG_ADDR))
        pbpctl_dev->bypass_wdt_on_time=jiffies;
#ifdef BP_SYNC_FLAG
    spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#else
    atomic_set(&pbpctl_dev->wdt_busy,0);
#endif

}


static void write_data(bpctl_dev_t *pbpctl_dev, unsigned char value){
    write_reg(pbpctl_dev, value, CMND_REG_ADDR);
}

static int read_reg(bpctl_dev_t *pbpctl_dev, unsigned char addr){
    uint32_t ctrl_ext=0, ctrl=0 , ctrl_value=0;
    bpctl_dev_t *pbpctl_dev_c=NULL;
#ifdef BP_SYNC_FLAG
    unsigned long flags;
#endif

    if (pbpctl_dev->bp_10g9) {
        if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
            return -1;
    }
	
	
#ifdef BP_SYNC_FLAG
    spin_lock_irqsave(&pbpctl_dev->bypass_wr_lock, flags);
#else
    atomic_set(&pbpctl_dev->wdt_busy,1);
#endif



    if (pbpctl_dev->bp_10g9) {
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,I2CCTL);
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);

        /* BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
        /* DATA 0 CLK 0 */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9)));


    } else if (pbpctl_dev->bp_fiber5) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);

        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5 |
                                               BPCTLI_CTRL_EXT_MDIO_DIR5)&~(BPCTLI_CTRL_EXT_MDIO_DATA5|BPCTLI_CTRL_EXT_MCLK_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT); 

        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80)&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80 )&~BPCTLI_CTRL_EXT_MCLK_DATA80));
    } else if (pbpctl_dev->bp_540) {
        ctrl_ext = BP10G_READ_REG(pbpctl_dev, ESDP);
        ctrl = BP10G_READ_REG(pbpctl_dev, ESDP); 

        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | BP540_MCLK_DIR|
                                            BP540_MDIO_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));
    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);


        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));
#if 0

        /*BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext | BP10GB_MCLK_DIR | BP10GB_MDIO_DIR|
                                                     BP10GB_MCLK_CLR|BP10GB_MDIO_CLR));
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);
        printk("1reg=%x\n", ctrl_ext);*/

        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, ((ctrl_ext |
                                                      BP10GB_MCLK_SET|BP10GB_MDIO_CLR))&~(BP10GB_MCLK_CLR|BP10GB_MDIO_SET| BP10GB_MCLK_DIR | BP10GB_MDIO_DIR));


        /*   bnx2x_set_spio(pbpctl_dev, 5, MISC_REGISTERS_SPIO_OUTPUT_LOW);
           bnx2x_set_spio(pbpctl_dev, 4, MISC_REGISTERS_SPIO_OUTPUT_LOW);
           bnx2x_set_spio(pbpctl_dev, 4, MISC_REGISTERS_SPIO_INPUT_HI_Z);*/


        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);

        //printk("2reg=%x\n", ctrl_ext);


#ifdef BP_SYNC_FLAG
        spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#else
        atomic_set(&pbpctl_dev->wdt_busy,0);
#endif



        return 0;

#endif

    } else if (!pbpctl_dev->bp_10g) {

        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);

        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR |
                                                   BPCTLI_CTRL_EXT_MDIO_DIR)&~(BPCTLI_CTRL_EXT_MDIO_DATA|BPCTLI_CTRL_EXT_MCLK_DATA)));
    } else {

        //   writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
        ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,EODSDP);
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));

    }

    usec_delay_bp(CMND_INTERVAL);

    /*send sync cmd*/
    write_pulse(pbpctl_dev,ctrl_ext,SYNC_CMD_VAL,SYNC_CMD_LEN);
    /*send rd cmd*/
    write_pulse(pbpctl_dev,ctrl_ext,RD_CMD_VAL,RD_CMD_LEN);
    /*send addr*/
    write_pulse(pbpctl_dev,ctrl_ext,addr, ADDR_CMD_LEN);
    /*read data*/
    /* zero */
    if (pbpctl_dev->bp_10g9) {
        /* DATA 0 CLK 1*/
        /*BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext|BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9));*/
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext|BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DATA_OUT9|BP10G_MCLK_DIR_OUT9));

    } else if (pbpctl_dev->bp_fiber5) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5|BPCTLI_CTRL_EXT_MCLK_DATA5)&~(BPCTLI_CTRL_EXT_MDIO_DIR5|BPCTLI_CTRL_EXT_MDIO_DATA5)));

    } else if (pbpctl_dev->bp_i80) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, (ctrl_ext  &~(BPCTLI_CTRL_EXT_MDIO_DATA80| BPCTLI_CTRL_EXT_MDIO_DIR80)));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl | 
                                                  BPCTLI_CTRL_EXT_MCLK_DIR80|BPCTLI_CTRL_EXT_MCLK_DATA80));

    } else if (pbpctl_dev->bp_540) {
        BP10G_WRITE_REG(pbpctl_dev, ESDP, (((ctrl|BP540_MDIO_DIR|BP540_MCLK_DIR|BP540_MCLK_DATA)&~BP540_MDIO_DATA )));

    } else if (pbpctl_dev->bp_10gb) {

        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_DIR|
                                                     BP10GB_MCLK_SET)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_SET|BP10GB_MDIO_CLR| BP10GB_MCLK_CLR));

    } else if (!pbpctl_dev->bp_10g)
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR|BPCTLI_CTRL_EXT_MCLK_DATA)&~(BPCTLI_CTRL_EXT_MDIO_DIR|BPCTLI_CTRL_EXT_MDIO_DATA)));
    else {

        // writel((0x8), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ; 
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext|BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT));

        // BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl &~(BP10G_MDIO_DATA|BP10G_MDIO_DIR)));

    }
    usec_delay_bp(PULSE_TIME);

    ctrl_value= read_pulse(pbpctl_dev,ctrl_ext,RD_DATA_LEN);

    if (pbpctl_dev->bp_10g9) {
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,I2CCTL);
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);

        /* BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
        /* DATA 0 CLK 0 */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9))); 

    } else if (pbpctl_dev->bp_fiber5) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5 |
                                               BPCTLI_CTRL_EXT_MDIO_DIR5)&~(BPCTLI_CTRL_EXT_MDIO_DATA5|BPCTLI_CTRL_EXT_MCLK_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80)&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80 )&~BPCTLI_CTRL_EXT_MCLK_DATA80));

    } else if (pbpctl_dev->bp_540) {
        ctrl= BP10G_READ_REG(pbpctl_dev, ESDP);
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | BP540_MCLK_DIR|
                                            BP540_MDIO_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));

    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);
        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));



    } else if (!pbpctl_dev->bp_10g) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR |
                                                   BPCTLI_CTRL_EXT_MDIO_DIR)&~(BPCTLI_CTRL_EXT_MDIO_DATA|BPCTLI_CTRL_EXT_MCLK_DATA)));
    } else {

        //writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
        ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,EODSDP);
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));

    }

    usec_delay_bp(CMND_INTERVAL);
#ifdef BP_SYNC_FLAG
    spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#else
    atomic_set(&pbpctl_dev->wdt_busy,0);
#endif

    return ctrl_value;
}

static int wdt_pulse(bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0, ctrl=0;
    bpctl_dev_t *pbpctl_dev_c=NULL;
#ifdef BP_SYNC_FLAG
    unsigned long flags;
#endif
	if (!(pbpctl_dev->bp_caps&BP_CAP))
		return -1;

    if ((pbpctl_dev->bp_10g9)||
		(pbpctl_dev->bp_40g)||
		(pbpctl_dev->bp_rang)) {
        if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
            return -1;
    }
#ifdef BP_SYNC_FLAG
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang))
		spin_lock_irqsave(&pbpctl_dev_c->bypass_wr_lock, flags);
	else
		spin_lock_irqsave(&pbpctl_dev->bypass_wr_lock, flags);
#else 
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
		if ((atomic_read(&pbpctl_dev_c->wdt_busy))==1)
			return -1;
	} else {
		if ((atomic_read(&pbpctl_dev->wdt_busy))==1)
			return -1;
	}
#endif

	if(pbpctl_dev->bp_rang) {
		unsigned long value;

		if(!na_base)
			return -1;

		value = inl(na_base + 0x80);
		if(!(value & (BIT_17 | BIT_18))) {
			value |= (BIT_17 | BIT_18);
			outl(value, (na_base + 0x80));
		}
	}
	
	if(pbpctl_dev->bp_rang) {
		if(pbpctl_dev->func == 0) {
			unsigned long value;

			value = inl(na_base + 0x84);
			value &= ~BIT_17;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value |= BIT_17;
			outl(value, (na_base + 0x88));

		} else {
			unsigned long value;

			value = inl(na_base + 0x84);
			value &= ~BIT_18;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value |= BIT_18;
			outl(value, (na_base + 0x88));

		}

	} else if (pbpctl_dev->bp_40g) {
		
		
		writel(0x3f80010, (void *)((pbpctl_dev_c->mem_map) + 0x88108));
		BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		writel(0x3f80010, (void *)((pbpctl_dev_c->mem_map) + 0x8810c));
		BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT); 

		/* DATA 0 CLK 1 */
		if(pbpctl_dev->func == 0) {
			unsigned char a;


			writel(0x62, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
			BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		    a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		    /* if(!(a & BIT_2))
		    		printk("!!SDP2 stuck low! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/
		} else {
			unsigned char a;


			writel(0x63, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
			BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);

	    	a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
			/*if(!(a & BIT_3))
		    		printk("SDP3 stuck low! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/

		}

    } else if (pbpctl_dev->bp_10g9) {
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,I2CCTL);
        ctrl= BP10G_READ_REG(pbpctl_dev_c, ESDP);

        /* BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
        /* DATA 0 CLK 0 */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9))); 

    } else if (pbpctl_dev->bp_fiber5) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5 |
                                               BPCTLI_CTRL_EXT_MDIO_DIR5)&~(BPCTLI_CTRL_EXT_MDIO_DATA5|BPCTLI_CTRL_EXT_MCLK_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL);
        ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80)&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80 )&~BPCTLI_CTRL_EXT_MCLK_DATA80));
    } else if (pbpctl_dev->bp_540) {
        ctrl_ext =ctrl = BP10G_READ_REG(pbpctl_dev, ESDP);
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | BP540_MCLK_DIR|
                                            BP540_MDIO_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));
    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);
        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));

    } else if (!pbpctl_dev->bp_10g) {

        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR |
                                                   BPCTLI_CTRL_EXT_MDIO_DIR)&~(BPCTLI_CTRL_EXT_MDIO_DATA|BPCTLI_CTRL_EXT_MCLK_DATA)));
    } else {

        // writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
        ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
        ctrl_ext=BP10G_READ_REG(pbpctl_dev,EODSDP);
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));

    }
	if(pbpctl_dev->bp_rang) {
		if(pbpctl_dev->func == 0) {
			unsigned long value;

			value = inl(na_base + 0x84);
			value &= ~BIT_17;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value &= ~BIT_17;
			outl(value, (na_base + 0x88));

		} else {
			unsigned long value;

			value = inl(na_base + 0x84);
			value &= ~BIT_18;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value &= ~BIT_18;
			outl(value, (na_base + 0x88));

		}

	} else if (pbpctl_dev->bp_40g) {
		 /*struct timeval tv;
	     printk("0x%x \n", BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT)); */

		
		/* DATA 0 CLK 0 */
			if(pbpctl_dev->func_p == 0) {
				unsigned char a;

				writel(0x42, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
				BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
			    a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
			    /*if(a & BIT_2)
			    	printk("SDP2 stuck high! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/

			} else {
				 unsigned char a;

				writel(0x43, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
				BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
			    a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		   	    /*if(a & BIT_3)
			    	printk("SDP3 stuck high! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/

		}
	  /*  do_gettimeofday(&tv);
    	printk("0x%x %d\n", BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT), (long)tv.tv_usec);*/


    } else if (pbpctl_dev->bp_10g9) {
        /*   BP10G_WRITE_REG(pbpctl_dev, I2CCTL, ((ctrl_ext|BP10G_MCLK_DATA_OUT9)&~BP10G_MDIO_DATA_OUT9));*/
        /* DATA 0 CLK 1*/
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, (ctrl|BP10G_MCLK_DATA_OUT9|BP10G_MCLK_DIR_OUT9));

    } else if (pbpctl_dev->bp_fiber5) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5|
                                               BPCTLI_CTRL_EXT_MDIO_DIR5 |
                                               BPCTLI_CTRL_EXT_MCLK_DATA5)&~(BPCTLI_CTRL_EXT_MDIO_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80 )&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (ctrl | 
                                                  BPCTLI_CTRL_EXT_MCLK_DIR80|
                                                  BPCTLI_CTRL_EXT_MCLK_DATA80));

    } else if (pbpctl_dev->bp_540) {
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl | 
                                            BP540_MDIO_DIR |BP540_MCLK_DIR|BP540_MCLK_DATA)&~BP540_MDIO_DATA));

    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);

        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_SET)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_CLR));



    } else if (!pbpctl_dev->bp_10g)
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR|
                                                   BPCTLI_CTRL_EXT_MDIO_DIR |
                                                   BPCTLI_CTRL_EXT_MCLK_DATA)&~(BPCTLI_CTRL_EXT_MDIO_DATA)));
    else {

        //writel((0x8), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, ((ctrl_ext|BP10G_MCLK_DATA_OUT)&~BP10G_MDIO_DATA_OUT));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));

    }

    usec_delay_bp(WDT_INTERVAL);
/*{
	struct timeval tv;
	
		
	do_gettimeofday(&tv);
    printk("0x%x %d\n", BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT), (long)tv.tv_usec);
} */
	if(pbpctl_dev->bp_rang) {
		if(pbpctl_dev->func == 0) {
			unsigned long value;
#if 0
			value = BPCTL_READ_REG(pbpctl_dev, CTRL);
			/* Make SDP0 Pin Directonality to Output */
			value |= BPCTLI_CTRL_SDP0_DIR;
			BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);

			value |= BPCTLI_CTRL_SDP0_DATA;
			BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);
#endif
			value = inl(na_base + 0x84);
			value &= ~BIT_17;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value |= BIT_17;
			outl(value, (na_base + 0x88));
		} else {
			unsigned long value;
#if 0
			value = BPCTL_READ_REG(pbpctl_dev, CTRL);
			/* Make SDP0 Pin Directonality to Output */
			value |= BPCTLI_CTRL_SDP1_DIR;
			BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);

			value |= BPCTLI_CTRL_SDP1_DATA;
			BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);
#endif

			value = inl(na_base + 0x84);
			value &= ~BIT_18;
			outl(value, (na_base + 0x84));
			value = inl(na_base + 0x88);
			value |= BIT_18;
			outl(value, (na_base + 0x88));

		}

	} else if (pbpctl_dev->bp_40g) {
		/* DATA 0 CLK 1 */
		if(pbpctl_dev->func_p == 0) {
			unsigned char a;

			writel(0x62, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
			BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		    a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);

		   	/*if(! ( a & BIT_2))
			     printk("!!!SDP2 stuck low! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/

		} else {
		   unsigned char a;

		   writel(0x63, (void *)((pbpctl_dev_c->mem_map) + 0x88184));
			BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);
		    a = BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT);

  		  /* if(! ( a & BIT_3))
		    	 printk("SDP3 stuck low! 0x%x %x\n", a, BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT));*/

		}
	 /* printk("0x%x \n\n", BP40G_RD_REG(pbpctl_dev_c, GPIO_STAT)); */

    } else if (pbpctl_dev->bp_10g9) {
        /* BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~(BP10G_MCLK_DATA_OUT9|BP10G_MDIO_DATA_OUT9)));*/
        /* DATA 0 CLK 0 */
        BP10G_WRITE_REG(pbpctl_dev, I2CCTL, (ctrl_ext&~BP10G_MDIO_DATA_OUT9));
        BP10G_WRITE_REG(pbpctl_dev_c, ESDP, ((ctrl|BP10G_MCLK_DIR_OUT9)&~(BP10G_MCLK_DATA_OUT9))); 


    } else if (pbpctl_dev->bp_fiber5) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MCLK_DIR5|
                                               BPCTLI_CTRL_EXT_MDIO_DIR5)&~(BPCTLI_CTRL_EXT_MCLK_DATA5|BPCTLI_CTRL_EXT_MDIO_DATA5)));
    } else if (pbpctl_dev->bp_i80) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_MDIO_DIR80)&~BPCTLI_CTRL_EXT_MDIO_DATA80));
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR80)&~BPCTLI_CTRL_EXT_MCLK_DATA80));

    } else if (pbpctl_dev->bp_540) {
        BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP540_MCLK_DIR| 
                                            BP540_MDIO_DIR)&~(BP540_MDIO_DATA|BP540_MCLK_DATA)));

    } else if (pbpctl_dev->bp_10gb) {
        ctrl_ext = BP10GB_READ_REG(pbpctl_dev, MISC_REG_SPIO);
        BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_SPIO, (ctrl_ext |  BP10GB_MDIO_CLR|
                                                     BP10GB_MCLK_CLR)&~(BP10GB_MCLK_DIR| BP10GB_MDIO_DIR| BP10GB_MDIO_SET|BP10GB_MCLK_SET));



    } else if (!pbpctl_dev->bp_10g)
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR|
                                                   BPCTLI_CTRL_EXT_MDIO_DIR)&~(BPCTLI_CTRL_EXT_MCLK_DATA|BPCTLI_CTRL_EXT_MDIO_DATA)));
    else {

        //writel((0x0), (void *)(((pbpctl_dev)->mem_map) + 0x28)) ;
        BP10G_WRITE_REG(pbpctl_dev, EODSDP, (ctrl_ext&~(BP10G_MCLK_DATA_OUT|BP10G_MDIO_DATA_OUT)));
        //BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl |BP10G_MDIO_DIR)&~BP10G_MDIO_DATA));
    }
    if ((pbpctl_dev->wdt_status==WDT_STATUS_EN)/*&&
        (pbpctl_dev->bp_ext_ver<PXG4BPFI_VER)*/)
        pbpctl_dev->bypass_wdt_on_time=jiffies;
#ifdef BP_SYNC_FLAG
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang))
		spin_unlock_irqrestore(&pbpctl_dev_c->bypass_wr_lock, flags);
	else
		spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#endif
   /* usec_delay_bp(CMND_INTERVAL);*/
    return 0;
}  

static void data_pulse(bpctl_dev_t *pbpctl_dev, unsigned char value){

    uint32_t ctrl_ext=0;
#ifdef BP_SYNC_FLAG
    unsigned long flags;
#endif  
    wdt_time_left(pbpctl_dev);
#ifdef BP_SYNC_FLAG
    spin_lock_irqsave(&pbpctl_dev->bypass_wr_lock, flags);
#else 
    atomic_set(&pbpctl_dev->wdt_busy,1);
#endif

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_SDP6_DIR|
                                               BPCTLI_CTRL_EXT_SDP7_DIR)&~(BPCTLI_CTRL_EXT_SDP6_DATA|BPCTLI_CTRL_EXT_SDP7_DATA)));

    usec_delay_bp(INIT_CMND_INTERVAL);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                               BPCTLI_CTRL_EXT_SDP6_DIR|
                                               BPCTLI_CTRL_EXT_SDP7_DIR | BPCTLI_CTRL_EXT_SDP6_DATA)&~(BPCTLI_CTRL_EXT_SDP7_DATA)));
    usec_delay_bp(INIT_CMND_INTERVAL);


    while (value) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ctrl_ext |
                           BPCTLI_CTRL_EXT_SDP6_DIR|
                           BPCTLI_CTRL_EXT_SDP7_DIR|
                           BPCTLI_CTRL_EXT_SDP6_DATA|
                           BPCTLI_CTRL_EXT_SDP7_DATA);
        usec_delay_bp(PULSE_INTERVAL);
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (( ctrl_ext| 
                                                    BPCTLI_CTRL_EXT_SDP6_DIR|
                                                    BPCTLI_CTRL_EXT_SDP7_DIR| 
                                                    BPCTLI_CTRL_EXT_SDP6_DATA)&~BPCTLI_CTRL_EXT_SDP7_DATA));
        usec_delay_bp(PULSE_INTERVAL);
        value--;



    }
    usec_delay_bp(INIT_CMND_INTERVAL-PULSE_INTERVAL);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (( ctrl_ext | 
                                                BPCTLI_CTRL_EXT_SDP6_DIR|
                                                BPCTLI_CTRL_EXT_SDP7_DIR)&~(BPCTLI_CTRL_EXT_SDP6_DATA|BPCTLI_CTRL_EXT_SDP7_DATA)));
    usec_delay_bp(WDT_TIME_CNT);
    if (pbpctl_dev->wdt_status==WDT_STATUS_EN)
        pbpctl_dev->bypass_wdt_on_time=jiffies;
#ifdef BP_SYNC_FLAG
    spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#else
    atomic_set(&pbpctl_dev->wdt_busy,0);
#endif


}

static int send_wdt_pulse(bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;

#ifdef BP_SYNC_FLAG
    unsigned long flags;

    spin_lock_irqsave(&pbpctl_dev->bypass_wr_lock, flags);
#else

    if ((atomic_read(&pbpctl_dev->wdt_busy))==1)
        return -1;
#endif
    wdt_time_left(pbpctl_dev);
    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT); 

    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ctrl_ext |     /* 1 */
                       BPCTLI_CTRL_EXT_SDP7_DIR | 
                       BPCTLI_CTRL_EXT_SDP7_DATA);
    usec_delay_bp(PULSE_INTERVAL);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext |     /* 0 */
                                               BPCTLI_CTRL_EXT_SDP7_DIR)&~BPCTLI_CTRL_EXT_SDP7_DATA));


    usec_delay_bp(PULSE_INTERVAL);
    if (pbpctl_dev->wdt_status==WDT_STATUS_EN)
        pbpctl_dev->bypass_wdt_on_time=jiffies;
#ifdef BP_SYNC_FLAG
    spin_unlock_irqrestore(&pbpctl_dev->bypass_wr_lock, flags);
#endif

    return 0;
}  

void send_bypass_clear_pulse(bpctl_dev_t *pbpctl_dev, unsigned int value){
    uint32_t ctrl_ext=0;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext |     /* 0 */
                                               BPCTLI_CTRL_EXT_SDP6_DIR)&~BPCTLI_CTRL_EXT_SDP6_DATA));

    usec_delay_bp(PULSE_INTERVAL);
    while (value) {
        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ctrl_ext |     /* 1 */
                           BPCTLI_CTRL_EXT_SDP6_DIR | 
                           BPCTLI_CTRL_EXT_SDP6_DATA);
        usec_delay_bp(PULSE_INTERVAL);
        value--;
    }
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext |     /* 0 */
                                               BPCTLI_CTRL_EXT_SDP6_DIR)&~BPCTLI_CTRL_EXT_SDP6_DATA));
    usec_delay_bp(PULSE_INTERVAL);
}
/*  #endif  OLD_FW */
#ifdef BYPASS_DEBUG

int pulse_set_fn (bpctl_dev_t *pbpctl_dev, unsigned int counter){
    uint32_t ctrl_ext=0;

    if (!pbpctl_dev)
        return -1;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    write_pulse_1(pbpctl_dev,ctrl_ext,counter,counter);

    pbpctl_dev->bypass_wdt_status=0;
    if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
        write_pulse_1(pbpctl_dev,ctrl_ext,counter,counter);
    } else {
        wdt_time_left(pbpctl_dev);
        if (pbpctl_dev->wdt_status==WDT_STATUS_EN) {
            pbpctl_dev->wdt_status=0;
            data_pulse(pbpctl_dev,counter);
            pbpctl_dev->wdt_status= WDT_STATUS_EN;
            pbpctl_dev->bypass_wdt_on_time=jiffies;

        } else
            data_pulse(pbpctl_dev,counter);
    }

    return 0;
}


int zero_set_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0, ctrl_value=0;
    if (!pbpctl_dev)
        return -1;

    if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
        printk("zero_set");

        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);

        BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl_ext | 
                                                   BPCTLI_CTRL_EXT_MCLK_DIR)&~(BPCTLI_CTRL_EXT_MCLK_DATA|BPCTLI_CTRL_EXT_MDIO_DIR|BPCTLI_CTRL_EXT_MDIO_DATA)));

    }
    return ctrl_value;
}


int pulse_get2_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0, ctrl_value=0;
    if (!pbpctl_dev)
        return -1;

    if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
        printk("pulse_get_fn\n");
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        ctrl_value=read_pulse_2(pbpctl_dev,ctrl_ext);
        printk("read:%d\n",ctrl_value);
    }
    return ctrl_value;
}

int pulse_get1_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0, ctrl_value=0;
    if (!pbpctl_dev)
        return -1;


    if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {

        printk("pulse_get_fn\n");

        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        ctrl_value=read_pulse_1(pbpctl_dev,ctrl_ext);
        printk("read:%d\n",ctrl_value);
    }
    return ctrl_value;
}


int gpio6_set_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ctrl_ext |
                       BPCTLI_CTRL_EXT_SDP6_DIR  |
                       BPCTLI_CTRL_EXT_SDP6_DATA);
    return 0;
}



int gpio7_set_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ctrl_ext |
                       BPCTLI_CTRL_EXT_SDP7_DIR |
                       BPCTLI_CTRL_EXT_SDP7_DATA);
    return 0;
}

int gpio7_clear_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (( ctrl_ext |
                                                BPCTLI_CTRL_EXT_SDP7_DIR) & ~BPCTLI_CTRL_EXT_SDP7_DATA));
    return 0;
}

int gpio6_clear_fn (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;

    ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (( ctrl_ext |
                                                BPCTLI_CTRL_EXT_SDP6_DIR) & ~BPCTLI_CTRL_EXT_SDP6_DATA));
    return 0;
}
#endif /*BYPASS_DEBUG*/


static bpctl_dev_t *get_status40_port_fn(bpctl_dev_t *pbpctl_dev) {
    int idx_dev=0;

    if (pbpctl_dev==NULL)
        return NULL;

		if (pbpctl_dev->func_p != 0) {
			for (idx_dev = 0; ((idx_dev < device_num)&&(bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p == pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p == pbpctl_dev->slot_p)&&
					(bpctl_dev_arr[idx_dev].func_p == 0)) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
 			}
		} else 
			return pbpctl_dev;
	return NULL;
}

static bpctl_dev_t *get_status_port_fn(bpctl_dev_t *pbpctl_dev) {
    int idx_dev=0;

    if (pbpctl_dev==NULL) {
		return NULL;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
		if (pbpctl_dev->func_p != 0) {
			for (idx_dev = 0; ((idx_dev < device_num)&&(bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p == pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p == pbpctl_dev->slot_p)&&
					(bpctl_dev_arr[idx_dev].func_p == 0)) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
 			}
		} else 
			return pbpctl_dev;

	}  else if (pbpctl_dev->bp_10g9) {
		if (pbpctl_dev->func_p==0) {
			for (idx_dev = 0; ((idx_dev < device_num)&&(bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p == pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p == pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p == 1)&&(pbpctl_dev->func_p == 0))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
 			}
		}

	} else { 
		if ((pbpctl_dev->func_p==0)||(pbpctl_dev->func_p==2)) {
			for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p==pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p==pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p==1)&&(pbpctl_dev->func_p==0))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
				if ((bpctl_dev_arr[idx_dev].bus_p==pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p==pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p==3)&&(pbpctl_dev->func_p==2))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
			}
		}
	}
    return NULL;
}

static bpctl_dev_t *get_master_port_fn(bpctl_dev_t *pbpctl_dev) {
    int idx_dev=0;

    if (pbpctl_dev==NULL) {
		return NULL;
	}

	if (pbpctl_dev->bp_10g9) {
		if (pbpctl_dev->func_p == 1) {
			for (idx_dev = 0; ((idx_dev < device_num)&&(bpctl_dev_arr[idx_dev].pdev != NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p == pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p == pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p == 0)&&(pbpctl_dev->func_p == 1))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
			}
		}

	} else {
		if ((pbpctl_dev->func_p==1)||(pbpctl_dev->func_p==3)) {
			for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p==pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p==pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p==0)&&(pbpctl_dev->func_p==1))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
				if ((bpctl_dev_arr[idx_dev].bus_p==pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p==pbpctl_dev->slot_p)&&
					((bpctl_dev_arr[idx_dev].func_p==2)&&(pbpctl_dev->func_p==3))) {

					return(&(bpctl_dev_arr[idx_dev]));
				}
			}
		}
	}
    return NULL;
}




/**************************************/
/**************INTEL API***************/
/**************************************/

static void write_data_port_int(bpctl_dev_t *pbpctl_dev, unsigned char ctrl_value){
    uint32_t value;

    value = BPCTL_READ_REG(pbpctl_dev, CTRL);
/* Make SDP0 Pin Directonality to Output */
    value |= BPCTLI_CTRL_SDP0_DIR;
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);

    value &= ~BPCTLI_CTRL_SDP0_DATA;
    value |= ((ctrl_value & 0x1) << BPCTLI_CTRL_SDP0_SHIFT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, value);

    value = (BPCTL_READ_REG(pbpctl_dev, CTRL_EXT));
/* Make SDP2 Pin Directonality to Output */
    value |= BPCTLI_CTRL_EXT_SDP6_DIR;
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, value);

    value &= ~BPCTLI_CTRL_EXT_SDP6_DATA;
    value |= (((ctrl_value & 0x2) >> 1) << BPCTLI_CTRL_EXT_SDP6_SHIFT);
    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, value);

}

static int write_data_int(bpctl_dev_t *pbpctl_dev, unsigned char value){
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
        return -1;
    atomic_set(&pbpctl_dev->wdt_busy,1);
    write_data_port_int(pbpctl_dev, value&0x3);
    write_data_port_int(pbpctl_dev_b,((value & 0xc) >> 2));
    atomic_set(&pbpctl_dev->wdt_busy,0);

    return 0;
}   

static int wdt_pulse_int(bpctl_dev_t *pbpctl_dev){

    if ((atomic_read(&pbpctl_dev->wdt_busy))==1)
        return -1;

    if ((write_data_int(pbpctl_dev, RESET_WDT_INT))<0)
        return -1;
    msec_delay_bp(CMND_INTERVAL_INT);
    if ((write_data_int(pbpctl_dev, CMND_OFF_INT))<0)
        return -1;
    msec_delay_bp(CMND_INTERVAL_INT);

    if (pbpctl_dev->wdt_status==WDT_STATUS_EN)
        pbpctl_dev->bypass_wdt_on_time=jiffies;

    return 0;
}


/*************************************/
/************* COMMANDS **************/
/*************************************/


/* CMND_ON  0x4 (100)*/
int cmnd_on(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
            return 0;
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)
            write_data(pbpctl_dev,CMND_ON);
        else
            data_pulse(pbpctl_dev,CMND_ON);
        ret=0;
    }
    return ret;
}


/* CMND_OFF  0x2 (10)*/
int cmnd_off(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,CMND_OFF_INT);
            msec_delay_bp(CMND_INTERVAL_INT);
        } else if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)
            write_data(pbpctl_dev,CMND_OFF);
        else
            data_pulse(pbpctl_dev,CMND_OFF);
        ret=0;
    };
    return ret;
}

/* BYPASS_ON (0xa)*/
int bypass_on(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&BP_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,BYPASS_ON_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            pbpctl_dev->bp_status_un=0;
        } else if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            write_data(pbpctl_dev,BYPASS_ON);
            if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
                msec_delay_bp(LATCH_DELAY);

        } else
            data_pulse(pbpctl_dev,BYPASS_ON);
        ret=0;
    };
    return ret;
}

/* BYPASS_OFF (0x8 111)*/
int bypass_off(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&BP_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,DIS_BYPASS_CAP_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            write_data_int(pbpctl_dev,PWROFF_BYPASS_ON_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            pbpctl_dev->bp_status_un=0;
        } else if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            write_data(pbpctl_dev,BYPASS_OFF);
            if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
                msec_delay_bp(LATCH_DELAY);
        } else
            data_pulse(pbpctl_dev,BYPASS_OFF);
        ret=0;
    }
    return ret;
}

/* TAP_OFF (0x9)*/
int tap_off(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;
    if ((pbpctl_dev->bp_caps&TAP_CAP)&&(pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)) {
        write_data(pbpctl_dev,TAP_OFF);
        msec_delay_bp(LATCH_DELAY);
        ret=0;
    };
    return ret;
}

/* TAP_ON (0xb)*/
int tap_on(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;
    if ((pbpctl_dev->bp_caps&TAP_CAP)&&(pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)) {
        write_data(pbpctl_dev,TAP_ON);
        msec_delay_bp(LATCH_DELAY);
        ret=0;
    };
    return ret;
}

/* DISC_OFF (0x9)*/
int disc_off(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    if ((pbpctl_dev->bp_caps&DISC_CAP)&&(pbpctl_dev->bp_ext_ver>=0x8)) {
        write_data(pbpctl_dev,DISC_OFF);
        msec_delay_bp(LATCH_DELAY);
    } else  ret=BP_NOT_CAP;
    return ret;
}

/* DISC_ON (0xb)*/
int disc_on(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    if ((pbpctl_dev->bp_caps&DISC_CAP)&&(pbpctl_dev->bp_ext_ver>=0x8)) {
        write_data(pbpctl_dev,/*DISC_ON*/0x85);
        msec_delay_bp(LATCH_DELAY);
    } else  ret=BP_NOT_CAP;
    return ret;
}

/* DISC_PORT_ON */
int disc_port_on(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    bpctl_dev_t *pbpctl_dev_m;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1) {

            write_data(pbpctl_dev_m,TX_DISA);
        } else {

            write_data(pbpctl_dev_m,TX_DISB);
        }

        msec_delay_bp(LATCH_DELAY);

    }
    return ret;
}

/* DISC_PORT_OFF */
int disc_port_off(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    bpctl_dev_t *pbpctl_dev_m;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1)
            write_data(pbpctl_dev_m,TX_ENA);
        else
            write_data(pbpctl_dev_m,TX_ENB);

        msec_delay_bp(LATCH_DELAY);

    }
    return ret;
}



/*TWO_PORT_LINK_HW_EN (0xe)*/
int tpl_hw_on (bpctl_dev_t *pbpctl_dev){
    int ret=0, ctrl=0;
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
        return BP_NOT_CAP;

    if (pbpctl_dev->bp_caps_ex&TPL2_CAP_EX) {
        cmnd_on(pbpctl_dev);
        write_data(pbpctl_dev,TPL2_ON);
        /*msec_delay_bp(LATCH_DELAY);*/
        msec_delay_bp(EEPROM_WR_DELAY);
        cmnd_off(pbpctl_dev);
        return ret;
    }

    if (TPL_IF_SERIES(pbpctl_dev->subdevice)) {
        ctrl = BPCTL_READ_REG(pbpctl_dev_b, CTRL);
        BPCTL_BP_WRITE_REG(pbpctl_dev_b, CTRL, ((ctrl|BPCTLI_CTRL_SWDPIO0)&~BPCTLI_CTRL_SWDPIN0));
    } else ret=BP_NOT_CAP;
    return ret;
}


/*TWO_PORT_LINK_HW_DIS (0xc)*/
int tpl_hw_off (bpctl_dev_t *pbpctl_dev){
    int ret=0, ctrl=0;
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
        return BP_NOT_CAP;
    if (pbpctl_dev->bp_caps_ex&TPL2_CAP_EX) {
        cmnd_on(pbpctl_dev);
        write_data(pbpctl_dev,TPL2_OFF);
        /*msec_delay_bp(LATCH_DELAY);*/
        msec_delay_bp(EEPROM_WR_DELAY);
        cmnd_off(pbpctl_dev);
        return ret;
    }
    if (TPL_IF_SERIES(pbpctl_dev->subdevice)) {
        ctrl = BPCTL_READ_REG(pbpctl_dev_b, CTRL);
        BPCTL_BP_WRITE_REG(pbpctl_dev_b, CTRL, (ctrl|BPCTLI_CTRL_SWDPIO0|BPCTLI_CTRL_SWDPIN0));
    } else ret=BP_NOT_CAP;
    return ret;
}



/* WDT_OFF (0x6 110)*/
int wdt_off(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            bypass_off(pbpctl_dev);
        } else if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)
            write_data(pbpctl_dev,WDT_OFF);
        else
            data_pulse(pbpctl_dev,WDT_OFF);
        pbpctl_dev->wdt_status=WDT_STATUS_DIS;
        ret=0;
    };
    return ret;
}

/* WDT_ON (0x10)*/

/***Global***/
static unsigned int 
wdt_val_array[]={1000, 1500, 2000, 3000, 4000, 8000, 16000, 32000, 0} ;

int wdt_on(bpctl_dev_t *pbpctl_dev, unsigned int timeout){

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        unsigned int pulse=0, temp_value=0, temp_cnt=0;
        pbpctl_dev->wdt_status=0; 

        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            for (;wdt_val_array[temp_cnt];temp_cnt++)
                if (timeout<=wdt_val_array[temp_cnt])
                    break;

            if (!wdt_val_array[temp_cnt])
                temp_cnt--;

            timeout=wdt_val_array[temp_cnt];
            temp_cnt+=0x7;

            write_data_int(pbpctl_dev,DIS_BYPASS_CAP_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            pbpctl_dev->bp_status_un=0;
            write_data_int(pbpctl_dev,temp_cnt);
            pbpctl_dev->bypass_wdt_on_time=jiffies;
            msec_delay_bp(CMND_INTERVAL_INT);
            pbpctl_dev->bypass_timer_interval=timeout;  
        } else {
            timeout=(timeout<TIMEOUT_UNIT?TIMEOUT_UNIT:(timeout>WDT_TIMEOUT_MAX?WDT_TIMEOUT_MAX:timeout));
            temp_value=timeout/100;
            while ((temp_value>>=1))
                temp_cnt++;
            if (timeout > ((1<<temp_cnt)*100))
                temp_cnt++;
            pbpctl_dev->bypass_wdt_on_time=jiffies;
            pulse=(WDT_ON | temp_cnt);
            if (pbpctl_dev->bp_ext_ver==OLD_IF_VER)
                data_pulse(pbpctl_dev,pulse);
            else
                write_data(pbpctl_dev,pulse);
            pbpctl_dev->bypass_timer_interval=(1<<temp_cnt)*100;
        }
        pbpctl_dev->wdt_status=WDT_STATUS_EN;
        return 0;    
    }
    return BP_NOT_CAP;
}

void bp75_put_hw_semaphore_generic(bpctl_dev_t *pbpctl_dev)
{
    u32 swsm;


    swsm = BPCTL_READ_REG(pbpctl_dev, SWSM);

    swsm &= ~(BPCTLI_SWSM_SMBI | BPCTLI_SWSM_SWESMBI);

    BPCTL_WRITE_REG(pbpctl_dev, SWSM, swsm);
}


s32 bp75_get_hw_semaphore_generic(bpctl_dev_t *pbpctl_dev)
{
    u32 swsm;
    s32 ret_val = 0;
    s32 timeout = 8192 + 1;
    s32 i = 0;


    /* Get the SW semaphore */
    while (i < timeout) {
        swsm = BPCTL_READ_REG(pbpctl_dev, SWSM);
        if (!(swsm & BPCTLI_SWSM_SMBI))
            break;

        usec_delay_bp(50);
        i++;
    }

    if (i == timeout) {
        printk("bpctl_mod: Driver can't access device - SMBI bit is set.\n");
        ret_val = -1;
        goto out;
    }

    /* Get the FW semaphore. */
    for (i = 0; i < timeout; i++) {
        swsm = BPCTL_READ_REG(pbpctl_dev, SWSM);
        BPCTL_WRITE_REG(pbpctl_dev, SWSM, swsm | BPCTLI_SWSM_SWESMBI);

        /* Semaphore acquired if bit latched */
        if (BPCTL_READ_REG(pbpctl_dev, SWSM) & BPCTLI_SWSM_SWESMBI)
            break;

        usec_delay_bp(50);
    }

    if (i == timeout) {
        /* Release semaphores */
        bp75_put_hw_semaphore_generic(pbpctl_dev);
        printk("bpctl_mod: Driver can't access the NVM\n");
        ret_val = -1;
        goto out;
    }

    out:
    return ret_val;
}



static void bp75_release_phy(bpctl_dev_t *pbpctl_dev)
{
    u16 mask = BPCTLI_SWFW_PHY0_SM;
    u32 swfw_sync;



    if ((pbpctl_dev->func_p==1)||(pbpctl_dev->func_p==3))
        mask = BPCTLI_SWFW_PHY1_SM;

    while (bp75_get_hw_semaphore_generic(pbpctl_dev) != 0);
    /* Empty */

    swfw_sync = BPCTL_READ_REG(pbpctl_dev, SW_FW_SYNC);
    swfw_sync &= ~mask;
    BPCTL_WRITE_REG(pbpctl_dev, SW_FW_SYNC, swfw_sync);

    bp75_put_hw_semaphore_generic(pbpctl_dev);
} 



static s32 bp75_acquire_phy(bpctl_dev_t *pbpctl_dev)
{
    u16 mask = BPCTLI_SWFW_PHY0_SM;
    u32 swfw_sync;
    u32 swmask ;
    u32 fwmask ;
    s32 ret_val = 0;
    s32 i = 0, timeout = 200; 


    if ((pbpctl_dev->func_p==1)||(pbpctl_dev->func_p==3))
        mask = BPCTLI_SWFW_PHY1_SM;

    swmask = mask;
    fwmask = mask << 16;

    while (i < timeout) {
        if (bp75_get_hw_semaphore_generic(pbpctl_dev)) {
            ret_val = -1;
            goto out;
        }

        swfw_sync = BPCTL_READ_REG(pbpctl_dev, SW_FW_SYNC);
        if (!(swfw_sync & (fwmask | swmask)))
            break;

        bp75_put_hw_semaphore_generic(pbpctl_dev);
        mdelay(5);
        i++;
    }

    if (i == timeout) {
        printk("bpctl_mod: Driver can't access resource, SW_FW_SYNC timeout.\n");
        ret_val = -1;
        goto out;
    }

    swfw_sync |= swmask;
    BPCTL_WRITE_REG(pbpctl_dev, SW_FW_SYNC, swfw_sync);

    bp75_put_hw_semaphore_generic(pbpctl_dev);

    out:
    return ret_val;
}


s32 bp75_read_phy_reg_mdic(bpctl_dev_t *pbpctl_dev, u32 offset, u16 *data)
{
    u32 i, mdic = 0;
    s32 ret_val = 0;
    u32 phy_addr = 1;


    mdic = ((offset << BPCTLI_MDIC_REG_SHIFT) |
            (phy_addr << BPCTLI_MDIC_PHY_SHIFT) |
            (BPCTLI_MDIC_OP_READ));

    BPCTL_WRITE_REG(pbpctl_dev, MDIC, mdic);

    for (i = 0; i < (BPCTLI_GEN_POLL_TIMEOUT * 3); i++) {
        usec_delay_bp(50);
        mdic = BPCTL_READ_REG(pbpctl_dev, MDIC);
        if (mdic & BPCTLI_MDIC_READY)
            break;
    }
    if (!(mdic & BPCTLI_MDIC_READY)) {
        printk("bpctl_mod: MDI Read did not complete\n");
        ret_val = -1;
        goto out;
    }
    if (mdic & BPCTLI_MDIC_ERROR) {
        printk("bpctl_mod: MDI Error\n");
        ret_val = -1;
        goto out;
    }
    *data = (u16) mdic;

    out:
    return ret_val;
}

s32 bp75_write_phy_reg_mdic(bpctl_dev_t *pbpctl_dev, u32 offset, u16 data)
{
    u32 i, mdic = 0;
    s32 ret_val = 0;
    u32 phy_addr = 1;



    mdic = (((u32)data) |
            (offset << BPCTLI_MDIC_REG_SHIFT) |
            (phy_addr << BPCTLI_MDIC_PHY_SHIFT) |
            (BPCTLI_MDIC_OP_WRITE));

    BPCTL_WRITE_REG(pbpctl_dev, MDIC, mdic);

    for (i = 0; i < (BPCTLI_GEN_POLL_TIMEOUT * 3); i++) {
        usec_delay_bp(50);
        mdic = BPCTL_READ_REG(pbpctl_dev, MDIC);
        if (mdic & BPCTLI_MDIC_READY)
            break;
    }
    if (!(mdic & BPCTLI_MDIC_READY)) {
        printk("bpctl_mod: MDI Write did not complete\n");
        ret_val = -1;
        goto out;
    }
    if (mdic & BPCTLI_MDIC_ERROR) {
        printk("bpctl_mod: MDI Error\n");
        ret_val = -1;
        goto out;
    }

    out:
    return ret_val;
} 


static s32 bp75_read_phy_reg( bpctl_dev_t *pbpctl_dev, u32 offset, u16 *data)
{
    s32 ret_val = 0;


    ret_val = bp75_acquire_phy(pbpctl_dev);
    if (ret_val)
        goto out;

    if (offset > BPCTLI_MAX_PHY_MULTI_PAGE_REG) {
        ret_val = bp75_write_phy_reg_mdic(pbpctl_dev,
                                          BPCTLI_IGP01E1000_PHY_PAGE_SELECT,
                                          (u16)offset);
        if (ret_val)
            goto release;
    }

    ret_val = bp75_read_phy_reg_mdic(pbpctl_dev, BPCTLI_MAX_PHY_REG_ADDRESS & offset,
                                     data);

    release:
    bp75_release_phy(pbpctl_dev);
    out:
    return ret_val;
}

static s32 bp75_write_phy_reg(bpctl_dev_t *pbpctl_dev, u32 offset, u16 data)
{
    s32 ret_val = 0;


    ret_val = bp75_acquire_phy(pbpctl_dev);
    if (ret_val)
        goto out;

    if (offset > BPCTLI_MAX_PHY_MULTI_PAGE_REG) {
        ret_val = bp75_write_phy_reg_mdic(pbpctl_dev,
                                          BPCTLI_IGP01E1000_PHY_PAGE_SELECT,
                                          (u16)offset);
        if (ret_val)
            goto release;
    }

    ret_val = bp75_write_phy_reg_mdic(pbpctl_dev, BPCTLI_MAX_PHY_REG_ADDRESS & offset,
                                      data);

    release:
    bp75_release_phy(pbpctl_dev);

    out:
    return ret_val;
}


/* SET_TX  (non-Bypass command :)) */
static int set_tx (bpctl_dev_t *pbpctl_dev, int tx_state){
    int ret=0, ctrl=0;
    bpctl_dev_t *pbpctl_dev_m;
    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if ((pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX)&&(!pbpctl_dev->bp_10g9)) {
        ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL);
        if (!tx_state) {
            if (pbpctl_dev->bp_540) {
                ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
                BP10G_WRITE_REG(pbpctl_dev, ESDP,(ctrl|BP10G_SDP1_DIR|BP10G_SDP1_DATA));

            } else if (pbpctl_dev->bp_fiber5) {
                ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);

                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT,(ctrl|BPCTLI_CTRL_EXT_SDP6_DIR|BPCTLI_CTRL_EXT_SDP6_DATA));


            } else {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL,(ctrl|BPCTLI_CTRL_SDP1_DIR|BPCTLI_CTRL_SWDPIN1));
            }
        } else {
            if (pbpctl_dev->bp_540) {
                ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP10G_SDP1_DIR)&~BP10G_SDP1_DATA));
            } else if (pbpctl_dev->bp_fiber5) {
                ctrl= BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl|BPCTLI_CTRL_EXT_SDP6_DIR)&~BPCTLI_CTRL_EXT_SDP6_DATA));


            } else {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl|BPCTLI_CTRL_SDP1_DIR)&~BPCTLI_CTRL_SWDPIN1));
            }
            return ret;

        }
    } else if (pbpctl_dev->bp_caps&TX_CTL_CAP) {
        if (PEG5_IF_SERIES(pbpctl_dev->subdevice)) {
            if (tx_state) {
                uint16_t mii_reg;
                if (!(ret=bp75_read_phy_reg(pbpctl_dev, BPCTLI_PHY_CONTROL, &mii_reg))) {
                    if (mii_reg & BPCTLI_MII_CR_POWER_DOWN) {
                        ret=bp75_write_phy_reg(pbpctl_dev, BPCTLI_PHY_CONTROL, mii_reg&~BPCTLI_MII_CR_POWER_DOWN);
                    }
                }
            }

            else {
                uint16_t mii_reg;
                if (!(ret=bp75_read_phy_reg(pbpctl_dev, BPCTLI_PHY_CONTROL, &mii_reg))) {

                    mii_reg |= BPCTLI_MII_CR_POWER_DOWN;
                    ret=bp75_write_phy_reg(pbpctl_dev, BPCTLI_PHY_CONTROL, mii_reg);
                }
            }

        }
        if (pbpctl_dev->bp_fiber5) {
            ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);

        } else if (pbpctl_dev->bp_10gb)
            ctrl = BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO);


        else if (!pbpctl_dev->bp_10g)
            ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL);
        else
            //ctrl =readl((void *)((pbpctl_dev)->mem_map) + 0x20);
            ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);

        if (!tx_state)
            if (pbpctl_dev->bp_10g9) {
                BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl |BP10G_SDP3_DATA|BP10G_SDP3_DIR));


            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT,(ctrl|BPCTLI_CTRL_EXT_SDP6_DIR|BPCTLI_CTRL_EXT_SDP6_DATA));


            } else if (pbpctl_dev->bp_10gb) {
                if ((pbpctl_dev->func==1)||(pbpctl_dev->func==3))
                    BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_GPIO,(ctrl|BP10GB_GPIO0_SET_P1)&~(BP10GB_GPIO0_CLR_P1|BP10GB_GPIO0_OE_P1));
                else
                    BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_GPIO,(ctrl|BP10GB_GPIO0_OE_P0|BP10GB_GPIO0_SET_P0));

            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL,(ctrl|BPCTLI_CTRL_SDP1_DIR|BPCTLI_CTRL_SWDPIN1));

            } else if (pbpctl_dev->bp_540) {
                ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
                BP10G_WRITE_REG(pbpctl_dev, ESDP,(ctrl|BP10G_SDP1_DIR|BP10G_SDP1_DATA));

            }


            else if (!pbpctl_dev->bp_10g)
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL,(ctrl|BPCTLI_CTRL_SWDPIO0|BPCTLI_CTRL_SWDPIN0));

            else
                //writel((ctrl|(0x1|0x100)), (void *)(((pbpctl_dev)->mem_map) + 0x20)) ;
                BP10G_WRITE_REG(pbpctl_dev, ESDP, (ctrl |BP10G_SDP0_DATA|BP10G_SDP0_DIR));


        else {
            if (pbpctl_dev->bp_10g9) {

                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP10G_SDP3_DIR) &~BP10G_SDP3_DATA));


            } else if (pbpctl_dev->bp_fiber5) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, ((ctrl|BPCTLI_CTRL_EXT_SDP6_DIR)&~BPCTLI_CTRL_EXT_SDP6_DATA));


            } else if (pbpctl_dev->bp_10gb) {
                if ((bpctl_dev_arr->func==1)||(bpctl_dev_arr->func==3))
                    BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_GPIO,(ctrl|BP10GB_GPIO0_CLR_P1)&~(BP10GB_GPIO0_SET_P1|BP10GB_GPIO0_OE_P1));
                else
                    BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_GPIO,(ctrl|BP10GB_GPIO0_OE_P0|BP10GB_GPIO0_CLR_P0));



            } else if (pbpctl_dev->bp_i80) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl|BPCTLI_CTRL_SDP1_DIR)&~BPCTLI_CTRL_SWDPIN1));
            } else if (pbpctl_dev->bp_540) {
                ctrl=BP10G_READ_REG(pbpctl_dev,ESDP);
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP10G_SDP1_DIR)&~BP10G_SDP1_DATA));
            }


            else if (!pbpctl_dev->bp_10g) {
                BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, ((ctrl|BPCTLI_CTRL_SWDPIO0)&~BPCTLI_CTRL_SWDPIN0));
                if (!PEGF_IF_SERIES(pbpctl_dev->subdevice)) {
                    BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL, 
                                       (ctrl&~(BPCTLI_CTRL_SDP0_DATA|BPCTLI_CTRL_SDP0_DIR)));
                }
            } else
                //writel(((ctrl|0x100)&~0x1), (void *)(((pbpctl_dev)->mem_map) + 0x20)) ;
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP10G_SDP0_DIR) &~BP10G_SDP0_DATA));


        }

    } else ret=BP_NOT_CAP;
    return ret;

}

/* SET_FORCE_LINK  (non-Bypass command :)) */
static int set_bp_force_link (bpctl_dev_t *pbpctl_dev, int tx_state){
    int ret=0, ctrl=0;

    if (DBI_IF_SERIES(pbpctl_dev->subdevice)) {

        if ((pbpctl_dev->bp_10g)|| (pbpctl_dev->bp_10g9)) {

            ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL);
            if (!tx_state)
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ctrl&~BP10G_SDP1_DIR);
            else
                BP10G_WRITE_REG(pbpctl_dev, ESDP, ((ctrl|BP10G_SDP1_DIR)&~BP10G_SDP1_DATA));
            return ret;
        }

    }
    return BP_NOT_CAP;
}




/*RESET_CONT 0x20 */
int reset_cont (bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
            return BP_NOT_CAP;
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)
            write_data(pbpctl_dev,RESET_CONT);
        else
            data_pulse(pbpctl_dev,RESET_CONT);
        ret=0;
    };
    return ret;
}

/*DIS_BYPASS_CAP 0x22 */
int dis_bypass_cap(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&BP_DIS_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,DIS_BYPASS_CAP_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
        } else {
            write_data(pbpctl_dev,BYPASS_OFF);
            msec_delay_bp(LATCH_DELAY);
            write_data(pbpctl_dev,DIS_BYPASS_CAP);
            msec_delay_bp(BYPASS_CAP_DELAY);
        }
        return 0;
    }
    return BP_NOT_CAP;
}


/*EN_BYPASS_CAP 0x24 */
int en_bypass_cap(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&BP_DIS_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,PWROFF_BYPASS_ON_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
        } else {
            write_data(pbpctl_dev,EN_BYPASS_CAP);
            msec_delay_bp(BYPASS_CAP_DELAY);
        }
        return 0;
    }
    return BP_NOT_CAP;
}

/* BYPASS_STATE_PWRON 0x26*/
int bypass_state_pwron(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&BP_PWUP_CTL_CAP) {
        write_data(pbpctl_dev,BYPASS_STATE_PWRON);
        if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
            msec_delay_bp(DFLT_PWRON_DELAY);
        else msec_delay_bp(EEPROM_WR_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/* NORMAL_STATE_PWRON 0x28*/
int normal_state_pwron(bpctl_dev_t *pbpctl_dev){
    if ((pbpctl_dev->bp_caps&BP_PWUP_CTL_CAP)||(pbpctl_dev->bp_caps&TAP_PWUP_CTL_CAP)) {
        write_data(pbpctl_dev,NORMAL_STATE_PWRON);
        if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
            msec_delay_bp(DFLT_PWRON_DELAY);
        else msec_delay_bp(EEPROM_WR_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/* BYPASS_STATE_PWROFF 0x27*/
int bypass_state_pwroff(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&BP_PWOFF_CTL_CAP) {
        write_data(pbpctl_dev,BYPASS_STATE_PWROFF);
        msec_delay_bp(EEPROM_WR_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/* NORMAL_STATE_PWROFF 0x29*/
int normal_state_pwroff(bpctl_dev_t *pbpctl_dev){
    if ((pbpctl_dev->bp_caps&BP_PWOFF_CTL_CAP)) {
        write_data(pbpctl_dev,NORMAL_STATE_PWROFF);
        msec_delay_bp(EEPROM_WR_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/*TAP_STATE_PWRON 0x2a*/
int tap_state_pwron(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_PWUP_CTL_CAP) {
        write_data(pbpctl_dev,TAP_STATE_PWRON);
        msec_delay_bp(EEPROM_WR_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/*DIS_TAP_CAP 0x2c*/
int dis_tap_cap(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_DIS_CAP) {
        write_data(pbpctl_dev,DIS_TAP_CAP);
        msec_delay_bp(BYPASS_CAP_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}

/*EN_TAP_CAP 0x2e*/
int en_tap_cap(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_DIS_CAP) {
        write_data(pbpctl_dev,EN_TAP_CAP);
        msec_delay_bp(BYPASS_CAP_DELAY);
        return 0;
    }
    return BP_NOT_CAP;
}
/*DISC_STATE_PWRON 0x2a*/
int disc_state_pwron(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&DISC_PWUP_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            write_data(pbpctl_dev,DISC_STATE_PWRON);
            msec_delay_bp(EEPROM_WR_DELAY);
            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

/*DIS_DISC_CAP 0x2c*/
int dis_disc_cap(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&DISC_DIS_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            write_data(pbpctl_dev,DIS_DISC_CAP);
            msec_delay_bp(BYPASS_CAP_DELAY);
            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

/*DISC_STATE_PWRON 0x2a*/
int disc_port_state_pwron(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    bpctl_dev_t *pbpctl_dev_m;

    return BP_NOT_CAP;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1)
            write_data(pbpctl_dev_m,TX_DISA_PWRUP);
        else
            write_data(pbpctl_dev_m,TX_DISB_PWRUP);

        msec_delay_bp(LATCH_DELAY);

    }
    return ret;
}

int normal_port_state_pwron(bpctl_dev_t *pbpctl_dev){
    int ret=0;
    bpctl_dev_t *pbpctl_dev_m;
    return BP_NOT_CAP;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1)
            write_data(pbpctl_dev_m,TX_ENA_PWRUP);
        else
            write_data(pbpctl_dev_m,TX_ENB_PWRUP);

        msec_delay_bp(LATCH_DELAY);

    }
    return ret;
}




/*EN_TAP_CAP 0x2e*/
int en_disc_cap(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&DISC_DIS_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            write_data(pbpctl_dev,EN_DISC_CAP);
            msec_delay_bp(BYPASS_CAP_DELAY);
            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}


int std_nic_on(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&STD_NIC_CAP) {

        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,DIS_BYPASS_CAP_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            pbpctl_dev->bp_status_un=0;
            return BP_OK;
        }

        if (pbpctl_dev->bp_ext_ver>=0x8) {
            write_data(pbpctl_dev,STD_NIC_ON);
            msec_delay_bp(BYPASS_CAP_DELAY);
            return BP_OK;

        }


        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            wdt_off(pbpctl_dev);

            if (pbpctl_dev->bp_caps&BP_CAP) {
                write_data(pbpctl_dev,BYPASS_OFF);
                msec_delay_bp(LATCH_DELAY);
            }

            if (pbpctl_dev->bp_caps&TAP_CAP) {
                write_data(pbpctl_dev,TAP_OFF);
                msec_delay_bp(LATCH_DELAY);
            }

            write_data(pbpctl_dev,NORMAL_STATE_PWRON);
            if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
                msec_delay_bp(DFLT_PWRON_DELAY);
            else msec_delay_bp(EEPROM_WR_DELAY);

            if (pbpctl_dev->bp_caps&BP_DIS_CAP) {
                write_data(pbpctl_dev,DIS_BYPASS_CAP);
                msec_delay_bp(BYPASS_CAP_DELAY);
            }

            if (pbpctl_dev->bp_caps&TAP_DIS_CAP) {
                write_data(pbpctl_dev,DIS_TAP_CAP);
                msec_delay_bp(BYPASS_CAP_DELAY);

            }
            return 0;
        }
    }
    return BP_NOT_CAP;
}

int std_nic_off(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&STD_NIC_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            write_data_int(pbpctl_dev,PWROFF_BYPASS_ON_INT);
            msec_delay_bp(BYPASS_DELAY_INT);
            return BP_OK;
        }
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            write_data(pbpctl_dev,STD_NIC_OFF);
            msec_delay_bp(BYPASS_CAP_DELAY);
            return BP_OK;

        }

        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {

            if (pbpctl_dev->bp_caps&TAP_PWUP_CTL_CAP) {
                write_data(pbpctl_dev,TAP_STATE_PWRON);
                msec_delay_bp(EEPROM_WR_DELAY);
            }

            if (pbpctl_dev->bp_caps&BP_PWUP_CTL_CAP) {
                write_data(pbpctl_dev,BYPASS_STATE_PWRON);
                if (pbpctl_dev->bp_ext_ver>PXG2BPI_VER)
                    msec_delay_bp(EEPROM_WR_DELAY);
                else
                    msec_delay_bp(DFLT_PWRON_DELAY);
            }

            if (pbpctl_dev->bp_caps&TAP_DIS_CAP) {
                write_data(pbpctl_dev,EN_TAP_CAP);
                msec_delay_bp(BYPASS_CAP_DELAY);
            }
            if (pbpctl_dev->bp_caps&DISC_DIS_CAP) {
                write_data(pbpctl_dev,EN_DISC_CAP);
                msec_delay_bp(BYPASS_CAP_DELAY);
            }


            if (pbpctl_dev->bp_caps&BP_DIS_CAP) {
                write_data(pbpctl_dev,EN_BYPASS_CAP);
                msec_delay_bp(BYPASS_CAP_DELAY);
            }

            return 0;
        }
    }
    return BP_NOT_CAP;
}

int wdt_time_left (bpctl_dev_t *pbpctl_dev)
{

    //unsigned long curr_time=((long long)(jiffies*1000))/HZ, delta_time=0,wdt_on_time=((long long)(pbpctl_dev->bypass_wdt_on_time*1000))/HZ;
    unsigned long curr_time=jiffies, delta_time=0, wdt_on_time=pbpctl_dev->bypass_wdt_on_time, delta_time_msec=0;
    int time_left = 0;
#if 0	 
	int exp_flag = 0, ctrl_ext = 0;


	bpctl_dev_t *pbpctl_dev_b=NULL;


	if ((pbpctl_dev->bp_40g) &&
		(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))  {
		if(pbpctl_dev->func_p == 0) {
			ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 2);
			BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 2, (ctrl_ext &
				 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));
			exp_flag = ((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_2)!=0?0:1;

		} else if(pbpctl_dev->func_p == 2) {
			ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 3);
			BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 3, (ctrl_ext &
					 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));
        
		  exp_flag = ((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_3)!=0?0:1;

		} 
	}
	if (exp_flag) {
		printk("bpmod: WDT expired!\n");
		pbpctl_dev->wdt_status = WDT_STATUS_EXP;
		return -1;
	}
#endif
    switch (pbpctl_dev->wdt_status) {
    case WDT_STATUS_DIS:
        time_left=0;
        break;
    case WDT_STATUS_EN:
        delta_time = (curr_time>=wdt_on_time)?(curr_time-wdt_on_time):(~wdt_on_time+curr_time);
        delta_time_msec=jiffies_to_msecs(delta_time);
        time_left= pbpctl_dev->bypass_timer_interval-delta_time_msec;
        if (time_left<0) {
            time_left=-1;
            pbpctl_dev->wdt_status=WDT_STATUS_EXP;
        }
        break;
    case WDT_STATUS_EXP:
        time_left=-1;
        break;
    }

    return time_left;
}

static int wdt_timer(bpctl_dev_t *pbpctl_dev, int *time_left){
    int ret=0;
    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        {

            if (pbpctl_dev->wdt_status==WDT_STATUS_UNKNOWN)
                ret=BP_NOT_CAP;
            else
                *time_left=wdt_time_left(pbpctl_dev);
        } 

    } else ret=BP_NOT_CAP;
    return ret;
}

static int wdt_timer_reload(bpctl_dev_t *pbpctl_dev){

    int ret=0;

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
			wdt_pulse(pbpctl_dev);
			return 1;
		}
		else return BP_NOT_CAP;
	}
	
    if ((pbpctl_dev->bp_caps&WD_CTL_CAP)&&
        (pbpctl_dev->wdt_status!=WDT_STATUS_UNKNOWN)) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)
            ret= wdt_pulse(pbpctl_dev);
        else if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
            ret=wdt_pulse_int(pbpctl_dev);
        else  ret=send_wdt_pulse(pbpctl_dev);
        //if (ret==-1)
        //    mod_timer(&pbpctl_dev->bp_timer, jiffies+1);
        return 1;
    }
    return BP_NOT_CAP; 
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
static void wd_reset_timer(unsigned long param){
    bpctl_dev_t *pbpctl_dev= (bpctl_dev_t *) param;
#else
static void wd_reset_timer(struct timer_list *t){
    bpctl_dev_t *pbpctl_dev= from_timer(pbpctl_dev, t, bp_timer);
#endif
#ifdef BP_SELF_TEST
    struct sk_buff *skb_tmp; 
#endif 

    if ((pbpctl_dev->bp_ext_ver>=PXG2BPI_VER)&&
        ((atomic_read(&pbpctl_dev->wdt_busy))==1)) {
        mod_timer(&pbpctl_dev->bp_timer, jiffies+1);
        return;
    }
#ifdef BP_SELF_TEST

    if (pbpctl_dev->bp_self_test_flag==1) {
        skb_tmp=dev_alloc_skb(BPTEST_DATA_LEN+2); 
        if ((skb_tmp)&&(pbpctl_dev->ndev)&&(pbpctl_dev->bp_tx_data)) {
            memcpy(skb_put(skb_tmp,BPTEST_DATA_LEN),pbpctl_dev->bp_tx_data,BPTEST_DATA_LEN);
            skb_tmp->dev=pbpctl_dev->ndev;
            skb_tmp->protocol=eth_type_trans(skb_tmp,pbpctl_dev->ndev);
            skb_tmp->ip_summed=CHECKSUM_UNNECESSARY;
            netif_receive_skb(skb_tmp);
            goto bp_timer_reload;
            return;
        }
    }
#endif


    wdt_timer_reload(pbpctl_dev); 
#ifdef BP_SELF_TEST
    bp_timer_reload:
#endif
    if (pbpctl_dev->reset_time) {

        mod_timer(&pbpctl_dev->bp_timer, jiffies+(HZ*pbpctl_dev->reset_time)/1000);
    }
}


//#ifdef PMC_FIX_FLAG
/*WAIT_AT_PWRUP 0x80   */
int bp_wait_at_pwup_en(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
            write_data(pbpctl_dev,BP_WAIT_AT_PWUP_EN);
            msec_delay_bp(EEPROM_WR_DELAY);

            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

/*DIS_WAIT_AT_PWRUP       0x81 */
int bp_wait_at_pwup_dis(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {

        if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
            write_data(pbpctl_dev,BP_WAIT_AT_PWUP_DIS);
            msec_delay_bp(EEPROM_WR_DELAY);

            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

/*EN_HW_RESET  0x82   */

int bp_hw_reset_en(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
            write_data(pbpctl_dev,BP_HW_RESET_EN);
            msec_delay_bp(EEPROM_WR_DELAY);

            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

/*DIS_HW_RESET             0x83   */

int bp_hw_reset_dis(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
            write_data(pbpctl_dev,BP_HW_RESET_DIS);
            msec_delay_bp(EEPROM_WR_DELAY);

            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

//#endif /*PMC_FIX_FLAG*/


int wdt_exp_mode(bpctl_dev_t *pbpctl_dev, int mode){
    uint32_t status_reg=0, status_reg1=0;

    if ((pbpctl_dev->bp_caps&(TAP_STATUS_CAP|DISC_CAP))&&
        (pbpctl_dev->bp_caps&BP_CAP)) {
        if (pbpctl_dev->bp_ext_ver>=PXE2TBPI_VER) {

            if ((pbpctl_dev->bp_ext_ver>=0x8)&&
                (mode==2)&& 
                (pbpctl_dev->bp_caps&DISC_CAP)) {
                status_reg1=read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR);
                if (!(status_reg1&WDTE_DISC_BPN_MASK))
                    write_reg(pbpctl_dev,status_reg1 | WDTE_DISC_BPN_MASK, STATUS_DISC_REG_ADDR);
                return BP_OK;
            }
        }
        status_reg=read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR);

        if ((mode==0)&&(pbpctl_dev->bp_caps&BP_CAP)) {
            if (pbpctl_dev->bp_ext_ver>=0x8) {
                status_reg1=read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR);
                if (status_reg1&WDTE_DISC_BPN_MASK)
                    write_reg(pbpctl_dev,status_reg1 & ~WDTE_DISC_BPN_MASK, STATUS_DISC_REG_ADDR);
            }
            if (status_reg&WDTE_TAP_BPN_MASK)
                write_reg(pbpctl_dev,status_reg & ~WDTE_TAP_BPN_MASK, STATUS_TAP_REG_ADDR);
            return BP_OK;

        } else if ((mode==1)&&(pbpctl_dev->bp_caps&TAP_CAP)) {
            if (!(status_reg&WDTE_TAP_BPN_MASK))
                write_reg(pbpctl_dev,status_reg | WDTE_TAP_BPN_MASK, STATUS_TAP_REG_ADDR);
            /*else return BP_NOT_CAP;*/
            return BP_OK;
        }

    }
    return BP_NOT_CAP;
}



int bypass_fw_ver(bpctl_dev_t *pbpctl_dev){
    if (is_bypass_fn(pbpctl_dev)) {
		
		if ((pbpctl_dev->bp_40g) ||
			(pbpctl_dev->bp_rang)) {
			bp_cmd_t bp_cmd_buf;
			bp_cmd_rsp_t bp_rsp_buf;

			bpctl_dev_t *pbpctl_dev_c;
			int ret=-1;

			memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
			memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
			bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
			bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_INFO;
			if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
					return -1;

			if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
				ret = bp_rsp_buf.rsp.rsp_data.bypass_info.fw_ver;
			}
			return ret;

		} else return((read_reg(pbpctl_dev,VER_REG_ADDR)));
	}	
    else return BP_NOT_CAP;
}

int bypass_sign_check(bpctl_dev_t *pbpctl_dev){

    if (is_bypass_fn(pbpctl_dev))
        return(((read_reg(pbpctl_dev,PIC_SIGN_REG_ADDR))==PIC_SIGN_VALUE)?1:0);
    else return BP_NOT_CAP;
}

static int tx_status (bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl=0;
    bpctl_dev_t *pbpctl_dev_m;
    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;
    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {

        ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL);
        if (pbpctl_dev->bp_i80)
            return((ctrl&BPCTLI_CTRL_SWDPIN1)!=0?0:1);
        if (pbpctl_dev->bp_540) {
            ctrl = BP10G_READ_REG(pbpctl_dev, ESDP);

            return((ctrl&BP10G_SDP1_DATA)!=0?0:1);
        }



    }

    if (pbpctl_dev->bp_caps&TX_CTL_CAP) {
        if (PEG5_IF_SERIES(pbpctl_dev->subdevice)) {
            uint16_t mii_reg;
            if (!(bp75_read_phy_reg(pbpctl_dev, BPCTLI_PHY_CONTROL, &mii_reg))) {
                if (mii_reg & BPCTLI_MII_CR_POWER_DOWN)
                    return 0;

                else
                    return 1;
            }return -1;
        }

        if (pbpctl_dev->bp_10g9) {

            return((BP10G_READ_REG(pbpctl_dev,ESDP)&BP10G_SDP3_DATA)!=0?0:1);

        } else if (pbpctl_dev->bp_fiber5) {
            ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
            if (ctrl&BPCTLI_CTRL_EXT_SDP6_DATA)
                return 0;
            return 1; 
        } else if (pbpctl_dev->bp_10gb) {
            ctrl= BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO);
            BP10GB_WRITE_REG(pbpctl_dev, MISC_REG_GPIO,(ctrl|BP10GB_GPIO0_OE_P1)&~(BP10GB_GPIO0_SET_P1|BP10GB_GPIO0_CLR_P1));

            if ((pbpctl_dev->func==1)||(pbpctl_dev->func==3))
                return(((BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO)) & BP10GB_GPIO0_P1)!=0?0:1);
            else
                return(((BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO)) & BP10GB_GPIO0_P0)!=0?0:1);
        }

        if (!pbpctl_dev->bp_10g) {

            ctrl = BPCTL_READ_REG(pbpctl_dev, CTRL);
            if (pbpctl_dev->bp_i80)
                return((ctrl&BPCTLI_CTRL_SWDPIN1)!=0?0:1);
            if (pbpctl_dev->bp_540) {
                ctrl = BP10G_READ_REG(pbpctl_dev, ESDP);

                return((ctrl&BP10G_SDP1_DATA)!=0?0:1);
            }

            return((ctrl&BPCTLI_CTRL_SWDPIN0)!=0?0:1);
        } else
            return((BP10G_READ_REG(pbpctl_dev,ESDP)&BP10G_SDP0_DATA)!=0?0:1);      

    }
    return BP_NOT_CAP;
}

static int bp_force_link_status (bpctl_dev_t *pbpctl_dev){

    if (DBI_IF_SERIES(pbpctl_dev->subdevice)) {

        if ((pbpctl_dev->bp_10g) || (pbpctl_dev->bp_10g9)) {
            return((BP10G_READ_REG(pbpctl_dev,ESDP)&BP10G_SDP1_DIR)!=0?1:0); 

        }
    }
    return BP_NOT_CAP;
}



int bypass_from_last_read(bpctl_dev_t *pbpctl_dev){
    uint32_t ctrl_ext=0;
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if ((pbpctl_dev->bp_caps&SW_CTL_CAP)&&(pbpctl_dev_b=get_status_port_fn(pbpctl_dev))) {
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT);
        BPCTL_BP_WRITE_REG(pbpctl_dev_b, CTRL_EXT, ( ctrl_ext & ~BPCTLI_CTRL_EXT_SDP7_DIR));
        ctrl_ext = BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT);
        if (ctrl_ext&BPCTLI_CTRL_EXT_SDP7_DATA)
            return 0;
        return 1;
    } else return BP_NOT_CAP;
}

int bypass_status_clear(bpctl_dev_t *pbpctl_dev){
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if ((pbpctl_dev->bp_caps&SW_CTL_CAP)&&(pbpctl_dev_b=get_status_port_fn(pbpctl_dev))) {

        send_bypass_clear_pulse(pbpctl_dev_b, 1);
        return 0;
    } else
        return BP_NOT_CAP;
}

int bypass_flag_status(bpctl_dev_t *pbpctl_dev){

    if ((pbpctl_dev->bp_caps&BP_CAP)) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & BYPASS_FLAG_MASK)==BYPASS_FLAG_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}

int bypass_flag_status_clear(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&BP_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            uint32_t status_reg=0;
            status_reg=read_reg(pbpctl_dev,STATUS_REG_ADDR);
            write_reg(pbpctl_dev,status_reg & ~BYPASS_FLAG_MASK, STATUS_REG_ADDR);
            return 0;
        }
    }
    return BP_NOT_CAP;
}


int bypass_change_status(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&BP_STATUS_CHANGE_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            ret=bypass_flag_status(pbpctl_dev);
            bypass_flag_status_clear(pbpctl_dev);
        } else if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            ret=bypass_flag_status(pbpctl_dev);
            bypass_flag_status_clear(pbpctl_dev);
        } else {
            ret=bypass_from_last_read(pbpctl_dev);
            bypass_status_clear(pbpctl_dev);
        }
    }
    return ret;
}



int bypass_off_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&BP_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & BYPASS_OFF_MASK)==BYPASS_OFF_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}

static int bypass_status(bpctl_dev_t *pbpctl_dev){
    u32 ctrl_ext=0;
    if (pbpctl_dev->bp_caps&BP_CAP) {

        bpctl_dev_t *pbpctl_dev_b=NULL;

        if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
            return BP_NOT_CAP;


		if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
#if 0
			if(pbpctl_dev->func_p == 0) {
				ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 2);
				BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 2, (ctrl_ext &
									 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));


				return(((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_2)!=0?1:0);

			} else if(pbpctl_dev->func_p == 2) {
				ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 3);
				BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 3, (ctrl_ext &
									 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));


				return(((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_3)!=0?1:0);

			} else 
				return BP_NOT_CAP;
#else

        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
				return -1;

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_EN)
					ret=1;
				else if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_DIS)
					ret=0;
			}
		}
		return ret;
#endif
			
		} else if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {

            if (!pbpctl_dev->bp_status_un)
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT)) & BPCTLI_CTRL_EXT_SDP7_DATA)!=0?1:0);
            else
                return BP_NOT_CAP;
        }
        if (pbpctl_dev->bp_ext_ver>=0x8) {

            //BPCTL_BP_WRITE_REG(pbpctl_dev, CTRL_EXT, (BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT))&~BPCTLI_CTRL_EXT_SDP7_DIR);
            if (pbpctl_dev->bp_10g9) {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,I2CCTL);
                BP10G_WRITE_REG(pbpctl_dev_b, I2CCTL, (ctrl_ext|BP10G_I2C_CLK_OUT));
                //return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28))&0x4)!=0?0:1);
                return((BP10G_READ_REG(pbpctl_dev_b,I2CCTL)&BP10G_I2C_CLK_IN)!=0?0:1);


            } else if (pbpctl_dev->bp_540) {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,ESDP);
                BP10G_WRITE_REG(pbpctl_dev_b, ESDP, (ctrl_ext|BIT_11)&~BIT_3);
                return(((BP10G_READ_REG(pbpctl_dev_b, ESDP)) & BP10G_SDP0_DATA)!=0?0:1);
            }



            else if ((pbpctl_dev->bp_fiber5)||(pbpctl_dev->bp_i80)) {
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL)) & BPCTLI_CTRL_SWDPIN0)!=0?0:1);
            } else if (pbpctl_dev->bp_10gb) {
                ctrl_ext= BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO);
                BP10GB_WRITE_REG(pbpctl_dev,MISC_REG_GPIO, (ctrl_ext| BP10GB_GPIO3_OE_P0)&~(BP10GB_GPIO3_SET_P0|BP10GB_GPIO3_CLR_P0));


                return(((BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO)) & BP10GB_GPIO3_P0)!=0?0:1);
            }


            else if (!pbpctl_dev->bp_10g)
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT)) & BPCTLI_CTRL_EXT_SDP7_DATA)!=0?0:1);

            else {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,EODSDP);
                BP10G_WRITE_REG(pbpctl_dev_b, EODSDP, (ctrl_ext|BP10G_SDP7_DATA_OUT));
                //return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28))&0x4)!=0?0:1);
                return((BP10G_READ_REG(pbpctl_dev_b,EODSDP)&BP10G_SDP7_DATA_IN)!=0?0:1);
            }

        } else
            if (pbpctl_dev->media_type == bp_copper) {


            return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL)) & BPCTLI_CTRL_SWDPIN1)!=0?1:0);
        } else {
            if ((bypass_status_clear(pbpctl_dev))>=0)
                return(bypass_from_last_read(pbpctl_dev));
        }    

    }
    return BP_NOT_CAP;
}


static int wd_exp_status(bpctl_dev_t *pbpctl_dev){
    u32 ctrl_ext=0;
    if (pbpctl_dev->bp_caps&BP_CAP) {

        bpctl_dev_t *pbpctl_dev_b=NULL;

		if ((pbpctl_dev->bp_rang) && (!na_base))
			return BP_NOT_CAP;

        if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
            return BP_NOT_CAP;

		if(pbpctl_dev->bp_rang) {
			unsigned long value;

			if(!na_base)
				return BP_NOT_CAP;

			value = inl(na_base + 0x80);
			if(!(value & (BIT_17 | BIT_18))) {
				value |= (BIT_17 | BIT_18);
				outl(value, (na_base + 0x80));
			}
		}

		if (pbpctl_dev->bp_rang) {

			if(pbpctl_dev->func_p == 0) {
				unsigned long value;

				value = inl(na_base + 0x84);
				value |= BIT_17;
				outl(value, (na_base + 0x84));
				return((inl(na_base + 0x88) & BIT_17) !=0 ? 0:1);


			} else {
				unsigned long value;

				value = inl(na_base + 0x84);
				value |= BIT_18;
				outl(value, (na_base + 0x84));
				return((inl(na_base + 0x88) & BIT_18) !=0 ? 0:1);
			}

		} else if (pbpctl_dev->bp_40g) {
#if 1
			if(pbpctl_dev->func_p == 0) {
				ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 2);
				BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 2, (ctrl_ext &
									 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));

				BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT);
				return(((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_2)!=0?0:1);

			} else if(pbpctl_dev->func_p == 2) {
				ctrl_ext = BP40G_READ_GPIO_CTL(pbpctl_dev_b, 3);
				BP40G_WRITE_GPIO_CTL(pbpctl_dev_b, 3, (ctrl_ext &
									 ~(BP40GB_GPIO_SDP_MODE_MASK | BP40GB_GPIO_OE)));

				BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT);
				return(((BP40G_RD_REG(pbpctl_dev_b, GPIO_STAT)) & BIT_3)!=0?0:1);

			} else 
				return BP_NOT_CAP;
#endif

			

		}
	}
    return BP_NOT_CAP;
}



int default_pwron_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_caps&BP_PWUP_CTL_CAP) {
            if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
                return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & DFLT_PWRON_MASK)==DFLT_PWRON_MASK)?0:1);
            }
        } /*else if ((!pbpctl_dev->bp_caps&BP_DIS_CAP)&&
                   (pbpctl_dev->bp_caps&BP_PWUP_ON_CAP))
            return 1;*/
    }
    return BP_NOT_CAP;
}

static int default_pwroff_status(bpctl_dev_t *pbpctl_dev){

    /*if ((!pbpctl_dev->bp_caps&BP_DIS_CAP)&&
        (pbpctl_dev->bp_caps&BP_PWOFF_ON_CAP))
        return 1;*/
    if ((pbpctl_dev->bp_caps&SW_CTL_CAP)&&(pbpctl_dev->bp_caps&BP_PWOFF_CTL_CAP)) {
        return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & DFLT_PWROFF_MASK)==DFLT_PWROFF_MASK)?0:1);
    }
    return BP_NOT_CAP;
}



int dis_bypass_cap_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&BP_DIS_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & DIS_BYPASS_CAP_MASK)==DIS_BYPASS_CAP_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}


int cmd_en_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & CMND_EN_MASK)==CMND_EN_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}

int wdt_en_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            return((((read_reg(pbpctl_dev,STATUS_REG_ADDR)) & WDT_EN_MASK)==WDT_EN_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}

int wdt_programmed(bpctl_dev_t *pbpctl_dev, int *timeout){
    int ret=0;
    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            if ((read_reg(pbpctl_dev,STATUS_REG_ADDR))&WDT_EN_MASK) {
                u8 wdt_val;
                wdt_val=read_reg(pbpctl_dev,WDT_REG_ADDR);
                *timeout=  (1<<wdt_val)*100;
            } else *timeout=0;
        } else {
            int curr_wdt_status= pbpctl_dev->wdt_status;
            if (curr_wdt_status==WDT_STATUS_UNKNOWN)
                *timeout=-1;
            else
                *timeout=curr_wdt_status==0?0:pbpctl_dev->bypass_timer_interval;
        };
    } else ret=BP_NOT_CAP;
    return ret;
}

int bypass_support(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
            ret=((((read_reg(pbpctl_dev,PRODUCT_CAP_REG_ADDR)) & BYPASS_SUPPORT_MASK)==BYPASS_SUPPORT_MASK)?1:0);
        } else if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
            ret=1;
    } else ret=BP_NOT_CAP;
    return ret;
}

int tap_support(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
            ret=((((read_reg(pbpctl_dev,PRODUCT_CAP_REG_ADDR)) & TAP_SUPPORT_MASK)==TAP_SUPPORT_MASK)?1:0);
        } else if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
            ret=0;
    } else ret=BP_NOT_CAP;
    return ret;
}

int normal_support(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
            ret=((((read_reg(pbpctl_dev,PRODUCT_CAP_REG_ADDR)) & NORMAL_UNSUPPORT_MASK)==NORMAL_UNSUPPORT_MASK)?0:1);
        } else
            ret=1;
    };
    return ret;
}
int get_bp_prod_caps(bpctl_dev_t *pbpctl_dev){
    if ((pbpctl_dev->bp_caps&SW_CTL_CAP)&&
        (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER))
        return(read_reg(pbpctl_dev,PRODUCT_CAP_REG_ADDR));
    return BP_NOT_CAP;

}


int tap_flag_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&TAP_STATUS_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TAP_FLAG_MASK)==TAP_FLAG_MASK)?1:0);

    }
    return BP_NOT_CAP;
}

int tap_flag_status_clear(bpctl_dev_t *pbpctl_dev){
    uint32_t status_reg=0;
    if (pbpctl_dev->bp_caps&TAP_STATUS_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
            status_reg=read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR);
            write_reg(pbpctl_dev,status_reg & ~TAP_FLAG_MASK, STATUS_TAP_REG_ADDR);
            return 0;
        }
    }
    return BP_NOT_CAP;
}

int tap_change_status(bpctl_dev_t *pbpctl_dev){
    int ret= BP_NOT_CAP;
    if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
        if (pbpctl_dev->bp_caps&TAP_CAP) {
            if (pbpctl_dev->bp_caps&BP_CAP) {
                ret=tap_flag_status(pbpctl_dev);
                tap_flag_status_clear(pbpctl_dev);
            } else {
                ret=bypass_from_last_read(pbpctl_dev);
                bypass_status_clear(pbpctl_dev);
            }
        }
    }
    return ret;
}


int tap_off_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TAP_OFF_MASK)==TAP_OFF_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int tap_status(bpctl_dev_t *pbpctl_dev){
    u32 ctrl_ext=0;

    if (pbpctl_dev->bp_caps&TAP_CAP) {
        bpctl_dev_t *pbpctl_dev_b=NULL;

        if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
            return BP_NOT_CAP;

        if (pbpctl_dev->bp_ext_ver>=0x8) {
            if (!pbpctl_dev->bp_10g)
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT)) & BPCTLI_CTRL_EXT_SDP6_DATA)!=0?0:1);
            else {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,EODSDP);
                BP10G_WRITE_REG(pbpctl_dev_b, EODSDP, (ctrl_ext|BP10G_SDP6_DATA_OUT));
                // return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28))&0x1)!=0?0:1);
                return((BP10G_READ_REG(pbpctl_dev_b,EODSDP)&BP10G_SDP6_DATA_IN)!=0?0:1);
            }


        } else
            if (pbpctl_dev->media_type == bp_copper)
            return(((BPCTL_READ_REG(pbpctl_dev, CTRL)) & BPCTLI_CTRL_SWDPIN0)!=0?1:0);
        else {
            if ((bypass_status_clear(pbpctl_dev))>=0)
                return(bypass_from_last_read(pbpctl_dev));
        }   

    }
    return BP_NOT_CAP;
}



int default_pwron_tap_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_PWUP_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & DFLT_PWRON_TAP_MASK)==DFLT_PWRON_TAP_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int dis_tap_cap_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&TAP_PWUP_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER)
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & DIS_TAP_CAP_MASK)==DIS_TAP_CAP_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int disc_flag_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&DISC_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8)
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DISC_FLAG_MASK)==DISC_FLAG_MASK)?1:0);

    }
    return BP_NOT_CAP;
}

int disc_flag_status_clear(bpctl_dev_t *pbpctl_dev){
    uint32_t status_reg=0;
    if (pbpctl_dev->bp_caps&DISC_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8) {
            status_reg=read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR);
            write_reg(pbpctl_dev,status_reg & ~DISC_FLAG_MASK, STATUS_DISC_REG_ADDR);
            return BP_OK;
        }
    }
    return BP_NOT_CAP;
}

int disc_change_status(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;
    if (pbpctl_dev->bp_caps&DISC_CAP) {
        ret=disc_flag_status(pbpctl_dev);
        disc_flag_status_clear(pbpctl_dev);
        return ret;
    }
    return BP_NOT_CAP;
}

int disc_off_status(bpctl_dev_t *pbpctl_dev){
    bpctl_dev_t *pbpctl_dev_b=NULL;
    u32 ctrl_ext=0;

    if (pbpctl_dev->bp_caps&DISC_CAP) {
        if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
            return BP_NOT_CAP;
        if (DISCF_IF_SERIES(pbpctl_dev->subdevice))
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DISC_OFF_MASK)==DISC_OFF_MASK)?1:0);

        if (pbpctl_dev->bp_i80) {
            //  return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DISC_OFF_MASK)==DISC_OFF_MASK)?1:0);
            return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT)) & BPCTLI_CTRL_EXT_SDP6_DATA)!=0?1:0);

        }
        if (pbpctl_dev->bp_540) {
            ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,ESDP);
            BP10G_WRITE_REG(pbpctl_dev_b, ESDP, (ctrl_ext|BIT_11)&~BIT_3);
            //return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28))&0x4)!=0?0:1);
            return((BP10G_READ_REG(pbpctl_dev_b,ESDP)&BP10G_SDP2_DATA)!=0?1:0);

        }

        //if (pbpctl_dev->device==SILICOM_PXG2TBI_SSID) {
        if (pbpctl_dev->media_type == bp_copper) {

#if 0	
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DISC_OFF_MASK)==DISC_OFF_MASK)?1:0);
#endif
            if (!pbpctl_dev->bp_10g)
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL)) & BPCTLI_CTRL_SWDPIN1)!=0?1:0);
            else
                // return(((readl((void *)((pbpctl_dev)->mem_map) + 0x20)) & 0x2)!=0?1:0); 
                return((BP10G_READ_REG(pbpctl_dev_b,ESDP)&BP10G_SDP1_DATA)!=0?1:0);


        } else {

            if (pbpctl_dev->bp_10g9) {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,I2CCTL);
                BP10G_WRITE_REG(pbpctl_dev_b, I2CCTL, (ctrl_ext|BP10G_I2C_DATA_OUT));
                //return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28))&0x4)!=0?0:1);
                return((BP10G_READ_REG(pbpctl_dev_b,I2CCTL)&BP10G_I2C_DATA_IN)!=0?1:0);

            } else if (pbpctl_dev->bp_fiber5) {
                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL)) & BPCTLI_CTRL_SWDPIN1)!=0?1:0);
            } else if (pbpctl_dev->bp_10gb) {
                ctrl_ext= BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO);
                BP10GB_WRITE_REG(pbpctl_dev,MISC_REG_GPIO, (ctrl_ext| BP10GB_GPIO3_OE_P1)&~(BP10GB_GPIO3_SET_P1|BP10GB_GPIO3_CLR_P1));


                return(((BP10GB_READ_REG(pbpctl_dev, MISC_REG_GPIO))&BP10GB_GPIO3_P1)!=0?1:0);
            }
            if (!pbpctl_dev->bp_10g) {

                return(((BPCTL_READ_REG(pbpctl_dev_b, CTRL_EXT)) & BPCTLI_CTRL_EXT_SDP6_DATA)!=0?1:0);
            } else {
                ctrl_ext= BP10G_READ_REG(pbpctl_dev_b,EODSDP);
                BP10G_WRITE_REG(pbpctl_dev_b, EODSDP, (ctrl_ext|BP10G_SDP6_DATA_OUT));
                // temp=  (((BP10G_READ_REG(pbpctl_dev_b,EODSDP))&BP10G_SDP6_DATA_IN)!=0?1:0);
                //return(((readl((void *)((pbpctl_dev)->mem_map) + 0x28)) & 0x1)!=0?1:0);
                return(((BP10G_READ_REG(pbpctl_dev_b,EODSDP))&BP10G_SDP6_DATA_IN)!=0?1:0);
            }

        }
    }
    return BP_NOT_CAP;
}

static int disc_status(bpctl_dev_t *pbpctl_dev){
    int ctrl=0;
    if (pbpctl_dev->bp_caps&DISC_CAP) {

        if ((ctrl=disc_off_status(pbpctl_dev))<0)
            return ctrl;
        return((ctrl==0)?1:0);

    }
    return BP_NOT_CAP;
}


int default_pwron_disc_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&DISC_PWUP_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8)
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DFLT_PWRON_DISC_MASK)==DFLT_PWRON_DISC_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int dis_disc_cap_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&DIS_DISC_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8)
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & DIS_DISC_CAP_MASK)==DIS_DISC_CAP_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int disc_port_status(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;
    bpctl_dev_t *pbpctl_dev_m;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1) {
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TX_DISA_MASK)==TX_DISA_MASK)?1:0);
        } else
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TX_DISB_MASK)==TX_DISB_MASK)?1:0);

    }
    return ret;
}
int default_pwron_disc_port_status(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;
    bpctl_dev_t *pbpctl_dev_m;

    if ((is_bypass_fn(pbpctl_dev))==1)
        pbpctl_dev_m=pbpctl_dev;
    else
        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m==NULL)
        return BP_NOT_CAP;

    if (pbpctl_dev_m->bp_caps_ex&DISC_PORT_CAP_EX) {
        if (is_bypass_fn(pbpctl_dev)==1)
            return ret;
        //  return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TX_DISA_MASK)==TX_DISA_MASK)?1:0);
        else
            return ret;
        //   return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & TX_DISA_MASK)==TX_DISA_MASK)?1:0);

    }
    return ret;
}



int wdt_exp_mode_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver<=PXG2BPI_VER)
            return 0;  /* bypass mode */
        else if (pbpctl_dev->bp_ext_ver==PXG2TBPI_VER)
            return 1; /* tap mode */
        else if (pbpctl_dev->bp_ext_ver>=PXE2TBPI_VER) {
            if (pbpctl_dev->bp_ext_ver>=0x8) {
                if (((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & WDTE_DISC_BPN_MASK)==WDTE_DISC_BPN_MASK)
                    return 2;
            }
            return((((read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR)) & WDTE_TAP_BPN_MASK)==WDTE_TAP_BPN_MASK)?1:0);
        }
    }
    return BP_NOT_CAP;
}

int tpl2_flag_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps_ex&TPL2_CAP_EX) {
        return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & TPL2_FLAG_MASK)==TPL2_FLAG_MASK)?1:0);

    }
    return BP_NOT_CAP;
}


int tpl_hw_status (bpctl_dev_t *pbpctl_dev){
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
        return BP_NOT_CAP;

    if (TPL_IF_SERIES(pbpctl_dev->subdevice))
        return(((BPCTL_READ_REG(pbpctl_dev, CTRL)) & BPCTLI_CTRL_SWDPIN0)!=0?1:0);
    return BP_NOT_CAP;
}

//#ifdef PMC_FIX_FLAG


int bp_wait_at_pwup_status(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {
        if (pbpctl_dev->bp_ext_ver>=0x8)
            return((((read_reg(pbpctl_dev,CONT_CONFIG_REG_ADDR)) & WAIT_AT_PWUP_MASK)==WAIT_AT_PWUP_MASK)?1:0);
    }
    return BP_NOT_CAP;
}

int bp_hw_reset_status(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&SW_CTL_CAP) {

        if (pbpctl_dev->bp_ext_ver>=0x8)
            return((((read_reg(pbpctl_dev,CONT_CONFIG_REG_ADDR)) & EN_HW_RESET_MASK)==EN_HW_RESET_MASK)?1:0);
    }
    return BP_NOT_CAP;
}
//#endif /*PMC_FIX_FLAG*/


int std_nic_status(bpctl_dev_t *pbpctl_dev){
    int status_val=0;

    if (pbpctl_dev->bp_caps&STD_NIC_CAP) {
        if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
            return BP_NOT_CAP;
        if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
            return((((read_reg(pbpctl_dev,STATUS_DISC_REG_ADDR)) & STD_NIC_ON_MASK)==STD_NIC_ON_MASK)?1:0);
        }


        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            if (pbpctl_dev->bp_caps&BP_CAP) {
                status_val=read_reg(pbpctl_dev,STATUS_REG_ADDR);
                if (((!(status_val&WDT_EN_MASK))&& ((status_val & STD_NIC_MASK)==STD_NIC_MASK)))
                    status_val=1;
                else
                    return 0;
            }
            if (pbpctl_dev->bp_caps&TAP_CAP) {
                status_val=read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR);
                if ((status_val & STD_NIC_TAP_MASK)==STD_NIC_TAP_MASK)
                    status_val=1;
                else
                    return 0;
            }
            if (pbpctl_dev->bp_caps&TAP_CAP) {
                if ((disc_off_status(pbpctl_dev)))
                    status_val=1;
                else
                    return 0; 
            }

            return status_val;
        }
    }
    return BP_NOT_CAP;
}  




/******************************************************/
/**************SW_INIT*********************************/
/******************************************************/
void bypass_caps_init (bpctl_dev_t *pbpctl_dev){
    u_int32_t  ctrl_ext=0;
    bpctl_dev_t *pbpctl_dev_m=NULL;

    
#ifdef BYPASS_DEBUG
    int ret=0;
    if (!(INTEL_IF_SERIES(adapter->bp_device_block.subdevice))) {
        ret=read_reg(pbpctl_dev,VER_REG_ADDR) ;
        printk("VER_REG reg1=%x\n",ret);
        ret=read_reg(pbpctl_dev,PRODUCT_CAP_REG_ADDR) ;
        printk("PRODUCT_CAP reg=%x\n",ret);
        ret=read_reg(pbpctl_dev,STATUS_TAP_REG_ADDR) ;
        printk("STATUS_TAP reg1=%x\n",ret);
        ret=read_reg(pbpctl_dev,0x7) ;
        printk("SIG_REG reg1=%x\n",ret);
        ret=read_reg(pbpctl_dev,STATUS_REG_ADDR);
        printk("STATUS_REG_ADDR=%x\n",ret);
        ret=read_reg(pbpctl_dev,WDT_REG_ADDR);
        printk("WDT_REG_ADDR=%x\n",ret);
        ret=read_reg(pbpctl_dev,TMRL_REG_ADDR);
        printk("TMRL_REG_ADDR=%x\n",ret);
        ret=read_reg(pbpctl_dev,TMRH_REG_ADDR);
        printk("TMRH_REG_ADDR=%x\n",ret);
    }
#endif

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
		pbpctl_dev->bp_caps=get_bypass_caps_fn(pbpctl_dev);
		return;
	} 

    if ((pbpctl_dev->bp_fiber5) ||(pbpctl_dev->bp_10g9)) {
        pbpctl_dev->media_type=  bp_fiber;
    } else if (pbpctl_dev->bp_10gb) {
        if (BP10GB_CX4_SERIES(pbpctl_dev->subdevice))
            pbpctl_dev->media_type=  bp_cx4;
        else pbpctl_dev->media_type=  bp_fiber;

    }

    else if ( pbpctl_dev->bp_540)
        pbpctl_dev->media_type=bp_none;
    else if (!pbpctl_dev->bp_10g) {

        ctrl_ext = BPCTL_READ_REG(pbpctl_dev, CTRL_EXT);
        if ((ctrl_ext & BPCTLI_CTRL_EXT_LINK_MODE_MASK) ==0x0)
            pbpctl_dev->media_type=bp_copper;
        else
            pbpctl_dev->media_type=bp_fiber;

    }

    //if (!pbpctl_dev->bp_10g)
    //  pbpctl_dev->media_type=((BPCTL_READ_REG(pbpctl_dev, STATUS))&BPCTLI_STATUS_TBIMODE)?bp_fiber:bp_copper;
    else {
        if (BP10G_CX4_SERIES(pbpctl_dev->subdevice))
            pbpctl_dev->media_type=  bp_cx4;
        else pbpctl_dev->media_type=  bp_fiber;
    }


    //pbpctl_dev->bp_fw_ver=0xa8;
    if (is_bypass_fn(pbpctl_dev)) {

        pbpctl_dev->bp_caps|=BP_PWOFF_ON_CAP;
        if (pbpctl_dev->media_type==bp_fiber)
            pbpctl_dev->bp_caps|=(TX_CTL_CAP| TX_STATUS_CAP|TPL_CAP);


        if (TPL_IF_SERIES(pbpctl_dev->subdevice)) {
            pbpctl_dev->bp_caps|=TPL_CAP;
        }
        if ((pbpctl_dev->subdevice&0xfe0)==0xb40)
            pbpctl_dev->bp_caps&= ~TPL_CAP;
        if (pbpctl_dev->bp_10g9)
            pbpctl_dev->bp_caps&= ~TPL_CAP;

        if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
            pbpctl_dev->bp_caps|=(BP_CAP | BP_STATUS_CAP | SW_CTL_CAP |  
                                  BP_PWUP_ON_CAP |BP_PWUP_OFF_CAP | 
                                  BP_PWOFF_OFF_CAP |
                                  WD_CTL_CAP | WD_STATUS_CAP | STD_NIC_CAP |
                                  WD_TIMEOUT_CAP);

            pbpctl_dev->bp_ext_ver=OLD_IF_VER;
            return;
        }

        if ((pbpctl_dev->bp_fw_ver==0xff)&&
            OLD_IF_SERIES(pbpctl_dev->subdevice)) {

            pbpctl_dev->bp_caps|=(BP_CAP | BP_STATUS_CAP | BP_STATUS_CHANGE_CAP | SW_CTL_CAP |  
                                  BP_PWUP_ON_CAP | WD_CTL_CAP | WD_STATUS_CAP | 
                                  WD_TIMEOUT_CAP);

            pbpctl_dev->bp_ext_ver=OLD_IF_VER;
            return; 
        }

        else {
            switch (pbpctl_dev->bp_fw_ver) {
            case BP_FW_VER_A0:
            case BP_FW_VER_A1 :{ 
                    pbpctl_dev->bp_ext_ver=(pbpctl_dev->bp_fw_ver & EXT_VER_MASK);
                    break;
                }
            default: { 
                    if ((bypass_sign_check(pbpctl_dev))!=1) {
                        pbpctl_dev->bp_caps=0;
                        return;
                    }
                    pbpctl_dev->bp_ext_ver=(pbpctl_dev->bp_fw_ver & EXT_VER_MASK);
                }
            }
        }
        if (pbpctl_dev->bp_ext_ver>=0x9)
            pbpctl_dev->bp_caps&= ~TPL_CAP;


        if (pbpctl_dev->bp_ext_ver==PXG2BPI_VER)
            pbpctl_dev->bp_caps|=(BP_CAP|BP_STATUS_CAP|BP_STATUS_CHANGE_CAP|SW_CTL_CAP|BP_DIS_CAP|BP_DIS_STATUS_CAP|
                                  BP_PWUP_ON_CAP|BP_PWUP_OFF_CAP|BP_PWUP_CTL_CAP|WD_CTL_CAP|
                                  STD_NIC_CAP|WD_STATUS_CAP|WD_TIMEOUT_CAP);
        else if (pbpctl_dev->bp_ext_ver>=PXG2TBPI_VER) {
            int cap_reg;

            pbpctl_dev->bp_caps|=(SW_CTL_CAP|WD_CTL_CAP|WD_STATUS_CAP|WD_TIMEOUT_CAP);
            cap_reg=get_bp_prod_caps(pbpctl_dev);

            if ((cap_reg & NORMAL_UNSUPPORT_MASK)==NORMAL_UNSUPPORT_MASK)
                pbpctl_dev->bp_caps|= NIC_CAP_NEG;
            else
                pbpctl_dev->bp_caps|= STD_NIC_CAP;

            if ((normal_support(pbpctl_dev))==1)

                pbpctl_dev->bp_caps|= STD_NIC_CAP;

            else
                pbpctl_dev->bp_caps|= NIC_CAP_NEG;
            if ((cap_reg & BYPASS_SUPPORT_MASK)==BYPASS_SUPPORT_MASK) {
                pbpctl_dev->bp_caps|=(BP_CAP|BP_STATUS_CAP|BP_STATUS_CHANGE_CAP|BP_DIS_CAP|BP_DIS_STATUS_CAP|
                                      BP_PWUP_ON_CAP|BP_PWUP_OFF_CAP|BP_PWUP_CTL_CAP);
                if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER7)
                    pbpctl_dev->bp_caps|= BP_PWOFF_ON_CAP|BP_PWOFF_OFF_CAP|BP_PWOFF_CTL_CAP;
            }
            if ((cap_reg & TAP_SUPPORT_MASK)==TAP_SUPPORT_MASK) {
                pbpctl_dev->bp_caps|=(TAP_CAP|TAP_STATUS_CAP|TAP_STATUS_CHANGE_CAP|TAP_DIS_CAP|TAP_DIS_STATUS_CAP|
                                      TAP_PWUP_ON_CAP|TAP_PWUP_OFF_CAP|TAP_PWUP_CTL_CAP);
            }
            if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER8) {
                if ((cap_reg & DISC_SUPPORT_MASK)==DISC_SUPPORT_MASK)
                    pbpctl_dev->bp_caps|=(DISC_CAP|DISC_DIS_CAP|DISC_PWUP_CTL_CAP);
                if ((cap_reg & TPL2_SUPPORT_MASK)==TPL2_SUPPORT_MASK) {
                    pbpctl_dev->bp_caps_ex|=TPL2_CAP_EX;
                    pbpctl_dev->bp_caps|=TPL_CAP;
                    pbpctl_dev->bp_tpl_flag=tpl2_flag_status(pbpctl_dev);
                }

            }

            if (pbpctl_dev->bp_ext_ver>=BP_FW_EXT_VER9) {
                if ((cap_reg & DISC_PORT_SUPPORT_MASK)==DISC_PORT_SUPPORT_MASK) {
                    pbpctl_dev->bp_caps_ex|=DISC_PORT_CAP_EX;
                    pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);
                }

            } 

        }

		if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
			int sta = read_reg(pbpctl_dev,STATUS_REG_ADDR);
			if (sta & WD_EXP_FLAG_MASK)
				 pbpctl_dev->wdt_status=WDT_STATUS_EXP;
            else if (sta & WDT_EN_MASK)

                pbpctl_dev->wdt_status=WDT_STATUS_EN;
            else  pbpctl_dev->wdt_status=WDT_STATUS_DIS;
        }
        if (pbpctl_dev->bp_ext_ver>=PXG2BPI_VER) {
            if ((read_reg(pbpctl_dev,STATUS_REG_ADDR))&WDT_EN_MASK)
                pbpctl_dev->wdt_status=WDT_STATUS_EN;
            else  pbpctl_dev->wdt_status=WDT_STATUS_DIS;
        }

        if ((PEG5_IF_SERIES(pbpctl_dev->subdevice)) ||
			(PEG80_IF_SERIES(pbpctl_dev->subdevice)))
			pbpctl_dev->bp_caps |= TPL_CAP;


    } else if ((P2BPFI_IF_SERIES(pbpctl_dev->subdevice))||
               (PEGF5_IF_SERIES(pbpctl_dev->subdevice))||
               (PEGF80_IF_SERIES(pbpctl_dev->subdevice))||
               (BP10G9_IF_SERIES(pbpctl_dev->subdevice))) {
        pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);
    }
    if ((pbpctl_dev->subdevice&0xfe0)==0xaa0)
        pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);
    if (PEG5_IF_SERIES(pbpctl_dev->subdevice))
        pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);
    if (pbpctl_dev->bp_fiber5)
        pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);

    if (BP10GB_IF_SERIES  (pbpctl_dev->subdevice)) {
        pbpctl_dev->bp_caps&= ~(TX_CTL_CAP| TX_STATUS_CAP);
    }
    pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
    if (pbpctl_dev_m!=NULL) {
        int cap_reg=0;
        if (pbpctl_dev_m->bp_ext_ver>=0x9) {
            cap_reg=get_bp_prod_caps(pbpctl_dev_m);
            if ((cap_reg & DISC_PORT_SUPPORT_MASK)==DISC_PORT_SUPPORT_MASK)
                pbpctl_dev->bp_caps|= (TX_CTL_CAP| TX_STATUS_CAP);
            pbpctl_dev->bp_caps_ex|= DISC_PORT_CAP_EX;
        }
    }
}

int bypass_off_init(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if ((ret=cmnd_on(pbpctl_dev))<0)
        return ret;
    if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
        return(dis_bypass_cap(pbpctl_dev));
    wdt_off(pbpctl_dev);
    if (pbpctl_dev->bp_caps&BP_CAP)
        bypass_off(pbpctl_dev);
    if (pbpctl_dev->bp_caps&TAP_CAP)
        tap_off(pbpctl_dev);
    cmnd_off(pbpctl_dev);
    return 0;
}


void remove_bypass_wd_auto(bpctl_dev_t *pbpctl_dev){
#ifdef BP_SELF_TEST
    bpctl_dev_t *pbpctl_dev_sl= NULL;
#endif

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {

        del_timer_sync(&pbpctl_dev->bp_timer);
#ifdef BP_SELF_TEST
        pbpctl_dev_sl= get_status_port_fn(pbpctl_dev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
        if (pbpctl_dev_sl&&(pbpctl_dev_sl->ndev)&& (pbpctl_dev_sl->ndev->hard_start_xmit)&&
            (pbpctl_dev_sl->hard_start_xmit_save)  ) {
            rtnl_lock();
            pbpctl_dev_sl->ndev->hard_start_xmit=pbpctl_dev_sl->hard_start_xmit_save;
            rtnl_unlock();
        }
#else
        if (pbpctl_dev_sl&&(pbpctl_dev_sl->ndev)) {
            if ((pbpctl_dev_sl->ndev->netdev_ops)
                &&(pbpctl_dev_sl->old_ops)) {
                rtnl_lock();
                pbpctl_dev_sl->ndev->netdev_ops = pbpctl_dev_sl->old_ops;
                pbpctl_dev_sl->old_ops = NULL;

                rtnl_unlock();

            }

        }



#endif
#endif
    }

}


int init_bypass_wd_auto(bpctl_dev_t *pbpctl_dev){
    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
        init_timer(&pbpctl_dev->bp_timer);
        pbpctl_dev->bp_timer.function=&wd_reset_timer;
        pbpctl_dev->bp_timer.data=(unsigned long)pbpctl_dev;
#else
     timer_setup(&pbpctl_dev->bp_timer, wd_reset_timer, 0);
#endif
        return 1;
    }
    return BP_NOT_CAP; 
}

#ifdef BP_SELF_TEST
int
bp_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{   
    bpctl_dev_t *pbpctl_dev=NULL,*pbpctl_dev_m=NULL;
    int idx_dev=0;
    struct ethhdr *eth=(struct ethhdr *) skb->data;

    for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].ndev!=NULL)); idx_dev++) {
        if (bpctl_dev_arr[idx_dev].ndev==dev) {
            pbpctl_dev=  &bpctl_dev_arr[idx_dev];
            break;
        }
    }
    if (! pbpctl_dev)
        return 1;
    if ((htons(ETH_P_BPTEST)==eth->h_proto)) {

        pbpctl_dev_m=get_master_port_fn(pbpctl_dev);
        if (pbpctl_dev_m) {

            if (bypass_status(pbpctl_dev_m)) {
                cmnd_on(pbpctl_dev_m);
                bypass_off(pbpctl_dev_m);
                cmnd_off(pbpctl_dev_m);
            }
            wdt_timer_reload(pbpctl_dev_m); 
        }
        dev_kfree_skb_irq(skb);
        return 0;
    }
    return(pbpctl_dev->hard_start_xmit_save(skb,dev)); 
}
#endif



int set_bypass_wd_auto(bpctl_dev_t *pbpctl_dev, unsigned int param){
    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (pbpctl_dev->reset_time!=param) {
            if (INTEL_IF_SERIES(pbpctl_dev->subdevice))
                pbpctl_dev->reset_time=(param<WDT_AUTO_MIN_INT)?WDT_AUTO_MIN_INT:param;
            else pbpctl_dev->reset_time=param;
            if (param)
                mod_timer(&pbpctl_dev->bp_timer, jiffies);
        }
        return 0;
    }
    return BP_NOT_CAP; 
}

int get_bypass_wd_auto(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        return pbpctl_dev->reset_time;
    }
    return BP_NOT_CAP; 
}

#ifdef  BP_SELF_TEST

int set_bp_self_test(bpctl_dev_t *pbpctl_dev, unsigned int param){
    bpctl_dev_t *pbpctl_dev_sl=NULL;


    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        pbpctl_dev->bp_self_test_flag=param==0?0:1; 
        pbpctl_dev_sl= get_status_port_fn(pbpctl_dev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
        if ((pbpctl_dev_sl->ndev)&&
            (pbpctl_dev_sl->ndev->hard_start_xmit)) {
            rtnl_lock();
            if (pbpctl_dev->bp_self_test_flag==1) {

                pbpctl_dev_sl->hard_start_xmit_save=pbpctl_dev_sl->ndev->hard_start_xmit;  
                pbpctl_dev_sl->ndev->hard_start_xmit=bp_hard_start_xmit;   
            } else if (pbpctl_dev_sl->hard_start_xmit_save) {
                pbpctl_dev_sl->ndev->hard_start_xmit=pbpctl_dev_sl->hard_start_xmit_save;
            }
            rtnl_unlock();
        }
#else
        if ((pbpctl_dev_sl->ndev)&&
            (pbpctl_dev_sl->ndev->netdev_ops)) {
            rtnl_lock();
            if (pbpctl_dev->bp_self_test_flag==1) {

                pbpctl_dev_sl->old_ops = pbpctl_dev_sl->ndev->netdev_ops;
                pbpctl_dev_sl->new_ops = *pbpctl_dev_sl->old_ops;
                pbpctl_dev_sl->new_ops.ndo_start_xmit = bp_hard_start_xmit;
                pbpctl_dev_sl->ndev->netdev_ops = &pbpctl_dev_sl->new_ops;

            } else if (pbpctl_dev_sl->old_ops) {
                pbpctl_dev_sl->ndev->netdev_ops = pbpctl_dev_sl->old_ops;
                pbpctl_dev_sl->old_ops = NULL;
            }
            rtnl_unlock();
        }

#endif





        set_bypass_wd_auto(pbpctl_dev, param);
        return 0;
    }
    return BP_NOT_CAP; 
}

int get_bp_self_test(bpctl_dev_t *pbpctl_dev){

    if (pbpctl_dev->bp_caps&WD_CTL_CAP) {
        if (pbpctl_dev->bp_self_test_flag==1)
            return pbpctl_dev->reset_time;
        else return 0;
    }
    return BP_NOT_CAP; 
}

#endif



/**************************************************************/
/************************* API ********************************/
/**************************************************************/


int is_bypass_fn(bpctl_dev_t *pbpctl_dev){


    if (!pbpctl_dev) {
		return -1;
	}

	if (pbpctl_dev->bp_10g9) {
		return((pbpctl_dev->func_p == 0) ? 1:0);
	}
		
    return(((pbpctl_dev->func_p==0)||(pbpctl_dev->func_p==2))?1:0);
}




int set_bypass_fn (bpctl_dev_t *pbpctl_dev, int bypass_mode){
    int ret=0;

	if (!(pbpctl_dev->bp_caps & BP_CAP))
			return BP_NOT_CAP;
	

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BYPASS;
		bp_cmd_buf.cmd.cmd_data.bypass_mode=bypass_mode?1:0;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
			return -1;

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(LATCH_DELAY);	 
		return ret;

	} else {

		if ((ret=cmnd_on(pbpctl_dev))<0)
			return ret;
		if (!bypass_mode)
			ret=bypass_off(pbpctl_dev);
		else
			ret=bypass_on(pbpctl_dev);
		cmnd_off(pbpctl_dev);
	}

    return ret;
}

int get_wd_expire_fn (bpctl_dev_t *pbpctl_dev){
    return(wd_exp_status(pbpctl_dev));
}


int get_bypass_fn (bpctl_dev_t *pbpctl_dev){
    return(bypass_status(pbpctl_dev));
}



int get_bypass_change_fn(bpctl_dev_t *pbpctl_dev){
    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps & BP_CAP)) {
		return BP_NOT_CAP;
	}

	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;

		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_CHANGE;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
				return -1;

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
            if (bp_rsp_buf.rsp.rsp_data.bypass_change==BYPASS_CHANGED)
                ret=1;
            else if (bp_rsp_buf.rsp.rsp_data.bypass_change==BYPASS_NOT_CHANGED)
                ret=0;
		}
		return ret;

	} else 
		return(bypass_change_status(pbpctl_dev));
}



int set_dis_bypass_fn(bpctl_dev_t *pbpctl_dev, int dis_bypass){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}
	
	if (!(pbpctl_dev->bp_caps & BP_DIS_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_DIS_BYPASS;
		bp_cmd_buf.cmd.cmd_data.dis_bypass=dis_bypass?DIS_BYPASS_DISABLE:DIS_BYPASS_ENABLE;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(BYPASS_CAP_DELAY);
		return ret;

	} else {

		if (!(pbpctl_dev->bp_caps & BP_DIS_CAP)) {
			return BP_NOT_CAP;
		}
		if ((ret=cmnd_on(pbpctl_dev))<0) {
			return ret;
		}
		if (dis_bypass) {
			ret=dis_bypass_cap(pbpctl_dev);
		} else {
			ret=en_bypass_cap(pbpctl_dev);
		}
		cmnd_off(pbpctl_dev);
		return ret;
	}
}



int get_dis_bypass_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps & BP_DIS_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;

		bp_cmd_buf.cmd.cmd_id=CMD_GET_DIS_BYPASS;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
            if (bp_rsp_buf.rsp.rsp_data.dis_bypass==DIS_BYPASS_DISABLE) {
				ret=1;
			} else if (bp_rsp_buf.rsp.rsp_data.dis_bypass==DIS_BYPASS_ENABLE) {
				ret=0;
			}
		}
		return ret;

	} else {
		return(dis_bypass_cap_status(pbpctl_dev));
	}
}
 

int set_bypass_pwoff_fn (bpctl_dev_t *pbpctl_dev, int bypass_mode){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & BP_PWOFF_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
		int ret=-1;
		bpctl_dev_t *pbpctl_dev_c;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BYPASS_PWOFF;
		bp_cmd_buf.cmd.cmd_data.bypass_pwoff=bypass_mode?BYPASS_PWOFF_EN:BYPASS_PWOFF_DIS;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
					ret=0;
			}
		}
		msec_delay_bp(EEPROM_WR_DELAY);
		return ret;
	} else {
		if ((ret=cmnd_on(pbpctl_dev))<0) {
			return ret;
		}
		if (bypass_mode) {
			ret=bypass_state_pwroff(pbpctl_dev);
		} else {
			ret=normal_state_pwroff(pbpctl_dev);
		}
		cmnd_off(pbpctl_dev);
		return ret;
	}	  
}

int get_bypass_pwoff_fn(bpctl_dev_t *pbpctl_dev){
    if (!pbpctl_dev) {
		return -1;
	}
    
    if (!(pbpctl_dev->bp_caps & BP_PWOFF_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_PWOFF;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_EN) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_DIS) {
					ret=0;
				}
			}
		}

		return ret;

	} else {
		return(default_pwroff_status(pbpctl_dev));
	}
}


int set_bypass_pwup_fn(bpctl_dev_t *pbpctl_dev, int bypass_mode){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}


    if (!(pbpctl_dev->bp_caps & BP_PWUP_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BYPASS_PWUP;
		bp_cmd_buf.cmd.cmd_data.bypass_pwup=bypass_mode?BYPASS_PWUP_EN:BYPASS_PWUP_DIS;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
					ret=0;
			}
		}
		msec_delay_bp(EEPROM_WR_DELAY);
		return ret;

	} else {
		if ((ret=cmnd_on(pbpctl_dev))<0) {
			return ret;
		} 
		if (bypass_mode) {
			ret=bypass_state_pwron(pbpctl_dev);
		} else {
			ret=normal_state_pwron(pbpctl_dev);
		}
		cmnd_off(pbpctl_dev);

		return ret;
	}
}

int get_bypass_pwup_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    if (!(pbpctl_dev->bp_caps & BP_PWUP_CTL_CAP)) {
        return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_PWUP;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {  
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_EN) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.bypass_pwoff==BYPASS_PWOFF_DIS) {
					ret=0;
				}
			}
		}
		return ret;

	} else {
		return(default_pwron_status(pbpctl_dev));
	}
}




int set_bypass_wd_fn(bpctl_dev_t *pbpctl_dev, int timeout){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BYPASS_WD;
		bp_cmd_buf.cmd.cmd_data.timeout=htonl(timeout);


		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
					ret = ntohl(bp_rsp_buf.rsp.rsp_data.timeout_set);
					if(ret) {
						pbpctl_dev->wdt_status=WDT_STATUS_EN;
					}
			}
		}
		return ret;

	} else {

		if ((ret=cmnd_on(pbpctl_dev))<0) {
			return ret;
		}
		if (!timeout) {
			ret=wdt_off(pbpctl_dev);
		} else {
			wdt_on(pbpctl_dev,timeout);
			ret = pbpctl_dev->bypass_timer_interval;
		}
		cmnd_off(pbpctl_dev);
		return ret;
	}
}



int get_bypass_wd_fn(bpctl_dev_t *pbpctl_dev, int *timeout){

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));	   
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_WD;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				*timeout = ntohl(bp_rsp_buf.rsp.rsp_data.timeout_set);
				if(*timeout) {
					pbpctl_dev->wdt_status=WDT_STATUS_EN;
				}
				ret=0;
			}
		}
		return ret;

	} else {
		return wdt_programmed(pbpctl_dev, timeout);
	}
}



int get_wd_expire_time_fn(bpctl_dev_t *pbpctl_dev, int *time_left){
#if 0
	bpctl_dev_t *pbpctl_dev_b = NULL;
#endif
	
    if (!pbpctl_dev) {
		return -1;
	}

   if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
	   return BP_NOT_CAP;
   }
	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_WD_EXPIRE_TIME;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				*time_left = ntohl(bp_rsp_buf.rsp.rsp_data.time_left);
				if (*time_left==0) {
					ret=0;
				}
				else {
					ret=1;
				}

			}
		}
		return ret;

	} else {
		return(wdt_timer(pbpctl_dev, time_left));
	}

}

int reset_bypass_wd_timer_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    return(wdt_timer_reload(pbpctl_dev));
}



int get_wd_set_caps_fn(bpctl_dev_t *pbpctl_dev){
    int bp_status=0;
    unsigned int step_value=TIMEOUT_MAX_STEP+1, bit_cnt=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (INTEL_IF_SERIES(pbpctl_dev->subdevice)) {
		return BP_NOT_CAP;
	}
	
   if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
	   return BP_NOT_CAP;
   }

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
        bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_WD_SET_CAPS;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
					ret=ntohl(bp_rsp_buf.rsp.rsp_data.wd_set_caps);
				}

			}
		}
		return ret;

	} else {

		while ((step_value>>=1)) {
			bit_cnt++;
		}

		if (is_bypass_fn(pbpctl_dev)) {
			bp_status= WD_STEP_COUNT_MASK(bit_cnt)|WDT_STEP_TIME|WD_MIN_TIME_MASK(TIMEOUT_UNIT/100);
		} else {
			return -1;
		}

		return bp_status;
	}
}


int set_std_nic_fn(bpctl_dev_t *pbpctl_dev, int nic_mode){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & STD_NIC_CAP)) {
		return BP_NOT_CAP;
	}
	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_STD_NIC;
		bp_cmd_buf.cmd.cmd_data.std_nic=nic_mode?STD_NIC_EN:STD_NIC_DIS;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(BYPASS_CAP_DELAY);
		return ret;

	} else {

		if ((ret=cmnd_on(pbpctl_dev))<0) {
			return ret;
		}
		if (nic_mode) {
			ret=std_nic_on(pbpctl_dev);
		} else {
			ret=std_nic_off(pbpctl_dev);
		}
		cmnd_off(pbpctl_dev);
		return ret;
	}
}


int get_std_nic_fn(bpctl_dev_t *pbpctl_dev){
    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps&STD_NIC_CAP)) {
		return -1;
	}

	 if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_STD_NIC;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.std_nic==STD_NIC_EN) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.std_nic==STD_NIC_DIS) {
					ret=0;
				}
			}
		}
		return ret;

	} else { 
		return(std_nic_status(pbpctl_dev));
	}
}

int set_tap_fn (bpctl_dev_t *pbpctl_dev, int tap_mode){
    if (!pbpctl_dev) {
		return -1;
	}

    if ((pbpctl_dev->bp_caps & TAP_CAP)&&((cmnd_on(pbpctl_dev))>=0)) {
        if (!tap_mode) {
			tap_off(pbpctl_dev);
		} else {
			tap_on(pbpctl_dev);
		}
        cmnd_off(pbpctl_dev);
        return 0;
    }
    return BP_NOT_CAP;
}

int get_tap_fn (bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    return(tap_status(pbpctl_dev));
}

int set_tap_pwup_fn(bpctl_dev_t *pbpctl_dev, int tap_mode){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if ((pbpctl_dev->bp_caps & TAP_PWUP_CTL_CAP)&&((cmnd_on(pbpctl_dev))>=0)) {
        if (tap_mode) {
			ret=tap_state_pwron(pbpctl_dev);
		} else {
			ret=normal_state_pwron(pbpctl_dev);
		}
        cmnd_off(pbpctl_dev);
    } else {
		ret=BP_NOT_CAP;
	}
    return ret;
}

int get_tap_pwup_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if ((ret=default_pwron_tap_status(pbpctl_dev))<0) {
		return ret;
	}
    return((ret==0)?1:0);
}

int get_tap_change_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    return(tap_change_status(pbpctl_dev));
}

int set_dis_tap_fn(bpctl_dev_t *pbpctl_dev, int dis_param){
    int ret=0;

    if (!pbpctl_dev) { 
		return -1;
	}

    if ((pbpctl_dev->bp_caps & TAP_DIS_CAP)&&((cmnd_on(pbpctl_dev))>=0)) {
        if (dis_param) {
			ret=dis_tap_cap(pbpctl_dev);
		} else {
			ret=en_tap_cap(pbpctl_dev);
		}
        cmnd_off(pbpctl_dev);
        return ret;
    } else {
		return BP_NOT_CAP;
	}
}

int get_dis_tap_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    return(dis_tap_cap_status(pbpctl_dev));
}


int set_disc_fn (bpctl_dev_t *pbpctl_dev, int disc_mode){

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & DISC_CAP)) {
		return BP_NOT_CAP;
	}
	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_DISC;
		bp_cmd_buf.cmd.cmd_data.disc_mode=disc_mode?1:0;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(LATCH_DELAY);

		return ret;

	} else {
     
		if ((cmnd_on(pbpctl_dev)>=0)) {
			if (!disc_mode) {
				disc_off(pbpctl_dev);
			} else {
				disc_on(pbpctl_dev);
			}
			cmnd_off(pbpctl_dev);

			return BP_OK;
		}
		return BP_NOT_CAP;
	}
}



int get_disc_fn (bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps&DISC_CAP)) { 
		return -1;
	}

	 if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_DISC;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.disc_mode==1) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.disc_mode==0) {
					ret=0;
				}
			}
		}
		return ret;

	} else {
		ret=disc_status(pbpctl_dev);
	}

    return ret;
}



int set_disc_pwup_fn(bpctl_dev_t *pbpctl_dev, int disc_mode){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & DISC_PWUP_CTL_CAP)) {
		return BP_NOT_CAP;
	}
	
	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;


		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_DISC_PWUP;
		bp_cmd_buf.cmd.cmd_data.disc_pwup=disc_mode?DISC_PWUP_EN:DISC_PWUP_DIS;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(EEPROM_WR_DELAY);
		return ret;

	} else {
		if ((cmnd_on(pbpctl_dev)>=0)) {
			if (disc_mode) {
				ret=disc_state_pwron(pbpctl_dev);
			} else {
				ret=normal_state_pwron(pbpctl_dev);
			}
			cmnd_off(pbpctl_dev);
		} else  {
			ret=BP_NOT_CAP;
		}

		return ret;
	}
}



int get_disc_pwup_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & DISC_PWUP_CTL_CAP)) {
		return BP_NOT_CAP;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_DISC_PWUP;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.disc_pwup==DISC_PWUP_EN) {
					ret=1;
				}
				else if (bp_rsp_buf.rsp.rsp_data.disc_pwup==DISC_PWUP_DIS) {
					ret=0;
				}
			}
		}
		return ret;

	} else	{
		ret=default_pwron_disc_status(pbpctl_dev);
		return(ret==0?1:(ret<0?BP_NOT_CAP:0));
	}
}



int get_disc_change_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps&DISC_CAP)) {
		return -1;
	}

	 if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_DISC_CHANGE;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev)))
				return -1;

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.disc_change==DISC_CHANGED) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.disc_change==DISC_NOT_CHANGED) {
					ret=0;
				}
			}
		}
		return ret;

	} else	{
		ret=disc_change_status(pbpctl_dev);
		return ret;
	}
}


int set_dis_disc_fn(bpctl_dev_t *pbpctl_dev, int dis_param){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps&DISC_DIS_CAP)) {
		return -1;
	}


	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_DIS_DISC;
		bp_cmd_buf.cmd.cmd_data.dis_disc=dis_param?DIS_DISC_DISABLE:DIS_DISC_ENABLE;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(BYPASS_CAP_DELAY);
		return ret;

	} else {

		if ((cmnd_on(pbpctl_dev)>=0)) {
			if (dis_param) {
				ret=dis_disc_cap(pbpctl_dev);
			} else {
				ret=en_disc_cap(pbpctl_dev);
			}
			cmnd_off(pbpctl_dev);
			return ret;
		} else
			return BP_NOT_CAP;
	}
} 

int get_dis_disc_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if (((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) &&
		(pbpctl_dev->bp_caps&DISC_DIS_CAP)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_DIS_DISC;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				if (bp_rsp_buf.rsp.rsp_data.dis_disc==DIS_DISC_DISABLE) {
					ret=1;
				} else if (bp_rsp_buf.rsp.rsp_data.dis_disc==DIS_DISC_ENABLE) {
					ret=0;
				}
			}
		}
		return ret;

	} else	{
		ret=dis_disc_cap_status(pbpctl_dev);
		return ret;
	}
}


int set_disc_port_fn (bpctl_dev_t *pbpctl_dev, int disc_mode){
    int ret=BP_NOT_CAP;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!disc_mode) {
		ret=disc_port_off(pbpctl_dev);
	} else {
		ret=disc_port_on(pbpctl_dev);
	}
    return ret;
}

int get_disc_port_fn (bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

    return(disc_port_status(pbpctl_dev));
}

int set_disc_port_pwup_fn(bpctl_dev_t *pbpctl_dev, int disc_mode){
    int ret=BP_NOT_CAP;

    if (!pbpctl_dev) {
		return -1;
	}

    if (!disc_mode) {
		ret=normal_port_state_pwron(pbpctl_dev);
	} else {
		ret=disc_port_state_pwron(pbpctl_dev);
	}

    return ret;
}

int get_disc_port_pwup_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev)
        return -1;


    if ((ret=default_pwron_disc_port_status(pbpctl_dev))<0) {
		return ret;
	}
    return((ret==0)?1:0);
}


int get_wd_exp_mode_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

    if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
		return BP_NOT_CAP;
	}


	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_WD_EXP_MODE;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=bp_rsp_buf.rsp.rsp_data.wd_exp_mode;
			}
		}
		return ret;

	} else {
		return(wdt_exp_mode_status(pbpctl_dev));
	}
}  



int set_wd_exp_mode_fn(bpctl_dev_t *pbpctl_dev, int param){

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps & WD_CTL_CAP)) {
		return BP_NOT_CAP;
	}


	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_WD_EXP_MODE;
		bp_cmd_buf.cmd.cmd_data.wd_exp_mode=param;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(BYPASS_CAP_DELAY);
		return ret;

	} else {
		return(wdt_exp_mode(pbpctl_dev,param));
	}
}

int reset_cont_fn (bpctl_dev_t *pbpctl_dev){
   int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

    if ((ret=cmnd_on(pbpctl_dev))<0) {
		return ret;
	}
    return(reset_cont(pbpctl_dev));
}


int set_tx_fn(bpctl_dev_t *pbpctl_dev, int tx_state){

    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!pbpctl_dev) {
		return -1;
	}

	if (!(pbpctl_dev->bp_caps&TX_CTL_CAP)) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
		bpctl_dev_t *pbpctl_dev_c, *pbpctl_dev_m;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		if(is_bypass_fn(pbpctl_dev)) {
			pbpctl_dev_m = pbpctl_dev;
		} else {
			pbpctl_dev_m = get_master_port_fn(pbpctl_dev);
		}

		if (!pbpctl_dev_m) {
			return -1;
		}

		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev_m->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_TX;
		bp_cmd_buf.cmd.cmd_data.tx_dis.mode=tx_state?TX_OFF:TX_ON;
		bp_cmd_buf.cmd.cmd_data.tx_dis.port_num=
			(pbpctl_dev_m->func_p == 0)?pbpctl_dev->func_p:(pbpctl_dev->func_p-2);

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		return ret;

	} else {

		if ((pbpctl_dev->bp_caps&TPL_CAP)&&
			(pbpctl_dev->bp_caps&SW_CTL_CAP) ) {
			if ((pbpctl_dev->bp_tpl_flag)) {
				return BP_NOT_CAP;
			}
		} else if ((pbpctl_dev_b=get_master_port_fn(pbpctl_dev))) {
			if ((pbpctl_dev_b->bp_caps&TPL_CAP)&&
				(pbpctl_dev_b->bp_tpl_flag)) {
				return BP_NOT_CAP;
			}
		}
		return(set_tx(pbpctl_dev,tx_state));
	}
}   

int set_bp_force_link_fn(int dev_num, int tx_state){
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    bpctl_dev_curr=&bpctl_dev_arr[dev_num];

    return(set_bp_force_link(bpctl_dev_curr,tx_state));
} 



int set_wd_autoreset_fn(bpctl_dev_t *pbpctl_dev, int param){

    if (!pbpctl_dev) {
		return -1;
	}

    return(set_bypass_wd_auto(pbpctl_dev, param));
}

int get_wd_autoreset_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

    return(get_bypass_wd_auto(pbpctl_dev));
}

#ifdef BP_SELF_TEST
int set_bp_self_test_fn(bpctl_dev_t *pbpctl_dev, int param){
    if (!pbpctl_dev) {
		return -1;
	}
    return(set_bp_self_test(pbpctl_dev, param));
}

int get_bp_self_test_fn(bpctl_dev_t *pbpctl_dev){
    if (!pbpctl_dev) {
		return -1;
	}

    return(get_bp_self_test(pbpctl_dev));
}

#endif


int get_bypass_caps_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
		
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
		bpctl_dev_t *pbpctl_dev_c, *pbpctl_dev_m;
		int ret=-1;
        
		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));

		if(is_bypass_fn(pbpctl_dev)) {
			pbpctl_dev_m = pbpctl_dev;
		} else { 
			pbpctl_dev_m = get_master_port_fn(pbpctl_dev);
		}


		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev_m->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_CAPS;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=(ntohl(bp_rsp_buf.rsp.rsp_data.bypass_caps));
				if ((is_bypass_fn(pbpctl_dev))!=1) {
					int ret1=0;
					if(ret&TX_CTL_CAP) {
						ret1=(TX_CTL_CAP|TX_STATUS_CAP);
					}
					return ret1;
					
				}
			}
		}
		return ret;

	} else {
		return(pbpctl_dev->bp_caps);
	}
}


int get_bypass_caps_ex_fn(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_CAPS_EX;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=ntohl(bp_rsp_buf.rsp.rsp_data.bypass_caps);

			}
		}
		return ret;

	} else {
		return(pbpctl_dev->bp_caps);
	}
}

int get_bypass_slave_fn(bpctl_dev_t *pbpctl_dev,bpctl_dev_t **pbpctl_dev_out){
    int idx_dev=0;

    if (!pbpctl_dev) {
		return -1;
	}
	
	if (pbpctl_dev->bp_10g9) {
		if (pbpctl_dev->func_p == 0) {
			for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p == pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p == pbpctl_dev->slot_p)) {
					if ((pbpctl_dev->func_p == 0)&&
						(bpctl_dev_arr[idx_dev].func_p == 1)) {
						*pbpctl_dev_out = &bpctl_dev_arr[idx_dev];
						return 1;
					}
 				}
			}
			return -1;
		} else {
			return 0;
		}

	} else {

		if ((pbpctl_dev->func_p==0)||(pbpctl_dev->func_p==2)) {
			for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
				if ((bpctl_dev_arr[idx_dev].bus_p==pbpctl_dev->bus_p)&&
					(bpctl_dev_arr[idx_dev].slot_p==pbpctl_dev->slot_p)) {
					if ((pbpctl_dev->func_p==0)&&
						(bpctl_dev_arr[idx_dev].func_p==1)) {
						*pbpctl_dev_out=&bpctl_dev_arr[idx_dev];
						return 1;
					}
					if ((pbpctl_dev->func_p==2)&&
						(bpctl_dev_arr[idx_dev].func_p==3)) {
						*pbpctl_dev_out=&bpctl_dev_arr[idx_dev];
						return 1;
					}
				}
			}
			return -1;
		} else {
			return 0;
		}
	}
}



int get_tx_fn(bpctl_dev_t *pbpctl_dev){
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!pbpctl_dev){
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;
		bpctl_dev_t *pbpctl_dev_c, *pbpctl_dev_m;
		int ret=-1;

		if (!(pbpctl_dev->bp_caps&TX_CTL_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		if(is_bypass_fn(pbpctl_dev)) {
			pbpctl_dev_m = pbpctl_dev;
		} else {
			pbpctl_dev_m = get_master_port_fn(pbpctl_dev);
		}

		if (!pbpctl_dev_m) {
			return -1;
		}

		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev_m->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_data.tx_dis.port_num=
			(pbpctl_dev_m->func_p == 0)?pbpctl_dev->func_p:(pbpctl_dev->func_p-2);

		bp_cmd_buf.cmd.cmd_id=CMD_GET_TX;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) { 
				if (bp_rsp_buf.rsp.rsp_data.tx_dis.mode==TX_ON) {
					ret=0;
				} else if (bp_rsp_buf.rsp.rsp_data.tx_dis.mode==TX_OFF) {
					ret=1;
				}
			}
			else {
				return -1;
			}
		}
		return ret;

	} else {
		if ((pbpctl_dev->bp_caps&TPL_CAP)&&
			(pbpctl_dev->bp_caps&SW_CTL_CAP)) {
			if ((pbpctl_dev->bp_tpl_flag)) {
				return BP_NOT_CAP;
			}
		} else if ((pbpctl_dev_b=get_master_port_fn(pbpctl_dev))) {
			if ((pbpctl_dev_b->bp_caps&TPL_CAP)&&
				(pbpctl_dev_b->bp_tpl_flag)) {
				return BP_NOT_CAP;
			}
		}
		return(tx_status(pbpctl_dev));
	}
}

int get_bp_force_link_fn(int dev_num){
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
        return -1;
	}
    bpctl_dev_curr=&bpctl_dev_arr[dev_num];
    return(bp_force_link_status(bpctl_dev_curr));
}



static int get_bypass_link_status (bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}

    if (pbpctl_dev->media_type == bp_fiber) {
		return((BPCTL_READ_REG(pbpctl_dev, CTRL) & BPCTLI_CTRL_SWDPIN1));
	} else {
		return((BPCTL_READ_REG(pbpctl_dev, STATUS) & BPCTLI_STATUS_LU));
	}

}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
static void bp_tpl_timer_fn(unsigned long param){
    bpctl_dev_t *pbpctl_dev=(bpctl_dev_t *) param;
#else
static void bp_tpl_timer_fn(struct timer_list *t){
    bpctl_dev_t *pbpctl_dev=from_timer(pbpctl_dev, t, bp_tpl_timer);
#endif
    uint32_t link1, link2;
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!(pbpctl_dev_b=get_status_port_fn(pbpctl_dev))) {
		return;
	}

    if (!pbpctl_dev->bp_tpl_flag) {
        set_tx(pbpctl_dev_b,1);
        set_tx(pbpctl_dev,1);
        return;
    }
    link1 = get_bypass_link_status(pbpctl_dev);

    link2 = get_bypass_link_status(pbpctl_dev_b);


    if(link1) {
		pbpctl_dev->tpl_wait = 0;
	}
    if(link2) {
		pbpctl_dev_b->tpl_wait = 0;
	}

		
    if ((link1)&&(tx_status(pbpctl_dev))) {
        if ((!link2)&&(tx_status(pbpctl_dev_b))) {
	     if (pbpctl_dev_b->tpl_wait){
				pbpctl_dev_b->tpl_wait = 0;
				mod_timer(&pbpctl_dev->bp_tpl_timer, jiffies+BP_LINK_MON_DELAY*2*HZ);
				return;
    	    }	
            set_tx(pbpctl_dev,0);
        } else if (!tx_status(pbpctl_dev_b)) {
			pbpctl_dev_b->tpl_wait = 1;
            set_tx(pbpctl_dev_b,1);
        }
    } else if ((!link1)&&(tx_status(pbpctl_dev))) {
        if ((link2)&&(tx_status(pbpctl_dev_b))) {
			if (pbpctl_dev->tpl_wait){
				pbpctl_dev->tpl_wait = 0;
				mod_timer(&pbpctl_dev->bp_tpl_timer, jiffies+BP_LINK_MON_DELAY*2*HZ);
				return;
    	    }
            set_tx(pbpctl_dev_b,0);
        }
    } else if ((link1)&&(!tx_status(pbpctl_dev))) {
        if ((link2)&&(tx_status(pbpctl_dev_b))) {
			pbpctl_dev->tpl_wait = 1;
            set_tx(pbpctl_dev,1);
        }
    } else if ((!link1)&&(!tx_status(pbpctl_dev))) {
        if ((link2)&&(tx_status(pbpctl_dev_b))) {
			pbpctl_dev->tpl_wait = 1;
            set_tx(pbpctl_dev,1);  
        }
    }

    mod_timer(&pbpctl_dev->bp_tpl_timer, jiffies+BP_LINK_MON_DELAY*HZ);
}

int get_tpl_fn(bpctl_dev_t *pbpctl_dev);
void remove_bypass_tpl_auto(bpctl_dev_t *pbpctl_dev){
    bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!pbpctl_dev) {
		return;
	}
    if (pbpctl_dev->bp_caps_ex&TPL2_CAP_EX) {
		return;
	}

    pbpctl_dev_b=get_status_port_fn(pbpctl_dev);

    if (pbpctl_dev->bp_caps&TPL_CAP) {
        if (get_tpl_fn(pbpctl_dev)) {
            del_timer_sync(&pbpctl_dev->bp_tpl_timer);
            pbpctl_dev->bp_tpl_flag=0;
            pbpctl_dev_b=get_status_port_fn(pbpctl_dev);
            if (pbpctl_dev_b) {
				set_tx(pbpctl_dev_b,1);
			}
            set_tx(pbpctl_dev,1);
        }
    }
    return;    
}


int init_bypass_tpl_auto(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) {
		return -1;
	}
    if (pbpctl_dev->bp_caps&TPL_CAP) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
        init_timer(&pbpctl_dev->bp_tpl_timer);
        pbpctl_dev->bp_tpl_timer.function=&bp_tpl_timer_fn;
        pbpctl_dev->bp_tpl_timer.data=(unsigned long)pbpctl_dev;
#else
        timer_setup(&pbpctl_dev->bp_tpl_timer, bp_tpl_timer_fn, 0);
#endif
        return BP_OK;
    }
    return BP_NOT_CAP; 
}

int set_bypass_tpl_auto(bpctl_dev_t *pbpctl_dev, unsigned int param){
    if (!pbpctl_dev)
        return -1;
    if (pbpctl_dev->bp_caps&TPL_CAP) {
        if ((param)&&(!pbpctl_dev->bp_tpl_flag)) {
            pbpctl_dev->bp_tpl_flag=param;
            mod_timer(&pbpctl_dev->bp_tpl_timer, jiffies+1);
            return BP_OK;
        };
        if ((!param)&&(pbpctl_dev->bp_tpl_flag)) {
			remove_bypass_tpl_auto(pbpctl_dev);
		}

        return BP_OK;
    }
    return BP_NOT_CAP; 
}

int get_bypass_tpl_auto(bpctl_dev_t *pbpctl_dev){

    if (!pbpctl_dev) { 
		return -1;
	}

    if (pbpctl_dev->bp_caps&TPL_CAP) {
        return pbpctl_dev->bp_tpl_flag;
    }
    return BP_NOT_CAP; 
}


int set_tpl_fn(bpctl_dev_t *pbpctl_dev, int tpl_mode){
	bpctl_dev_t *pbpctl_dev_b=NULL;

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang))  {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		if (!(pbpctl_dev->bp_caps&TPL_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_TPL;
		bp_cmd_buf.cmd.cmd_data.tpl_mode=tpl_mode?TPL_ON:TPL_OFF;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		return ret;

	} else {
		pbpctl_dev_b=get_status_port_fn(pbpctl_dev);

		if (pbpctl_dev->bp_caps&TPL_CAP) {
			if (tpl_mode) {
				if ((pbpctl_dev_b=get_status_port_fn(pbpctl_dev)))
					set_tx(pbpctl_dev_b,1);
				set_tx(pbpctl_dev,1);
			}
			if ((TPL_IF_SERIES(pbpctl_dev->subdevice))||
				(pbpctl_dev->bp_caps_ex&TPL2_CAP_EX)) {
				pbpctl_dev->bp_tpl_flag=tpl_mode;
				if (!tpl_mode) {
					tpl_hw_off(pbpctl_dev);
				} else {
					tpl_hw_on(pbpctl_dev);
				}
			} else {
				set_bypass_tpl_auto(pbpctl_dev, tpl_mode);
			}
			return 0;
		}
		return BP_NOT_CAP;
	}
}


int get_tpl_fn(bpctl_dev_t *pbpctl_dev){
    int ret=BP_NOT_CAP;

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;
		
		if (!(pbpctl_dev->bp_caps&TPL_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_TPL;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
            if (bp_rsp_buf.rsp.rsp_data.tpl_mode==TPL_ON) {
				ret=1;
			} else if (bp_rsp_buf.rsp.rsp_data.tpl_mode==TPL_OFF) {
				ret=0;
			}
		}
		return ret;

	} else {
		if (pbpctl_dev->bp_caps&TPL_CAP) {
			if (pbpctl_dev->bp_caps_ex&TPL2_CAP_EX) {
				return(tpl2_flag_status(pbpctl_dev));
			}
			ret=pbpctl_dev->bp_tpl_flag;
		}
	}
    return ret;
}



//#ifdef PMC_FIX_FLAG
int set_bp_wait_at_pwup_fn(bpctl_dev_t *pbpctl_dev, int bp_wait_at_pwup){

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		if (!(pbpctl_dev->bp_caps&BP_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BP_WAIT_AT_PWUP;
		bp_cmd_buf.cmd.cmd_data.bp_wait_at_pwup=bp_wait_at_pwup?1:0;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(EEPROM_WR_DELAY);
		return ret;

	} else {
		if (pbpctl_dev->bp_caps & SW_CTL_CAP) {
			//bp_lock(pbp_device_block);
			cmnd_on(pbpctl_dev);
			if (!bp_wait_at_pwup) {
				bp_wait_at_pwup_dis(pbpctl_dev);
			} else {
				bp_wait_at_pwup_en(pbpctl_dev);
			}
			cmnd_off(pbpctl_dev);
			// bp_unlock(pbp_device_block);
			return BP_OK;
		}
		return BP_NOT_CAP;
	}
}



int get_bp_wait_at_pwup_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		if (!(pbpctl_dev->bp_caps&BP_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BP_WAIT_AT_PWUP;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
            if (bp_rsp_buf.rsp.rsp_data.bp_wait_at_pwup==1) {
				ret=1;
			} else if (bp_rsp_buf.rsp.rsp_data.bp_wait_at_pwup==0) {
				ret=0;
			}
		}
		return ret;

	} else {
		// bp_lock(pbp_device_block);
		ret=bp_wait_at_pwup_status(pbpctl_dev);
		// bp_unlock(pbp_device_block);
		return ret;
	}

}


int set_bp_hw_reset_fn(bpctl_dev_t *pbpctl_dev, int bp_hw_reset){

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;
        
		if (!(pbpctl_dev->bp_caps&BP_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BP_HW_RESET;
		bp_cmd_buf.cmd.cmd_data.bp_hw_reset=bp_hw_reset?1:0;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		}
		msec_delay_bp(EEPROM_WR_DELAY);
		return ret;

	} else {

		if (pbpctl_dev->bp_caps & SW_CTL_CAP) {
			//   bp_lock(pbp_device_block);
			cmnd_on(pbpctl_dev);

			if (!bp_hw_reset) {
				bp_hw_reset_dis(pbpctl_dev);
			} else {
				bp_hw_reset_en(pbpctl_dev);
			}
			cmnd_off(pbpctl_dev);
			//    bp_unlock(pbp_device_block);
			return BP_OK;
		}
		return BP_NOT_CAP;
	}
}



int get_bp_hw_reset_fn(bpctl_dev_t *pbpctl_dev){
    int ret=0;

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		ret=-1;
        
		if (!(pbpctl_dev->bp_caps&BP_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BP_HW_RESET;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
            if (bp_rsp_buf.rsp.rsp_data.bp_hw_reset==1) {
				ret=1;
			} else if (bp_rsp_buf.rsp.rsp_data.bp_hw_reset==0) {
				ret=0;
			}
		}
		return ret;

	} else {

		//bp_lock(pbp_device_block);
		ret=bp_hw_reset_status(pbpctl_dev);

		//bp_unlock(pbp_device_block);

		return ret;
	}
}
//#endif  /*PMC_FIX_FLAG*/




int get_bypass_info_fn(bpctl_dev_t *pbpctl_dev, char *dev_name, char *add_param){

    if (!pbpctl_dev) {
		return -1;
	}

    if (!is_bypass_fn(pbpctl_dev)) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		int ret=-1;

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_GET_BYPASS_INFO;

		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
				strcpy(dev_name,pbpctl_dev->name);
				*add_param = bp_rsp_buf.rsp.rsp_data.bypass_info.fw_ver;
				ret=0;
		}
		return ret;

	} else {
		strcpy(dev_name,pbpctl_dev->name);
		*add_param= pbpctl_dev->bp_fw_ver;
		return 0;
	}
}


int get_dev_idx_bsf(int bus, int slot, int func){
    int idx_dev=0;
    //if_scan();
    for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
        if ((bus==bpctl_dev_arr[idx_dev].bus) &&
            (slot==bpctl_dev_arr[idx_dev].slot) &&
            (func==bpctl_dev_arr[idx_dev].func) ) {
			return idx_dev;
		}
    }
    return -1;
}


int set_bp_manuf_fn(bpctl_dev_t *pbpctl_dev){
    int ret=-1;

    if (!pbpctl_dev) {
		return -1;
	}

	if ((pbpctl_dev->bp_40g) || (pbpctl_dev->bp_rang)) {
        bp_cmd_t bp_cmd_buf;
		bp_cmd_rsp_t bp_rsp_buf;

		bpctl_dev_t *pbpctl_dev_c;
		ret=-1;
        
		if (!(pbpctl_dev->bp_caps&BP_CAP)) {
			return -1;
		}

		memset(&bp_cmd_buf, 0, sizeof(bp_cmd_buf));
		memset(&bp_rsp_buf, 0, sizeof(bp_cmd_rsp_t));
		bp_cmd_buf.cmd.cmd_dev_num= (pbpctl_dev->func_p == 0)? 0:1;
		bp_cmd_buf.cmd.cmd_id=CMD_SET_BP_MANUF;
		if (!(pbpctl_dev_c=get_status_port_fn(pbpctl_dev))) {
			return -1;
		}

		if(bp_cmd_request(pbpctl_dev_c, &bp_cmd_buf, &bp_rsp_buf)) {
			if (bp_rsp_buf.rsp.rsp_id == BP_ERR_OK) {
				ret=0;
			}
		} 
        msec_delay_bp(BYPASS_CAP_DELAY);
		return ret;

	} else {

		//bp_lock(pbp_device_block);

		//bp_unlock(pbp_device_block);

		return ret;
	}
}






static int get_dev_idx(int ifindex){
    int idx_dev=0;

    for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
        if (ifindex==bpctl_dev_arr[idx_dev].ifindex) {
			return idx_dev;
		}
    }

    return -1;
}

static bpctl_dev_t *get_dev_idx_p(int ifindex){
    int idx_dev=0;

    for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
        if (ifindex==bpctl_dev_arr[idx_dev].ifindex) {
			return &bpctl_dev_arr[idx_dev];
		}
    }

    return NULL;
}


static void if_scan_init(void)
{
	struct net_device *dev;

	/* rcu_read_lock(); */
	/* rtnl_lock();     */
	/* rcu_read_lock(); */
#if (LINUX_VERSION_CODE >= 0x020618)
    	for_each_netdev(&init_net, dev)
#elif (LINUX_VERSION_CODE >= 0x20616)
	for_each_netdev(dev)
#else
    	for (dev = dev_base; dev; dev = dev->next)
#endif 

	{
		int idx_dev;

		if (bp_get_dev_idx_bsf(dev, &idx_dev))
			continue;

		if (idx_dev == -1)
			continue;

		bpctl_dev_arr[idx_dev].ifindex = dev->ifindex;
		bpctl_dev_arr[idx_dev].ndev = dev;
	}
	/* rtnl_unlock();     */
	/* rcu_read_unlock(); */
}


#ifdef BPVM_KVM

#include "bp_cmd_vm.h"

static struct net_device *netdev;

static char str[8] = {0x00,0xe0, 0xed, 0x18, 0x75, 0x8a}; 

char bpvm_driver_name[] = "bpctl_mod";
const char bpvm_driver_version[] = BP_MOD_VER;


struct bpvm_priv {
    struct net_device *netdev; 
    bp_cmd_vm_t bp_cmd_vm_buf;
    struct work_struct bpvm_task;
    atomic_t  bpvm_busy;
};


void bpvm_setup(struct net_device *dev)
{
    /* dev->header_ops         = &eth_header_ops;
       dev->type               = ARPHRD_ETHER;
       random_ether_addr(dev->dev_addr); */

    memcpy(dev->dev_addr, &str[0], 6);
	ether_setup(dev); 
#ifdef BPVM_VMWARE
    dev->hard_header_len    = ETH_HLEN;
    dev->mtu                = ETH_DATA_LEN;
    dev->addr_len           = ETH_ALEN;
    dev->tx_queue_len       = 1000; /* Ethernet wants good queues */
    dev->flags              = IFF_BROADCAST|IFF_MULTICAST;
    memset(dev->broadcast, 0xFF, ETH_ALEN); 
#endif

}


int bpvm_open(struct net_device *netdev)
{
    /*  struct bpvm_priv  *adapter = netdev_priv(netdev);
     tweak tx_queue_len according to speed/duplex
     and adjust the timeout factor 
     netdev->tx_queue_len = adapter->tx_queue_len; */
    netdev->features &= ~NETIF_F_TSO;
    netif_carrier_on(netdev);
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
    netif_wake_subqueue(netdev, 0);
#endif
    netif_wake_queue(netdev);

    return 0;
}

int bpvm_close(struct net_device *netdev)
{
#ifdef NETIF_F_LLTX
#ifdef CONFIG_NETDEVICES_MULTIQUEUE
    netif_stop_subqueue(netdev, 0);
#else
    netif_stop_queue(netdev);
#endif
#else
    netif_tx_disable(netdev);
#endif
    netif_carrier_off(netdev);
    return 0;
}

 


s32 bpvm_poll_eerd_eewr_done(bpctl_dev_t *pbpctl_dev, int ee_reg)
{
    u32 attempts = 100000;
    u32 i, reg = 0;
    s32 ret_val = -BPCTL_ERR_NVM;

    for (i = 0; i < attempts; i++) {
        if (ee_reg == BPCTL_NVM_POLL_READ) {
			reg = BPCTL_READ_REG(pbpctl_dev, EERD);
		} else  {
			reg = BPCTL_READ_REG(pbpctl_dev, EEWR);
		}

        if (reg & BPCTL_NVM_RW_REG_DONE) {
            ret_val = BPCTL_SUCCESS;
            break;
        }

        usec_delay_bp(5);
    }

    return ret_val;
}



s32 bpvm_read_nvm_eerd(bpctl_dev_t *pbpctl_dev, u16 offset, u16 words, u16 *data)
{
    u32 i, eerd = 0;
    s32 ret_val = BPCTL_SUCCESS;
    for (i = 0; i < words; i++) {
        eerd = ((offset+i) << BPCTL_NVM_RW_ADDR_SHIFT) +
               BPCTL_NVM_RW_REG_START;

        BPCTL_WRITE_REG(pbpctl_dev, EERD, eerd);
        ret_val = bpvm_poll_eerd_eewr_done(pbpctl_dev, BPCTL_NVM_POLL_READ);
        if (ret_val)
            break;

        data[i] = (BPCTL_READ_REG(pbpctl_dev, EERD) >>
                   BPCTL_NVM_RW_REG_DATA);
    }

    return ret_val;
}


static int32_t
bpvm_id_led_init(bpctl_dev_t *pbpctl_dev)
{
    uint32_t ledctl;
    const uint32_t ledctl_mask = 0x000000FF;
    const uint32_t ledctl_on = BPCTLI_LEDCTL_MODE_LED_ON;
    const uint32_t ledctl_off = BPCTLI_LEDCTL_MODE_LED_OFF;
    uint16_t  i, temp;
    const uint16_t led_mask = 0x0F;
    uint16_t eeprom_data;

    if ((pbpctl_dev->bp_10g)||(pbpctl_dev->bp_10g9)||(pbpctl_dev->bp_10gb)) {
		return 0;
	}

    ledctl = BPCTL_READ_REG(pbpctl_dev, LEDCTL);
    pbpctl_dev->ledctl_default = ledctl;
    pbpctl_dev->ledctl_mode1 = pbpctl_dev->ledctl_default;
    pbpctl_dev->ledctl_mode2 = pbpctl_dev->ledctl_default;
#if 1
    if (pbpctl_dev->bp_i80) {


        if (bpvm_read_nvm_eerd(pbpctl_dev, BPCTLI_EEPROM_ID_LED_SETTINGS, 1, &eeprom_data) < 0) {
            printk("bpvm: EEPROM Read Error\n");
            return -1;
        }
        if ((eeprom_data == ID_LED_RESERVED_0000) ||
            (eeprom_data == ID_LED_RESERVED_FFFF)) {
            eeprom_data = /*ID_LED_DEFAULT*/ 0x8918;
        }
    }
    else
#endif    
    eeprom_data = /*ID_LED_DEFAULT*/ 0x8818;
    for (i = 0; i < 4; i++) {
        temp = (eeprom_data >> (i << 2)) & led_mask;
        switch (temp) {
        case ID_LED_ON1_DEF2:
        case ID_LED_ON1_ON2:
        case ID_LED_ON1_OFF2:
            pbpctl_dev->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
            pbpctl_dev->ledctl_mode1 |= ledctl_on << (i << 3);
            break;
        case ID_LED_OFF1_DEF2:
        case ID_LED_OFF1_ON2:
        case ID_LED_OFF1_OFF2:
            pbpctl_dev->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
            pbpctl_dev->ledctl_mode1 |= ledctl_off << (i << 3);
            break;
        default:
            /* Do nothing */
            break;
        }
        switch (temp) {
        case ID_LED_DEF1_ON2:
        case ID_LED_ON1_ON2:
        case ID_LED_OFF1_ON2:
            pbpctl_dev->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
            pbpctl_dev->ledctl_mode2 |= ledctl_on << (i << 3);
            break;
        case ID_LED_DEF1_OFF2:
        case ID_LED_ON1_OFF2:
        case ID_LED_OFF1_OFF2:
            pbpctl_dev->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
            pbpctl_dev->ledctl_mode2 |= ledctl_off << (i << 3);
            break;
        default:
            /* Do nothing */
            break;
        }
    }
    return 0;
} 


s32 bpvm1_led_on(bpctl_dev_t *pbpctl_dev)
{

    BPCTL_WRITE_REG(pbpctl_dev, LEDCTL, pbpctl_dev->ledctl_mode2);

    return 0;
}

s32 bpvm1_led_off(bpctl_dev_t *pbpctl_dev)
{

    BPCTL_WRITE_REG(pbpctl_dev, LEDCTL, pbpctl_dev->ledctl_mode1);
    return 0;
}

/**
 *  ixgbe_led_on_generic - Turns on the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn on
 **/
s32 bpvm10_led_on(bpctl_dev_t *pbpctl_dev)
{
    u32 led_reg = BP10G_READ_REG(pbpctl_dev, LEDCTL);
    /* To turn on the LED, set mode to ON. */
    led_reg &= ~0xf0000;
    led_reg |= 0xe0000;
    BP10G_WRITE_REG(pbpctl_dev, LEDCTL, led_reg);
    BP10G_WRITE_FLUSH(pbpctl_dev);

    return 0;
}

/**
 *  ixgbe_led_off_generic - Turns off the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn off
 **/
s32 bpvm10_led_off(bpctl_dev_t *pbpctl_dev)
{
    u32 led_reg = BP10G_READ_REG(pbpctl_dev, LEDCTL);


    led_reg &= ~0xf0000;
    led_reg |= 0xf0000;


    /* To turn off the LED, set mode to OFF. */
    BP10G_WRITE_REG(pbpctl_dev, LEDCTL, led_reg);      
    BP10G_WRITE_FLUSH(pbpctl_dev);

    return 0;
} 

void bpvm_led_on(bpctl_dev_t *pbpctl_dev){


    if ((pbpctl_dev->bp_10g)||(pbpctl_dev->bp_10g9)) {
		bpvm10_led_on(pbpctl_dev);
	} else {
		bpvm1_led_on(pbpctl_dev);
	}
}



void bpvm_led_off(bpctl_dev_t *pbpctl_dev){


    if ((pbpctl_dev->bp_10g)||(pbpctl_dev->bp_10g9)) {
		bpvm10_led_off(pbpctl_dev);
	} else {
		bpvm1_led_off(pbpctl_dev);
	}
}

int32_t
bpvm_cleanup_led(bpctl_dev_t *pbpctl_dev)
{
    if ((pbpctl_dev->bp_10g)||(pbpctl_dev->bp_10g9)) {
		BP10G_WRITE_REG(pbpctl_dev, LEDCTL, pbpctl_dev->ledctl_default);
	} else {
        BPCTL_WRITE_REG(pbpctl_dev, LEDCTL, pbpctl_dev->ledctl_default);
	}

    return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))    
static void bpvm_led_blink_callback(unsigned long data)
{

    bpctl_dev_t *pbpctl_dev = (bpctl_dev_t *) data;
#else
static void bpvm_led_blink_callback(struct timer_list *t)
{
	bpctl_dev_t *pbpctl_dev = from_timer(pbpctl_dev, t, blink_timer);
#endif

    if (test_and_change_bit(0, (volatile unsigned long *)&pbpctl_dev->led_status)) {
        bpvm_led_off(pbpctl_dev);
    } else {
        bpvm_led_on(pbpctl_dev);
    }

    mod_timer(&pbpctl_dev->blink_timer, jiffies + BPVM_ID_INTERVAL);
}

static void
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
wait_callback(unsigned long data)
{
    bpctl_dev_t *pbpctl_dev = (bpctl_dev_t *) data;
#else
wait_callback(struct timer_list *t)
{
    bpctl_dev_t *pbpctl_dev = from_timer(pbpctl_dev, t, wait_timer);
#endif 

    del_timer_sync(&pbpctl_dev->blink_timer);

    bpvm_led_off(pbpctl_dev);
    clear_bit(0, (volatile unsigned long *)&pbpctl_dev->led_status);
    bpvm_cleanup_led(pbpctl_dev);
}

int32_t
bpvm_setup_led(bpctl_dev_t *pbpctl_dev)
{        
    if ((pbpctl_dev->bp_10g)||(pbpctl_dev->bp_10g9)) { 
        pbpctl_dev->ledctl_default= BP10G_READ_REG(pbpctl_dev, LEDCTL);
    } else {
#if 0
        if (pbpctl_dev->media_type == bp_fiber) {
            ledctl = BPCTL_READ_REG(pbpctl_dev, LEDCTL);
            /* Save current LEDCTL settings */
            pbpctl_dev->ledctl_default = ledctl;
            /* Turn off LED0 */
            ledctl &= ~(BPCTLI_LEDCTL_LED0_IVRT |
                        BPCTLI_LEDCTL_LED0_BLINK |
                        BPCTLI_LEDCTL_LED0_MODE_MASK);
            ledctl |= (BPCTLI_LEDCTL_MODE_LED_OFF <<
                       BPCTLI_LEDCTL_LED0_MODE_SHIFT);


            ledctl &= ~(BPCTLI_LEDCTL_LED1_IVRT |
                        BPCTLI_LEDCTL_LED1_BLINK |
                        BPCTLI_LEDCTL_LED1_MODE_MASK);
            ledctl |= (BPCTLI_LEDCTL_MODE_LED_OFF <<
                       BPCTLI_LEDCTL_LED1_MODE_SHIFT);

            ledctl &= ~(BPCTLI_LEDCTL_LED2_IVRT |
                        BPCTLI_LEDCTL_LED2_BLINK |
                        BPCTLI_LEDCTL_LED2_MODE_MASK);
            ledctl |= (BPCTLI_LEDCTL_MODE_LED_OFF <<
                       BPCTLI_LEDCTL_LED2_MODE_SHIFT);

            ledctl &= ~(BPCTLI_LEDCTL_LED3_IVRT |
                        BPCTLI_LEDCTL_LED3_BLINK |
                        BPCTLI_LEDCTL_LED3_MODE_MASK);
            ledctl |= (BPCTLI_LEDCTL_MODE_LED_OFF <<
                       BPCTLI_LEDCTL_LED3_MODE_SHIFT);
            BPCTL_WRITE_REG(pbpctl_dev, LEDCTL, ledctl);
        } else if (pbpctl_dev->media_type == bp_copper)
#endif
            BPCTL_WRITE_REG(pbpctl_dev, LEDCTL, pbpctl_dev->ledctl_mode1);
    }
    return 0;
} 

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
        init_timer(&pbpctl_dev->bp_tpl_timer);
        pbpctl_dev->bp_tpl_timer.function=&bp_tpl_timer_fn;
        pbpctl_dev->bp_tpl_timer.data=(unsigned long)pbpctl_dev;
#else
        timer_setup(&pbpctl_dev->bp_tpl_timer, bp_tpl_timer_fn, 0);
#endif




static int
bpvm_blink(bpctl_dev_t *pbpctl_dev, uint32_t data)
{   

    if (!data || data > (uint32_t)(MAX_SCHEDULE_TIMEOUT / HZ)) {
		data = (uint32_t)(MAX_SCHEDULE_TIMEOUT / HZ);
	}

    if (pbpctl_dev->blink_timer.function) {
        del_timer_sync(&pbpctl_dev->wait_timer);
        del_timer_sync(&pbpctl_dev->blink_timer);
        bpvm_led_off(pbpctl_dev);
        clear_bit(0, (volatile unsigned long *)&pbpctl_dev->led_status);
        bpvm_cleanup_led(pbpctl_dev);    
    }


    if (!pbpctl_dev->wait_timer.function) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
        init_timer(&pbpctl_dev->wait_timer);
        pbpctl_dev->wait_timer.function = wait_callback;
        pbpctl_dev->wait_timer.data = (unsigned long) pbpctl_dev;
#else
        timer_setup(&pbpctl_dev->wait_timer, wait_callback, 0);
#endif
    }

    if (!pbpctl_dev->blink_timer.function) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0))
        init_timer(&pbpctl_dev->blink_timer);
        pbpctl_dev->blink_timer.function = bpvm_led_blink_callback;
        pbpctl_dev->blink_timer.data = (unsigned long) pbpctl_dev;
#else
        timer_setup(&pbpctl_dev->blink_timer, bpvm_led_blink_callback, 0);
#endif
    }

    bpvm_setup_led(pbpctl_dev);
    mod_timer(&pbpctl_dev->blink_timer, jiffies);
    mod_timer(&pbpctl_dev->wait_timer, jiffies+data*HZ);

    return 0;
} 

int set_bp_blink_fn(int dev_num, int timeout){
    int ret=0;
    static bpctl_dev_t *pbpctl_dev;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    pbpctl_dev=&bpctl_dev_arr[dev_num];

    bpvm_blink(pbpctl_dev,timeout);
    return ret;
}


int get_bp_vmnic_info_fn(int dev_num, char *add_param){
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    bpctl_dev_curr=&bpctl_dev_arr[dev_num];
    add_param[0]=dev_num+1;

    if (bpctl_dev_curr->ndev) {
		memcpy(&add_param[1],bpctl_dev_curr->ndev->name, 12);
	}
    return 0;
}

int get_bp_oem_data_fn(int dev_num, unsigned int key, char *add_param){
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    add_param[0]=dev_num+1;
    if (key!=7335) {
		return 0;
	}
    bpctl_dev_curr=&bpctl_dev_arr[dev_num];
    memcpy(&add_param[1],bpctl_dev_curr->oem_data, 20);
    return 0;
}

int get_bp_pci_info_fn(int dev_num, char *add_param){

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    add_param[0]=dev_num+1;
    add_param[1]= bpctl_dev_arr[dev_num].bus;
    add_param[2]= bpctl_dev_arr[dev_num].slot;
    add_param[3]= bpctl_dev_arr[dev_num].func;
    return 0;
}

int get_bp_all_info_fn(int dev_num, char *add_param){
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}
    bpctl_dev_curr=&bpctl_dev_arr[dev_num];
    add_param[0]=dev_num+1;
    add_param[1]= bpctl_dev_arr[dev_num].bus;
    add_param[2]= bpctl_dev_arr[dev_num].slot;
    add_param[3]= bpctl_dev_arr[dev_num].func;

    if (bpctl_dev_curr->ndev) {
		memcpy(&add_param[4], bpctl_dev_curr->ndev->dev_addr, 6);
	}
    return 0;
}

int get_bypass_slave_fn_vm(int dev_num,int *dev_idx_slave){
    int idx_dev=0;
    static bpctl_dev_t *bpctl_dev_curr;

    if ((dev_num<0)||(dev_num>device_num)||(bpctl_dev_arr[dev_num].pdev==NULL)) {
		return -1;
	}

    bpctl_dev_curr=&bpctl_dev_arr[dev_num];
    if ((bpctl_dev_curr->func_p==0)||(bpctl_dev_curr->func_p==2)) {
        for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
            if ((bpctl_dev_arr[idx_dev].bus_p==bpctl_dev_curr->bus_p)&&
                (bpctl_dev_arr[idx_dev].slot_p==bpctl_dev_curr->slot_p)) {
                if ((bpctl_dev_curr->func_p==0)&&
                    (bpctl_dev_arr[idx_dev].func_p==1)) {
                    *dev_idx_slave= idx_dev;
                    return 1;
                }
                if ((bpctl_dev_curr->func_p==2)&&
                    (bpctl_dev_arr[idx_dev].func_p==3)) {
                    *dev_idx_slave= idx_dev;
                    return 1;
                }
            }
        }
        return -1;
    } else return 0;
}


int get_bypass_slave_sd_vm(int dev_idx){
    int dev_idx_slave=0;

    int ret=get_bypass_slave_fn_vm(dev_idx, &dev_idx_slave);
    if (ret==1) {
		return(dev_idx_slave+1) ;
	}
    return -1;

}



static int bpvm_xmit_frame_ring(struct sk_buff *skb,
                                struct net_device *netdev)
{
    unsigned long flags;
    struct bpvm_priv *adapter = netdev_priv(netdev);
    bp_cmd_vm_t *bp_cmd_vm_buf;
    bp_cmd_vm_rsp_t bp_rsp_buf;
    int dev_idx=0;
    struct sk_buff *skb_tmp;
    unsigned int cmd_vm_id=0;
	static bpctl_dev_t *pbpctl_dev;

    if (unlikely(skb->len <= 0)) {
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }
    if (!spin_trylock_irqsave(&bypass_wr_lock, flags)) {
        /* Collision - tell upper layer to requeue */
        return NETDEV_TX_LOCKED;
    }
    memset(&bp_rsp_buf, 0, sizeof(bp_cmd_vm_rsp_t));


    bp_rsp_buf.rsp.rsp_id = -1;

    if (skb->len == sizeof(bp_cmd_vm_t) ) {
        memset(&adapter->bp_cmd_vm_buf, 0, sizeof(bp_cmd_vm_t));
        memcpy(&adapter->bp_cmd_vm_buf, skb->data, sizeof(bp_cmd_vm_t));

        if (!(memcmp(&adapter->bp_cmd_vm_buf.cmd_vm.str, &str, 6))) {
            dev_kfree_skb_any(skb);
#if 0
            printk("cmd_vm_dev_num=%d\n",adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_dev_num);
            printk("cmd_vm_id=%d\n", adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_id);
            printk("!!!bypass_mode=%d\n",  adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_data);
#endif
            bp_cmd_vm_buf = &adapter->bp_cmd_vm_buf;
            dev_idx = adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_dev_num-1;
            bp_rsp_buf.rsp.rsp_id = 0;
            cmd_vm_id=bp_cmd_vm_buf->cmd_vm.cmd_vm_id;
            if (cmd_vm_id != CMD_VM_GET_BP_DEV_NUM) {
                if (dev_idx>=device_num) {
                    bp_rsp_buf.rsp.rsp_id=-1;
                    printk("bpvm: Device number error!\n");
                    goto bpvm_out;
                }
            }

#ifdef BPVM_VMWARE
            if (!netif_queue_stopped(bpctl_dev_arr[dev_idx].ndev)) {
                printk("bpvm: netif queue started!!!\n");
            } else {
				printk("bpvm: netif queue stopped!!!\n");
			}
            netif_carrier_on(bpctl_dev_arr[dev_idx].ndev);
            netif_tx_start_all_queues(bpctl_dev_arr[dev_idx].ndev);
            netif_carrier_off(bpctl_dev_arr[dev_idx].ndev);
            if (!netif_queue_stopped(bpctl_dev_arr[dev_idx].ndev)) {
                printk("bpvm: netif queue started!!!\n");
            } else {
				printk("bpvm: netif queue stopped!!!\n");
			}
            printk("bpvm: name=%s %lx %x\n", bpctl_dev_arr[dev_idx].ndev->name, bpctl_dev_arr[dev_idx].ndev->state,
                   bpctl_dev_arr[dev_idx].ndev->flags);
            if (unlikely(test_bit(__LINK_STATE_BLOCKED,&bpctl_dev_arr[dev_idx].ndev->state))) {
                printk("bpvm: _LINK_STATE_BLOCKED!!!!!\n");
            }
#endif

			pbpctl_dev=&bpctl_dev_arr[dev_idx];

		
            switch (bp_cmd_vm_buf->cmd_vm.cmd_vm_id) {

            case CMD_VM_SET_BYPASS_PWOFF :
                bp_rsp_buf.rsp.rsp_id= set_bypass_pwoff_fn(pbpctl_dev,  bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break; 

            case CMD_VM_GET_BYPASS_PWOFF :
                bp_rsp_buf.rsp.rsp_data= get_bypass_pwoff_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_BYPASS_PWUP :
                bp_rsp_buf.rsp.rsp_id= set_bypass_pwup_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_BYPASS_PWUP :

                bp_rsp_buf.rsp.rsp_data= get_bypass_pwup_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_BYPASS_WD :
                bp_rsp_buf.rsp.rsp_data= set_bypass_wd_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_SET_BP_BLINK :
                bp_rsp_buf.rsp.rsp_data= set_bp_blink_fn(dev_idx, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;


            case CMD_VM_GET_BYPASS_WD :
                bp_rsp_buf.rsp.rsp_id= get_bypass_wd_fn(pbpctl_dev,(int *)&(bp_rsp_buf.rsp.rsp_data));
                break;

            case CMD_VM_GET_WD_EXPIRE_TIME :
                bp_rsp_buf.rsp.rsp_id= get_wd_expire_time_fn(pbpctl_dev, (int *)&(bp_rsp_buf.rsp.rsp_data));
                break;

            case CMD_VM_RESET_BYPASS_WD_TIMER :
                bp_rsp_buf.rsp.rsp_data= reset_bypass_wd_timer_fn(pbpctl_dev);
                break;

            case CMD_VM_GET_WD_SET_CAPS :
                bp_rsp_buf.rsp.rsp_data= get_wd_set_caps_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_STD_NIC :
                bp_rsp_buf.rsp.rsp_id= set_std_nic_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_STD_NIC :
                bp_rsp_buf.rsp.rsp_data= get_std_nic_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_TAP :
                bp_rsp_buf.rsp.rsp_id= set_tap_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_TAP :
                bp_rsp_buf.rsp.rsp_data= get_tap_fn(pbpctl_dev);
                break;

            case CMD_VM_GET_TAP_CHANGE :
                bp_rsp_buf.rsp.rsp_data= get_tap_change_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_DIS_TAP :
                bp_rsp_buf.rsp.rsp_id= set_dis_tap_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_DIS_TAP :
                bp_rsp_buf.rsp.rsp_data= get_dis_tap_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_TAP_PWUP :
                bp_rsp_buf.rsp.rsp_id= set_tap_pwup_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_TAP_PWUP :
                bp_rsp_buf.rsp.rsp_data= get_tap_pwup_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_WD_EXP_MODE:
                bp_rsp_buf.rsp.rsp_id= set_wd_exp_mode_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_WD_EXP_MODE:
                bp_rsp_buf.rsp.rsp_data= get_wd_exp_mode_fn(pbpctl_dev);
                break;

            case CMD_VM_GET_DIS_BYPASS:
                bp_rsp_buf.rsp.rsp_data= get_dis_bypass_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_DIS_BYPASS:
                bp_rsp_buf.rsp.rsp_id= set_dis_bypass_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_BYPASS_CHANGE:
                bp_rsp_buf.rsp.rsp_data= get_bypass_change_fn(pbpctl_dev);
                break;

            case CMD_VM_GET_BYPASS:
                bp_rsp_buf.rsp.rsp_data= get_bypass_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_BYPASS:
                bp_rsp_buf.rsp.rsp_id= set_bypass_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_BYPASS_CAPS:
                bp_rsp_buf.rsp.rsp_data= get_bypass_caps_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_WD_AUTORESET:
                bp_rsp_buf.rsp.rsp_id= set_wd_autoreset_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);

                break;
            case CMD_VM_GET_WD_AUTORESET:

                bp_rsp_buf.rsp.rsp_data= get_wd_autoreset_fn(pbpctl_dev);
                break;


            case CMD_VM_GET_BYPASS_SLAVE:
                bp_rsp_buf.rsp.rsp_data= get_bypass_slave_sd_vm(dev_idx);
                break;

            case CMD_VM_IS_BYPASS:
                if (adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_dev_num==0) {
                    bp_rsp_buf.rsp.rsp_data= 2;
                } else
                    bp_rsp_buf.rsp.rsp_data= is_bypass_fn(pbpctl_dev);
                break;
            case CMD_VM_SET_TX:
                bp_rsp_buf.rsp.rsp_id= set_tx_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;
            case CMD_VM_GET_TX:
                bp_rsp_buf.rsp.rsp_data= get_tx_fn(pbpctl_dev);
                break;

            case CMD_VM_SET_DISC :
                bp_rsp_buf.rsp.rsp_id=set_disc_fn(pbpctl_dev,bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;


            case CMD_VM_GET_DISC :
                bp_rsp_buf.rsp.rsp_data=get_disc_fn(pbpctl_dev);
                break;
            case CMD_VM_GET_DISC_CHANGE :
                bp_rsp_buf.rsp.rsp_data=get_disc_change_fn(pbpctl_dev);
                break;
            case CMD_VM_SET_DIS_DISC :
                bp_rsp_buf.rsp.rsp_id=set_dis_disc_fn(pbpctl_dev,bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;
            case CMD_VM_GET_DIS_DISC :
                bp_rsp_buf.rsp.rsp_data=get_dis_disc_fn(pbpctl_dev);
                break;
            case CMD_VM_SET_DISC_PWUP :
                bp_rsp_buf.rsp.rsp_id=set_disc_pwup_fn(pbpctl_dev,bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;
            case CMD_VM_GET_DISC_PWUP :
                bp_rsp_buf.rsp.rsp_data=get_disc_pwup_fn(pbpctl_dev);
                break;  
            case CMD_VM_GET_BYPASS_INFO:

                bp_rsp_buf.rsp.rsp_id= get_bypass_info_fn(pbpctl_dev, (char *)&bp_rsp_buf.rsp_info.info, (char *)&bp_rsp_buf.rsp_info.fw_ver);
                break; 
            case CMD_VM_GET_BP_DRV_VER:
                bp_rsp_buf.rsp.rsp_id=0;
                memcpy(&bp_rsp_buf.rsp_info.info, BP_MOD_VER, sizeof(BP_MOD_VER));
                break;

            case CMD_VM_GET_BP_DEV_NUM:

                bp_rsp_buf.rsp.rsp_id= device_num; 
                break; 

            case CMD_VM_GET_BP_VMNIC_INFO:
                if (adapter->bp_cmd_vm_buf.cmd_vm.cmd_vm_dev_num==0) {
                    bp_rsp_buf.rsp_info.info[0]=0;
                    memcpy(&bp_rsp_buf.rsp_info.info[1],netdev->name, 12);
                    bp_rsp_buf.rsp.rsp_id=0;
                } else
                    bp_rsp_buf.rsp.rsp_id= get_bp_vmnic_info_fn(dev_idx, (char *)&bp_rsp_buf.rsp_info.info);
                break; 

            case CMD_VM_GET_BP_OEM_DATA:

                bp_rsp_buf.rsp.rsp_id= get_bp_oem_data_fn(dev_idx, bp_cmd_vm_buf->cmd_vm.cmd_vm_data,
                                                          (char *)&bp_rsp_buf.rsp_info.info);
                break; 

            case CMD_VM_GET_BP_PCI_INFO:

                bp_rsp_buf.rsp.rsp_id= get_bp_pci_info_fn(dev_idx, (char *)&bp_rsp_buf.rsp_info.info);
                break; 

            case CMD_VM_GET_BP_ALL_INFO:
                bp_rsp_buf.rsp.rsp_id= get_bp_all_info_fn(dev_idx, (char *)&bp_rsp_buf.rsp_info.info);
                break;

            case CMD_VM_SET_TPL :
                bp_rsp_buf.rsp.rsp_id= set_tpl_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_TPL :
                bp_rsp_buf.rsp.rsp_data= get_tpl_fn(pbpctl_dev);
                break;
            case CMD_VM_SET_BP_WAIT_AT_PWUP :
                bp_rsp_buf.rsp.rsp_id= set_bp_wait_at_pwup_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_BP_WAIT_AT_PWUP :
                bp_rsp_buf.rsp.rsp_data= get_bp_wait_at_pwup_fn(pbpctl_dev);
                break;
            case CMD_VM_SET_BP_HW_RESET :
                bp_rsp_buf.rsp.rsp_id= set_bp_hw_reset_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);
                break;

            case CMD_VM_GET_BP_HW_RESET :
                bp_rsp_buf.rsp.rsp_data= get_bp_hw_reset_fn(pbpctl_dev);
                break;
#ifdef BP_SELF_TEST
            case CMD_VM_SET_BP_SELF_TEST:
                bp_rsp_buf.rsp.rsp_id= set_bp_self_test_fn(pbpctl_dev, bp_cmd_vm_buf->cmd_vm.cmd_vm_data);

                break;
            case CMD_VM_GET_BP_SELF_TEST:
                bp_rsp_buf.rsp.rsp_data= get_bp_self_test_fn(pbpctl_dev);
                break;
#endif
            default:
                bp_rsp_buf.rsp.rsp_id=-1;
                break;
            /* return -EOPNOTSUPP;*/
            };
            /* schedule_work(&adapter->bpvm_task);	*/
        } else goto bpvm_out1;
    } else goto bpvm_out1;
    bpvm_out:
    /* response->rsp.multiplex_id==request->cmd_vm.multiplex_id)&&
                         (response->rsp.request_id==request->cmd_vm.request_id*/

    skb_tmp=netdev_alloc_skb(netdev,sizeof(bp_rsp_buf)+2); 
    if (skb_tmp) {
        /* memset(&bp_rsp_buf.rsp.str, 0xff, 6);
        memcpy(&bp_rsp_buf.rsp.str1,&str,6); */
        memcpy(&bp_rsp_buf.rsp.str1,&str,6);

        memset(&bp_rsp_buf.rsp.type, 0xba, 2);
        memcpy(&bp_rsp_buf.rsp.str,&adapter->bp_cmd_vm_buf.cmd_vm.str1,6);
        bp_rsp_buf.rsp.multiplex_id= adapter->bp_cmd_vm_buf.cmd_vm.multiplex_id;
        bp_rsp_buf.rsp.request_id= adapter->bp_cmd_vm_buf.cmd_vm.request_id;
        /* memset(&bp_rsp_buf.rsp.str, 0xff, 6);
        bp_rsp_buf.rsp.rsp_id=-1; */
        if (cmd_vm_id!=CMD_VM_GET_BP_DEV_NUM)
            bp_rsp_buf.rsp.rsp_id= bp_rsp_buf.rsp.rsp_id==0?BP_ERR_OK:BP_ERR_NOT_CAP;
        memcpy(skb_put(skb_tmp,sizeof(bp_rsp_buf)),&bp_rsp_buf,sizeof(bp_cmd_vm_rsp_t));

        skb_tmp->protocol=eth_type_trans(skb_tmp,netdev);
        skb_tmp->ip_summed=CHECKSUM_UNNECESSARY;
        /* netif_rx(skb_tmp); */
        netif_receive_skb(skb_tmp);
    }

    /* schedule_work(&adapter->bpvm_task); */
    spin_unlock_irqrestore(&bypass_wr_lock, flags);
    return NETDEV_TX_OK;   
    bpvm_out1:
    dev_kfree_skb_any(skb);
    spin_unlock_irqrestore(&bypass_wr_lock, flags);
    return NETDEV_TX_OK;
}

int bpvm_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
    return (bpvm_xmit_frame_ring(skb, netdev));
}


int bpvm_change_mtu(struct net_device *netdev, int new_mtu)
{ 
    if ((new_mtu < 68)) {
        printk("bpvm: Invalid MTU setting\n");
        return -EINVAL;
    }

    netdev->mtu = new_mtu;
    return 0;
}


static int bpvm_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd_vm)
{
    switch (cmd_vm) {
/*#ifdef ETHTOOL_OPS_COMPAT
    case SIOCETHTOOL:
        return ethtool_ioctl(ifr);
#endif*/
    default:
        return -EOPNOTSUPP;
    }
}

static void bpvm_work_task(struct work_struct *work){
    bp_cmd_vm_t *bp_cmd_vm_buf;

    bp_cmd_vm_rsp_t bp_rsp_buf;
    struct bpvm_priv *adapter;
    /* int dev_idx; */
    struct sk_buff *skb_tmp;


    adapter = container_of(work, struct bpvm_priv, bpvm_task);

    bp_cmd_vm_buf=&adapter->bp_cmd_vm_buf;


    skb_tmp=netdev_alloc_skb(netdev,sizeof(bp_rsp_buf)+2); 
    if (skb_tmp) {
        memset(&bp_rsp_buf, 0xff, sizeof(bp_rsp_buf));
        /* memset(skb_put(skb_tmp,sizeof(bp_rsp_buf)),0xff,sizeof(bp_rsp_buf)); */
        memcpy(skb_put(skb_tmp,sizeof(bp_rsp_buf)),&bp_rsp_buf,sizeof(bp_cmd_vm_rsp_t));

        skb_tmp->protocol=eth_type_trans(skb_tmp,netdev);


        skb_tmp->ip_summed=CHECKSUM_UNNECESSARY;
        netif_rx(skb_tmp);
    }

}


void bpvm_set_rx_mode(struct net_device *netdev)
{
}

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29) )
static const struct net_device_ops bpvm_netdev_ops = {
	.ndo_open		= bpvm_open,
	.ndo_stop		= bpvm_close,
	.ndo_start_xmit	= bpvm_xmit_frame,
    .ndo_change_mtu = bpvm_change_mtu, 
	.ndo_do_ioctl	= bpvm_ioctl,
	.ndo_set_rx_mode = bpvm_set_rx_mode,
};
#endif


static u32 bpvm_get_link(struct net_device *netdev){
    return 1;
}

static int bpvm_get_settings(struct net_device *netdev,
                             struct ethtool_cmd *ecmd)
{
    ecmd->speed = 100;
	ecmd->duplex = 1;

    return 0;
} 

static void bpvm_get_drvinfo(struct net_device *netdev,
                             struct ethtool_drvinfo *drvinfo)
{  
    strncpy(drvinfo->driver,  bpvm_driver_name, 32);
    strncpy(drvinfo->version, bpvm_driver_version, 32);
    strcpy(drvinfo->fw_version, "N/A");
}


static struct ethtool_ops bpvm_ethtool_ops = {
    .get_settings           = bpvm_get_settings,
    .get_drvinfo            = bpvm_get_drvinfo,
    /* .get_link               = ethtool_op_get_link, */
    .get_link               = bpvm_get_link,

};


static inline struct device *pci_dev_to_dev(struct pci_dev *pdev)
{
    return &pdev->dev;
}

void bpvm_set_ethtool_ops(struct net_device *netdev)
{
    SET_ETHTOOL_OPS(netdev, &bpvm_ethtool_ops);
}  

int bpvm_probe(void)
{
    struct bpvm_priv *adapter;
    int err;
    char dev_name[16];
    err =  -EIO;

    sprintf(dev_name,"bpvm0");
    netdev = alloc_netdev(sizeof(struct bpvm_priv), dev_name, bpvm_setup);
    /* netdev = alloc_etherdev(sizeof(struct bpvm_priv)); */

    if (!netdev) {
		return 0;
	}
    memcpy(netdev->dev_addr, &str[0], 6);

    /* SET_MODULE_OWNER(netdev); */
    SET_NETDEV_DEV(netdev, pci_dev_to_dev(bpctl_dev_arr[0].pdev));

    adapter = netdev_priv(netdev);
    adapter->netdev = netdev;

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29) )
	netdev->netdev_ops = &bpvm_netdev_ops;
#else /* HAVE_NET_DEVICE_OPS */
	netdev->open = &bpvm_open; 
    netdev->stop = &bpvm_close;
    netdev->hard_start_xmit = &bpvm_xmit_frame; 
#endif
    
    /* memcpy(&request->cmd.str[0], &str, 6);
    memcpy(netdev->dev_addr, &str[0], 6); */
	{
		int i;

		printk(KERN_INFO "bpvm addr: ");
		for (i = 0; i < 6; i++)
			printk("%2.2x%c",
					netdev->dev_addr[i], i == 5 ? '\n' : ':');
	}

    /* netdev->get_stats = &bpvm_get_stats; 
    netdev->set_multicast_list = &bpvm_set_multi; 
    netdev->set_mac_address = &bpvm_set_mac; */ 
    netdev->mtu=1500;
    /* netdev->do_ioctl = &bpvm_ioctl;  */

    bpvm_set_ethtool_ops(netdev);

    INIT_WORK(&adapter->bpvm_task, bpvm_work_task);


    /* tell the stack to leave us alone until bpvm_open() is called */
    netif_carrier_off(netdev);
    netif_stop_queue(netdev);

#if !defined(__VMKLNX__)
   /* strcpy(netdev->name, "eth%d"); */
#else /* defined(__VMKLNX__) */
   /*    strcpy(netdev->name, " "); */
#endif /* !defined(__VMKLNX__) */

	printk(KERN_INFO "bpvm netdev name %s\n", netdev->name);
    err = register_netdev(netdev);
    if (err) {
		return 0;
	}

#ifdef NETIF_F_LLTX
    netdev->features |= NETIF_F_LLTX;
#else

#endif 

    /* bpvm_pkt_type.type=cpu_to_be16(ETH_P_ARP);
    bpvm_pkt_type.func=bpvm_packet_rcv;
    __dev_remove_pack(&bpvm_pkt_type); */

#ifdef BPVM_SEP_VER
    {
        struct net_device *dev_c=NULL;
        int idx_dev=0;
        rtnl_lock();


        /* Find Teton devices */
#if (LINUX_VERSION_CODE >= 0x020618)
        for_each_netdev(&init_net, dev_c)
#elif (LINUX_VERSION_CODE >= 0x20616)
        for_each_netdev(dev)
#else
        for (dev_c = dev_base; dev_c; dev_c = dev_c->next)
#endif
        {
            //printk("!!!!bpvm: dev_c->name=%s\n", dev_c->name);
            for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) {
                if (!(pci_dev_driver(bpctl_dev_arr[idx_dev].pdev)))
                    continue;
                if (bpctl_dev_arr[idx_dev].pdev== dev_c->pdev) {
                    printk("bpvm: %s is bypass adapter!\n", dev_c->name);
                    bpctl_dev_arr[idx_dev].ndev=dev_c;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
                    if (dev_c->hard_start_xmit) {
                        bpctl_dev_arr[idx_dev].hard_start_xmit_save=dev_c->hard_start_xmit; 
                        // printk("bpvm: module_id =%x\n", dev_c->module_id);
                        dev_c->hard_start_xmit=bpvms_hard_start_xmit;   
                    }
#else
                    if (pbpctl_dev_sl->ndev->netdev_ops) {

                        pbpctl_dev_sl->old_ops = pbpctl_dev_sl->ndev->netdev_ops;
                        pbpctl_dev_sl->new_ops = *pbpctl_dev_sl->old_ops;
                        pbpctl_dev_sl->new_ops.ndo_start_xmit = bpvms_hard_start_xmit;
                        pbpctl_dev_sl->ndev->netdev_ops = &pbpctl_dev_sl->new_ops;

                    }
#endif

                }
            }


        }
        rtnl_unlock();
    }
#endif /* BPVM_SEP_VER */    

    return 0;

}

#endif






#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30))
static int device_ioctl(struct inode *inode, /* see include/linux/fs.h */
                        struct file *file, /* ditto */
                        unsigned int ioctl_num, /* number and param for ioctl */
                        unsigned long ioctl_param)
#else
static long device_ioctl(struct file *file, /* ditto */
                         unsigned int ioctl_num, /* number and param for ioctl */
                         unsigned long ioctl_param)

#endif
{
    struct bpctl_cmd bpctl_cmd;
    int dev_idx=0;
    bpctl_dev_t *pbpctl_dev_out;
    void __user *argp = (void __user *)ioctl_param; 
    int ret=0;
    bpctl_dev_t *pbpctl_dev;


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
    //lock_kernel();
#endif
    lock_bpctl();

/*
* Switch according to the ioctl called
*/
    if (ioctl_num==IOCTL_TX_MSG(IF_SCAN)) {
        if_scan_init();
        ret=SUCCESS;
        goto bp_exit;
    }
    if (copy_from_user(&bpctl_cmd, argp, sizeof(struct bpctl_cmd))) {

        ret= -EFAULT; 
        goto bp_exit;
    }

    if (ioctl_num==IOCTL_TX_MSG(GET_DEV_NUM)) {
        bpctl_cmd.out_param[0]= device_num;
        if (copy_to_user(argp,(void *)&bpctl_cmd,sizeof(struct bpctl_cmd))) {
            ret=-EFAULT;
            goto bp_exit;
        }
        ret=SUCCESS;
        goto bp_exit;

    }

    if (ioctl_num==IOCTL_TX_MSG(RESET_BYPASS_WD_TIMER)) {
		if (bpctl_cmd.in_param[0]==-1) {
			int idx_dev=0;

            for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL));
				 idx_dev++) {
				if (is_bypass_fn(&bpctl_dev_arr[idx_dev])) {
					reset_bypass_wd_timer_fn(&bpctl_dev_arr[idx_dev]);
				}
			}
			bpctl_cmd.status=1;
			goto bpcmd_exit;
		}
    }

    if ((bpctl_cmd.in_param[5])||
        (bpctl_cmd.in_param[6])||
        (bpctl_cmd.in_param[7]))
        dev_idx=get_dev_idx_bsf(bpctl_cmd.in_param[5],
                                bpctl_cmd.in_param[6],
                                bpctl_cmd.in_param[7]);
    else if (bpctl_cmd.in_param[1]==0)
        dev_idx= bpctl_cmd.in_param[0];
    else dev_idx=get_dev_idx(bpctl_cmd.in_param[1]);

    if (dev_idx<0||dev_idx>device_num) {
        ret= -EOPNOTSUPP;
        goto bp_exit;
    }

    bpctl_cmd.out_param[0]= bpctl_dev_arr[dev_idx].bus;
    bpctl_cmd.out_param[1]= bpctl_dev_arr[dev_idx].slot;
    bpctl_cmd.out_param[2]= bpctl_dev_arr[dev_idx].func;
    bpctl_cmd.out_param[3]= bpctl_dev_arr[dev_idx].ifindex;

    if ((bpctl_dev_arr[dev_idx].bp_10gb)&&(!(bpctl_dev_arr[dev_idx].ifindex))) {
        printk("Please load network driver for %s adapter!\n",bpctl_dev_arr[dev_idx].name);
        bpctl_cmd.status=-1;
        ret= SUCCESS;
        goto bp_exit;

    }
    if ((bpctl_dev_arr[dev_idx].bp_10gb)&&(bpctl_dev_arr[dev_idx].ndev)) {
        if (!(bpctl_dev_arr[dev_idx].ndev->flags&IFF_UP)) {
            if (!(bpctl_dev_arr[dev_idx].ndev->flags&IFF_UP)) {
                printk("Please bring up network interfaces for %s adapter!\n",
                       bpctl_dev_arr[dev_idx].name);
                bpctl_cmd.status=-1; 
                ret= SUCCESS;
                goto bp_exit;
            }

        }
    }

    if ((dev_idx<0)||(dev_idx>device_num)||(bpctl_dev_arr[dev_idx].pdev==NULL)) {
        bpctl_cmd.status=-1;
        goto bpcmd_exit;
    }

    pbpctl_dev=&bpctl_dev_arr[dev_idx]; 



    switch (ioctl_num) {
    case IOCTL_TX_MSG(SET_BYPASS_PWOFF) :
        bpctl_cmd.status= set_bypass_pwoff_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BYPASS_PWOFF) :
        bpctl_cmd.status= get_bypass_pwoff_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_BYPASS_PWUP) :
        bpctl_cmd.status= set_bypass_pwup_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BYPASS_PWUP) :
        bpctl_cmd.status= get_bypass_pwup_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_BYPASS_WD) :
        bpctl_cmd.status= set_bypass_wd_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BYPASS_WD) :
        bpctl_cmd.status= get_bypass_wd_fn(pbpctl_dev,(int *)&(bpctl_cmd.data[0]));
        break;

    case IOCTL_TX_MSG(GET_WD_EXPIRE_TIME) :
        bpctl_cmd.status= get_wd_expire_time_fn(pbpctl_dev, (int *)&(bpctl_cmd.data[0]));
        break;

    case IOCTL_TX_MSG(RESET_BYPASS_WD_TIMER) :
        bpctl_cmd.status= reset_bypass_wd_timer_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(GET_WD_SET_CAPS) :
        bpctl_cmd.status= get_wd_set_caps_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_STD_NIC) :
        bpctl_cmd.status= set_std_nic_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_STD_NIC) :
        bpctl_cmd.status= get_std_nic_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_TAP) :
        bpctl_cmd.status= set_tap_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_TAP) :
        bpctl_cmd.status= get_tap_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(GET_TAP_CHANGE) :
        bpctl_cmd.status= get_tap_change_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_DIS_TAP) :
        bpctl_cmd.status= set_dis_tap_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_DIS_TAP) :
        bpctl_cmd.status= get_dis_tap_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_TAP_PWUP) :
        bpctl_cmd.status= set_tap_pwup_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_TAP_PWUP) :
        bpctl_cmd.status= get_tap_pwup_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_WD_EXP_MODE):
        bpctl_cmd.status= set_wd_exp_mode_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_WD_EXP_MODE):
        bpctl_cmd.status= get_wd_exp_mode_fn(pbpctl_dev);
        break;

    case  IOCTL_TX_MSG(GET_DIS_BYPASS):
        bpctl_cmd.status= get_dis_bypass_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_DIS_BYPASS):
        bpctl_cmd.status= set_dis_bypass_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BYPASS_CHANGE):
        bpctl_cmd.status= get_bypass_change_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(GET_WD_EXPIRE):
        bpctl_cmd.status= get_wd_expire_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(GET_BYPASS):
        bpctl_cmd.status= get_bypass_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_BYPASS):
        bpctl_cmd.status= set_bypass_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BYPASS_CAPS):
        bpctl_cmd.status= get_bypass_caps_fn(pbpctl_dev);
        if (copy_to_user(argp, (void *)&bpctl_cmd, sizeof(struct bpctl_cmd))) {
            ret= -EFAULT;   
            goto bp_exit;
        }
        goto bp_exit;

    case IOCTL_TX_MSG(GET_BYPASS_SLAVE):
        bpctl_cmd.status= get_bypass_slave_fn(pbpctl_dev, &pbpctl_dev_out);
        if (bpctl_cmd.status==1) {
            bpctl_cmd.out_param[4]= pbpctl_dev_out->bus;
            bpctl_cmd.out_param[5]= pbpctl_dev_out->slot;
            bpctl_cmd.out_param[6]= pbpctl_dev_out->func;
            bpctl_cmd.out_param[7]= pbpctl_dev_out->ifindex;
        }
        break;

    case IOCTL_TX_MSG(IS_BYPASS):
        bpctl_cmd.status= is_bypass_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_TX):
        bpctl_cmd.status= set_tx_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;
    case IOCTL_TX_MSG(GET_TX):
        bpctl_cmd.status= get_tx_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_WD_AUTORESET):
        bpctl_cmd.status= set_wd_autoreset_fn(pbpctl_dev, bpctl_cmd.in_param[2]);

        break;
    case IOCTL_TX_MSG(GET_WD_AUTORESET):

        bpctl_cmd.status= get_wd_autoreset_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_DISC) :
        bpctl_cmd.status=set_disc_fn(pbpctl_dev,bpctl_cmd.in_param[2]);
        break;
    case IOCTL_TX_MSG(GET_DISC) :
        bpctl_cmd.status=get_disc_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(GET_DISC_CHANGE) :
        bpctl_cmd.status=get_disc_change_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_DIS_DISC) :
        bpctl_cmd.status=set_dis_disc_fn(pbpctl_dev,bpctl_cmd.in_param[2]);
        break;
    case IOCTL_TX_MSG(GET_DIS_DISC) :
        bpctl_cmd.status=get_dis_disc_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_DISC_PWUP) :
        bpctl_cmd.status=set_disc_pwup_fn(pbpctl_dev,bpctl_cmd.in_param[2]);
        break;
    case IOCTL_TX_MSG(GET_DISC_PWUP) :
        bpctl_cmd.status=get_disc_pwup_fn(pbpctl_dev);
        break;  

    case IOCTL_TX_MSG(GET_BYPASS_INFO):

        bpctl_cmd.status= get_bypass_info_fn(pbpctl_dev, (char *)&bpctl_cmd.data, (char *)&bpctl_cmd.out_param[4]);
        break;

    case IOCTL_TX_MSG(SET_TPL) :
        bpctl_cmd.status= set_tpl_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_TPL) :
        bpctl_cmd.status= get_tpl_fn(pbpctl_dev);
        break;
//#ifdef PMC_FIX_FLAG
    case IOCTL_TX_MSG(SET_BP_WAIT_AT_PWUP) :
        bpctl_cmd.status= set_bp_wait_at_pwup_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BP_WAIT_AT_PWUP) :
        bpctl_cmd.status= get_bp_wait_at_pwup_fn(pbpctl_dev);
        break;
    case IOCTL_TX_MSG(SET_BP_HW_RESET) :
        bpctl_cmd.status= set_bp_hw_reset_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BP_HW_RESET) :
        bpctl_cmd.status= get_bp_hw_reset_fn(pbpctl_dev);
        break;
//#endif
#ifdef BP_SELF_TEST
    case IOCTL_TX_MSG(SET_BP_SELF_TEST):
        bpctl_cmd.status= set_bp_self_test_fn(pbpctl_dev, bpctl_cmd.in_param[2]);

        break;
    case IOCTL_TX_MSG(GET_BP_SELF_TEST):
        bpctl_cmd.status= get_bp_self_test_fn(pbpctl_dev);
        break;


#endif
#if 0
    case IOCTL_TX_MSG(SET_DISC_PORT) :
        bpctl_cmd.status= set_disc_port_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_DISC_PORT) :
        bpctl_cmd.status= get_disc_port_fn(pbpctl_dev);
        break;

    case IOCTL_TX_MSG(SET_DISC_PORT_PWUP) :
        bpctl_cmd.status= set_disc_port_pwup_fn(pbpctl_dev, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_DISC_PORT_PWUP) :
        bpctl_cmd.status= get_disc_port_pwup_fn(pbpctl_dev);
        break;
#endif
    case IOCTL_TX_MSG(SET_BP_FORCE_LINK):
        bpctl_cmd.status= set_bp_force_link_fn(dev_idx, bpctl_cmd.in_param[2]);
        break;

    case IOCTL_TX_MSG(GET_BP_FORCE_LINK):
        bpctl_cmd.status= get_bp_force_link_fn(dev_idx);
        break;

    case IOCTL_TX_MSG(SET_BP_MANUF) :
        bpctl_cmd.status= set_bp_manuf_fn(pbpctl_dev);
        break;
 

    default:

        ret= -EOPNOTSUPP;
        goto bp_exit;
    }
    bpcmd_exit:
    if (copy_to_user(argp, (void *)&bpctl_cmd, sizeof(struct bpctl_cmd)))
        ret= -EFAULT;
    ret= SUCCESS;
    bp_exit:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
#endif
    unlock_bpctl();
    return ret;
}


struct file_operations Fops = {
    .owner = THIS_MODULE,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)) 
    .ioctl = device_ioctl,
#else
    .unlocked_ioctl = device_ioctl,
#endif

    .open = device_open,
    .release = device_release, /* a.k.a. close */
};

#ifndef PCI_DEVICE
    #define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID
#endif


#define SILICOM_BPCTLBP_ETHERNET_DEVICE(device_id) {\
	PCI_DEVICE(SILICOM_VID, device_id)}


typedef enum {
    PXG2BPFI,
    PXG2BPFIL,
    PXG2BPFILX,
    PXG2BPFILLX,
    PXGBPI,
    PXGBPIG,
    PXG2TBFI,
    PXG4BPI,
    PXG4BPFI,
    PEG4BPI,
    PEG2BPI,
    PEG4BPIN,
    PEG2BPFI,
    PEG2BPFILX,
    PMCXG2BPFI,
    PMCXG2BPFIN,
    PEG4BPII,
    PEG4BPFII,
    PXG4BPFILX,
    PMCXG2BPIN,
    PMCXG4BPIN,
    PXG2BISC1,
    PEG2TBFI,
    PXG2TBI,
    PXG4BPFID,
    PEG4BPFI,
    PEG4BPIPT,
    PXG6BPI,
    PEG4BPIL,
    PMCXG2BPIN2,
    PMCXG4BPIN2,
    PMCX2BPI,
    PEG2BPFID,
    PEG2BPFIDLX,
    PMCX4BPI,
    MEG2BPFILN,
    MEG2BPFINX,
    PEG4BPFILX,
    PE10G2BPISR,
    PE10G2BPILR,
    MHIO8AD,
    PE10G2BPICX4,
    PEG2BPI5,
    PEG6BPI,
    PEG4BPFI5,
    PEG4BPFI5LX,
    MEG2BPFILXLN,
    PEG2BPIX1,
    MEG2BPFILXNX,
    XE10G2BPIT,
    XE10G2BPICX4,
    XE10G2BPISR,
    XE10G2BPILR,
    PEG4BPIIO,
    XE10G2BPIXR,
    PE10GDBISR,
    PE10GDBILR,
    PEG2BISC6,
    PEG6BPIFC,
    PE10G2BPTCX4,
    PE10G2BPTSR,
    PE10G2BPTLR,
    PE10G2BPTT,
    PEG4BPI6,
    PEG4BPFI6,
    PEG4BPFI6LX,
    PEG4BPFI6ZX,
    PEG4BPFI6CS,
    PEG2BPI6,
    PEG2BPFI6,
    PEG2BPFI6LX,
    PEG2BPFI6ZX,

    PEG2BPFI6FLXM,
    PEG2BPFI6FLXMRB2,

    PEG4BPI6FC,
    PEG4BPFI6FC,
    PEG4BPFI6FCLX,
    PEG4BPFI6FCZX,
    PEG6BPI6,
    PEG2BPI6SC6,
    MEG2BPI6,
    XEG2BPI6,
    MEG4BPI6,
    PEG2BPFI5,
    PEG2BPFI5LX,
    PXEG4BPFI,
    M1EG2BPI6,
    M1EG2BPFI6,
    M1EG2BPFI6LX,
    M1EG2BPFI6ZX,
    M1EG4BPI6,
    M1EG4BPFI6,
    M1EG4BPFI6LX,
    M1EG4BPFI6ZX,
    M1EG6BPI6,
    M1E2G4BPi80,
    M4E2G4BPi80CP,
    M4E2G4BPi80CQ,
    M1E2G4BPFi80,
    M1E2G4BPFi80LX,
    M1E2G4BPFi80ZX,
    PE210G2SPI9,
    M1E10G2BPI9CX4, 
    M1E10G2BPI9SR,
    M4E210G2BPI9SRCP,
    M4E210G2BPI9SRCQ,
    M1E10G2BPI9LR, 
    PE210G2BPI9CX4,
    PE210G2BPI9SR,
    PE210G2BPI9LR,
    PE210G2BPI9SRD,
    PE210G2BPI9LRD,
    PE210G2BPI9T,
    M1E210G2BPI9SRDJP,
    M1E210G2BPI9SRDJP1,
    M1E210G2BPI9LRDJP,
    M1E210G2BPI9LRDJP1,
    M2EG2BPFI6,
    M2EG2BPFI6LX,
    M2EG2BPFI6ZX,
    M2EG4BPI6,
    M2EG4BPFI6,
    M2EG4BPFI6LX,
    M2EG4BPFI6ZX,
    M2EG6BPI6,
    PEG2DBI6,   
    PEG2DBFI6,  
    PEG2DBFI6LX,
    PEG2DBFI6ZX,
    PE2G4BPi80,
    PE2G4BPi80RB1,
    PE2G4BPFi80, 
    PE2G4BPFi80LX,
    PE2G4BPFi80ZX,
    PE2G4BPi80L,
    M6E2G8BPi80A,

    PE2G2BPi35,
    PAC1200BPi35,
    PE2G2BPFi35,
    PE2G2BPFi35ARB2,
    PE2G2BPFi35LX,
    PE2G2BPFi35ALXRB2,
    PE2G2BPFi35ZX,
    PE2G4BPi35,
    PE2G4BPi35L,
    PE2G4BPi35ALRB2,
    PE2G4BPFi35,
    PE2G4BPFi35ARB2,
    PE2G4BPFi35CS,
    PE2G4BPFi35LX,
    PE2G4BPFi35ALXRB2,
    PE2G4BPFi35ZX,

    PE2G6BPi35,
    PE2G6BPi35RB2,
    PE2G6BPi35CX,



    PE2G2BPi80, 
    PE2G2BPFi80, 
    PE2G2BPFi80LX,
    PE2G2BPFi80ZX,
    M2E10G2BPI9CX4, 
    M2E10G2BPI9SR, 
    M2E10G2BPI9LR, 
    M2E10G2BPI9T,
    M6E2G8BPi80,
    PE210G2DBi9SR,
    PE210G2DBi9SRRB2,

    PE210G2DBi9LR,

    PE210G2DBi9LRRB2,
    PE310G4DBi940SR,
    PE310G4DBi940SRRB2,

    PE310G4DBi940LR,
    PE310G4DBi940LRRB2,

    PE310G4DBi940T,
    PE310G4DBi940TRB2,

    PE310G4BPi9T,
    PE310G4BPi9SR,
    PE310G4BPi9LR,
    PE310G4BPi9SRD,
    PE310G4BPi9LRD,

    PE210G2BPi40,
    PE310G4BPi40,
    M1E210G2BPI40T,
    M6E310G4BPi9SR,
    M6E310G4BPi9LR,
    PE2G6BPI6CS,
    PE2G6BPI6,

    M1E2G4BPi35,
    M1E2G4BPFi35,
    M1E2G4BPFi35LX,
    M1E2G4BPFi35ZX,

    M1E2G4BPi35JP,
    M1E2G4BPi35JP1,

    PE310G4DBi9T,

	PE310G4BPI71,
	PE340G2BPI71,
	PE310G4BPI71SRD,
	PE310G4BPI71LRD,
	PE310G2BPI71SRD,
	PE310G2BPI71LRD,
	PE340G2BPI71QS4,
	PE340G2BPI71QS43,
	PE340G2BPI71QL4,
	ADI_RANGELEY,
} board_t;

typedef struct _bpmod_info_t {
    unsigned int vendor;
    unsigned int device;
    unsigned int subvendor;
    unsigned int subdevice;
    unsigned int index;
    char *bp_name;

} bpmod_info_t;


typedef struct _dev_desc {
    char *name;
} dev_desc_t;

dev_desc_t dev_desc[]={
    {"Silicom Bypass PXG2BPFI-SD series adapter"},
    {"Silicom Bypass PXG2BPFIL-SD series adapter"}, 
    {"Silicom Bypass PXG2BPFILX-SD series adapter"},
    {"Silicom Bypass PXG2BPFILLX-SD series adapter"},
    {"Silicom Bypass PXG2BPI-SD series adapter"},    
    {"Silicom Bypass PXG2BPIG-SD series adapter"},   
    {"Silicom Bypass PXG2TBFI-SD series adapter"},  
    {"Silicom Bypass PXG4BPI-SD series adapter"},   
    {"Silicom Bypass PXG4BPFI-SD series adapter"},   
    {"Silicom Bypass PEG4BPI-SD series adapter"},
    {"Silicom Bypass PEG2BPI-SD series adapter"},
    {"Silicom Bypass PEG4BPIN-SD series adapter"},
    {"Silicom Bypass PEG2BPFI-SD series adapter"},
    {"Silicom Bypass PEG2BPFI-LX-SD series adapter"},
    {"Silicom Bypass PMCX2BPFI-SD series adapter"},
    {"Silicom Bypass PMCX2BPFI-N series adapter"},  
    {"Intel Bypass PEG2BPII series adapter"},
    {"Intel Bypass PEG2BPFII series adapter"},
    {"Silicom Bypass PXG4BPFILX-SD series adapter"},
    {"Silicom Bypass PMCX2BPI-N series adapter"},
    {"Silicom Bypass PMCX4BPI-N series adapter"},
    {"Silicom Bypass PXG2BISC1-SD series adapter"},
    {"Silicom Bypass PEG2TBFI-SD series adapter"},
    {"Silicom Bypass PXG2TBI-SD series adapter"},
    {"Silicom Bypass PXG4BPFID-SD series adapter"},
    {"Silicom Bypass PEG4BPFI-SD series adapter"},
    {"Silicom Bypass PEG4BPIPT-SD series adapter"},
    {"Silicom Bypass PXG6BPI-SD series adapter"},
    {"Silicom Bypass PEG4BPIL-SD series adapter"},
    {"Silicom Bypass PMCX2BPI-N2 series adapter"},
    {"Silicom Bypass PMCX4BPI-N2 series adapter"},
    {"Silicom Bypass PMCX2BPI-SD series adapter"}, 
    {"Silicom Bypass PEG2BPFID-SD series adapter"},
    {"Silicom Bypass PEG2BPFIDLX-SD series adapter"},
    {"Silicom Bypass PMCX4BPI-SD series adapter"}, 
    {"Silicom Bypass MEG2BPFILN-SD series adapter"},
    {"Silicom Bypass MEG2BPFINX-SD series adapter"},
    {"Silicom Bypass PEG4BPFILX-SD series adapter"},
    {"Silicom Bypass PE10G2BPISR-SD series adapter"},
    {"Silicom Bypass PE10G2BPILR-SD series adapter"},
    {"Silicom Bypass MHIO8AD-SD series adapter"},
    {"Silicom Bypass PE10G2BPICX4-SD series adapter"},
    {"Silicom Bypass PEG2BPI5-SD series adapter"},
    {"Silicom Bypass PEG6BPI5-SD series adapter"},
    {"Silicom Bypass PEG4BPFI5-SD series adapter"},
    {"Silicom Bypass PEG4BPFI5LX-SD series adapter"},
    {"Silicom Bypass MEG2BPFILXLN-SD series adapter"},
    {"Silicom Bypass PEG2BPIX1-SD series adapter"},
    {"Silicom Bypass MEG2BPFILXNX-SD series adapter"},
    {"Silicom Bypass XE10G2BPIT-SD series adapter"},
    {"Silicom Bypass XE10G2BPICX4-SD series adapter"}, 
    {"Silicom Bypass XE10G2BPISR-SD series adapter"},
    {"Silicom Bypass XE10G2BPILR-SD series adapter"},
    {"Intel Bypass PEG2BPFII0 series adapter"},
    {"Silicom Bypass XE10G2BPIXR series adapter"},
    {"Silicom Bypass PE10G2DBISR series adapter"},
    {"Silicom Bypass PEG2BI5SC6 series adapter"},
    {"Silicom Bypass PEG6BPI5FC series adapter"},

    {"Silicom Bypass PE10G2BPTCX4 series adapter"},
    {"Silicom Bypass PE10G2BPTSR series adapter"},
    {"Silicom Bypass PE10G2BPTLR series adapter"},
    {"Silicom Bypass PE10G2BPTT series adapter"},
    {"Silicom Bypass PEG4BPI6 series adapter"},
    {"Silicom Bypass PEG4BPFI6 series adapter"},
    {"Silicom Bypass PEG4BPFI6LX series adapter"},
    {"Silicom Bypass PEG4BPFI6ZX series adapter"},
    {"Silicom Bypass PEG4BPFI6CS series adapter"},
    {"Silicom Bypass PEG2BPI6 series adapter"},
    {"Silicom Bypass PEG2BPFI6 series adapter"},
    {"Silicom Bypass PEG2BPFI6LX series adapter"},
    {"Silicom Bypass PEG2BPFI6ZX series adapter"},

    {"Silicom Bypass PEG2BPFI6FLXM series adapter"},
    {"Silicom Bypass PEG2BPFI6FLXMRB2 series adapter"},


    {"Silicom Bypass PEG4BPI6FC series adapter"},
    {"Silicom Bypass PEG4BPFI6FC series adapter"},
    {"Silicom Bypass PEG4BPFI6FCLX series adapter"},
    {"Silicom Bypass PEG4BPFI6FCZX series adapter"},
    {"Silicom Bypass PEG6BPI6 series adapter"},
    {"Silicom Bypass PEG2BPI6SC6 series adapter"},
    {"Silicom Bypass MEG2BPI6 series adapter"},
    {"Silicom Bypass XEG2BPI6 series adapter"},
    {"Silicom Bypass MEG4BPI6 series adapter"},
    {"Silicom Bypass PEG2BPFI5-SD series adapter"},
    {"Silicom Bypass PEG2BPFI5LX-SD series adapter"},
    {"Silicom Bypass PXEG4BPFI-SD series adapter"},
    {"Silicom Bypass MxEG2BPI6 series adapter"},
    {"Silicom Bypass MxEG2BPFI6 series adapter"},
    {"Silicom Bypass MxEG2BPFI6LX series adapter"},
    {"Silicom Bypass MxEG2BPFI6ZX series adapter"},
    {"Silicom Bypass MxEG4BPI6 series adapter"},
    {"Silicom Bypass MxEG4BPFI6 series adapter"},
    {"Silicom Bypass MxEG4BPFI6LX series adapter"},
    {"Silicom Bypass MxEG4BPFI6ZX series adapter"},
    {"Silicom Bypass MxEG6BPI6 series adapter"},
    {"Silicom Bypass MxE2G4BPi80 series adapter"},
    {"Silicom Bypass M4E2G4BPi80CP series adapter"},
    {"Silicom Bypass M4E2G4BPi80CQ series adapter"},
    {"Silicom Bypass MxE2G4BPFi80 series adapter"},
    {"Silicom Bypass MxE2G4BPFi80LX series adapter"},
    {"Silicom Bypass MxE2G4BPFi80ZX series adapter"},


    {"Silicom Bypass PE210G2SPI9 series adapter"},


    {"Silicom Bypass MxE210G2BPI9CX4 series adapter"},
    {"Silicom Bypass MxE210G2BPI9SR series adapter"},
    {"Silicom Bypass M4E210G2BPI9SRCP series adapter"},
    {"Silicom Bypass M4E210G2BPI9SRCQ series adapter"},
    {"Silicom Bypass MxE210G2BPI9LR series adapter"},
    {"Silicom Bypass MxE210G2BPI9T series adapter"},

    {"Silicom Bypass PE210G2BPI9CX4 series adapter"},
    {"Silicom Bypass PE210G2BPI9SR series adapter"},
    {"Silicom Bypass PE210G2BPI9LR series adapter"},
    {"Silicom Bypass PE210G2BPI9SRD series adapter"},
    {"Silicom Bypass PE210G2BPI9LRD series adapter"},

    {"Silicom Bypass PE210G2BPI9T series adapter"},
    {"Silicom Bypass M1E210G2BPI9SRDJP series adapter"},
    {"Silicom Bypass M1E210G2BPI9SRDJP1 series adapter"},
    {"Silicom Bypass M1E210G2BPI9LRDJP series adapter"},
    {"Silicom Bypass M1E210G2BPI9LRDJP1 series adapter"},

    {"Silicom Bypass M2EG2BPFI6 series adapter"},
    {"Silicom Bypass M2EG2BPFI6LX series adapter"},
    {"Silicom Bypass M2EG2BPFI6ZX series adapter"},
    {"Silicom Bypass M2EG4BPI6 series adapter"},
    {"Silicom Bypass M2EG4BPFI6 series adapter"},
    {"Silicom Bypass M2EG4BPFI6LX series adapter"},
    {"Silicom Bypass M2EG4BPFI6ZX series adapter"},
    {"Silicom Bypass M2EG6BPI6 series adapter"},



    {"Silicom Bypass PEG2DBI6    series adapter"},
    {"Silicom Bypass PEG2DBFI6   series adapter"},
    {"Silicom Bypass PEG2DBFI6LX series adapter"},
    {"Silicom Bypass PEG2DBFI6ZX series adapter"},


    {"Silicom Bypass PE2G4BPi80 series adapter"},
	{"Silicom Bypass PE2G4BPi80RB1 series adapter"},
    {"Silicom Bypass PE2G4BPFi80 series adapter"},
    {"Silicom Bypass PE2G4BPFi80LX series adapter"},
    {"Silicom Bypass PE2G4BPFi80ZX series adapter"},

    {"Silicom Bypass PE2G4BPi80L series adapter"},
    {"Silicom Bypass MxE2G8BPi80A series adapter"},




    {"Silicom Bypass PE2G2BPi35 series adapter"},
    {"Silicom Bypass PAC1200BPi35 series adapter"},
    {"Silicom Bypass PE2G2BPFi35 series adapter"},
    {"Silicom Bypass PE2G2BPFi35ARB2 series adapter"},
    {"Silicom Bypass PE2G2BPFi35LX series adapter"},
    {"Silicom Bypass PE2G2BPFi35ALXRB2 series adapter"},
    {"Silicom Bypass PE2G2BPFi35ZX series adapter"},



    {"Silicom Bypass PE2G4BPi35 series adapter"},
    {"Silicom Bypass PE2G4BPi35L series adapter"},
    {"Silicom Bypass PE2G4BPi35ALRB2 series adapter"},
    {"Silicom Bypass PE2G4BPFi35 series adapter"},
    {"Silicom Bypass PE2G4BPFi35ARB2 series adapter"},
    {"Silicom Bypass PE2G4BPFi35CS series adapter"},
    {"Silicom Bypass PE2G4BPFi35LX series adapter"},
    {"Silicom Bypass PE2G4BPFi35ALXRB2 series adapter"},
    {"Silicom Bypass PE2G4BPFi35ZX series adapter"},

    {"Silicom Bypass PE2G6BPi35 series adapter"},
    {"Silicom Bypass PE2G6BPi35RB2 series adapter"},
    {"Silicom Bypass PE2G6BPi35CX series adapter"},


    {"Silicom Bypass PE2G2BPi80 series adapter"}, 
    {"Silicom Bypass PE2G2BPFi80 series adapter"},
    {"Silicom Bypass PE2G2BPFi80LX series adapter"},
    {"Silicom Bypass PE2G2BPFi80ZX series adapter"},


    {"Silicom Bypass M2E10G2BPI9CX4 series adapter"},
    {"Silicom Bypass M2E10G2BPI9SR series adapter"},
    {"Silicom Bypass M2E10G2BPI9LR series adapter"},
    {"Silicom Bypass M2E10G2BPI9T series adapter"},
    {"Silicom Bypass MxE2G8BPi80 series adapter"},
    {"Silicom Bypass PE210G2DBi9SR series adapter"},  
    {"Silicom Bypass PE210G2DBi9SRRB2 series adapter"},
    {"Silicom Bypass PE210G2DBi9LR series adapter"},  
    {"Silicom Bypass PE210G2DBi9LRRB2 series adapter"},
    {"Silicom Bypass PE310G4DBi9-SR series adapter"},
    {"Silicom Bypass PE310G4DBi9-SRRB2 series adapter"},
    {"Silicom Bypass PE310G4BPi9T series adapter"},
    {"Silicom Bypass PE310G4BPi9SR series adapter"},
    {"Silicom Bypass PE310G4BPi9LR series adapter"},
    {"Silicom Bypass PE310G4BPi9SRD series adapter"},
    {"Silicom Bypass PE310G4BPi9LRD series adapter"},

    {"Silicom Bypass PE210G2BPi40T series adapter"},
    {"Silicom Bypass PE310G4BPi40T series adapter"},
    {"Silicom Bypass M1E210G2BPI40T series adapter"},
    {"Silicom Bypass M6E310G4BPi9SR series adapter"},
    {"Silicom Bypass M6E310G4BPi9LR series adapter"},
    {"Silicom Bypass PE2G6BPI6CS series adapter"},
    {"Silicom Bypass PE2G6BPI6 series adapter"},

    {"Silicom Bypass M1E2G4BPi35 series adapter"},
    {"Silicom Bypass M1E2G4BPFi35 series adapter"},
    {"Silicom Bypass M1E2G4BPFi35LX series adapter"},
    {"Silicom Bypass M1E2G4BPFi35ZX series adapter"},
    {"Silicom Bypass M1E2G4BPi35JP series adapter"},
    {"Silicom Bypass M1E2G4BPi35JP1 series adapter"},

    {"Silicom Bypass PE310G4DBi9T series adapter"},

	{"Silicom Bypass PE310G4BPI71 series adapter"},
	{"Silicom Bypass PE340G2BPI71 series adapter"},
	{"Silicom Bypass PE310G4BPI71SRD series adapter"},
	{"Silicom Bypass PE310G4BPI71LRD series adapter"},
	{"Silicom Bypass PE310G2BPI71SRD series adapter"},
	{"Silicom Bypass PE310G2BPI71LRD series adapter"},
	{"Silicom Bypass PE340G2BPI71QS4 series adapter"},
	{"Silicom Bypass PE340G2BPI71QL4 series adapter"},
	{"Silicom Bypass ADI_RANGELEY series adapter"},

    {0},
};

static bpmod_info_t tx_ctl_pci_tbl[] = {
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG2BPFI_SSID, PXG2BPFI, "PXG2BPFI-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG2BPFIL_SSID, PXG2BPFIL, "PXG2BPFIL-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG2BPFILX_SSID, PXG2BPFILX, "PXG2BPFILX-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG2BPFILLX_SSID, PXG2BPFILLX, "PXG2BPFILLXSD"},
    {0x8086, 0x1010, SILICOM_SVID, SILICOM_PXGBPI_SSID, PXGBPI, "PXG2BPI-SD"},
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PXGBPIG_SSID, PXGBPIG, "PXG2BPIG-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG2TBFI_SSID, PXG2TBFI, "PXG2TBFI-SD"},
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PXG4BPI_SSID, PXG4BPI, "PXG4BPI-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG4BPFI_SSID, PXG4BPFI, "PXG4BPFI-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG4BPFILX_SSID, PXG4BPFILX, "PXG4BPFILX-SD"},
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PEG4BPI_SSID, PEG4BPI, "PEXG4BPI-SD"},
    {0x8086, 0x105e, SILICOM_SVID, SILICOM_PEG2BPI_SSID, PEG2BPI, "PEG2BPI-SD"},
    {0x8086, 0x105e, SILICOM_SVID, SILICOM_PEG4BPIN_SSID, PEG4BPIN, "PEG4BPI-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG2BPFI_SSID, PEG2BPFI, "PEG2BPFI-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG2BPFILX_SSID, PEG2BPFILX, "PEG2BPFILX-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PMCXG2BPFI_SSID, PMCXG2BPFI, "PMCX2BPFI-SD"},    
    {0x8086, 0x107a, NOKIA_PMCXG2BPFIN_SVID, NOKIA_PMCXG2BPFIN_SSID, PMCXG2BPFIN, "PMCX2BPFI-N"},    
    {0x8086, INTEL_PEG4BPII_PID,  0x8086, INTEL_PEG4BPII_SSID, PEG4BPII, "PEG4BPII"},
    {0x8086, INTEL_PEG4BPIIO_PID,  0x8086, INTEL_PEG4BPIIO_SSID, PEG4BPIIO, "PEG4BPII0"},
    {0x8086, INTEL_PEG4BPFII_PID, 0x8086, INTEL_PEG4BPFII_SSID, PEG4BPFII, "PEG4BPFII"},    
    {0x8086, 0x1079, NOKIA_PMCXG2BPFIN_SVID, NOKIA_PMCXG2BPIN_SSID, PMCXG2BPIN, "PMCX2BPI-N"},    
    {0x8086, 0x1079, NOKIA_PMCXG2BPFIN_SVID, NOKIA_PMCXG4BPIN_SSID, PMCXG4BPIN, "PMCX4BPI-N"},    
    {0x8086, 0x1079, SILICOM_SVID,SILICOM_PXG2BISC1_SSID, PXG2BISC1, "PXG2BISC1-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG2TBFI_SSID, PEG2TBFI, "PEG2TBFI-SD"},
    {0x8086, 0x1079, SILICOM_SVID,SILICOM_PXG2TBI_SSID, PXG2TBI, "PXG2TBI-SD"},
    {0x8086, 0x107a, SILICOM_SVID, SILICOM_PXG4BPFID_SSID, PXG4BPFID, "PXG4BPFID-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG4BPFI_SSID, PEG4BPFI, "PEG4BPFI-SD"},
    {0x8086, 0x105e, SILICOM_SVID, SILICOM_PEG4BPIPT_SSID, PEG4BPIPT, "PEG4BPIPT-SD"},   
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PXG6BPI_SSID, PXG6BPI, "PXG6BPI-SD"},
    {0x8086, 0x10a7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPIL_SSID /*PCI_ANY_ID*/, PEG4BPIL, "PEG4BPIL-SD"},
    {0x8086, 0x1079, NOKIA_PMCXG2BPFIN_SVID, NOKIA_PMCXG2BPIN2_SSID, PMCXG2BPIN2, "PMCX2BPI-N2"},    
    {0x8086, 0x1079, NOKIA_PMCXG2BPFIN_SVID, NOKIA_PMCXG4BPIN2_SSID, PMCXG4BPIN2, "PMCX4BPI-N2"},    
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PMCX2BPI_SSID, PMCX2BPI, "PMCX2BPI-SD"},
    {0x8086, 0x1079, SILICOM_SVID, SILICOM_PMCX4BPI_SSID, PMCX4BPI, "PMCX4BPI-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG2BPFID_SSID, PEG2BPFID, "PEG2BPFID-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG2BPFIDLX_SSID, PEG2BPFIDLX, "PEG2BPFIDLXSD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_MEG2BPFILN_SSID, MEG2BPFILN, "MEG2BPFILN-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_MEG2BPFINX_SSID, MEG2BPFINX, "MEG2BPFINX-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PEG4BPFILX_SSID, PEG4BPFILX, "PEG4BPFILX-SD"},
    {0x8086, 0x10C6, SILICOM_SVID, SILICOM_PE10G2BPISR_SSID, PE10G2BPISR, "PE10G2BPISR"},
    {0x8086, 0x10C6, SILICOM_SVID, SILICOM_PE10G2BPILR_SSID, PE10G2BPILR, "PE10G2BPILR"},
    {0x8086, 0x10a9, SILICOM_SVID , SILICOM_MHIO8AD_SSID , MHIO8AD, "MHIO8AD-SD"},
    {0x8086, 0x10DD, SILICOM_SVID, SILICOM_PE10G2BPICX4_SSID, PE10G2BPISR, "PE10G2BPICX4"},
    {0x8086, 0x10a7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPI5_SSID /*PCI_ANY_ID*/, PEG2BPI5, "PEG2BPI5-SD"},
    {0x8086, 0x10a7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG6BPI_SSID /*PCI_ANY_ID*/, PEG6BPI, "PEG6BPI5"},
    {0x8086, 0x10a9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PEG4BPFI5_SSID, PEG4BPFI5, "PEG4BPFI5"},
    {0x8086, 0x10a9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PEG4BPFI5LX_SSID, PEG4BPFI5LX, "PEG4BPFI5LX"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_MEG2BPFILXLN_SSID, MEG2BPFILXLN, "MEG2BPFILXLN"},
    {0x8086, 0x105e, SILICOM_SVID, SILICOM_PEG2BPIX1_SSID, PEG2BPIX1, "PEG2BPIX1-SD"},
    {0x8086, 0x105f, SILICOM_SVID, SILICOM_MEG2BPFILXNX_SSID, MEG2BPFILXNX, "MEG2BPFILXNX"},
    {0x8086, 0x10C8, SILICOM_SVID, SILICOM_XE10G2BPIT_SSID, XE10G2BPIT, "XE10G2BPIT"},
    {0x8086, 0x10DD, SILICOM_SVID, SILICOM_XE10G2BPICX4_SSID, XE10G2BPICX4, "XE10G2BPICX4"},
    {0x8086, 0x10C6, SILICOM_SVID, SILICOM_XE10G2BPISR_SSID, XE10G2BPISR, "XE10G2BPISR"},
    {0x8086, 0x10C6, SILICOM_SVID, SILICOM_XE10G2BPILR_SSID, XE10G2BPILR, "XE10G2BPILR"},
    {0x8086, 0x10C6, NOKIA_XE10G2BPIXR_SVID, NOKIA_XE10G2BPIXR_SSID, XE10G2BPIXR, "XE10G2BPIXR"},
    {0x8086, 0x10C6, SILICOM_SVID,SILICOM_PE10GDBISR_SSID, PE10GDBISR, "PE10G2DBISR"},
    {0x8086, 0x10C6, SILICOM_SVID,SILICOM_PE10GDBILR_SSID, PE10GDBILR, "PE10G2DBILR"},
    {0x8086, 0x10a7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BISC6_SSID /*PCI_ANY_ID*/, PEG2BISC6, "PEG2BI5SC6"},
    {0x8086, 0x10a7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG6BPIFC_SSID /*PCI_ANY_ID*/, PEG6BPIFC, "PEG6BPI5FC"},

    {BROADCOM_VID, BROADCOM_PE10G2_PID, SILICOM_SVID, SILICOM_PE10G2BPTCX4_SSID, PE10G2BPTCX4, "PE10G2BPTCX4"},
    {BROADCOM_VID, BROADCOM_PE10G2_PID, SILICOM_SVID, SILICOM_PE10G2BPTSR_SSID, PE10G2BPTSR, "PE10G2BPTSR"},
    {BROADCOM_VID, BROADCOM_PE10G2_PID, SILICOM_SVID, SILICOM_PE10G2BPTLR_SSID, PE10G2BPTLR, "PE10G2BPTLR"},
    {BROADCOM_VID, BROADCOM_PE10G2_PID, SILICOM_SVID, SILICOM_PE10G2BPTT_SSID, PE10G2BPTT, "PE10G2BPTT"},

    //{BROADCOM_VID, BROADCOM_PE10G2_PID, PCI_ANY_ID, PCI_ANY_ID, PE10G2BPTCX4, "PE10G2BPTCX4"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPI6_SSID /*PCI_ANY_ID*/, PEG4BPI6, "PEG4BPI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6_SSID /*PCI_ANY_ID*/, PEG4BPFI6, "PEG4BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6LX_SSID /*PCI_ANY_ID*/, PEG4BPFI6LX, "PEG4BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6ZX_SSID /*PCI_ANY_ID*/, PEG4BPFI6ZX, "PEG4BPFI6ZX"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPI6_SSID /*PCI_ANY_ID*/, PEG2BPI6, "PEG2BPI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPFI6_SSID /*PCI_ANY_ID*/, PEG2BPFI6, "PEG2BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPFI6LX_SSID /*PCI_ANY_ID*/, PEG2BPFI6LX, "PEG2BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPFI6ZX_SSID /*PCI_ANY_ID*/, PEG2BPFI6ZX, "PEG2BPFI6ZX"},

    {0x8086, 0x10e7, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPFI6FLXM_SSID /*PCI_ANY_ID*/, PEG2BPFI6FLXM, "PEG2BPFI6FLXM"},
    {0x8086, 0x10e7, 0x1b2e /*PCI_ANY_ID*/, SILICOM_PEG2BPFI6FLXM_SSID /*PCI_ANY_ID*/, PEG2BPFI6FLXMRB2, "PEG2BPFI6FLXMRB2"},


    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPI6FC_SSID /*PCI_ANY_ID*/, PEG4BPI6FC, "PEG4BPI6FC"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6FC_SSID /*PCI_ANY_ID*/, PEG4BPFI6FC, "PEG4BPFI6FC"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6FCLX_SSID /*PCI_ANY_ID*/, PEG4BPFI6FCLX, "PEG4BPFI6FCLX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG4BPFI6FCZX_SSID /*PCI_ANY_ID*/, PEG4BPFI6FCZX, "PEG4BPFI6FCZX"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG6BPI6_SSID /*PCI_ANY_ID*/, PEG6BPI6, "PEG6BPI6"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2BPI6SC6_SSID /*PCI_ANY_ID*/, PEG2BPI6SC6, "PEG6BPI62SC6"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_MEG2BPI6_SSID /*PCI_ANY_ID*/, MEG2BPI6, "MEG2BPI6"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_XEG2BPI6_SSID /*PCI_ANY_ID*/, XEG2BPI6, "XEG2BPI6"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_MEG4BPI6_SSID /*PCI_ANY_ID*/, MEG4BPI6, "MEG4BPI6"},

    {0x8086, 0x10a9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PEG2BPFI5_SSID, PEG2BPFI5, "PEG2BPFI5"},
    {0x8086, 0x10a9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PEG2BPFI5LX_SSID, PEG2BPFI5LX, "PEG2BPFI5LX"},

    {0x8086, 0x105f, SILICOM_SVID, SILICOM_PXEG4BPFI_SSID, PXEG4BPFI, "PXEG4BPFI-SD"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG2BPI6_SSID /*PCI_ANY_ID*/, M1EG2BPI6, "MxEG2BPI6"},

    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG2BPFI6_SSID /*PCI_ANY_ID*/, M1EG2BPFI6, "MxEG2BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG2BPFI6LX_SSID /*PCI_ANY_ID*/, M1EG2BPFI6LX, "MxEG2BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG2BPFI6ZX_SSID /*PCI_ANY_ID*/, M1EG2BPFI6ZX, "MxEG2BPFI6ZX"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG4BPI6_SSID /*PCI_ANY_ID*/, M1EG4BPI6, "MxEG4BPI6"},

    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG4BPFI6_SSID /*PCI_ANY_ID*/, M1EG4BPFI6, "MxEG4BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG4BPFI6LX_SSID /*PCI_ANY_ID*/, M1EG4BPFI6LX, "MxEG4BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG4BPFI6ZX_SSID /*PCI_ANY_ID*/, M1EG4BPFI6ZX, "MxEG4BPFI6ZX"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1EG6BPI6_SSID /*PCI_ANY_ID*/, M1EG6BPI6, "MxEG6BPI6"},


    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPi80_SSID /*PCI_ANY_ID*/, M1E2G4BPi80, "MxE2G4BPi80"},
    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M4E2G4BPi80CP_SSID /*PCI_ANY_ID*/, M4E2G4BPi80CP, "M4E2G4BPi80CP"},
    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M4E2G4BPi80CQ_SSID /*PCI_ANY_ID*/, M4E2G4BPi80CQ, "M4E2G4BPi80CQ"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi80_SSID /*PCI_ANY_ID*/, M1E2G4BPFi80, "MxE2G4BPFi80"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi80LX_SSID /*PCI_ANY_ID*/, M1E2G4BPFi80LX, "MxE2G4BPFi80LX"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi80ZX_SSID /*PCI_ANY_ID*/, M1E2G4BPFi80ZX, "MxE2G4BPFi80ZX"},


    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG2BPFI6_SSID /*PCI_ANY_ID*/, M2EG2BPFI6, "M2EG2BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG2BPFI6LX_SSID /*PCI_ANY_ID*/, M2EG2BPFI6LX, "M2EG2BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG2BPFI6ZX_SSID /*PCI_ANY_ID*/, M2EG2BPFI6ZX, "M2EG2BPFI6ZX"},

    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG4BPI6_SSID /*PCI_ANY_ID*/, M2EG4BPI6, "M2EG4BPI6"},

    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG4BPFI6_SSID /*PCI_ANY_ID*/, M2EG4BPFI6, "M2EG4BPFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG4BPFI6LX_SSID /*PCI_ANY_ID*/, M2EG4BPFI6LX, "M2EG4BPFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG4BPFI6ZX_SSID /*PCI_ANY_ID*/, M2EG4BPFI6ZX, "M2EG4BPFI6ZX"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2EG6BPI6_SSID /*PCI_ANY_ID*/, M2EG6BPI6, "M2EG6BPI6"},


    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2DBI6_SSID /*PCI_ANY_ID*/, PEG2DBI6, "PEG2DBI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2DBFI6_SSID /*PCI_ANY_ID*/, PEG2DBFI6, "PEG2DBFI6"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2DBFI6LX_SSID /*PCI_ANY_ID*/, PEG2DBFI6LX, "PEG2DBFI6LX"},
    {0x8086, 0x10e6, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PEG2DBFI6ZX_SSID /*PCI_ANY_ID*/, PEG2DBFI6ZX, "PEG2DBFI6ZX"},

    {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE210G2DBi9SR_SSID,    PE210G2DBi9SR,   "PE210G2DBi9SR"},



    {0x8086, 0x10F9, 0X1B2E /*PCI_ANY_ID*/, SILICOM_PE210G2DBi9SRRB_SSID , PE210G2DBi9SRRB2, "PE210G2DBi9SRRB2"}, 
    {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE210G2DBi9LR_SSID  ,  PE210G2DBi9LR,   "PE210G2DBi9LR"},  
    {0x8086, 0x10F9, 0X1B2E /*PCI_ANY_ID*/, SILICOM_PE210G2DBi9LRRB_SSID , PE210G2DBi9LRRB2, "PE210G2DBi9LRRB2"},

    {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940SR_SSID , PE310G4DBi940SR, "PE310G4DBi9SR"},
    {0x8086, 0x10F9, 0X1B2E /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940SR_SSID , PE310G4DBi940SRRB2, "PE310G4DBi9SRRB2"},

    {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940LR_SSID , PE310G4DBi940LR, "PE310G4DBi9LR"},
    {0x8086, 0x10F9, 0X1B2E /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940LR_SSID , PE310G4DBi940LRRB2, "PE310G4DBi9LRRB2"},

     {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940T_SSID , PE310G4DBi940T, "PE310G4DBi9T"},
    {0x8086, 0x10F9, 0X1B2E /*PCI_ANY_ID*/, SILICOM_PE310G4DBi940T_SSID , PE310G4DBi940TRB2, "PE310G4DBi9TRB2"},





    {0x8086, 0x10F9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PE310G4DBi9T_SSID , PE310G4DBi9T, "PE310G4DBi9T"},



    {0x8086, 0x10Fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi9T_SSID,  PE310G4BPi9T, "PE310G4BPi9T"},
    {0x8086, 0x10Fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi9SR_SSID, PE310G4BPi9SR, "PE310G4BPi9SR"},
    {0x8086, 0x10Fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi9LR_SSID, PE310G4BPi9LR, "PE310G4BPi9LR"},
    {0x8086, 0x10Fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi9SRD_SSID, PE310G4BPi9SRD, "PE310G4BPi9SRD"},
    {0x8086, 0x10Fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi9LRD_SSID, PE310G4BPi9LRD, "PE310G4BPi9LRD"},

    {0x8086, 0x10Fb, SILICOM_SVID,  SILICOM_M6E310G4BPi9LR_SSID, M6E310G4BPi9LR, "M6E310G4BPi9LR"},
    {0x8086, 0x10Fb, SILICOM_SVID,  SILICOM_M6E310G4BPi9SR_SSID, M6E310G4BPi9SR, "M6E310G4BPi9SR"},





    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPi80_SSID /*PCI_ANY_ID*/,    PE2G4BPi80, "PE2G4BPi80"},
	{0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPi80RB1_SSID /*PCI_ANY_ID*/, PE2G4BPi80RB1, "PE2G4BPi80RB1"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi80_SSID /*PCI_ANY_ID*/,   PE2G4BPFi80, "PE2G4BPFi80"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi80LX_SSID /*PCI_ANY_ID*/, PE2G4BPFi80LX, "PE2G4BPFi80LX"},
    {0x8086, 0x150f, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi80ZX_SSID /*PCI_ANY_ID*/, PE2G4BPFi80ZX, "PE2G4BPFi80ZX"},

    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPi80L_SSID /*PCI_ANY_ID*/,    PE2G4BPi80L, "PE2G4BPi80L"},

    {0x8086, 0x150e, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M6E2G8BPi80A_SSID /*PCI_ANY_ID*/,    M6E2G8BPi80A, "MxE2G8BPi80A"},



    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPi35_SSID /*PCI_ANY_ID*/,    PE2G2BPi35, "PE2G2BPi35"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PAC1200BPi35_SSID /*PCI_ANY_ID*/,    PAC1200BPi35, "PAC1200BPi35"},

    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi35_SSID /*PCI_ANY_ID*/,    PE2G2BPFi35, "PE2G2BPFi35"},
    {0x8086, 0x1522, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi35_SSID /*PCI_ANY_ID*/,    PE2G2BPFi35ARB2, "PE2G2BPFi35ARB2"},

    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi35LX_SSID /*PCI_ANY_ID*/,    PE2G2BPFi35LX, "PE2G2BPFi35LX"},
    {0x8086, 0x1522, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi35LX_SSID /*PCI_ANY_ID*/,    PE2G2BPFi35ALXRB2, "PE2G2BPFi35ALXRB2"},


    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi35ZX_SSID /*PCI_ANY_ID*/,    PE2G2BPFi35ZX, "PE2G2BPFi35ZX"},

    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPi35_SSID /*PCI_ANY_ID*/,    PE2G4BPi35, "PE2G4BPi35"},
    

    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPi35L_SSID /*PCI_ANY_ID*/,    PE2G4BPi35L, "PE2G4BPi35L"},
    {0x8086, 0x1521, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G4BPi35L_SSID /*PCI_ANY_ID*/,    PE2G4BPi35ALRB2, "PE2G4BPi35ALRB2"},


    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35, "PE2G4BPFi35"},
    {0x8086, 0x1522, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35ARB2, "PE2G4BPFi35ARB2"},




    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35CS_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35CS, "PE2G4BPFi35CS"},

    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35LX_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35LX, "PE2G4BPFi35LX"},
    {0x8086, 0x1522, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35LX_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35ALXRB2, "PE2G4BPFi35ALXRB2"},


    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G4BPFi35ZX_SSID /*PCI_ANY_ID*/,    PE2G4BPFi35ZX, "PE2G4BPFi35ZX"},


    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPi35_SSID /*PCI_ANY_ID*/,    M1E2G4BPi35, "M1E2G4BPi35"},
    {0x8086, 0x1521, 0x1304 /*PCI_ANY_ID*/, SILICOM_M1E2G4BPi35JP_SSID /*PCI_ANY_ID*/,    M1E2G4BPi35JP, "M1E2G4BPi35JP"},
    {0x8086, 0x1521, 0x1304 /*PCI_ANY_ID*/, SILICOM_M1E2G4BPi35JP1_SSID /*PCI_ANY_ID*/,    M1E2G4BPi35JP1, "M1E2G4BPi35JP1"},


    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi35_SSID /*PCI_ANY_ID*/,    M1E2G4BPFi35, "M1E2G4BPFi35"},
    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi35LX_SSID /*PCI_ANY_ID*/,    M1E2G4BPFi35LX, "M1E2G4BPFi35LX"},
    {0x8086, 0x1522, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E2G4BPFi35ZX_SSID /*PCI_ANY_ID*/,    M1E2G4BPFi35ZX, "M1E2G4BPFi35ZX"},


    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G6BPi35_SSID /*PCI_ANY_ID*/,    PE2G6BPi35, "PE2G6BPi35"},
    {0x8086, 0x1521, 0x1B2E /*PCI_ANY_ID*/, SILICOM_PE2G6BPi35_SSID /*PCI_ANY_ID*/,    PE2G6BPi35RB2, "PE2G6BPi35RB2"},

    // {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/,0xaa0,PE2G6BPi35CX,"PE2G6BPi35CX"},
    // {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/,0xaa1,PE2G6BPi35CX,"PE2G6BPi35CX"},
    // {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/,0xaa2,PE2G6BPi35CX,"PE2G6BPi35CX"},

    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B40,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B41,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B42,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B43,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B44,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B45,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B46,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B47,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B48,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B49,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4a,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4b,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4c,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4d,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4e,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B4F,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B50,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B51,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B52,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B53,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B54,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B55,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B56,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B57,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B58,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B59,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5A,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5B,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5C,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5D,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5E,PE2G6BPI6CS,"PE2G6BPI6CS"},
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,0x0B5F,PE2G6BPI6CS,"PE2G6BPI6CS"},

    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B60,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B61,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B62,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B63,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B64,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B65,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B66,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B67,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B68,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B69,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6a,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6b,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6c,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6d,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6e,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B6f,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B71,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B72,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B73,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B74,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B75,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B76,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B77,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B78,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B79,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7a,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7b,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7c,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7d,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7e,PEG4BPFI6CS,"PEG4BPFI6CS"},
    {0x8086, 0x10E6, SILICOM_SVID /*PCI_ANY_ID*/,0x0B7f,PEG4BPFI6CS,"PEG4BPFI6CS"},



    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/,SILICOM_PE2G6BPI6_SSID,PE2G6BPI6,"PE2G6BPI6"},


    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa0,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa1,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa2,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa3,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa4,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa5,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa6,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa7,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa8,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaa9,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaaa,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaab,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaac,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaad,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaae,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaaf,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab0,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab1,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab2,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab3,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab4,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab5,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab6,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab7,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab8,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xab9,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xaba,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xabb,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xabc,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xabd,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xabe,PE2G6BPi35CX,"PE2G6BPi35CX"},
    {0x8086, 0x1521, SILICOM_SVID /*PCI_ANY_ID*/,0xabf,PE2G6BPi35CX,"PE2G6BPi35CX"},

    {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPi80_SSID /*PCI_ANY_ID*/,    PE2G2BPi80, "PE2G2BPi80"},
    {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi80_SSID /*PCI_ANY_ID*/,   PE2G2BPFi80, "PE2G2BPFi80"},
    {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi80LX_SSID /*PCI_ANY_ID*/, PE2G2BPFi80LX, "PE2G2BPFi80LX"},
    {0x8086, PCI_ANY_ID, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE2G2BPFi80ZX_SSID /*PCI_ANY_ID*/, PE2G2BPFi80ZX, "PE2G2BPFi80ZX"},

    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_MEG2BPI6_SSID /*PCI_ANY_ID*/, MEG2BPI6, "MEG2BPI6"},
    {0x8086, 0x10c9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_XEG2BPI6_SSID /*PCI_ANY_ID*/, XEG2BPI6, "XEG2BPI6"},    

#if 0
    {0x8086, 0x10fb, 0x8086, INTEL_PE210G2SPI9_SSID, PE210G2SPI9, "PE210G2SPI9"},
#endif
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E10G2BPI9CX4_SSID /*PCI_ANY_ID*/, M1E10G2BPI9CX4, "MxE210G2BPI9CX4"},
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E10G2BPI9SR_SSID /*PCI_ANY_ID*/, M1E10G2BPI9SR, "MxE210G2BPI9SR"},
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E10G2BPI9LR_SSID /*PCI_ANY_ID*/, M1E10G2BPI9LR, "MxE210G2BPI9LR"},

    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M4E210G2BPI9SRCP_SSID /*PCI_ANY_ID*/, M4E210G2BPI9SRCP, "M4E210G2BPI9SRCP"},
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M4E210G2BPI9SRCQ_SSID /*PCI_ANY_ID*/, M4E210G2BPI9SRCQ, "M4E210G2BPI9SRCQ"},

    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2E10G2BPI9CX4_SSID /*PCI_ANY_ID*/, M2E10G2BPI9CX4, "M2E10G2BPI9CX4"},
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2E10G2BPI9SR_SSID /*PCI_ANY_ID*/, M2E10G2BPI9SR, "M2E10G2BPI9SR"},
    {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2E10G2BPI9LR_SSID /*PCI_ANY_ID*/, M2E10G2BPI9LR, "M2E10G2BPI9LR"},
   // {0x8086, 0x10fb, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M2E10G2BPI9T_SSID /*PCI_ANY_ID*/, M2E10G2BPI9T, "M2E10G2BPI9T"},



    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9CX4_SSID, PE210G2BPI9CX4, "PE210G2BPI9CX4"},
    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9SR_SSID,  PE210G2BPI9SR, "PE210G2BPI9SR"},
    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9LR_SSID,  PE210G2BPI9LR, "PE210G2BPI9LR"},
    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9SRD_SSID,  PE210G2BPI9SRD, "PE210G2BPI9SRD"},
    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9LRD_SSID,  PE210G2BPI9LRD, "PE210G2BPI9LRD"},
    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9T_SSID,   PE210G2BPI9T, "PE210G2BPI9T"},

    {0x8086, 0x10fb, SILICOM_SVID, SILICOM_PE210G2BPI9T_SSID,   PE210G2BPI9T, "PE210G2BPI9T"},
    {0x8086, 0x10fb, 0x1304, SILICOM_M1E210G2BPI9SRDJP_SSID,   M1E210G2BPI9SRDJP, "M1E210G2BPI9SRDJP"},
    {0x8086, 0x10fb, 0x1304, SILICOM_M1E210G2BPI9SRDJP1_SSID,   M1E210G2BPI9SRDJP1, "M1E210G2BPI9SRDJP1"},
    {0x8086, 0x10fb, 0x1304, SILICOM_M1E210G2BPI9LRDJP_SSID,   M1E210G2BPI9LRDJP, "M1E210G2BPI9LRDJP"},
    {0x8086, 0x10fb, 0x1304, SILICOM_M1E210G2BPI9LRDJP1_SSID,   M1E210G2BPI9LRDJP1, "M1E210G2BPI9LRDJP1"},

#if 0
    {0x1374, 0x2c, SILICOM_SVID, SILICOM_PXG4BPI_SSID, PXG4BPI, "PXG4BPI-SD"},

    {0x1374, 0x2d, SILICOM_SVID, SILICOM_PXG4BPFI_SSID, PXG4BPFI, "PXG4BPFI-SD"},


    {0x1374, 0x3f, SILICOM_SVID,SILICOM_PXG2TBI_SSID, PXG2TBI, "PXG2TBI-SD"},

    {0x1374, 0x3d, SILICOM_SVID,SILICOM_PXG2BISC1_SSID, PXG2BISC1, "PXG2BISC1-SD"},


    {0x1374, 0x40, SILICOM_SVID, SILICOM_PEG4BPFI_SSID, PEG4BPFI, "PEG4BPFI-SD"},



#ifdef BP_SELF_TEST
    {0x1374, 0x28, SILICOM_SVID,0x28,  PXGBPI, "PXG2BPI-SD"},
#endif
#endif
    {0x8086, 0x10C9, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M6E2G8BPi80_SSID /*PCI_ANY_ID*/,    M6E2G8BPi80, "MxE2G8BPi80"},
    {0x8086, 0x1528, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE210G2BPi40_SSID /*PCI_ANY_ID*/,    PE210G2BPi40, "PE210G2BPi40T"},
    {0x8086, 0x1528, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_PE310G4BPi40_SSID /*PCI_ANY_ID*/,    PE310G4BPi40, "PE310G4BPi40T"},

    {0x8086, 0x1528, SILICOM_SVID /*PCI_ANY_ID*/, SILICOM_M1E210G2BPI40T_SSID /*PCI_ANY_ID*/,    M1E210G2BPI40T, "M1E210G2BPi40T"},

	{0x8086, 0x1572, SILICOM_SVID, SILICOM_PE310G4BPI71SRD_SSID, PE310G4BPI71SRD, "PE310G4BPI71SRD"},
	{0x8086, 0x1572, SILICOM_SVID, SILICOM_PE310G4BPI71LRD_SSID, PE310G4BPI71LRD, "PE310G4BPI71LRD"},

	{0x8086, 0x1572, SILICOM_SVID, SILICOM_PE310G2BPI71SRD_SSID, PE310G2BPI71SRD, "PE310G2BPI71SRD"},
	{0x8086, 0x1572, SILICOM_SVID, SILICOM_PE310G2BPI71LRD_SSID, PE310G2BPI71LRD, "PE310G2BPI71LRD"},

	{0x8086, 0x1572, SILICOM_SVID, 0x0, PE310G4BPI71, "PE310G4BPI71"},

	{0x8086, 0x1583, SILICOM_SVID, SILICOM_PE340G2BPI71QS4_SSID, PE340G2BPI71QS4, "PE340G2BPI71QS4"},
	{0x8086, 0x1583, SILICOM_SVID, SILICOM_PE340G2BPI71QS43_SSID, PE340G2BPI71QS43, "PE340G2BPI71QS43"},
	{0x8086, 0x1583, SILICOM_SVID, SILICOM_PE340G2BPI71QL4_SSID, PE340G2BPI71QL4, "PE340G2BPI71QL4"},
	
	{0x8086, 0x1583, SILICOM_SVID, 0x0, PE340G2BPI71, "PE340G2BPI71"},

	{0x8086, 0x1f41, 0x8086, 0x1f41, ADI_RANGELEY, "ADI_RANGELEY"},


    /* required last entry */
    {0,}
}; 


/*
* Initialize the module - Register the character device
*/

static int v2_bypass_init_module(void)
{
    int ret_val, idx, idx_dev=0;
    struct pci_dev *pdev1=NULL;
    unsigned long mmio_start, mmio_len;

#ifdef BPVM_KVM
	printk("bpvm supported\n");
#endif
    ret_val = register_chrdev (major_num, DEVICE_NAME, &Fops);
    if (ret_val < 0) {
        printk("%s failed with %d\n",DEVICE_NAME,ret_val);
        return ret_val;
    }
    major_num = ret_val;    /* dynamic */
    for (idx = 0; tx_ctl_pci_tbl[idx].vendor; idx++) {
        while ((pdev1=pci_get_subsys(tx_ctl_pci_tbl[idx].vendor,
                                     tx_ctl_pci_tbl[idx].device,
                                     tx_ctl_pci_tbl[idx].subvendor,
                                     tx_ctl_pci_tbl[idx].subdevice,
                                     pdev1))) {
				if (BPRANG_IF_SERIES(tx_ctl_pci_tbl[idx].subdevice)) {
					if(!client)
						continue;
				}
				device_num++;
        }
    }
    if (!device_num) {
        printk("No such device\n"); 
        unregister_chrdev(major_num, DEVICE_NAME);
        return -1;
    }


    bpctl_dev_arr=kmalloc ((device_num)  * sizeof (bpctl_dev_t), GFP_KERNEL);

    if (!bpctl_dev_arr) {
        printk("Allocation error\n"); 
        unregister_chrdev(major_num, DEVICE_NAME);
        return -1;
    }
    memset(bpctl_dev_arr,0,((device_num)  * sizeof (bpctl_dev_t)));

    pdev1=NULL;
    for (idx = 0; tx_ctl_pci_tbl[idx].vendor; idx++) {
        while ((pdev1=pci_get_subsys(tx_ctl_pci_tbl[idx].vendor,
                                     tx_ctl_pci_tbl[idx].device,
                                     tx_ctl_pci_tbl[idx].subvendor,
                                     tx_ctl_pci_tbl[idx].subdevice,
                                     pdev1))) {
            
	     if (BPRANG_IF_SERIES(tx_ctl_pci_tbl[idx].subdevice)) {
		  if(!client)
		      continue;
	     }
            mmio_start = pci_resource_start (pdev1, 0);
            mmio_len = pci_resource_len (pdev1, 0);
            if((!mmio_len)||(!mmio_start))
				continue; 
			bpctl_dev_arr[idx_dev].pdev=pdev1;	

            bpctl_dev_arr[idx_dev].desc=dev_desc[tx_ctl_pci_tbl[idx].index].name;
            bpctl_dev_arr[idx_dev].name=tx_ctl_pci_tbl[idx].bp_name;
            bpctl_dev_arr[idx_dev].device=tx_ctl_pci_tbl[idx].device;
            bpctl_dev_arr[idx_dev].vendor=tx_ctl_pci_tbl[idx].vendor;
            bpctl_dev_arr[idx_dev].subdevice=tx_ctl_pci_tbl[idx].subdevice;
            bpctl_dev_arr[idx_dev].subvendor=tx_ctl_pci_tbl[idx].subvendor;
            //bpctl_dev_arr[idx_dev].pdev=pdev1;
            bpctl_dev_arr[idx_dev].func= PCI_FUNC(pdev1->devfn);
            bpctl_dev_arr[idx_dev].slot= PCI_SLOT(pdev1->devfn); 
            bpctl_dev_arr[idx_dev].bus=pdev1->bus->number;
            bpctl_dev_arr[idx_dev].mem_map=(unsigned long)ioremap(mmio_start,mmio_len);

	     spin_lock_init(&bpctl_dev_arr[idx_dev].bypass_wr_lock);

	     if (BPRANG_IF_SERIES(tx_ctl_pci_tbl[idx].subdevice))
			bpctl_dev_arr[idx_dev].bp_rang=1;
	     
            if (BP10G9_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                bpctl_dev_arr[idx_dev].bp_10g9=1;
            if (BP10G_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                bpctl_dev_arr[idx_dev].bp_10g=1;
            if (PEG540_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice)) {

                bpctl_dev_arr[idx_dev].bp_540=1;
            }
            if (PEGF5_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                bpctl_dev_arr[idx_dev].bp_fiber5=1;
            if (PEG80_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                bpctl_dev_arr[idx_dev].bp_i80=1;
            if (PEGF80_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                bpctl_dev_arr[idx_dev].bp_i80=1;
            if ((bpctl_dev_arr[idx_dev].subdevice&0xfe0)==0xb60)
                bpctl_dev_arr[idx_dev].bp_fiber5=1;
            if ((bpctl_dev_arr[idx_dev].subdevice&0xfc0)==0xb40)
                bpctl_dev_arr[idx_dev].bp_fiber5=1;
            if ((bpctl_dev_arr[idx_dev].subdevice&0xfe0)==0xaa0)
                bpctl_dev_arr[idx_dev].bp_i80=1;
			if (BP40_IF_SERIES(pdev1->subsystem_device)) {
                bpctl_dev_arr[idx_dev].bp_40g=1;
			}

			if (bpctl_dev_arr[idx_dev].bp_10g9) {
				unsigned int ctrl= 0;

				
				ctrl = BP10G_READ_REG(&bpctl_dev_arr[idx_dev], DCA_ID);

				bpctl_dev_arr[idx_dev].func_p = ctrl & 0x7;
				bpctl_dev_arr[idx_dev].slot_p = (ctrl >> 3) & 0x1f; 
				bpctl_dev_arr[idx_dev].bus_p = (ctrl >> 8) & 0xff;

			} else if (bpctl_dev_arr[idx_dev].bp_i80) {
            
				unsigned int ctrl= 0;
				
				ctrl = BPCTL_READ_REG(&bpctl_dev_arr[idx_dev], DCA_ID);

				bpctl_dev_arr[idx_dev].func_p = ctrl & 0x7;
				bpctl_dev_arr[idx_dev].slot_p = (ctrl >> 3) & 0x1f; 
				bpctl_dev_arr[idx_dev].bus_p = (ctrl >> 8) & 0xff;

			}

			else if (bpctl_dev_arr[idx_dev].bp_40g) {
				unsigned int ctrl= 0;

				
                ctrl = BP40G_RD_REG(&bpctl_dev_arr[idx_dev], PF_FUNC_RID);

				bpctl_dev_arr[idx_dev].func_p = ctrl & 0x7;
				bpctl_dev_arr[idx_dev].slot_p = (ctrl >> 3) & 0x1f; 
				bpctl_dev_arr[idx_dev].bus_p = (ctrl >> 8) & 0xff;


			} else {
                bpctl_dev_arr[idx_dev].func_p = bpctl_dev_arr[idx_dev].func;
				bpctl_dev_arr[idx_dev].slot_p = bpctl_dev_arr[idx_dev].slot; 
				bpctl_dev_arr[idx_dev].bus_p = bpctl_dev_arr[idx_dev].bus;

            }
#ifdef BPVM_KVM
			if ((PEGF5_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))||
                (PEG5_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))||
                (BP71_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))||
                (bpctl_dev_arr[idx_dev].bp_i80==1)) {
                int k=0;
                u8 data_cmp[]={0xf3, 0xb4, 0x26, 0x4b, 0x90, 0x25, 0x98, 0x9f, 0x12, 0x7f, 0x8e, 0x82, 0x99, 0x83, 0x23, 0x30, 0xef, 0xe0, 0x68, 0x8a};
                u16 offset= (BP71_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))?0x400:0x1000;

                bpvm_read_nvm_eerd(&bpctl_dev_arr[idx_dev], offset, 10, (u16 *)&bpctl_dev_arr[idx_dev].oem_data);

                if (!memcmp(&bpctl_dev_arr[idx_dev].oem_data, &data_cmp, 20))
                    bpctl_dev_arr[idx_dev].rb_oem=1;

                printk("bpvm oem data reading: ");
                for (k=0;k<20;k++) {
                    printk("%x ", bpctl_dev_arr[idx_dev].oem_data[k]);
                }
                printk("\n");
            }


            bpvm_id_led_init(&bpctl_dev_arr[idx_dev]);
 #endif /* BPVM_KVM */

            if (BP10GB_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice)) {
                if (bpctl_dev_arr[idx_dev].ifindex==0) {
                    unregister_chrdev(major_num, DEVICE_NAME);
                    printk("Please load network driver for %s adapter!\n",bpctl_dev_arr[idx_dev].name);
                    return -1;
                }

                if (bpctl_dev_arr[idx_dev].ndev) {
                    if (!(bpctl_dev_arr[idx_dev].ndev->flags&IFF_UP)) {
                        if (!(bpctl_dev_arr[idx_dev].ndev->flags&IFF_UP)) {
                            unregister_chrdev(major_num, DEVICE_NAME);
                            printk("Please bring up network interfaces for %s adapter!\n",
                                   bpctl_dev_arr[idx_dev].name);
                            return -1;
                        }

                    }
                }
                bpctl_dev_arr[idx_dev].bp_10gb=1;
            }

#ifdef BP_SELF_TEST
            if ((bpctl_dev_arr[idx_dev].bp_tx_data=kmalloc(BPTEST_DATA_LEN, GFP_KERNEL))) {


                memset( bpctl_dev_arr[idx_dev].bp_tx_data,0x0,BPTEST_DATA_LEN);

                memset( bpctl_dev_arr[idx_dev].bp_tx_data,0xff,6);
                memset( bpctl_dev_arr[idx_dev].bp_tx_data+6,0x0,1);
                memset( bpctl_dev_arr[idx_dev].bp_tx_data+7,0xaa,5);


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
                bpctl_dev_arr[idx_dev].bp_tx_data[12]=(ETH_P_BPTEST>>8)&0xff;
                bpctl_dev_arr[idx_dev].bp_tx_data[13]=ETH_P_BPTEST&0xff;
#else
                *(__be16 *)(bpctl_dev_arr[idx_dev].bp_tx_data+12)=htons(ETH_P_BPTEST);
#endif

            } else
                printk("bp_ctl: Memory allocation error!\n");
#endif
            idx_dev++;


        }
    }
    if_scan_init();

    sema_init (&bpctl_sema, 1);
#ifdef BPVM_KVM
	spin_lock_init(&bypass_wr_lock);
#endif

    {

        for (idx_dev = 0; ((idx_dev<device_num)&&(bpctl_dev_arr[idx_dev].pdev!=NULL)); idx_dev++) 
       {
			{
				bpctl_dev_t *pbpctl_dev_c= NULL;

                if((bpctl_dev_arr[idx_dev].device == 0x1572) ||
                    (bpctl_dev_arr[idx_dev].device == 0x1583)) 
                {

                    unsigned int ctrl= 0;


                    ctrl = BP40G_RD_REG(&bpctl_dev_arr[idx_dev], PF_FUNC_RID);

                    bpctl_dev_arr[idx_dev].func_p = ctrl & 0x7;
                    bpctl_dev_arr[idx_dev].slot_p = (ctrl >> 3) & 0x1f; 
                    bpctl_dev_arr[idx_dev].bus_p = (ctrl >> 8) & 0xff;

                }


                
                pbpctl_dev_c = get_status40_port_fn(&bpctl_dev_arr[idx_dev]);


				if  ((bpctl_dev_arr[idx_dev].func_p!=0)&&(pbpctl_dev_c)) {

					if(pbpctl_dev_c->bp_40g) {
						bpctl_dev_arr[idx_dev].bp_40g=1;
						bpctl_dev_arr[idx_dev].desc=pbpctl_dev_c->desc; 
						bpctl_dev_arr[idx_dev].name=pbpctl_dev_c->name;
					}
				}
			}
 


            if (is_bypass_fn(&bpctl_dev_arr[idx_dev])) {
                printk(KERN_INFO "%s found, ", bpctl_dev_arr[idx_dev].name);
				if (bpctl_dev_arr[idx_dev].bp_rang==1) {
					bpctl_dev_arr[idx_dev].bp_fw_ver=bypass_fw_ver(&bpctl_dev_arr[idx_dev]);
				   // printk("firmware version: 0x%x\n",bpctl_dev_arr[idx_dev].bp_fw_ver);
					//return 0;
				}
				else if (bpctl_dev_arr[idx_dev].bp_40g==1)
					bpctl_dev_arr[idx_dev].bp_fw_ver=bypass_fw_ver(&bpctl_dev_arr[idx_dev]);
                else if ((OLD_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice))||
                    (INTEL_IF_SERIES(bpctl_dev_arr[idx_dev].subdevice)))
                    bpctl_dev_arr[idx_dev].bp_fw_ver=0xff;
                else
                    bpctl_dev_arr[idx_dev].bp_fw_ver=bypass_fw_ver(&bpctl_dev_arr[idx_dev]);
                if ((bpctl_dev_arr[idx_dev].bp_10gb==1)&&
                    (bpctl_dev_arr[idx_dev].bp_fw_ver==0xff)) {
                    int cnt=100;
                    while (cnt--) {
                        iounmap ((void *)(bpctl_dev_arr[idx_dev].mem_map));
                        mmio_start = pci_resource_start (pdev1, 0);
                        mmio_len = pci_resource_len (pdev1, 0); 

                        bpctl_dev_arr[idx_dev].mem_map=(unsigned long)ioremap(mmio_start,mmio_len);

                        bpctl_dev_arr[idx_dev].bp_fw_ver=bypass_fw_ver(&bpctl_dev_arr[idx_dev]);
                        if (bpctl_dev_arr[idx_dev].bp_fw_ver==0xa8)
                            break;

                    }
                }
                //bpctl_dev_arr[idx_dev].bp_fw_ver=0xa8;
                printk("firmware version: 0x%x\n",bpctl_dev_arr[idx_dev].bp_fw_ver);
            }
            bpctl_dev_arr[idx_dev].wdt_status=WDT_STATUS_UNKNOWN;
            bpctl_dev_arr[idx_dev].reset_time=0;
            atomic_set(&bpctl_dev_arr[idx_dev].wdt_busy,0);
            bpctl_dev_arr[idx_dev].bp_status_un=1;



            bypass_caps_init(&bpctl_dev_arr[idx_dev]);
			get_tpl_fn(&bpctl_dev_arr[idx_dev]);
            init_bypass_wd_auto(&bpctl_dev_arr[idx_dev]);
            init_bypass_tpl_auto(&bpctl_dev_arr[idx_dev]);
            if (NOKIA_SERIES(bpctl_dev_arr[idx_dev].subdevice))
                reset_cont(&bpctl_dev_arr[idx_dev]) ;
        }
}





#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    inter_module_register("is_bypass_sd", THIS_MODULE, &is_bypass_sd);
    inter_module_register("get_bypass_slave_sd", THIS_MODULE, &get_bypass_slave_sd);
    inter_module_register("get_bypass_caps_sd", THIS_MODULE, &get_bypass_caps_sd);
    inter_module_register("get_wd_set_caps_sd", THIS_MODULE, &get_wd_set_caps_sd);
    inter_module_register("set_bypass_sd", THIS_MODULE, &set_bypass_sd);
    inter_module_register("get_bypass_sd", THIS_MODULE, &get_bypass_sd);
    inter_module_register("get_bypass_change_sd", THIS_MODULE, &get_bypass_change_sd);
    inter_module_register("set_dis_bypass_sd", THIS_MODULE, &set_dis_bypass_sd);
    inter_module_register("get_dis_bypass_sd", THIS_MODULE, &get_dis_bypass_sd);
    inter_module_register("set_bypass_pwoff_sd", THIS_MODULE, &set_bypass_pwoff_sd);
    inter_module_register("get_bypass_pwoff_sd", THIS_MODULE, &get_bypass_pwoff_sd);
    inter_module_register("set_bypass_pwup_sd", THIS_MODULE, &set_bypass_pwup_sd);
    inter_module_register("get_bypass_pwup_sd", THIS_MODULE, &get_bypass_pwup_sd);
    inter_module_register("get_bypass_wd_sd", THIS_MODULE, &get_bypass_wd_sd);
    inter_module_register("set_bypass_wd_sd", THIS_MODULE, &set_bypass_wd_sd);
    inter_module_register("get_wd_expire_time_sd", THIS_MODULE, &get_wd_expire_time_sd);
    inter_module_register("reset_bypass_wd_timer_sd", THIS_MODULE, &reset_bypass_wd_timer_sd);
    inter_module_register("set_std_nic_sd", THIS_MODULE, &set_std_nic_sd);
    inter_module_register("get_std_nic_sd", THIS_MODULE, &get_std_nic_sd);
    inter_module_register("set_tx_sd", THIS_MODULE, &set_tx_sd);
    inter_module_register("get_tx_sd", THIS_MODULE, &get_tx_sd);
    inter_module_register("set_tpl_sd", THIS_MODULE, &set_tpl_sd);
    inter_module_register("get_tpl_sd", THIS_MODULE, &get_tpl_sd);

    inter_module_register("set_bp_hw_reset_sd", THIS_MODULE, &set_bp_hw_reset_sd);
    inter_module_register("get_bp_hw_reset_sd", THIS_MODULE, &get_bp_hw_reset_sd);

    inter_module_register("set_tap_sd", THIS_MODULE, &set_tap_sd);
    inter_module_register("get_tap_sd", THIS_MODULE, &get_tap_sd);
    inter_module_register("get_tap_change_sd", THIS_MODULE, &get_tap_change_sd);
    inter_module_register("set_dis_tap_sd", THIS_MODULE, &set_dis_tap_sd);
    inter_module_register("get_dis_tap_sd", THIS_MODULE, &get_dis_tap_sd);
    inter_module_register("set_tap_pwup_sd", THIS_MODULE, &set_tap_pwup_sd);
    inter_module_register("get_tap_pwup_sd", THIS_MODULE, &get_tap_pwup_sd);
    inter_module_register("set_bp_disc_sd", THIS_MODULE, &set_bp_disc_sd);
    inter_module_register("get_bp_disc_sd", THIS_MODULE, &get_bp_disc_sd);
    inter_module_register("get_bp_disc_change_sd", THIS_MODULE, &get_bp_disc_change_sd);
    inter_module_register("set_bp_dis_disc_sd", THIS_MODULE, &set_bp_dis_disc_sd);
    inter_module_register("get_bp_dis_disc_sd", THIS_MODULE, &get_bp_dis_disc_sd);
    inter_module_register("set_bp_disc_pwup_sd", THIS_MODULE, &set_bp_disc_pwup_sd);
    inter_module_register("get_bp_disc_pwup_sd", THIS_MODULE, &get_bp_disc_pwup_sd);
    inter_module_register("set_wd_exp_mode_sd", THIS_MODULE, &set_wd_exp_mode_sd);
    inter_module_register("get_wd_exp_mode_sd", THIS_MODULE, &get_wd_exp_mode_sd);
    inter_module_register("set_wd_autoreset_sd", THIS_MODULE, &set_wd_autoreset_sd);
    inter_module_register("get_wd_autoreset_sd", THIS_MODULE, &get_wd_autoreset_sd);
    inter_module_register("get_bypass_info_sd", THIS_MODULE, &get_bypass_info_sd);
    inter_module_register("bp_if_scan_sd", THIS_MODULE, &bp_if_scan_sd);

#endif
    register_netdevice_notifier(&bp_notifier_block);
    register_reboot_notifier(&bp_reboot_block);
#ifdef BP_PROC_SUPPORT
    {
        int i=0;
        bp_proc_create();
        for (i = 0; i < device_num; i++) {
            if (bpctl_dev_arr[i].ifindex) {
                bypass_proc_remove_dev_sd(&bpctl_dev_arr[i]);
                bypass_proc_create_dev_sd(&bpctl_dev_arr[i]);
            }

        }
    }
#endif

    return 0;
}
/*
* Cleanup - unregister the appropriate file from /proc
*/
static void __exit bypass_cleanup_module(void)
{
    int i ;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))      
    int ret;
#endif
    unregister_netdevice_notifier(&bp_notifier_block);
    unregister_reboot_notifier(&bp_reboot_block);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
    inter_module_unregister("is_bypass_sd");
    inter_module_unregister("get_bypass_slave_sd");
    inter_module_unregister("get_bypass_caps_sd");
    inter_module_unregister("get_wd_set_caps_sd");
    inter_module_unregister("set_bypass_sd");
    inter_module_unregister("get_bypass_sd");
    inter_module_unregister("get_bypass_change_sd");
    inter_module_unregister("set_dis_bypass_sd");
    inter_module_unregister("get_dis_bypass_sd");
    inter_module_unregister("set_bypass_pwoff_sd");
    inter_module_unregister("get_bypass_pwoff_sd");
    inter_module_unregister("set_bypass_pwup_sd");
    inter_module_unregister("get_bypass_pwup_sd");
    inter_module_unregister("set_bypass_wd_sd");
    inter_module_unregister("get_bypass_wd_sd");
    inter_module_unregister("get_wd_expire_time_sd");
    inter_module_unregister("reset_bypass_wd_timer_sd");
    inter_module_unregister("set_std_nic_sd");
    inter_module_unregister("get_std_nic_sd");
    inter_module_unregister("set_tx_sd");
    inter_module_unregister("get_tx_sd");
    inter_module_unregister("set_tpl_sd");
    inter_module_unregister("get_tpl_sd");
    inter_module_unregister("set_tap_sd");
    inter_module_unregister("get_tap_sd");
    inter_module_unregister("get_tap_change_sd");
    inter_module_unregister("set_dis_tap_sd");
    inter_module_unregister("get_dis_tap_sd");
    inter_module_unregister("set_tap_pwup_sd");
    inter_module_unregister("get_tap_pwup_sd");
    inter_module_unregister("set_bp_disc_sd");
    inter_module_unregister("get_bp_disc_sd");
    inter_module_unregister("get_bp_disc_change_sd");
    inter_module_unregister("set_bp_dis_disc_sd");
    inter_module_unregister("get_bp_dis_disc_sd");
    inter_module_unregister("set_bp_disc_pwup_sd");
    inter_module_unregister("get_bp_disc_pwup_sd");
    inter_module_unregister("set_wd_exp_mode_sd");
    inter_module_unregister("get_wd_exp_mode_sd");
    inter_module_unregister("set_wd_autoreset_sd");
    inter_module_unregister("get_wd_autoreset_sd");
    inter_module_unregister("get_bypass_info_sd");
    inter_module_unregister("bp_if_scan_sd");

#endif

    for (i = 0; i < device_num; i++) {
        //unsigned long flags;
#ifdef BP_PROC_SUPPORT
        bypass_proc_remove_dev_sd(&bpctl_dev_arr[i]);
#endif
        remove_bypass_wd_auto(&bpctl_dev_arr[i]);
        bpctl_dev_arr[i].reset_time=0;

        remove_bypass_tpl_auto(&bpctl_dev_arr[i]);
    }
#ifdef BP_PROC_SUPPORT

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) )
        remove_proc_entry(BP_PROC_DIR, proc_net);
#else
	remove_proc_entry(BP_PROC_DIR, init_net.proc_net);
#endif

#endif


    /* unmap all devices */
    for (i = 0; i < device_num; i++) {
#ifdef BP_SELF_TEST
        if (bpctl_dev_arr[i].bp_tx_data)
            kfree (bpctl_dev_arr[i].bp_tx_data);
#endif     
        iounmap ((void *)(bpctl_dev_arr[i].mem_map));
    }

    /* free all devices space */
    if (bpctl_dev_arr) {
		kfree (bpctl_dev_arr);
	}


/*
* Unregister the device                             
*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))
    ret = unregister_chrdev(major_num, DEVICE_NAME);
/*
* If there's an error, report it
*/
    if (ret < 0) {
		printk("Error in module_unregister_chrdev: %d\n", ret);
	}
#else
    unregister_chrdev(major_num, DEVICE_NAME);

#endif
}

 
#ifdef ADI_RANGELEY_SUPPORT

static int sib_i2c_scan(struct device *dev, void *dummy)
{
	struct i2c_adapter *adap;
    int status = 0;
    struct i2c_client *tmp_client = NULL;
 
    if (dev->type != &i2c_adapter_type) {
		return 0;
	}
     
    adap = to_i2c_adapter(dev);
	tmp_client = kzalloc(sizeof(*client), GFP_KERNEL);

	if (!tmp_client) {
		return -ENOMEM;
	}
	snprintf(tmp_client->name, I2C_NAME_SIZE, "i2c-dev %d", adap->nr);
	tmp_client->adapter = adap;
	tmp_client->addr = 0x57;
	status = i2c_smbus_read_byte(tmp_client);
	if(status > 0) {
		client = tmp_client;
	} else {
		kfree(tmp_client);
	}
	return 0;
}
 
static void init_i2c_scan(void)
{ 
    i2c_for_each_dev(NULL, sib_i2c_scan); 
}

#endif /* ADI_RANGELEY_SUPPORT */

static int __init bypass_init_module(void)
{
    int ret;

    printk(KERN_INFO BP_MOD_DESCR" v"BP_MOD_VER"\n"); 

#ifdef ADI_RANGELEY_SUPPORT 

    init_i2c_scan();

	if(client) {
		struct pci_dev *pdev1 = NULL;
		unsigned int mmio_start = 0;
		int ret_val = 0;

		while ((pdev1 = pci_get_device(0x8086,
                                 PCI_ANY_ID,
                                 pdev1))) {
			if ((pdev1->bus->number == 0)&&(PCI_SLOT(pdev1->devfn) == 31)&&(PCI_FUNC(pdev1->devfn) == 0)) {
				na_device_num++;
				break;
			}
		}

		if (na_device_num != 1) 
			goto v2_init;

		pdev1 = pci_get_device(0x8086, 0x1f38, NULL);
		if(!pdev1)
			goto v2_init;

		ret_val = pci_read_config_dword(pdev1, 0x48, &(mmio_start));
		if(!mmio_start) {
			goto v2_init;
		}
	   // pci_dev_put(pdev1);
		mmio_start&= 0x0000ff80;

		na_base = mmio_start;


		/* obtain exclusive access to GPIO memory space */
    //  printk("!!!mmio_start=%x, ret_val=%d\n", mmio_start,ret_val);

#if 0
#define BPNAG_WRITE_REG(a, reg, value) ({ \
        outl((unsigned long)value,(((a)->na_base)+reg));})
        
         
#define BPNAG_READ_REG(a, reg, value) ({ \
        value=inl(((a)->na_base)+reg);})
 
		printk("bpmod SC_USE_SEL = 0x%x\n", inl(na_base + 0));
		printk("bpmod SC_IO_SEL = 0x%x\n", inl(na_base + 0x4));
		printk("bpmod SC_GP_LVL = 0x%x\n", inl(na_base + 0x8));

		printk("bpmod SUS_USE_SEL = 0x%x\n", inl(na_base + 0x80));
		outl((unsigned long)(BIT_17 | BIT_18),(na_base + 0x84));
		printk("bpmod SUS_IO_SEL = 0x%x\n", inl(na_base + 0x84));
		printk("bpmod SUS_GP_LVL = 0x%x\n", inl(na_base + 0x88));
#endif

	}

v2_init:
#endif /* ADI_RANGELEY_SUPPORT */
    ret=v2_bypass_init_module();
#ifdef BPVM_KVM
    if (!ret) {
		ret=bpvm_probe();
	}
#endif
    return ret;
}


static void __exit bypass_exit_module(void)
{
/*	pci_unregister_driver(&bpvm_driver); */
    bypass_cleanup_module();
#ifdef BPVM_KVM
    unregister_netdev(netdev);
    free_netdev(netdev);
#endif /* BPVM_KVM */
#ifdef ADI_RANGELEY_SUPPORT 
	if(client) {
		kfree(client);
	}
#endif /* ADI_RANGELEY_SUPPORT */
}

module_exit(bypass_exit_module);
module_init(bypass_init_module);


int is_bypass_sd(int ifindex){
    return(is_bypass_fn(get_dev_idx_p(ifindex)));
}

int set_bypass_sd (int ifindex, int bypass_mode){

    return(set_bypass_fn(get_dev_idx_p(ifindex),bypass_mode));
}


int get_bypass_sd (int ifindex){

    return(get_bypass_fn(get_dev_idx_p(ifindex)));
}

int get_wd_expire_sd (int ifindex){

    return(get_wd_expire_fn(get_dev_idx_p(ifindex)));
}


int get_bypass_change_sd(int ifindex){

    return(get_bypass_change_fn(get_dev_idx_p(ifindex)));
}

int set_dis_bypass_sd(int ifindex, int dis_param){
    return(set_dis_bypass_fn(get_dev_idx_p(ifindex),dis_param));
}

int get_dis_bypass_sd(int ifindex){

    return(get_dis_bypass_fn(get_dev_idx_p(ifindex)));
}

int set_bypass_pwoff_sd (int ifindex, int bypass_mode){
    return(set_bypass_pwoff_fn(get_dev_idx_p(ifindex),bypass_mode));

}


int get_bypass_pwoff_sd(int ifindex){
    return(get_bypass_pwoff_fn(get_dev_idx_p(ifindex)));

}


int set_bypass_pwup_sd(int ifindex, int bypass_mode){
    return(set_bypass_pwup_fn(get_dev_idx_p(ifindex),bypass_mode));

}

int get_bypass_pwup_sd(int ifindex){
    return(get_bypass_pwup_fn(get_dev_idx_p(ifindex)));

}

int set_bypass_wd_sd(int if_index, int ms_timeout, int *ms_timeout_set){
    if ((is_bypass_fn(get_dev_idx_p(if_index)))<=0)
        return BP_NOT_CAP;
    *ms_timeout_set= set_bypass_wd_fn(get_dev_idx_p(if_index),ms_timeout);
    return 0;
}

int get_bypass_wd_sd(int ifindex, int *timeout){
    return(get_bypass_wd_fn(get_dev_idx_p(ifindex),timeout));

}

int get_wd_expire_time_sd(int ifindex, int *time_left){
    return(get_wd_expire_time_fn(get_dev_idx_p(ifindex),time_left));
}

int reset_bypass_wd_timer_sd(int ifindex){
    return(reset_bypass_wd_timer_fn(get_dev_idx_p(ifindex)));

}

int get_wd_set_caps_sd(int ifindex){
    return(get_wd_set_caps_fn(get_dev_idx_p(ifindex)));

}

int set_std_nic_sd(int ifindex, int nic_mode){
    return(set_std_nic_fn(get_dev_idx_p(ifindex),nic_mode));

}

int get_std_nic_sd(int ifindex){
    return(get_std_nic_fn(get_dev_idx_p(ifindex)));

}

int set_tap_sd (int ifindex, int tap_mode){
    return(set_tap_fn(get_dev_idx_p(ifindex),tap_mode));

}

int get_tap_sd (int ifindex){
    return(get_tap_fn(get_dev_idx_p(ifindex)));

}

int set_tap_pwup_sd(int ifindex, int tap_mode){
    return(set_tap_pwup_fn(get_dev_idx_p(ifindex),tap_mode));

}

int get_tap_pwup_sd(int ifindex){
    return(get_tap_pwup_fn(get_dev_idx_p(ifindex)));

}

int get_tap_change_sd(int ifindex){
    return(get_tap_change_fn(get_dev_idx_p(ifindex)));

}

int set_dis_tap_sd(int ifindex, int dis_param){
    return(set_dis_tap_fn(get_dev_idx_p(ifindex),dis_param));

}

int get_dis_tap_sd(int ifindex){
    return(get_dis_tap_fn(get_dev_idx_p(ifindex)));

}
int set_bp_disc_sd (int ifindex, int disc_mode){
    return(set_disc_fn(get_dev_idx_p(ifindex),disc_mode));

}

int get_bp_disc_sd (int ifindex){
    return(get_disc_fn(get_dev_idx_p(ifindex)));

}

int set_bp_disc_pwup_sd(int ifindex, int disc_mode){
    return(set_disc_pwup_fn(get_dev_idx_p(ifindex),disc_mode));

}

int get_bp_disc_pwup_sd(int ifindex){
    return(get_disc_pwup_fn(get_dev_idx_p(ifindex)));

}

int get_bp_disc_change_sd(int ifindex){
    return(get_disc_change_fn(get_dev_idx_p(ifindex)));

}

int set_bp_dis_disc_sd(int ifindex, int dis_param){
    return(set_dis_disc_fn(get_dev_idx_p(ifindex),dis_param));

}

int get_bp_dis_disc_sd(int ifindex){
    return(get_dis_disc_fn(get_dev_idx_p(ifindex)));

}

int get_wd_exp_mode_sd(int ifindex){
    return(get_wd_exp_mode_fn(get_dev_idx_p(ifindex)));
}

int set_wd_exp_mode_sd(int ifindex, int param){
    return(set_wd_exp_mode_fn(get_dev_idx_p(ifindex),param));

}

int reset_cont_sd (int ifindex){
    return(reset_cont_fn(get_dev_idx_p(ifindex)));

}


int set_tx_sd(int ifindex, int tx_state){
    return(set_tx_fn(get_dev_idx_p(ifindex), tx_state));

}

int set_tpl_sd(int ifindex, int tpl_state){
    return(set_tpl_fn(get_dev_idx_p(ifindex), tpl_state));

}

int set_bp_hw_reset_sd(int ifindex, int status){
    return(set_bp_hw_reset_fn(get_dev_idx_p(ifindex), status));

}


int set_wd_autoreset_sd(int ifindex, int param){
    return(set_wd_autoreset_fn(get_dev_idx_p(ifindex),param));

}

int get_wd_autoreset_sd(int ifindex){
    return(get_wd_autoreset_fn(get_dev_idx_p(ifindex)));

}


int get_bypass_caps_sd(int ifindex){
    return(get_bypass_caps_fn(get_dev_idx_p(ifindex)));
}

int get_bypass_slave_sd(int ifindex){
    bpctl_dev_t *pbpctl_dev_out;
    int ret=get_bypass_slave_fn(get_dev_idx_p(ifindex), &pbpctl_dev_out);
    if (ret==1)
        return(pbpctl_dev_out->ifindex) ;
    return -1;

}

int get_tx_sd(int ifindex){
    return(get_tx_fn(get_dev_idx_p(ifindex)));

}

int get_tpl_sd(int ifindex){
    return(get_tpl_fn(get_dev_idx_p(ifindex)));

}

int get_bp_hw_reset_sd(int ifindex){
    return(get_bp_hw_reset_fn(get_dev_idx_p(ifindex)));

}


int get_bypass_info_sd(int ifindex, struct bp_info *bp_info) {
    return(get_bypass_info_fn(get_dev_idx_p(ifindex), bp_info->prod_name, &bp_info->fw_ver));
}

int bp_if_scan_sd(void) {
    if_scan_init();
    return 0;
}


EXPORT_SYMBOL_NOVERS(is_bypass_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_slave_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_caps_sd);
EXPORT_SYMBOL_NOVERS(get_wd_set_caps_sd);
EXPORT_SYMBOL_NOVERS(set_bypass_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_change_sd);
EXPORT_SYMBOL_NOVERS(set_dis_bypass_sd);
EXPORT_SYMBOL_NOVERS(get_dis_bypass_sd);
EXPORT_SYMBOL_NOVERS(set_bypass_pwoff_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_pwoff_sd);
EXPORT_SYMBOL_NOVERS(set_bypass_pwup_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_pwup_sd);
EXPORT_SYMBOL_NOVERS(set_bypass_wd_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_wd_sd);
EXPORT_SYMBOL_NOVERS(get_wd_expire_time_sd);
EXPORT_SYMBOL_NOVERS(reset_bypass_wd_timer_sd);
EXPORT_SYMBOL_NOVERS(set_std_nic_sd);
EXPORT_SYMBOL_NOVERS(get_std_nic_sd);
EXPORT_SYMBOL_NOVERS(set_tx_sd);
EXPORT_SYMBOL_NOVERS(get_tx_sd);
EXPORT_SYMBOL_NOVERS(set_tpl_sd);
EXPORT_SYMBOL_NOVERS(get_tpl_sd);
EXPORT_SYMBOL_NOVERS(set_bp_hw_reset_sd);
EXPORT_SYMBOL_NOVERS(get_bp_hw_reset_sd);
EXPORT_SYMBOL_NOVERS(set_tap_sd);
EXPORT_SYMBOL_NOVERS(get_tap_sd);
EXPORT_SYMBOL_NOVERS(get_tap_change_sd);
EXPORT_SYMBOL_NOVERS(set_dis_tap_sd);
EXPORT_SYMBOL_NOVERS(get_dis_tap_sd);
EXPORT_SYMBOL_NOVERS(set_tap_pwup_sd);
EXPORT_SYMBOL_NOVERS(get_tap_pwup_sd);
EXPORT_SYMBOL_NOVERS(set_wd_exp_mode_sd);
EXPORT_SYMBOL_NOVERS(get_wd_exp_mode_sd);
EXPORT_SYMBOL_NOVERS(set_wd_autoreset_sd);
EXPORT_SYMBOL_NOVERS(get_wd_autoreset_sd);
EXPORT_SYMBOL_NOVERS(set_bp_disc_sd);
EXPORT_SYMBOL_NOVERS(get_bp_disc_sd);
EXPORT_SYMBOL_NOVERS(get_bp_disc_change_sd);
EXPORT_SYMBOL_NOVERS(set_bp_dis_disc_sd);
EXPORT_SYMBOL_NOVERS(get_bp_dis_disc_sd);
EXPORT_SYMBOL_NOVERS(set_bp_disc_pwup_sd);
EXPORT_SYMBOL_NOVERS(get_bp_disc_pwup_sd);
EXPORT_SYMBOL_NOVERS(get_bypass_info_sd);
EXPORT_SYMBOL_NOVERS(bp_if_scan_sd);

#ifdef BP_PROC_SUPPORT

static struct proc_dir_entry *bp_procfs_dir;

static struct proc_dir_entry *
proc_getdir(char *name, struct proc_dir_entry *proc_dir) {
    struct proc_dir_entry *pde = proc_dir;
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )

    for (pde=pde->subdir; pde; pde = pde->next) {
        if (pde->namelen && (strcmp(name, pde->name) == 0)) {
            /* directory exists */
            break;
        }
    }
    if (pde == (struct proc_dir_entry *) 0)
#endif
    {
        /* create the directory */
#if (LINUX_VERSION_CODE > 0x20300)
        pde = proc_mkdir(name, proc_dir);
#else
        pde = create_proc_entry(name, S_IFDIR, proc_dir);
#endif
        if (pde == (struct proc_dir_entry *) 0) {

            return(pde);
        }
    }

    return(pde);
}

int bp_proc_create(void)
{
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) )
    bp_procfs_dir = proc_getdir(BP_PROC_DIR, proc_net);
#else
    bp_procfs_dir = proc_getdir(BP_PROC_DIR, init_net.proc_net);
#endif

    if (bp_procfs_dir == (struct proc_dir_entry *) 0) {
        printk(KERN_DEBUG "Could not create procfs nicinfo directory %s\n", BP_PROC_DIR);
        return -1;
    }
    return 0;
}





    #if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )

int
bypass_proc_create_entry_sd(struct pfs_unit_sd *pfs_unit_curr,
                            char* proc_name,
                            write_proc_t *write_proc,
                            read_proc_t *read_proc,
                            struct proc_dir_entry *parent_pfs,
                            void *data
                           )
{
    strcpy(pfs_unit_curr->proc_name,proc_name);
    pfs_unit_curr->proc_entry= create_proc_entry(pfs_unit_curr->proc_name,
                                                 S_IFREG|S_IRUSR|S_IWUSR|
                                                 S_IRGRP|S_IWGRP|
                                                 S_IROTH|S_IWOTH, parent_pfs);
    if (pfs_unit_curr->proc_entry == 0) {

        return -1;
    }

    pfs_unit_curr->proc_entry->read_proc = read_proc;
    pfs_unit_curr->proc_entry->write_proc = write_proc;
    pfs_unit_curr->proc_entry->data = data;

    return 0;


}
    #endif



int
get_bypass_info_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;
    int len=0;

    len += sprintf(page, "Name\t\t\t%s\n", pbp_device_block->name);
    len += sprintf(page+len, "Firmware version\t0x%x\n", pbp_device_block->bp_fw_ver);

    *eof = 1;
    return len;
}


int
get_bypass_slave_pfs (char *page, char **start, off_t off, int count,
                      int *eof, void *data)
{
    bpctl_dev_t * dev= (bpctl_dev_t *) data;

    int len=0;

    bpctl_dev_t *slave = get_status_port_fn(dev);
    if (!slave)
        slave = dev;
    if (slave) {
        if (slave->ndev)
            len = sprintf(page, "%s\n", slave->ndev->name);
        else
            len=sprintf(page, "fail\n");   
    }

    *eof = 1;
    return len;

} 



int
get_bypass_caps_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bypass_caps_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "-1\n");
    else
        len=sprintf(page, "0x%x\n", ret);
    *eof = 1;
    return len;


}



int
get_wd_set_caps_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_wd_set_caps_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "-1\n");
    else
        len=sprintf(page, "0x%x\n", ret);
    *eof = 1;
    return len;
}



int
set_bypass_pfs(struct file *file, const char *buffer,
               unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"on")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"off")==0)
        bypass_param=0;


    set_bypass_fn (pbp_device_block, bypass_param);

    return count;
}


int
set_tap_pfs(struct file *file, const char *buffer,
            unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_tap_fn(pbp_device_block, tap_param);

    return count;
}



int
set_disc_pfs(struct file *file, const char *buffer,
             unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_disc_fn(pbp_device_block, tap_param);

    return count;
}




int
get_bypass_pfs (char *page, char **start, off_t off, int count,
                int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bypass_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}

int
get_tap_pfs (char *page, char **start, off_t off, int count,
             int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_tap_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}


int
get_disc_pfs (char *page, char **start, off_t off, int count,
              int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_disc_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}

int
get_bypass_change_pfs (char *page, char **start, off_t off, int count,
                       int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bypass_change_fn (pbp_device_block);
    if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else  len=sprintf(page, "fail\n");

    *eof = 1;
    return len;
}




int
get_tap_change_pfs (char *page, char **start, off_t off, int count,
                    int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_tap_change_fn (pbp_device_block);
    if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else  len=sprintf(page, "fail\n");

    *eof = 1;
    return len;
}



int
get_disc_change_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_disc_change_fn (pbp_device_block);
    if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else  len=sprintf(page, "fail\n");

    *eof = 1;
    return len;
}




    #define isdigit(c) (c >= '0' && c <= '9')
__inline static int atoi( char **s) 
{
    int i = 0;
    while (isdigit(**s))
        i = i*10 + *((*s)++) - '0';
    return i;
}


int
set_bypass_wd_pfs(struct file *file, const char *buffer,
                  unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    unsigned int timeout=0;
    char *timeout_ptr=kbuf;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    timeout_ptr=kbuf;
    timeout=atoi(&timeout_ptr);

    set_bypass_wd_fn(pbp_device_block, timeout) ;

    return count;
}

int
get_bypass_wd_pfs (char *page, char **start, off_t off, int count,
                   int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0, timeout=0;

    ret=get_bypass_wd_fn (pbp_device_block, &timeout);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (timeout==-1)
        len=sprintf(page, "unknown\n");
    else if (timeout==0)
        len=sprintf(page, "disable\n");
    else
        len=sprintf(page, "%d\n", timeout);

    *eof = 1;
    return len;
}




int
get_wd_expire_time_pfs (char *page, char **start, off_t off, int count,
                        int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0, timeout=0;

    ret=get_wd_expire_time_fn (pbp_device_block, &timeout);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (timeout==-1)
        len=sprintf(page, "expire\n");
    else if (timeout==0)
        len=sprintf(page, "disable\n");

    else
        len=sprintf(page, "%d\n", timeout);
    *eof = 1;
    return len;
}



int
get_tpl_pfs (char *page, char **start, off_t off, int count,
             int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_tpl_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}



    #ifdef PMC_FIX_FLAG
int
get_wait_at_pwup_pfs (char *page, char **start, off_t off, int count,
                      int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bp_wait_at_pwup_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}



int
get_hw_reset_pfs (char *page, char **start, off_t off, int count,
                  int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bp_hw_reset_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==1)
        len=sprintf(page, "on\n");
    else if (ret==0)
        len=sprintf(page, "off\n");

    *eof = 1;
    return len;
}




    #endif /*PMC_WAIT_FLAG*/


int
reset_bypass_wd_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=reset_bypass_wd_timer_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "disable\n");
    else if (ret==1)
        len=sprintf(page, "success\n");

    *eof = 1;
    return len;
}



int
set_dis_bypass_pfs(struct file *file, const char *buffer,
                   unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"on")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"off")==0)
        bypass_param=0;

    set_dis_bypass_fn (pbp_device_block, bypass_param);

    return count;
}



int
set_dis_tap_pfs(struct file *file, const char *buffer,
                unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_dis_tap_fn (pbp_device_block, tap_param);

    return count;
}



int
set_dis_disc_pfs(struct file *file, const char *buffer,
                 unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_dis_disc_fn (pbp_device_block, tap_param);

    return count;
}




int
get_dis_bypass_pfs (char *page, char **start, off_t off, int count,
                    int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_dis_bypass_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}

int
get_dis_tap_pfs (char *page, char **start, off_t off, int count,
                 int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_dis_tap_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}

int
get_dis_disc_pfs (char *page, char **start, off_t off, int count,
                  int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_dis_disc_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}


int
set_bypass_pwup_pfs(struct file *file, const char *buffer,
                    unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"off")==0)
        bypass_param=0;

    set_bypass_pwup_fn (pbp_device_block, bypass_param);

    return count;
}


int
set_bypass_pwoff_pfs(struct file *file, const char *buffer,
                     unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"off")==0)
        bypass_param=0;

    set_bypass_pwoff_fn (pbp_device_block, bypass_param);

    return count;
}




int
set_tap_pwup_pfs(struct file *file, const char *buffer,
                 unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_tap_pwup_fn (pbp_device_block, tap_param);

    return count;
}



int
set_disc_pwup_pfs(struct file *file, const char *buffer,
                  unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tap_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tap_param=1;
    else if (strcmp(kbuf,"off")==0)
        tap_param=0;

    set_disc_pwup_fn (pbp_device_block, tap_param);

    return count;
}



int
get_bypass_pwup_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bypass_pwup_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}



int
get_bypass_pwoff_pfs (char *page, char **start, off_t off, int count,
                      int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_bypass_pwoff_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}


int
get_tap_pwup_pfs (char *page, char **start, off_t off, int count,
                  int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_tap_pwup_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}

int
get_disc_pwup_pfs (char *page, char **start, off_t off, int count,
                   int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_disc_pwup_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}



int
set_std_nic_pfs(struct file *file, const char *buffer,
                unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"on")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"off")==0)
        bypass_param=0;

    set_std_nic_fn (pbp_device_block, bypass_param);

    return count;
}


int
get_std_nic_pfs (char *page, char **start, off_t off, int count,
                 int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_std_nic_fn (pbp_device_block);
    if (ret==BP_NOT_CAP)
        len=sprintf(page, "fail\n");
    else if (ret==0)
        len=sprintf(page, "off\n");
    else
        len=sprintf(page, "on\n");

    *eof = 1;
    return len;
}



int
get_wd_exp_mode_pfs (char *page, char **start, off_t off, int count,
                     int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_wd_exp_mode_fn (pbp_device_block);
    if (ret==1)
        len=sprintf(page, "tap\n");
    else if (ret==0)
        len=sprintf(page, "bypass\n");
    else if (ret==2)
        len=sprintf(page, "disc\n");

    else len=sprintf(page, "fail\n");

    *eof = 1;
    return len;
}

int
set_wd_exp_mode_pfs(struct file *file, const char *buffer,
                    unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int bypass_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';


    if (strcmp(kbuf,"tap")==0)
        bypass_param=1;
    else if (strcmp(kbuf,"bypass")==0)
        bypass_param=0;
    else if (strcmp(kbuf,"disc")==0)
        bypass_param=2;

    set_wd_exp_mode_fn(pbp_device_block, bypass_param);

    return count;
}





int
get_wd_autoreset_pfs (char *page, char **start, off_t off, int count,
                      int *eof, void *data)
{
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int len=0, ret=0;

    ret=get_wd_autoreset_fn (pbp_device_block);
    if (ret>=0)
        len=sprintf(page, "%d\n",ret);
    else len=sprintf(page, "fail\n");

    *eof = 1;
    return len;
}


int
set_wd_autoreset_pfs(struct file *file, const char *buffer,
                     unsigned long count, void *data)
{
    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;
    u32 timeout=0;
    char *timeout_ptr=kbuf;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    timeout_ptr=kbuf;
    timeout=atoi(&timeout_ptr);

    set_wd_autoreset_fn(pbp_device_block, timeout) ;

    return count;
} 


int
set_tpl_pfs(struct file *file, const char *buffer,
            unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tpl_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tpl_param=1;
    else if (strcmp(kbuf,"off")==0)
        tpl_param=0;

    set_tpl_fn(pbp_device_block, tpl_param);

    return count;
}
    #ifdef PMC_FIX_FLAG
int
set_wait_at_pwup_pfs(struct file *file, const char *buffer,
                     unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tpl_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tpl_param=1;
    else if (strcmp(kbuf,"off")==0)
        tpl_param=0;

    set_bp_wait_at_pwup_fn(pbp_device_block, tpl_param);

    return count;
}

int
set_hw_reset_pfs(struct file *file, const char *buffer,
                 unsigned long count, void *data)
{

    char kbuf[256];
    bpctl_dev_t * pbp_device_block= (bpctl_dev_t *) data;

    int tpl_param=0, length=0;

    if (count>(sizeof(kbuf)-1))
        return -1;

    if (copy_from_user(&kbuf,buffer,count)) {
        return -1;
    }

    kbuf[count]='\0';
    length=strlen(kbuf);
    if (kbuf[length-1]=='\n')
        kbuf[--length]='\0';

    if (strcmp(kbuf,"on")==0)
        tpl_param=1;
    else if (strcmp(kbuf,"off")==0)
        tpl_param=0;

    set_bp_hw_reset_fn(pbp_device_block, tpl_param);

    return count;
}

    #endif /*PMC_FIX_FLAG*/
    #if ( LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0) )
        
static int user_on_off(const void __user *buffer, size_t count)
{

    char kbuf[256];
    int length = 0;

    if (count > (sizeof(kbuf) - 1))
        return -1;

    if (copy_from_user(&kbuf, buffer, count))
        return -1;

    kbuf[count] = '\0';
    length = strlen(kbuf);
    if (kbuf[length - 1] == '\n')
        kbuf[--length] = '\0';

    if (strcmp(kbuf, "on") == 0)
        return 1;
    if (strcmp(kbuf, "off") == 0)
        return 0;
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))

        #define RO_FOPS(name)	\
static int name##_open(struct inode *inode, struct file *file)	\
{								\
	return single_open(file, show_##name, PDE_DATA(inode));\
}								\
static const struct file_operations name##_ops = {		\
	.open = name##_open,					\
	.read = seq_read,					\
	.llseek = seq_lseek,					\
	.release = single_release,				\
};

        #define RW_FOPS(name)	\
static int name##_open(struct inode *inode, struct file *file)	\
{								\
	return single_open(file, show_##name, PDE_DATA(inode));\
}								\
static const struct file_operations name##_ops = {		\
	.open = name##_open,					\
	.read = seq_read,					\
	.write = name##_write,					\
	.llseek = seq_lseek,					\
	.release = single_release,				\
};
#else

        #define RO_FOPS(name)	\
static int name##_open(struct inode *inode, struct file *file)	\
{								\
	return single_open(file, show_##name, PDE_DATA(inode));\
}								\
static const struct proc_ops name##_ops = {		\
	.proc_open = name##_open,					\
	.proc_read = seq_read,					\
	.proc_lseek = seq_lseek,					\
	.proc_release = single_release,				\
};

        #define RW_FOPS(name)	\
static int name##_open(struct inode *inode, struct file *file)	\
{								\
	return single_open(file, show_##name, PDE_DATA(inode));\
}								\
static const struct proc_ops name##_ops = {		\
	.proc_open = name##_open,					\
	.proc_read = seq_read,					\
	.proc_write = name##_write,					\
	.proc_lseek = seq_lseek,					\
	.proc_release = single_release,				\
};
#endif

static int show_bypass_info(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;

    seq_printf(m, "Name\t\t\t%s\n", dev->name);
    seq_printf(m, "Firmware version\t0x%x\n", dev->bp_fw_ver);
    return 0;
}
RO_FOPS(bypass_info)

static int show_bypass_slave(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    bpctl_dev_t *slave;
    int ret = get_bypass_slave_fn(dev, &slave);

    if ((ret==1) && (slave) && (slave->ndev)) {
        seq_printf(m, "%s\n", slave->ndev->name);
    } else {
        seq_printf(m, "fail\n");
    }

    return 0;
}
RO_FOPS(bypass_slave)

static int show_bypass_caps(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bypass_caps_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "-1\n");
    else
        seq_printf(m, "0x%x\n", ret);
    return 0;
}
RO_FOPS(bypass_caps)

static ssize_t std_nic_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    int bypass_param = user_on_off(buffer, count);
    if (bypass_param < 0)
        return -EINVAL;

    set_std_nic_fn(PDE_DATA(file_inode(file)), bypass_param);
    return count;
}
static int show_std_nic(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_std_nic_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(std_nic)


static ssize_t bypass_write(struct file *file, const char __user *buffer,
                            size_t count, loff_t *pos)
{
    int bypass_param = user_on_off(buffer, count);
    if (bypass_param < 0)
        return -1;

    set_bypass_fn(PDE_DATA(file_inode(file)), bypass_param);
    return count;
}
static int show_bypass(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bypass_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
RW_FOPS(bypass)

static ssize_t tap_write(struct file *file, const char __user *buffer,
                         size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -1;

    set_tap_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}
static int show_tap(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_tap_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
RW_FOPS(tap)


static ssize_t disc_write(struct file *file, const char __user *buffer,
                          size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -1;

    set_disc_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}
static int show_disc(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_disc_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
RW_FOPS(disc)

static int show_bypass_change(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bypass_change_fn(dev);
    if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "fail\n");
    return 0;
}
RO_FOPS(bypass_change)

static int show_tap_change(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_tap_change_fn(dev);
    if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "fail\n");
    return 0;
}
RO_FOPS(tap_change)



static int show_disc_change(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_disc_change_fn(dev);
    if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "fail\n");
    return 0;
}
RO_FOPS(disc_change)

static int show_tpl(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_tpl_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
static ssize_t tpl_write(struct file *file, const char __user *buffer,
                         size_t count, loff_t *pos)
{
    bpctl_dev_t *dev = PDE_DATA(file_inode(file));
    int tpl_param = user_on_off(buffer, count);
    if (tpl_param < 0)
        return -1;

    set_tpl_fn(dev, tpl_param);
    return count;
}

RW_FOPS(tpl)

static ssize_t wait_at_pwup_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *pos)
{
    bpctl_dev_t *dev = PDE_DATA(file_inode(file));
    int tpl_param = user_on_off(buffer, count);
    if (tpl_param < 0)
        return -1;

    set_bp_wait_at_pwup_fn(dev, tpl_param);
    return count;
}
static int show_wait_at_pwup(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bp_wait_at_pwup_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
RW_FOPS(wait_at_pwup)

static ssize_t hw_reset_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *pos)
{
    bpctl_dev_t *dev = PDE_DATA(file_inode(file));
    int tpl_param = user_on_off(buffer, count);
    if (tpl_param < 0)
        return -1;

    set_bp_hw_reset_fn(dev, tpl_param);
    return count;
}


static int show_hw_reset(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bp_hw_reset_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 1)
        seq_printf(m, "on\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    return 0;
}
RW_FOPS(hw_reset)

static ssize_t dis_bypass_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *pos)
{
    int bypass_param = user_on_off(buffer, count);
    if (bypass_param < 0)
        return -EINVAL;

    set_dis_bypass_fn(PDE_DATA(file_inode(file)), bypass_param);
    return count;
}
static int show_dis_bypass(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_dis_bypass_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(dis_bypass)

static ssize_t dis_tap_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -EINVAL;

    set_dis_tap_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}

static int show_dis_tap(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_dis_tap_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(dis_tap)

static ssize_t dis_disc_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -EINVAL;

    set_dis_disc_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}
static int show_dis_disc(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_dis_disc_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(dis_disc)

static ssize_t bypass_pwup_write(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *pos)
{
    int bypass_param = user_on_off(buffer, count);
    if (bypass_param < 0)
        return -EINVAL;

    set_bypass_pwup_fn(PDE_DATA(file_inode(file)), bypass_param);
    return count;
}
static int show_bypass_pwup(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bypass_pwup_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(bypass_pwup)

static ssize_t bypass_pwoff_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *pos)
{
    int bypass_param = user_on_off(buffer, count);
    if (bypass_param < 0)
        return -EINVAL;

    set_bypass_pwoff_fn(PDE_DATA(file_inode(file)), bypass_param);
    return count;
}
static int show_bypass_pwoff(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_bypass_pwoff_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(bypass_pwoff)

static ssize_t tap_pwup_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -EINVAL;

    set_tap_pwup_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}
static int show_tap_pwup(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_tap_pwup_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(tap_pwup)

static ssize_t disc_pwup_write(struct file *file, const char __user *buffer,
                               size_t count, loff_t *pos)
{
    int tap_param = user_on_off(buffer, count);
    if (tap_param < 0)
        return -EINVAL;

    set_disc_pwup_fn(PDE_DATA(file_inode(file)), tap_param);
    return count;
}
static int show_disc_pwup(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_disc_pwup_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "off\n");
    else
        seq_printf(m, "on\n");
    return 0;
}
RW_FOPS(disc_pwup)


static int show_reset_bypass_wd(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = reset_bypass_wd_timer_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (ret == 0)
        seq_printf(m, "disable\n");
    else if (ret == 1)
        seq_printf(m, "success\n");
    return 0;
}
RO_FOPS(reset_bypass_wd)






static ssize_t bypass_wd_write(struct file *file, const char __user *buffer,
                               size_t count, loff_t *pos)
{
    bpctl_dev_t *dev = PDE_DATA(file_inode(file));
    int timeout;
    int ret = kstrtoint_from_user(buffer, count, 10, &timeout);
    if (ret)
        return ret;
    set_bypass_wd_fn(dev, timeout);
    return count;
}

static int show_bypass_wd(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = 0, timeout = 0;

    ret = get_bypass_wd_fn(dev, &timeout);
    if (ret == BP_NOT_CAP)
        seq_printf(m,  "fail\n");
    else if (timeout == -1)
        seq_printf(m,  "unknown\n");
    else if (timeout == 0)
        seq_printf(m,  "disable\n");
    else
        seq_printf(m, "%d\n", timeout);
    return 0;
}
RW_FOPS(bypass_wd)

static int show_wd_expire_time(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = 0, timeout = 0;
    ret = get_wd_expire_time_fn(dev, &timeout);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "fail\n");
    else if (timeout == -1)
        seq_printf(m, "expire\n");
    else if (timeout == 0)
        seq_printf(m, "disable\n");
    else
        seq_printf(m, "%d\n", timeout);
    return 0;
}
RO_FOPS(wd_expire_time)



static ssize_t wd_exp_mode_write(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *pos)
{
    char kbuf[256];
    int bypass_param = 0, length = 0;

    if (count > (sizeof(kbuf) - 1))
        return -1;

    if (copy_from_user(&kbuf, buffer, count))
        return -1;

    kbuf[count] = '\0';
    length = strlen(kbuf);
    if (kbuf[length - 1] == '\n')
        kbuf[--length] = '\0';

    if (strcmp(kbuf, "tap") == 0)
        bypass_param = 1;
    else if (strcmp(kbuf, "bypass") == 0)
        bypass_param = 0;
    else if (strcmp(kbuf, "disc") == 0)
        bypass_param = 2;

    set_wd_exp_mode_fn(PDE_DATA(file_inode(file)), bypass_param);

    return count;
}
static int show_wd_exp_mode(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_wd_exp_mode_fn(dev);
    if (ret == 1)
        seq_printf(m, "tap\n");
    else if (ret == 0)
        seq_printf(m, "bypass\n");
    else if (ret == 2)
        seq_printf(m, "disc\n");
    else
        seq_printf(m, "fail\n");
    return 0;
}
RW_FOPS(wd_exp_mode)

static int show_wd_set_caps(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_wd_set_caps_fn(dev);
    if (ret == BP_NOT_CAP)
        seq_printf(m, "-1\n");
    else
        seq_printf(m, "0x%x\n", ret);
    return 0;
}
RO_FOPS(wd_set_caps)



static ssize_t wd_autoreset_write(struct file *file, const char __user *buffer,
                                  size_t count, loff_t *pos)
{
    int timeout;
    int ret = kstrtoint_from_user(buffer, count, 10, &timeout);
    if (ret)
        return ret;
    set_wd_autoreset_fn(PDE_DATA(file_inode(file)), timeout);
    return count;
}
static int show_wd_autoreset(struct seq_file *m, void *v)
{
    bpctl_dev_t *dev = m->private;
    int ret = get_wd_autoreset_fn(dev);
    if (ret >= 0)
        seq_printf(m, "%d\n", ret);
    else
        seq_printf(m, "fail\n");
    return 0;
}
RW_FOPS(wd_autoreset)
    #endif




    #if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )
int bypass_proc_create_dev_sd(bpctl_dev_t * pbp_device_block){
    struct bypass_pfs_sd *current_pfs = &(pbp_device_block->bypass_pfs_set);
    static struct proc_dir_entry *procfs_dir=NULL;
    int ret=0;


    if (!pbp_device_block->ndev)
        return -1;

    sprintf(current_pfs->dir_name,"bypass_%s",pbp_device_block->ndev->name);

    if (!bp_procfs_dir)
        return -1;

    /* create device proc dir */
    procfs_dir = proc_getdir(current_pfs->dir_name,bp_procfs_dir);
    if (procfs_dir == 0) {
        printk(KERN_DEBUG "Could not create procfs directory %s\n",
               current_pfs->dir_name);
        return -1;
    }
    current_pfs->bypass_entry= procfs_dir;

    if (bypass_proc_create_entry_sd(&(current_pfs->bypass_info),
                                    BYPASS_INFO_ENTRY_SD,
                                    NULL,                  /* write */
                                    get_bypass_info_pfs,     /* read  */
                                    procfs_dir,
                                    pbp_device_block))
        ret= -1;

    if (pbp_device_block->bp_caps & SW_CTL_CAP) {


        /* Create set param proc's */
        if (bypass_proc_create_entry_sd(&(current_pfs->bypass_slave),
                                        BYPASS_SLAVE_ENTRY_SD,
                                        NULL,                  /* write */
                                        get_bypass_slave_pfs,     /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;


        if (bypass_proc_create_entry_sd(&(current_pfs->bypass_caps),
                                        BYPASS_CAPS_ENTRY_SD,
                                        NULL,                  /* write */
                                        get_bypass_caps_pfs,     /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;

        if (bypass_proc_create_entry_sd(&(current_pfs->wd_set_caps),
                                        WD_SET_CAPS_ENTRY_SD,
                                        NULL,                  /* write */
                                        get_wd_set_caps_pfs,     /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;
        if (bypass_proc_create_entry_sd(&(current_pfs->bypass_wd),
                                        BYPASS_WD_ENTRY_SD,
                                        set_bypass_wd_pfs,    /* write */
                                        get_bypass_wd_pfs,    /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;


        if (bypass_proc_create_entry_sd(&(current_pfs->wd_expire_time),
                                        WD_EXPIRE_TIME_ENTRY_SD,
                                        NULL,                   /* write */
                                        get_wd_expire_time_pfs, /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;

        if (bypass_proc_create_entry_sd(&(current_pfs->reset_bypass_wd),
                                        RESET_BYPASS_WD_ENTRY_SD,
                                        NULL,                   /* write */
                                        reset_bypass_wd_pfs,    /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;

        if (bypass_proc_create_entry_sd(&(current_pfs->std_nic),
                                        STD_NIC_ENTRY_SD,
                                        set_std_nic_pfs,        /* write */
                                        get_std_nic_pfs,        /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;


        if (pbp_device_block->bp_caps & BP_CAP) {
            if (bypass_proc_create_entry_sd(&(current_pfs->bypass),
                                            BYPASS_ENTRY_SD,
                                            set_bypass_pfs,     /* write */
                                            get_bypass_pfs,     /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->dis_bypass),
                                            DIS_BYPASS_ENTRY_SD,
                                            set_dis_bypass_pfs,    /* write */
                                            get_dis_bypass_pfs,    /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->bypass_pwup),
                                            BYPASS_PWUP_ENTRY_SD,
                                            set_bypass_pwup_pfs,  /* write */
                                            get_bypass_pwup_pfs,  /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
            if (bypass_proc_create_entry_sd(&(current_pfs->bypass_pwoff),
                                            BYPASS_PWOFF_ENTRY_SD,
                                            set_bypass_pwoff_pfs,  /* write */
                                            get_bypass_pwoff_pfs,  /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;


            if (bypass_proc_create_entry_sd(&(current_pfs->bypass_change),
                                            BYPASS_CHANGE_ENTRY_SD,
                                            NULL,                    /* write */
                                            get_bypass_change_pfs,       /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
        }

        if (pbp_device_block->bp_caps & TAP_CAP) {

            if (bypass_proc_create_entry_sd(&(current_pfs->tap),
                                            TAP_ENTRY_SD,
                                            set_tap_pfs,     /* write */
                                            get_tap_pfs,     /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->dis_tap),
                                            DIS_TAP_ENTRY_SD,
                                            set_dis_tap_pfs,    /* write */
                                            get_dis_tap_pfs,    /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->tap_pwup),
                                            TAP_PWUP_ENTRY_SD,
                                            set_tap_pwup_pfs,  /* write */
                                            get_tap_pwup_pfs,  /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->tap_change),
                                            TAP_CHANGE_ENTRY_SD,
                                            NULL,                    /* write */
                                            get_tap_change_pfs,       /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
        }
        if (pbp_device_block->bp_caps & DISC_CAP) {

            if (bypass_proc_create_entry_sd(&(current_pfs->tap),
                                            DISC_ENTRY_SD,
                                            set_disc_pfs,     /* write */
                                            get_disc_pfs,     /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
#if 1

            if (bypass_proc_create_entry_sd(&(current_pfs->dis_tap),
                                            DIS_DISC_ENTRY_SD,
                                            set_dis_disc_pfs,    /* write */
                                            get_dis_disc_pfs,    /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
#endif

            if (bypass_proc_create_entry_sd(&(current_pfs->tap_pwup),
                                            DISC_PWUP_ENTRY_SD,
                                            set_disc_pwup_pfs,  /* write */
                                            get_disc_pwup_pfs,  /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;

            if (bypass_proc_create_entry_sd(&(current_pfs->tap_change),
                                            DISC_CHANGE_ENTRY_SD,
                                            NULL,                    /* write */
                                            get_disc_change_pfs,       /* read  */
                                            procfs_dir,
                                            pbp_device_block))
                ret= -1;
        }


        if (bypass_proc_create_entry_sd(&(current_pfs->wd_exp_mode),
                                        WD_EXP_MODE_ENTRY_SD,
                                        set_wd_exp_mode_pfs,     /* write */
                                        get_wd_exp_mode_pfs,     /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;

        if (bypass_proc_create_entry_sd(&(current_pfs->wd_autoreset),
                                        WD_AUTORESET_ENTRY_SD,
                                        set_wd_autoreset_pfs,     /* write */
                                        get_wd_autoreset_pfs,     /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;
        if (bypass_proc_create_entry_sd(&(current_pfs->tpl),
                                        TPL_ENTRY_SD,
                                        set_tpl_pfs,           /* write */
                                        get_tpl_pfs,           /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;
#ifdef PMC_FIX_FLAG
        if (bypass_proc_create_entry_sd(&(current_pfs->tpl),
                                        WAIT_AT_PWUP_ENTRY_SD,
                                        set_wait_at_pwup_pfs,           /* write */
                                        get_wait_at_pwup_pfs,           /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;
        if (bypass_proc_create_entry_sd(&(current_pfs->tpl),
                                        HW_RESET_ENTRY_SD,
                                        set_hw_reset_pfs,           /* write */
                                        get_hw_reset_pfs,           /* read  */
                                        procfs_dir,
                                        pbp_device_block))
            ret= -1;

#endif

    }
    if (ret<0)
        printk(KERN_DEBUG "Create proc entry failed\n");

    return ret;
}
    #else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0))
static int procfs_add(char *proc_name, const struct file_operations *fops,
#else
static int procfs_add(char *proc_name, const struct proc_ops *fops,
#endif
                      bpctl_dev_t *dev)
{
    struct bypass_pfs_sd *pfs = &dev->bypass_pfs_set;
    if (!proc_create_data(proc_name, 0644, pfs->bypass_entry, fops, dev))
        return -1;
    return 0;
}

int bypass_proc_create_dev_sd(bpctl_dev_t *pbp_device_block)
{
    struct bypass_pfs_sd *current_pfs = &(pbp_device_block->bypass_pfs_set);
    static struct proc_dir_entry *procfs_dir = NULL;
    int ret = 0;

    if (!pbp_device_block->ndev)
        return -1;
    sprintf(current_pfs->dir_name, "bypass_%s",
            pbp_device_block->ndev->name);

    if (!bp_procfs_dir)
        return -1;

    /* create device proc dir */
    procfs_dir = proc_mkdir(current_pfs->dir_name, bp_procfs_dir);
    if (!procfs_dir) {
        printk(KERN_DEBUG "Could not create procfs directory %s\n",
               current_pfs->dir_name);
        return -1;
    }
    current_pfs->bypass_entry = procfs_dir;

#define ENTRY(x) ret |= procfs_add(#x, &x##_ops, pbp_device_block)
    ENTRY(bypass_info);
    if (pbp_device_block->bp_caps & SW_CTL_CAP) {
        /* Create set param proc's */
        ENTRY(bypass_slave);
        ENTRY(bypass_caps);
        ENTRY(wd_set_caps);
        ENTRY(bypass_wd);
        ENTRY(wd_expire_time);
        ENTRY(reset_bypass_wd);
        ENTRY(std_nic);
        if (pbp_device_block->bp_caps & BP_CAP) {
            ENTRY(bypass);
            ENTRY(dis_bypass);
            ENTRY(bypass_pwup);
            ENTRY(bypass_pwoff);
            ENTRY(bypass_change);
        }
        if (pbp_device_block->bp_caps & TAP_CAP) {
            ENTRY(tap);
            ENTRY(dis_tap);
            ENTRY(tap_pwup);
            ENTRY(tap_change);
        }
        if (pbp_device_block->bp_caps & DISC_CAP) {
            ENTRY(disc);
            ENTRY(dis_disc);
            ENTRY(disc_pwup);
            ENTRY(disc_change);
        }

        ENTRY(wd_exp_mode);
        ENTRY(wd_autoreset);
        ENTRY(tpl);
#ifdef PMC_FIX_FLAG
        ENTRY(wait_at_pwup);
        ENTRY(hw_reset);
#endif
    }
#undef ENTRY
    if (ret < 0)
        printk(KERN_DEBUG "Create proc entry failed\n");

    return ret;
}

    #endif
    #if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )
int bypass_proc_remove_dev_sd(bpctl_dev_t * pbp_device_block){

    struct bypass_pfs_sd *current_pfs = &pbp_device_block->bypass_pfs_set;
    struct proc_dir_entry *pde = current_pfs->bypass_entry, *pde_curr=NULL; 
    char name[256]; 
    if (!pde)
        return 0;
    for (pde=pde->subdir; pde; ) {
        strcpy(name,pde->name);
        pde_curr=pde;
        pde=pde->next;
        remove_proc_entry(name,current_pfs->bypass_entry);
    }
    if (!pde)
        remove_proc_entry(current_pfs->dir_name,bp_procfs_dir);

    current_pfs->bypass_entry=NULL;


    return 0;
}
    #else


int bypass_proc_remove_dev_sd(bpctl_dev_t *pbp_device_block)
{

    struct bypass_pfs_sd *current_pfs = &pbp_device_block->bypass_pfs_set;
    remove_proc_subtree(current_pfs->dir_name, bp_procfs_dir);
    current_pfs->bypass_entry = NULL;
    return 0;
}
    #endif


#endif //BP_PROC_SUPPORT















