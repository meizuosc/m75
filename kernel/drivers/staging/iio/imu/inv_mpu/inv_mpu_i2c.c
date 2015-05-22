/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_mpu_iio.h"
#include "linux/iio/sysfs.h"
#include "inv_test/inv_counters.h"

#ifdef CONFIG_DTS_INV_MPU_IIO
#include "inv_mpu_dts.h"
#endif

#include <mach/eint.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>

#ifdef QUEUE_WORK
struct inv_mpu_state *inv_st = NULL;
#endif

/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @i2c_addr:  i2c address of device.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
#define I2C_TRANS_TIMING (400) //kHz
#define I2C_DMA_LIMIT (128)

#define I2C_WR_MAX (7)
#define INV_ADDR (0x68)

static int sensor_read_data(struct i2c_client *client, u8 command, u8 *buf, int count)
{
#if 1
	int ret = 0;
	u16 old_flag = client->addr;
	buf[0] = command;

	client->addr = (client->addr & I2C_MASK_FLAG) |(I2C_WR_FLAG |I2C_RS_FLAG);
	client->ext_flag = 0x00;
	client->timing = I2C_TRANS_TIMING;
	//client->ext_flag = (I2C_WR_FLAG |I2C_RS_FLAG);
	ret = i2c_master_send(client, buf, (count << 8 | 1));
	if( ret!=(count << 8 | 1) ) {
		printk("invn: %s read error, reg:0x%x,count:%d, ext_flag:0x%x\n",__func__,command,count,client->addr&0xff00);
	}

	client->addr = old_flag;
#endif

#if 0
	int ret = 0;
	struct i2c_msg msgs[1] = {
			{
					.addr = INV_ADDR & I2C_MASK_FLAG,
					.flags = 0,
					.buf = buf,
					.len = count << 8 | 1,
					.timing = I2C_TRANS_TIMING,
					.ext_flag = I2C_WR_FLAG |I2C_RS_FLAG,

			}
	};

	ret = i2c_transfer(client->adapter, msgs, 1);
	if( ret!=1 ) {
		printk("invn: %s read error, reg:0x%x,count:%d, ext_flag:0x%x\n",__func__,command,count,client->addr&0xff00);
	}
#endif
	return ret;
}

