/*
 * vhba.c
 *
 * Copyright (C) 2007 Chia-I Wu
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>

#include <kernel.api.h>

MODULE_AUTHOR("Chia-I Wu");
MODULE_VERSION(VHBA_VERSION);
MODULE_DESCRIPTION("Virtual SCSI HBA");
MODULE_LICENSE("GPL");

#ifdef DEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define scmd_dbg(scmd, fmt, a...)	\
	dev_dbg(&(scmd)->device->sdev_gendev, fmt, ##a)

#define scmd_warn(scmd, fmt, a...)	\
	dev_warn(&(scmd)->device->sdev_gendev, fmt, ##a)


#define VHBA_MAX_SECTORS_PER_IO 128
#define VHBA_MAX_ID 32
#define VHBA_CAN_QUEUE 32
#define VHBA_INVALID_ID VHBA_MAX_ID

#define DATA_TO_DEVICE(dir) ((dir) == DMA_TO_DEVICE || (dir) == DMA_BIDIRECTIONAL)
#define DATA_FROM_DEVICE(dir) ((dir) == DMA_FROM_DEVICE || (dir) == DMA_BIDIRECTIONAL)

enum vhba_req_state {
	VHBA_REQ_FREE,
	VHBA_REQ_PENDING,
	VHBA_REQ_READING,
	VHBA_REQ_SENT,
	VHBA_REQ_WRITING,
};

struct vhba_command {
	struct scsi_cmnd *cmd;
	int status;
	struct list_head entry;
};

struct vhba_device {
	uint id;
	spinlock_t cmd_lock;
	struct list_head cmd_list;
	wait_queue_head_t cmd_wq;
	atomic_t refcnt;
};

struct vhba_host {
	struct Scsi_Host *shost;
	spinlock_t cmd_lock;
	int cmd_next;
	struct vhba_command commands[VHBA_CAN_QUEUE];
	spinlock_t dev_lock;
	struct vhba_device *devices[VHBA_MAX_ID];
	int num_devices;
	DECLARE_BITMAP(chgmap, VHBA_MAX_ID);
	struct work_struct scan_devices;
};

struct vhba_request {
	__u32 tag;
	__u32 lun;
#define MAX_COMMAND_SIZE	16
	__u8 cdb[MAX_COMMAND_SIZE];
	__u8 cdb_len;
	__u32 data_len;
};

struct vhba_response {
	__u32 tag;
	__u32 status;
	__u32 data_len;
};

static struct vhba_command *vhba_alloc_command(void);
static void vhba_free_command(struct vhba_command *vcmd);

static struct platform_device vhba_platform_device;

static struct vhba_device *vhba_device_alloc(void)
{
	struct vhba_device *vdev;

	vdev = kzalloc(sizeof(struct vhba_device), GFP_KERNEL);
	if (!vdev)
		return NULL;

	vdev->id = VHBA_INVALID_ID;
	spin_lock_init(&vdev->cmd_lock);
	INIT_LIST_HEAD(&vdev->cmd_list);
	init_waitqueue_head(&vdev->cmd_wq);
	atomic_set(&vdev->refcnt, 1);

	return vdev;
}

static void vhba_device_put(struct vhba_device *vdev)
{
	if (atomic_dec_and_test(&vdev->refcnt))
		kfree(vdev);
}

static struct vhba_device *vhba_device_get(struct vhba_device *vdev)
{
	atomic_inc(&vdev->refcnt);

	return vdev;
}

static int vhba_device_queue(struct vhba_device *vdev, struct scsi_cmnd *cmd)
{
	struct vhba_command *vcmd;

	vcmd = vhba_alloc_command();
	if (!vcmd)
		return SCSI_MLQUEUE_HOST_BUSY;

	vcmd->cmd = cmd;

	spin_lock_bh(&vdev->cmd_lock);
	list_add_tail(&vcmd->entry, &vdev->cmd_list);
	spin_unlock_bh(&vdev->cmd_lock);

	wake_up_interruptible(&vdev->cmd_wq);

	return 0;
}

static int vhba_device_dequeue(struct vhba_device *vdev, struct scsi_cmnd *cmd)
{
	struct vhba_command *vcmd;
	int retval;

	spin_lock_bh(&vdev->cmd_lock);
	list_for_each_entry(vcmd, &vdev->cmd_list, entry)
	{
		if (vcmd->cmd == cmd)
		{
			list_del_init(&vcmd->entry);

			break;
		}
	}

	/* command not found */
	if (&vcmd->entry == &vdev->cmd_list)
	{
		spin_unlock_bh(&vdev->cmd_lock);

		return SUCCESS;
	}

	while (vcmd->status == VHBA_REQ_READING || vcmd->status == VHBA_REQ_WRITING)
	{
		spin_unlock_bh(&vdev->cmd_lock);
		scmd_dbg(cmd, "wait for I/O before aborting\n");
		schedule_timeout(1);
		spin_lock_bh(&vdev->cmd_lock);
	}

	retval = (vcmd->status == VHBA_REQ_SENT) ? FAILED : SUCCESS;

	vhba_free_command(vcmd);

	spin_unlock_bh(&vdev->cmd_lock);

	return retval;
}