int inv_i2c_read_base(struct inv_mpu_state *st, u16 i2c_addr,
	u8 reg, u16 size, u8 *data)
{
	int i,retval = 0;
	u8 addr = reg;
	u8 buf[1024]={0,};
	int times = (size>1024?1024:size)/I2C_WR_MAX;

//	printk("%s reg:0x%x,size:%d\n",__func__,reg,size);

	mutex_lock(&st->inv_rw_mutex);
	for (i = 0; i < times; i++) {
		if( reg!=REG_FIFO_R_W ) {
			retval |= sensor_read_data(st->client, addr + 8*i, buf + 8*i, 8);
		} else {
			retval |= sensor_read_data(st->client, addr, buf + 8*i, 8);
		}
	}

	if (size % 8) {
		if( reg!=REG_FIFO_R_W ) {
			retval |= sensor_read_data(st->client, addr + 8*i, buf + 8*i, size % 8);
		} else {
			retval |= sensor_read_data(st->client, addr, buf + 8*i, size % 8);
		}
	}

	memcpy(data, buf, size);

	mutex_unlock(&st->inv_rw_mutex);

	return (retval<0)?retval:0;

#if 0
	int retval;
	unsigned char retry;
	unsigned char buf;
	unsigned char *buf_va = NULL;
	int full = size / I2C_DMA_LIMIT;
	int partial = size % I2C_DMA_LIMIT;
	int total;
	int last;
	int ii;
	static int msg_length = 0;

	if(!gprDMABuf_va){
	  gprDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 1024, &gprDMABuf_pa, GFP_KERNEL);
	  if(!gprDMABuf_va){
		printk("[Error] Allocate DMA I2C Buffer failed!\n");
	  }
	}

	buf_va = gprDMABuf_va;

	if ((full + 2) > msg_length) {
		kfree(read_msgs);
		msg_length = full + 2;
		read_msgs = kcalloc(msg_length, sizeof(struct i2c_msg), GFP_KERNEL);
	}

	read_msgs[0].addr = i2c_addr;
	read_msgs[0].flags = 0;
	read_msgs[0].len = 1;
	read_msgs[0].buf = &buf;
	read_msgs[0].timing = I2C_TRANS_TIMING;

	if (partial) {
		total = full + 1;
		last = partial;
	} else {
		total = full;
		last = I2C_DMA_LIMIT;
	}

	for (ii = 1; ii <= total; ii++) {
		read_msgs[ii].addr = i2c_addr;
		read_msgs[ii].flags = I2C_M_RD;
		read_msgs[ii].len = (ii == total) ? last : I2C_DMA_LIMIT;
		read_msgs[ii].buf = gprDMABuf_pa + I2C_DMA_LIMIT * (ii - 1);
		read_msgs[ii].ext_flag = (I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		read_msgs[ii].timing = I2C_TRANS_TIMING;
	}

	buf = reg & 0xff;

		if (i2c_transfer(st->client->adapter, read_msgs, (total + 1)) == (total + 1)) {
			retval = 0;
		} else {
			retval = -EIO;
		}

	memcpy(data, buf_va, size);

	return retval;
#endif



#if 0
	int ret = 0;
	u8 ureg = reg;
	int len = size;
	char *buf = NULL;
	dma_addr_t phyAddr;

	st->client->addr = i2c_addr & I2C_MASK_FLAG;
	st->client->timing = I2C_TRANS_TIMING;

	if (len > 255)
	{
		pr_err("inv: %s(), before align, len is 0x%x\n", __func__, len);
		if (len % 255) {
			len = ((len/255 + 1) << 8) | 0xff;
		} else {
			len = ((len/255) << 8) | 0xff;
		}
		pr_err("inv: %s(), after align, len is 0x%x\n", __func__, len);
	}

	buf = dma_alloc_coherent(NULL, len, &phyAddr, GFP_KERNEL);
	if (!buf) {
		pr_err("inv: %s(), can not allocate dma addr\n", __func__);
		return -ENOMEM;
	}

	memset(buf, 0, len);

	ret = i2c_master_send(st->client, &ureg, sizeof(ureg));
	if (ret < 0)
	{
		pr_err("inv: inv i2c_master_send address 0x%x to read return %d\n",
				ureg, ret);
		return ret;
	}

	ret = mt_i2c_master_recv(st->client, (char*)phyAddr, len, I2C_DMA_FLAG);
	if (ret < 0) {
		pr_err( "inv: %s(), failed Read[0x%x] at "
				"cat[%02x] cmd[%02x], ret %d\n", __func__,
				size, data, buf[0], ret);
	}

	memcpy(data, buf, size);
	dma_free_coherent(NULL, len, buf, phyAddr);

	return (ret < 0) ? ret : 0;
#endif
}

/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @i2c_addr:  I2C address of the device.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_state *st,
	u16 i2c_addr, u8 reg, u8 data)
{

	int ret = 0;
	u8 buf[2];
	struct i2c_msg msgs[1] ={{0}};

	mutex_lock(&st->inv_rw_mutex);
#if 0
	st->client->addr = i2c_addr & I2C_MASK_FLAG;
	st->client->timing = 400;
	st->client->ext_flag = 0;
	ret = i2c_master_send(st->client, buf, sizeof(buf));

	if (ret!=sizeof(buf))
	{
		pr_err("invn: i2c_master_send data address 0x%x 0x%x error return %d\n",
				buf[0], buf[1], ret);
	}
#endif
#if 1

	buf[0] = reg;
	buf[1] = data;

	msgs[0].addr = i2c_addr & I2C_MASK_FLAG,
	msgs[0].flags = 0,
	msgs[0].buf = buf,
	msgs[0].len = sizeof(buf),
	msgs[0].timing = I2C_TRANS_TIMING,
	msgs[0].ext_flag = 0,

	ret = i2c_transfer(st->client->adapter, msgs, 1);
	if (ret!=1)
	{
		pr_err("invn: i2c_master_send data address 0x%x 0x%x error return %d\n",
				buf[0], buf[1], ret);
	}
#endif
	mutex_unlock(&st->inv_rw_mutex);

	return (ret < 0) ? ret : 0;
}