static void vhba_scan_devices(struct work_struct *work)
{
	struct vhba_host *vhost = container_of(work, struct vhba_host, scan_devices);
	int id;

	/* no need to lock here; it'll be scheduled and run again if some device missed */
	while ((id = find_first_bit(vhost->chgmap, vhost->shost->max_id)) < vhost->shost->max_id)
	{
		int remove;

		spin_lock_bh(&vhost->dev_lock);
		clear_bit(id, vhost->chgmap);
		remove = !(vhost->devices[id]);
		spin_unlock_bh(&vhost->dev_lock);

		dev_dbg(&vhost->shost->shost_gendev, "try to %s target 0:%d:0\n",
				(remove) ? "remove" : "add", id);

		if (remove)
		{
			struct scsi_device *sdev;

			sdev = scsi_device_lookup(vhost->shost, 0, id, 0);
			if (sdev)
			{
				scsi_remove_device(sdev);
				scsi_device_put(sdev);
			}
		}
		else
			scsi_add_device(vhost->shost, 0, id, 0);
	}
}

static int vhba_add_device(struct vhba_device *vdev)
{
	struct vhba_host *vhost;
	int i;

	vhost = platform_get_drvdata(&vhba_platform_device);

	vhba_device_get(vdev);

	spin_lock_bh(&vhost->dev_lock);
	if (vhost->num_devices >= vhost->shost->max_id)
	{
		spin_unlock_bh(&vhost->dev_lock);
		vhba_device_put(vdev);

		return -EBUSY;
	}

	for (i = 0; i < vhost->shost->max_id; i++)
	{
		if (vhost->devices[i] == NULL)
		{
			vdev->id = i;
			vhost->devices[i] = vdev;
			vhost->num_devices++;
			set_bit(vdev->id, vhost->chgmap);

			break;
		}
	}
	spin_unlock_bh(&vhost->dev_lock);

	schedule_work(&vhost->scan_devices);

	return 0;
}

static int vhba_remove_device(struct vhba_device *vdev)
{
	struct vhba_host *vhost;

	vhost = platform_get_drvdata(&vhba_platform_device);

	spin_lock_bh(&vhost->dev_lock);
	set_bit(vdev->id, vhost->chgmap);
	vhost->devices[vdev->id] = NULL;
	vhost->num_devices--;
	vdev->id = VHBA_INVALID_ID;
	spin_unlock_bh(&vhost->dev_lock);

	vhba_device_put(vdev);

	schedule_work(&vhost->scan_devices);

	return 0;
}

static struct vhba_device *vhba_lookup_device(int id)
{
	struct vhba_host *vhost;
	struct vhba_device *vdev = NULL;

	vhost = platform_get_drvdata(&vhba_platform_device);

	if (likely(id < vhost->shost->max_id))
	{
		spin_lock_bh(&vhost->dev_lock);
		vdev = vhost->devices[id];
		if (vdev)
			vdev = vhba_device_get(vdev);
		spin_unlock_bh(&vhost->dev_lock);
	}

	return vdev;
}

static struct vhba_command *vhba_alloc_command(void)
{
	struct vhba_host *vhost;
	struct vhba_command *vcmd;
	int i;

	vhost = platform_get_drvdata(&vhba_platform_device);

	spin_lock_bh(&vhost->cmd_lock);