int inv_plat_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	return inv_i2c_single_write_base(st, st->i2c_addr, reg, data);
}
int inv_plat_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	return inv_i2c_read_base(st, st->i2c_addr, reg, len, data);
}



#define I2C_DMA_MAX (128-1)
#define I2C_FIFO_MAX (8-1)

int mpu_memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 size, u8 const *data)
{
#if 1
    int length = size;
    int writebytes = 0;
    int ret = 0;
    u8 const *ptr = data;
    unsigned char buf[I2C_WR_MAX + 1];

    struct i2c_msg msgs[3]={{0}, };


    mutex_lock(&st->inv_rw_mutex);

    memset(buf, 0, sizeof(buf));

    while(length > 0)
    {
              if (length > I2C_WR_MAX)
                       writebytes = I2C_WR_MAX;
              else
                       writebytes = length;


				buf[0] = REG_MEM_BANK_SEL;
				buf[1] = mem_addr >> 8;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
	                       goto write_error;
	              }

				buf[0] = REG_MEM_START_ADDR;
				buf[1] = mem_addr & 0xFF;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
	                       goto write_error;
	              }

				memset(buf, 0, sizeof(buf));
				buf[0] = REG_MEM_R_W;
				memcpy(buf+1, ptr, writebytes);
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = writebytes+1;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

          	ret = i2c_transfer(st->client->adapter, msgs, 1);
              if (ret!=1)
              {
                       printk("invn: i2c transfer error ret:%d, write_bytes:%d, Line %d\n", ret, writebytes+1, __LINE__);
                       goto write_error;
              }

              length -= writebytes;
              mem_addr += writebytes;
              ptr +=writebytes;
    }

    mutex_unlock(&st->inv_rw_mutex);
    return 0;
write_error:
	mutex_unlock(&st->inv_rw_mutex);
    return -1;

#endif

#if 0//its ok
	int retval = 0;
	unsigned char retry;
	unsigned char buf;
//	unsigned char *buf_va = NULL;
	u8 buf_va[1024];
	int full = size / I2C_FIFO_MAX;
	int partial = size % I2C_FIFO_MAX;
	int total;
	int last;
	int ii;
	static int msg_length = 0;
	u8 bank[2];
	u8 addr[2];
	int tx_size = 0, offset = 0;


	mutex_lock(&st->inv_rw_mutex);
#if 0
	if(!gpwDMABuf_va){
		gpwDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 2048, &gpwDMABuf_pa, GFP_KERNEL);
	  if(!gpwDMABuf_va){
		printk("[invensense] %s Allocate DMA I2C Buffer failed!\n",__func__);
	  }
	}

	buf_va = gpwDMABuf_va;
#endif

	if ((full + 3) > msg_length) {
		if( write_msgs!=NULL )
			kfree(write_msgs);
		msg_length = full + 3;
		write_msgs = kzalloc(msg_length*sizeof(struct i2c_msg), GFP_KERNEL);
		if(!write_msgs){
			printk("[invensense] %s Allocate i2c_msg space failed\n",__func__);
		}
	}

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;
	write_msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
	write_msgs[0].flags = 0;
	write_msgs[0].buf = bank;
	write_msgs[0].len = sizeof(bank);
	write_msgs[0].timing = I2C_TRANS_TIMING;
	write_msgs[0].ext_flag = 0x00;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;
	write_msgs[1].addr = mpu_addr & I2C_MASK_FLAG;
	write_msgs[1].flags = 0;
	write_msgs[1].buf = addr;
	write_msgs[1].len = sizeof(addr);
	write_msgs[1].timing = I2C_TRANS_TIMING;
	write_msgs[1].ext_flag = 0x00;

	if (partial) {
		total = full + 1;
		last = partial;
	} else {
		total = full;
		last = I2C_FIFO_MAX;//I2C_DMA_LIMIT;
	}

	for (ii = 1; ii <= total; ii++) {

		tx_size = (ii == total) ? last : I2C_FIFO_MAX;
		offset = I2C_FIFO_MAX * (ii - 1);
		addr[1] = (mem_addr & 0xFF) + offset;


		buf_va[0] = REG_MEM_R_W;
		memcpy(&buf_va[1], data+offset, tx_size);

		write_msgs[2].addr = mpu_addr & I2C_MASK_FLAG;
		write_msgs[2].flags = 0;
		write_msgs[2].len = tx_size+1;
//		write_msgs[2].buf = (u8*)gpwDMABuf_pa;
		write_msgs[2].buf = buf_va;
		write_msgs[2].timing = I2C_TRANS_TIMING;
//		write_msgs[2].ext_flag = I2C_ENEXT_FLAG | I2C_DMA_FLAG;
		write_msgs[2].ext_flag = 0x00;



		if (i2c_transfer(st->client->adapter, write_msgs, 3) == 3) {
			retval |= 0;
		} else {
			retval |= -EIO;
			printk("[invensensse] %s flag addr:0x%x, ext:0x%x\n",__func__, st->client->addr&0xff00, st->client->ext_flag);
			printk("[invensensse] %s write error\n",__func__);
		}
	}
	mutex_unlock(&st->inv_rw_mutex);

	return retval;
#endif

#if 0 // trans len should be less than 128 bytes, this case cant work
	int retval;
	unsigned char buf[size + 1];
	unsigned char *buf_va = NULL;
	u8 bank[2],addr[2],mem[1];

	if(!gpwDMABuf_va){
		gpwDMABuf_va = (u8 *)dma_alloc_coherent(NULL, 1024, &gpwDMABuf_pa, GFP_KERNEL);
		if(!gpwDMABuf_va){
			printk("[invensense] Allocate DMA I2C Buffer failed!\n");
		}
	}
	buf_va = gpwDMABuf_va;

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;
	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;
	mem[0] = REG_MEM_R_W;

	struct i2c_msg msg[] = {
			{
				.addr = mpu_addr & I2C_MASK_FLAG,
				.flags = 0,
				.len = sizeof(bank),
				.buf = bank,
				.ext_flag= 0,//I2C_ENEXT_FLAG|I2C_DMA_FLAG,
				.timing = I2C_TRANS_TIMING,
			},
			{
				.addr = mpu_addr & I2C_MASK_FLAG,
				.flags = 0,
				.len = sizeof(addr),
				.buf = addr,
				.ext_flag= 0,//I2C_ENEXT_FLAG|I2C_DMA_FLAG,
				.timing = I2C_TRANS_TIMING,
			},
			{
				.addr = mpu_addr & I2C_MASK_FLAG,
				.flags = 0,
				.len = size + 1,
				.buf = (u8*)gpwDMABuf_pa,
				.ext_flag= I2C_ENEXT_FLAG|I2C_DMA_FLAG,
				.timing = I2C_TRANS_TIMING,
			}
	};

	buf_va[0] = REG_MEM_R_W;

	memcpy(&buf_va[1],&data[0] , size);


	if (i2c_transfer(st->client->adapter, msg, 3) == 3) {
		retval |= 0;
	} else {
		retval |= -EIO;
		printk("%s flag addr:0x%x, ext:0x%x\n",__func__, st->client->addr&0xff00, st->client->ext_flag);
		printk("%s write error\n",__func__);
	}

	return retval;
#endif