	vcmd = vhost->commands + vhost->cmd_next++;
	if (vcmd->status != VHBA_REQ_FREE)
	{
		for (i = 0; i < vhost->shost->can_queue; i++)
		{
			vcmd = vhost->commands + i;

			if (vcmd->status == VHBA_REQ_FREE)
			{
				vhost->cmd_next = i + 1;

				break;
			}
		}

		if (i == vhost->shost->can_queue)
			vcmd = NULL;
	}

	if (vcmd)
		vcmd->status = VHBA_REQ_PENDING;
	vhost->cmd_next %= vhost->shost->can_queue;

	spin_unlock_bh(&vhost->cmd_lock);

	return vcmd;
}

static void vhba_free_command(struct vhba_command *vcmd)
{
	struct vhba_host *vhost;

	vhost = platform_get_drvdata(&vhba_platform_device);

	spin_lock_bh(&vhost->cmd_lock);
	vcmd->status = VHBA_REQ_FREE;
	spin_unlock_bh(&vhost->cmd_lock);
}

static int vhba_queuecommand(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
{
	struct vhba_device *vdev;
	int retval;

	scmd_dbg(cmd, "queue %lu\n", cmd->serial_number);

	vdev = vhba_lookup_device(cmd->device->id);
	if (!vdev)
	{
		scmd_dbg(cmd, "no such device\n");

		cmd->result = DID_NO_CONNECT << 16;
		done(cmd);

		return 0;
	}

	cmd->scsi_done = done;
	retval = vhba_device_queue(vdev, cmd);

	vhba_device_put(vdev);

	return retval;
}

static int vhba_abort(struct scsi_cmnd *cmd)
{
	struct vhba_device *vdev;
	int retval = SUCCESS;

	scmd_warn(cmd, "abort %lu\n", cmd->serial_number);

	vdev = vhba_lookup_device(cmd->device->id);
	if (vdev)
	{
		retval = vhba_device_dequeue(vdev, cmd);
		vhba_device_put(vdev);
	}
	else
		cmd->result = DID_NO_CONNECT << 16;

	return retval;
}

static struct scsi_host_template vhba_template = {
	.module = THIS_MODULE,
	.name = "vhba",
	.proc_name = "vhba",
	.queuecommand = vhba_queuecommand,
	.eh_abort_handler = vhba_abort,
	.can_queue = VHBA_CAN_QUEUE,
	.this_id = -1,
	.cmd_per_lun = 1,
	.max_sectors = VHBA_MAX_SECTORS_PER_IO,
	.sg_tablesize = 256,
};

static ssize_t do_request(struct scsi_cmnd *cmd, char __user *buf, size_t buf_len)
{
	struct vhba_request vreq;
	ssize_t ret;

	scmd_dbg(cmd, "request %lu, cdb 0x%x, bufflen %d, use_sg %d\n",
#ifdef KAT_HAVE_SCSI_MACROS
		cmd->serial_number, cmd->cmnd[0], scsi_bufflen(cmd), scsi_sg_count(cmd));
#else
        cmd->serial_number, cmd->cmnd[0], cmd->request_bufflen, cmd->use_sg);
#endif
    
	ret = sizeof(vreq);
	if (DATA_TO_DEVICE(cmd->sc_data_direction))
#ifdef KAT_HAVE_SCSI_MACROS
        ret += scsi_bufflen(cmd);
#else
        ret += cmd->request_bufflen;
#endif

	if (ret > buf_len)
	{
		scmd_warn(cmd, "buffer too small (%d < %d) for a request\n", buf_len, ret);

		return -EIO;
	}

	vreq.tag = cmd->serial_number;
	vreq.lun = cmd->device->lun;
	memcpy(vreq.cdb, cmd->cmnd, MAX_COMMAND_SIZE);
	vreq.cdb_len = cmd->cmd_len;
#ifdef KAT_HAVE_SCSI_MACROS
	vreq.data_len = scsi_bufflen(cmd);
#else
	vreq.data_len = cmd->request_bufflen;
#endif

	if (copy_to_user(buf, &vreq, sizeof(vreq)))
		return -EFAULT;

	if (DATA_TO_DEVICE(cmd->sc_data_direction) && vreq.data_len)
	{
		buf += sizeof(vreq);

		/* XXX use_sg? */
#ifdef KAT_HAVE_SCSI_MACROS
        if (copy_to_user(buf, scsi_sglist(cmd), vreq.data_len))
#else
        if (copy_to_user(buf, cmd->request_buffer, vreq.data_len))
#endif
			return -EFAULT;
	}

	return ret;
}

static ssize_t do_response(struct scsi_cmnd *cmd, const char __user *buf, size_t buf_len, struct vhba_response *res)
{
	ssize_t ret = 0;
       
	scmd_dbg(cmd, "response %lu, status %x, data len %d, use_sg %d\n",
#ifdef KAT_HAVE_SCSI_MACROS
             cmd->serial_number, res->status, res->data_len, scsi_sg_count(cmd));
#else
             cmd->serial_number, res->status, res->data_len, cmd->use_sg);
#endif

	if (res->status)
	{
		if (res->data_len > SCSI_SENSE_BUFFERSIZE)
		{
			scmd_warn(cmd, "truncate sense (%d < %d)", SCSI_SENSE_BUFFERSIZE, res->data_len);
			res->data_len = SCSI_SENSE_BUFFERSIZE;
		}

		if (copy_from_user(cmd->sense_buffer, buf, res->data_len))
			return -EFAULT;

		cmd->result = res->status;

		ret += res->data_len;
	}
#ifdef KAT_HAVE_SCSI_MACROS
	else if (DATA_FROM_DEVICE(cmd->sc_data_direction) && scsi_bufflen(cmd))
#else
	else if (DATA_FROM_DEVICE(cmd->sc_data_direction) && cmd->request_bufflen)
#endif
	{
		size_t to_read;
	       
#ifdef KAT_HAVE_SCSI_MACROS
        if (res->data_len > scsi_bufflen(cmd))
        {
			scmd_warn(cmd, "truncate data (%d < %d)\n", scsi_bufflen(cmd), res->data_len);
			res->data_len = scsi_bufflen(cmd);
		}
#else
        if (res->data_len > cmd->request_bufflen)
		{
			scmd_warn(cmd, "truncate data (%d < %d)\n", cmd->request_bufflen, res->data_len);
			res->data_len = cmd->request_bufflen;
		}
#endif
        
		to_read = res->data_len;

#ifdef KAT_HAVE_SCSI_MACROS
        if (scsi_sg_count(cmd))
#else
        if (cmd->use_sg)
#endif
		{
			unsigned char buf_stack[64];
			unsigned char *kaddr, *uaddr, *kbuf;
#ifdef KAT_HAVE_SCSI_MACROS
            struct scatterlist *sg = scsi_sglist(cmd);
#else
            struct scatterlist *sg = cmd->request_buffer;
#endif
			int i;

			uaddr = (unsigned char *) buf;

			if (res->data_len > 64)
				kbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
			else
				kbuf = buf_stack;

#ifdef KAT_HAVE_SCSI_MACROS
			for (i = 0; i < scsi_sg_count(cmd); i++)
#else
			for (i = 0; i < cmd->use_sg; i++)
#endif
			{
				size_t len = (sg[i].length < to_read) ? sg[i].length : to_read;

				if (copy_from_user(kbuf, uaddr, len))
				{
					if (kbuf != buf_stack)
						kfree(kbuf);

					return -EFAULT;
				}
				uaddr += len;

#ifdef KAT_SCATTERLIST_HAS_PAGE
				kaddr = kmap_atomic(sg[i].page, KM_USER0);
#else
				kaddr = kmap_atomic(sg_page(&sg[i]), KM_USER0);
#endif				
				memcpy(kaddr + sg[i].offset, kbuf, len);
				kunmap_atomic(kaddr, KM_USER0);

				to_read -= len;
				if (to_read == 0)
					break;
			}

			if (kbuf != buf_stack)
				kfree(kbuf);
		}
		else
		{
#ifdef KAT_HAVE_SCSI_MACROS
            if (copy_from_user(scsi_sglist(cmd), buf, res->data_len))
#else
            if (copy_from_user(cmd->request_buffer, buf, res->data_len))
#endif
				return -EFAULT;

			to_read -= res->data_len;
		}

#ifdef KAT_HAVE_SCSI_MACROS
        scsi_set_resid(cmd, to_read);
#else
        cmd->resid = to_read;
#endif

		ret += res->data_len - to_read;
	}

	return ret;
}