#if 0
	int write_size;
	int written_byte = 0;
	int ret =0 ;
	int total_size = size;

	while (size > 0)
	{
		if (size > ONE_PACKAGE_MAX)
			write_size = ONE_PACKAGE_MAX;
		else
			write_size = size;

		ret = mpu_memory_write_dma(st, mpu_addr, mem_addr+written_byte, write_size, data+written_byte);
		if (ret < 0)
		{
			pr_err("invensense: mpu_memory_write error. written_byte is %d.\n", written_byte);
		}
		written_byte += write_size;
		size -= write_size;
		//pr_debug("invensense: mpu_memory_write, total_size is %d, written_byte is %d, size is %d.\n", total_size, written_byte, size);
	}
	return (ret < 0) ? ret : 0;
#endif
}




int mpu_memory_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 size, u8 *data)
{
      int length = size;
      int readbytes = 0;
      int ret = 0;
      unsigned char buf[I2C_WR_MAX];
      struct i2c_msg msgs[3] = {{0}, };

      mutex_lock(&st->inv_rw_mutex);

      while(length > 0)
      {
                if (length > I2C_WR_MAX)
                         readbytes = I2C_WR_MAX;
                else
                         readbytes = length;

                buf[0] = REG_MEM_BANK_SEL;
                buf[1] = mem_addr >> 8;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, Line %d\n", ret, __LINE__);
	                       goto read_error;
	              }

                buf[0] = REG_MEM_START_ADDR;
                buf[1] = mem_addr & 0xFF;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 2;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d,  Line %d\n", ret,  __LINE__);
	                       goto read_error;
	              }


                buf[0] = REG_MEM_R_W;
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = 0;
				msgs[0].buf = buf;
				msgs[0].len = 1;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

	          	ret = i2c_transfer(st->client->adapter, msgs, 1);
	              if (ret!=1)
	              {
	                       printk("invn: i2c transfer error ret:%d, Line %d\n", ret, __LINE__);
	                       goto read_error;
	              }

				memset(buf, 0, sizeof(buf));
				msgs[0].addr = mpu_addr & I2C_MASK_FLAG;
				msgs[0].flags = I2C_M_RD;
				msgs[0].buf = buf;
				msgs[0].len = readbytes;
				msgs[0].timing = I2C_TRANS_TIMING;
				msgs[0].ext_flag = 0;

				ret = i2c_transfer(st->client->adapter, msgs, 1);
				  if (ret!=1)
				  {
						   printk("invn: i2c transfer error ret:%d, read_bytes:%d, Line %d\n", ret, readbytes, __LINE__);
						   goto read_error;
				  }

                length -= readbytes;
                mem_addr += readbytes;
                memcpy(data, buf, readbytes);
                data += readbytes;
      }

      mutex_unlock(&st->inv_rw_mutex);
      return 0;
read_error:
	mutex_unlock(&st->inv_rw_mutex);
	return -1;

#if 0
	int retval = 0;
	unsigned char retry;
	unsigned char buf;
//	unsigned char *buf_va = NULL;
	unsigned char buf_va[1024];
	int full = size / I2C_FIFO_MAX;
	int partial = size % I2C_FIFO_MAX;
	int total;
	int last;
	int ii;
	static int msg_length = 0;
	u8 bank[2];
	u8 addr[2];
	int tx_size = 0, offset = 0;

#if 0
	if(!gprDMABuf_va1){
	  gprDMABuf_va1 = (u8 *)dma_alloc_coherent(NULL, 2048, &gprDMABuf_pa1, GFP_KERNEL);
	  if(!gprDMABuf_va1){
		printk("[invensense] Allocate DMA I2C Buffer failed!\n");
	  }
	}

	buf_va = gprDMABuf_va1;
#endif

	if ((full + 4) > msg_length) {
		if( read_msgs1!=NULL )
			kfree(read_msgs1);
		msg_length = full + 4;
		read_msgs1 = kzalloc(msg_length*sizeof(struct i2c_msg), GFP_KERNEL);
		if(!read_msgs1){
			printk("[invensense] %s Allocate i2c_msg space failed\n",__func__);
		}
	}

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;
	read_msgs1[0].addr = mpu_addr & I2C_MASK_FLAG;
	read_msgs1[0].flags = 0;
	read_msgs1[0].buf = bank;
	read_msgs1[0].len = sizeof(bank);
	read_msgs1[0].timing = I2C_TRANS_TIMING;
	read_msgs1[0].ext_flag = 0x00;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;
	read_msgs1[1].addr = mpu_addr & I2C_MASK_FLAG;
	read_msgs1[1].flags = 0;
	read_msgs1[1].buf = addr;
	read_msgs1[1].len = sizeof(addr);
	read_msgs1[1].timing = I2C_TRANS_TIMING;
	read_msgs1[1].ext_flag = 0x00;

	buf = REG_MEM_R_W;
	read_msgs1[2].addr = mpu_addr & I2C_MASK_FLAG;
	read_msgs1[2].flags = 0;
	read_msgs1[2].buf = &buf;
	read_msgs1[2].len = 1;
	read_msgs1[2].timing = I2C_TRANS_TIMING;
	read_msgs1[2].ext_flag = 0x00;

	if (partial) {
		total = full + 1;
		last = partial;
	} else {
		total = full;
		last = I2C_FIFO_MAX;
	}

	for (ii = 1+2; ii <= total+2; ii++) {

		tx_size = (ii == total + 2) ? last : I2C_FIFO_MAX;
		offset = I2C_FIFO_MAX * (ii - 1 - 2);
		addr[1] = (mem_addr & 0xFF) + offset;

		read_msgs1[3].addr = mpu_addr & I2C_MASK_FLAG;
		read_msgs1[3].flags = I2C_M_RD;
		read_msgs1[3].len = tx_size;
		read_msgs1[3].buf = buf_va;
//		read_msgs1[3].buf = (u8*)gprDMABuf_pa1;
		read_msgs1[3].timing = I2C_TRANS_TIMING;
//		read_msgs1[3].ext_flag = I2C_ENEXT_FLAG | I2C_DMA_FLAG;
		read_msgs1[3].ext_flag = 0x00;


		if (i2c_transfer(st->client->adapter, read_msgs1, 4) == 4) {
			memcpy(&data[0] + offset, &buf_va[0], tx_size);
			retval |= 0;
		} else {
			retval |= -EIO;
			printk("[invensense] %s read error!\n",__func__);
		}
	}

	return retval;
#endif


#if 0
	int read_want_size;
	int read_already_byte = 0;
	int ret = 0;
	int total_size = size;

	while (size > 0)
	{
		if (size > ONE_PACKAGE_MAX)
			read_want_size = ONE_PACKAGE_MAX;
		else
			read_want_size = size;

		ret = mpu_memory_read_dma(st, mpu_addr, mem_addr+read_already_byte, read_want_size, data+read_already_byte);
		if (ret < 0)
		{
			pr_err("invensense: mpu_memory_read error. read_already_byte is %d.\n", read_already_byte);
		}
		read_already_byte += read_want_size;
		size -= read_want_size;
		//pr_debug("invensense: mpu_memory_read, total_size is %d, read_already_byte is %d, size is %d.\n",
		//		total_size, read_already_byte, size);
	}

	return ((ret < 0) ? ret : 0);
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sensor_early_suspend(struct early_suspend *h)
{
    pr_debug("******** invn sensor driver early suspend!! need_on:%d ********\n",inv_st->sensor_need_on );
    int result;

	mt_eint_mask(inv_st->irq);
    if( !inv_st->sensor_need_on ) {

    	result = inv_switch_power_in_lp(inv_st, true);
    	if (result) {
    		printk("%s enter lp mode failed\n",__func__);
    	}

    }

}

static void sensor_late_resume(struct early_suspend *h)
{
    pr_debug("invn sensor driver late resume!! need_on:%d ********\n",inv_st->sensor_need_on );
    int result;

	mt_eint_unmask(inv_st->irq);
	if( !inv_st->sensor_need_on ) {

		result = inv_switch_power_in_lp(inv_st, false);
		if (result) {
			printk("%s exit lp mode failed\n",__func__);
		}
    }
}

#endif


/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	pr_debug("%s\n",__func__);