static inline struct vhba_command *next_command(struct vhba_device *vdev)
{
	struct vhba_command *vcmd;

	list_for_each_entry(vcmd, &vdev->cmd_list, entry)
	{
		if (vcmd->status == VHBA_REQ_PENDING)
			break;
	}

	if (&vcmd->entry == &vdev->cmd_list)
		vcmd = NULL;

	return vcmd;
}

static inline struct vhba_command *match_command(struct vhba_device *vdev, u32 tag)
{
	struct vhba_command *vcmd;

	list_for_each_entry(vcmd, &vdev->cmd_list, entry)
	{
		if (vcmd->cmd->serial_number == tag)
			break;
	}

	if (&vcmd->entry == &vdev->cmd_list)
		vcmd = NULL;

	return vcmd;
}

static struct vhba_command *wait_command(struct vhba_device *vdev)
{
	struct vhba_command *vcmd;
	DEFINE_WAIT(wait);

	while (!(vcmd = next_command(vdev)))
	{
		if (signal_pending(current))
			break;

		prepare_to_wait(&vdev->cmd_wq, &wait, TASK_INTERRUPTIBLE);

		spin_unlock_bh(&vdev->cmd_lock);

		schedule();

		spin_lock_bh(&vdev->cmd_lock);
	}

	finish_wait(&vdev->cmd_wq, &wait);
	if (vcmd)
		vcmd->status = VHBA_REQ_READING;

	return vcmd;
}

static ssize_t vhba_ctl_read(struct file *file, char __user *buf, size_t buf_len, loff_t *offset)
{
	struct vhba_device *vdev;
	struct vhba_command *vcmd;
	ssize_t ret;

	vdev = file->private_data;

	spin_lock_bh(&vdev->cmd_lock);
	vcmd = wait_command(vdev);
	spin_unlock_bh(&vdev->cmd_lock);

	if (!vcmd)
		return -ERESTARTSYS;

	ret = do_request(vcmd->cmd, buf, buf_len);

	spin_lock_bh(&vdev->cmd_lock);
	if (ret >= 0)
	{
		vcmd->status = VHBA_REQ_SENT;
		*offset += ret;
	}
	else
		vcmd->status = VHBA_REQ_PENDING;
	spin_unlock_bh(&vdev->cmd_lock);

	return ret;
}

static ssize_t vhba_ctl_write(struct file *file, const char __user *buf, size_t buf_len, loff_t *offset)
{
	struct vhba_device *vdev;
	struct vhba_command *vcmd;
	struct vhba_response res;
	ssize_t ret;
       
	if (buf_len < sizeof(res))
		return -EIO;

	if (copy_from_user(&res, buf, sizeof(res)))
		return -EFAULT;

	vdev = file->private_data;

	spin_lock_bh(&vdev->cmd_lock);
	vcmd = match_command(vdev, res.tag);
	if (!vcmd || vcmd->status != VHBA_REQ_SENT)
	{
		spin_unlock_bh(&vdev->cmd_lock);
		DPRINTK("not expecting response\n");

		return -EIO;
	}
	vcmd->status = VHBA_REQ_WRITING;
	spin_unlock_bh(&vdev->cmd_lock);

	ret = do_response(vcmd->cmd, buf + sizeof(res), buf_len - sizeof(res), &res);

	spin_lock_bh(&vdev->cmd_lock);
	if (ret >= 0)
	{
		vcmd->cmd->scsi_done(vcmd->cmd);
		ret += sizeof(res);
		
		/* don't compete with vhba_device_dequeue */
		if (!list_empty(&vcmd->entry))
		{
			list_del_init(&vcmd->entry);
			vhba_free_command(vcmd);
		}
	}
	else
		vcmd->status = VHBA_REQ_SENT;
	spin_unlock_bh(&vdev->cmd_lock);

	return ret;
}

static unsigned int vhba_ctl_poll(struct file *file, poll_table *wait)
{
	struct vhba_device *vdev = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &vdev->cmd_wq, wait);

	spin_lock_bh(&vdev->cmd_lock);
	if (next_command(vdev))
		mask |= POLLIN | POLLRDNORM;
	spin_unlock_bh(&vdev->cmd_lock);

	return mask;
}