#ifdef CONFIG_DTS_INV_MPU_IIO
	enable_irq_wake(client->irq);
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		pr_err("I2c function error\n");
		goto out_no_free;
	}

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result =  -ENOMEM;
		goto out_no_free;
	}
	st = iio_priv(indio_dev);
	st->client = client;
	st->sl_handle = client->adapter;
	st->i2c_addr = client->addr & 0xff;

	/*
	 * its safer.
	 */
	st->i2c_dis = 0;
	st->client->addr &= 0xff;
	st->client->timing = I2C_TRANS_TIMING;
	st->client->ext_flag = 0x00;
#ifdef TURN_OFF_SENSOR_WHEN_BACKLIGHT_IS_0
	st->sensor_need_on = 0;
#endif
	inv_st = st;
	mutex_init(&inv_st->inv_rw_mutex);

#ifdef CONFIG_DTS_INV_MPU_IIO
	result = invensense_mpu_parse_dt(&client->dev, &st->plat_data);
	if (result)
		goto out_free;

	/*Power on device.*/
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(&client->dev,
					"power_on failed: %d\n", result);
			return result;
		}
	pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

msleep(100);
#else
	st->plat_data =
		*(struct mpu_platform_data *)dev_get_platdata(&client->dev);
#endif
	/* power is turned on inside check chip type*/
	result = inv_check_chip_type(indio_dev, id->name);
	if (result) {
		printk("inv_mpu_probe i2c fail\n");
		goto out_free;
	} else {
		printk("inv_mpu_probe i2c success\n");
	}
	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = &client->dev;
	st->dev = &client->dev;
	indio_dev->name = id->name;
	st->irq = client->irq;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
	result = iio_buffer_register(indio_dev, indio_dev->channels,
					indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}
	result = inv_mpu_probe_trigger(indio_dev);
	if (result) {
		pr_err("trigger probe fail\n");
		goto out_remove_ring;
	}

	/* Tell the i2c counter, we have an IRQ */
	INV_I2C_SETIRQ(IRQ_MPU, client->irq);

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_trigger;
	}

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		pr_err("create dmp sysfs failed\n");
		goto out_unreg_iio;
	}

	mutex_init(&st->suspend_resume_lock);

	inv_init_sensor_struct(st);

#ifdef CONFIG_HAS_EARLYSUSPEND
	st->sensor_early_suspend.suspend = sensor_early_suspend;
	st->sensor_early_suspend.resume = sensor_late_resume;
	st->sensor_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    register_early_suspend(&st->sensor_early_suspend);
#endif

	printk("%s is ready to go!\n", indio_dev->name);

	return 0;
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_trigger:
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_device_free(indio_dev);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	dev_dbg(&client->adapter->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		dev_err(&client->adapter->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(&client->adapter->dev, "Failed to turn off %s\n",
			st->hw->name);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);

	iio_device_unregister(indio_dev);
	if (indio_dev->modes & INDIO_BUFFER_TRIGGERED)
		inv_mpu_remove_trigger(indio_dev);
	iio_buffer_unregister(indio_dev);
	inv_mpu_unconfigure_ring(indio_dev);
	iio_device_free(indio_dev);

	dev_info(&client->adapter->dev, "inv-mpu-iio module removed.\n");

	return 0;
}


#ifdef CONFIG_PM
/*
 * inv_mpu_resume(): resume method for this driver.
 *    This method can be modified according to the request of different
 *    customers. It basically undo everything suspend_noirq is doing
 *    and recover the chip to what it was before suspend.
 */
static int inv_mpu_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_resume\n", st->hw->name);
	pr_debug("%s dmp_on:%d, batch_on:%d\n",__func__,st->chip_config.dmp_on,st->batch.on);


	mutex_lock(&indio_dev->mlock);

	if (st->chip_config.dmp_on) {
		if (st->batch.on) {
			inv_switch_power_in_lp(st, true);
			write_be32_to_mem(st, st->batch.counter, BM_BATCH_THLD);
			inv_plat_single_write(st, REG_INT_ENABLE_2,
							BIT_FIFO_OVERFLOW_EN_0);
			inv_switch_power_in_lp(st, false);
		}
	} else {
		inv_switch_power_in_lp(st, true);
	}
	mutex_unlock(&indio_dev->mlock);
	/* add code according to different request End */
	mutex_unlock(&st->suspend_resume_lock);

	return 0;
}

/*
 * inv_mpu_suspend(): suspend method for this driver.
 *    This method can be modified according to the request of different
 *    customers. If customer want some events, such as SMD to wake up the CPU,
 *    then data interrupt should be disabled in this interrupt to avoid
 *    unnecessary interrupts. If customer want pedometer running while CPU is
 *    asleep, then pedometer should be turned on while pedometer interrupt
 *    should be turned off.
 */
static int inv_mpu_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	pr_debug("%s inv_mpu_suspend\n", st->hw->name);
	pr_debug("%s dmp_on:%d, batch_on:%d\n",__func__,st->chip_config.dmp_on,st->batch.on);

	if (st->chip_config.dmp_on) {
		if (st->batch.on) {
			inv_switch_power_in_lp(st, true);
			write_be32_to_mem(st, INT_MAX, BM_BATCH_THLD);
			inv_plat_single_write(st, REG_INT_ENABLE_2, 0);
			inv_switch_power_in_lp(st, false);
		}
	} else {
		/* in non DMP case, just turn off the power */
		inv_set_power(st, false);
	}
	/* add code according to different request End */
	st->suspend_state = true;
	msleep(100);
	mutex_lock(&st->suspend_resume_lock);
	st->suspend_state = false;

	return 0;
}

static const struct dev_pm_ops inv_mpu_pmops = {
	.suspend       = inv_mpu_suspend,
	.resume        = inv_mpu_resume,
};
#define INV_MPU_PMOPS (&inv_mpu_pmops)
#else
#define INV_MPU_PMOPS NULL
#endif /* CONFIG_PM */


static const u16 normal_i2c[] = { I2C_CLIENT_END };
/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mpu_id[] = {
	{"mpu7400", ICM20628},
	{"icm20728", ICM20728},
	{"icm20628", ICM20628},
	{}
};

MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	inv_mpu_probe,
	.remove		=	inv_mpu_remove,
	.shutdown	=	inv_mpu_shutdown,
	.id_table	=	inv_mpu_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv-mpu-iio",
		.pm     =       INV_MPU_PMOPS,
	},
	.address_list = normal_i2c,
};

static struct mpu_platform_data gyro_platform_data = {
	.int_config  = 0x00,
	.level_shifter = 0,
	.orientation = {
	   0,  1,  0,
		-1,  0,  0,
		0,  0,  1 },
#if 1
	.sec_slave_type = SECONDARY_SLAVE_TYPE_COMPASS,
	.sec_slave_id   = COMPASS_ID_AK09911,
	.secondary_i2c_addr = 0x0C,
	.secondary_orientation = {
	   -1,  0, 0,
	   0,  1, 0,
	   0,  0, -1 },
#endif
	   //	.key = {0x9c, 0xc7, 0x49, 0x7, 0xb, 0x3, 0x34, 0x74, 0x9d, 0x65, 0x52, 0x99, 0xa8, 0xac, 0xc5, 0x5b},

};

static struct i2c_board_info __initdata single_chip_board_info[] = {
	{
		I2C_BOARD_INFO("mpu7400", 0x68),
		.irq = 15, //16
		.platform_data = &gyro_platform_data,
	},
};

static int __init inv_mpu_init(void)
{
	int result;
	pr_err("sensor hub init %s\n", __func__);
	i2c_register_board_info(3, single_chip_board_info, 1);

	result = i2c_add_driver(&inv_mpu_driver);
	if (result) {
		pr_err("failed\n");
		return result;
	}
	return 0;
}

static void __exit inv_mpu_exit(void)
{
	i2c_del_driver(&inv_mpu_driver);
}

module_init(inv_mpu_init);
module_exit(inv_mpu_exit);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv-mpu-iio");