static int vhba_ctl_open(struct inode *inode, struct file *file)
{
	struct vhba_device *vdev;
	int retval;

	DPRINTK("open\n");

	/* check if vhba is probed */
	if (!platform_get_drvdata(&vhba_platform_device))
		return -ENODEV;

	vdev = vhba_device_alloc();
	if (!vdev)
		return -ENOMEM;

	if (!(retval = vhba_add_device(vdev)))
		file->private_data = vdev;

	vhba_device_put(vdev);

	return retval;
}

static int vhba_ctl_release(struct inode *inode, struct file *file)
{
	struct vhba_device *vdev;
	struct vhba_command *vcmd;

	DPRINTK("release\n");

	vdev = file->private_data;

	vhba_device_get(vdev);
	vhba_remove_device(vdev);

	spin_lock_bh(&vdev->cmd_lock);
	list_for_each_entry(vcmd, &vdev->cmd_list, entry)
	{
		WARN_ON(vcmd->status == VHBA_REQ_READING || vcmd->status == VHBA_REQ_WRITING);

		scmd_warn(vcmd->cmd, "device released with command %lu\n", vcmd->cmd->serial_number);
		vcmd->cmd->result = DID_NO_CONNECT << 16;
		vcmd->cmd->scsi_done(vcmd->cmd);

		vhba_free_command(vcmd);
	}
	INIT_LIST_HEAD(&vdev->cmd_list);
	spin_unlock_bh(&vdev->cmd_lock);

	vhba_device_put(vdev);

	return 0;
}

static struct file_operations vhba_ctl_fops = {
	.owner = THIS_MODULE,
	.open = vhba_ctl_open,
	.release = vhba_ctl_release,
	.read = vhba_ctl_read,
	.write = vhba_ctl_write,
	.poll = vhba_ctl_poll,
};

static struct miscdevice vhba_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vhba_ctl",
	.fops = &vhba_ctl_fops,
};

static int vhba_probe(struct platform_device *pdev)
{
	struct Scsi_Host *shost;
	struct vhba_host *vhost;
	int i;

	shost = scsi_host_alloc(&vhba_template, sizeof(struct vhba_host));
	if (!shost)
		return -ENOMEM;

	shost->max_id = VHBA_MAX_ID;
	/* we don't support lun > 0 */
	shost->max_lun = 1;

	vhost = (struct vhba_host *) shost->hostdata;
	memset(vhost, 0, sizeof(*vhost));

	vhost->shost = shost;
	vhost->num_devices = 0;
	spin_lock_init(&vhost->dev_lock);
	spin_lock_init(&vhost->cmd_lock);
	INIT_WORK(&vhost->scan_devices, vhba_scan_devices);
	vhost->cmd_next = 0;
	for (i = 0; i < vhost->shost->can_queue; i++)
		vhost->commands[i].status = VHBA_REQ_FREE;

	platform_set_drvdata(pdev, vhost);

	if (scsi_add_host(shost, &pdev->dev)) {
		scsi_host_put(shost);

		return -ENOMEM;
	}

	return 0;
}

static int vhba_remove(struct platform_device *pdev)
{
	struct vhba_host *vhost;
	struct Scsi_Host *shost;

	vhost = platform_get_drvdata(pdev);
	shost = vhost->shost;

	scsi_remove_host(shost);
	scsi_host_put(shost);

	return 0;
}

static void vhba_release(struct device * dev)
{
	return;
}

static struct platform_device vhba_platform_device = {
	.name = "vhba",
	.id = -1,
	.dev = {
		.release = vhba_release,
	},
};

static struct platform_driver vhba_platform_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "vhba",
	},
	.probe = vhba_probe,
	.remove = vhba_remove,
};

static int __init vhba_init(void)
{
	int ret;

	ret = platform_device_register(&vhba_platform_device);
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&vhba_platform_driver);
	if (ret < 0) {
		platform_device_unregister(&vhba_platform_device);

		return ret;
	}

	ret = misc_register(&vhba_miscdev);
	if (ret < 0) {
		platform_driver_unregister(&vhba_platform_driver);
		platform_device_unregister(&vhba_platform_device);

		return ret;
	}

	return 0;
}

static void __exit vhba_exit(void)
{
	misc_deregister(&vhba_miscdev);
	platform_driver_unregister(&vhba_platform_driver);
	platform_device_unregister(&vhba_platform_device);
}

module_init(vhba_init);
module_exit(vhba_exit);
