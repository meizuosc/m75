#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/pagemap.h>
#include "rawfs.h"

#define CEILING(x, y) rawfs_div(((x)+(y)-1),(y))
#define FLOOR(x, y)   rawfs_div((x),(y))
#define page_uptodate(page)	test_bit(PG_uptodate, &(page)->flags)

extern void rawfs_read_page_user(struct super_block *sb, int block_no, int addr,
    const struct iovec *iov, unsigned long nr_segs, int size);

//-----------------------------------------------------------------------------
// Block File Operation
//-----------------------------------------------------------------------------
/**
 * rawfs_block_file_aio_read - read routine for block files
 * @iocb:	kernel I/O control block
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @pos:	current file position
 */
ssize_t
rawfs_block_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
//    struct address_space *mapping=filp->f_mapping;
    struct inode *inode = filp->f_mapping->host;
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);

	ssize_t retval;
//	unsigned long seg = 0;
	size_t count;
	loff_t *ppos = &iocb->ki_pos;
    // Always use direct I/O
	loff_t size;
    int block_no;

	retval = generic_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);

	if (retval)
		return retval;

    retval=iov_length(iov, nr_segs);

    mutex_lock(&rawfs_sb->rawfs_lock);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_block_file_aio_read %s, pos %lld, "
        "len %d\n", inode_info->i_name, pos, retval);

	size = i_size_read(inode);

    // Get inode ID
    block_no = filp->f_path.dentry->d_inode->i_ino - RAWFS_BLOCK0_INO;

    if ((retval + pos) >= size)
        retval = size - pos;

	if (pos < size) {
        rawfs_sb->dev.read_page_user(filp->f_path.dentry->d_inode->i_sb,
            block_no, pos, iov, nr_segs, retval);

		if (retval > 0)
			*ppos = pos + retval;

		if (retval < 0 || *ppos >= size) {
			file_accessed(filp);
            goto out;
		}
	}

out:

    mutex_unlock(&rawfs_sb->rawfs_lock);

    return retval;
}

/**
 * rawfs_block_file_aio_write - write data to a block file
 * @iocb:	IO state structure
 * @iov:	vector with data to write
 * @nr_segs:	number of segments in the vector
 * @pos:	position in file where to write
 */

ssize_t rawfs_block_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
//    struct super_block *sb = filp->f_path.dentry->d_sb;
//    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
//    struct address_space *mapping=filp->f_mapping;
    struct inode *inode = filp->f_mapping->host;
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);

    ssize_t retval;
//	unsigned long seg = 0;
	loff_t *ppos = &iocb->ki_pos;
//	loff_t size;

    retval=iov_length(iov, nr_segs);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_block_file_aio_write %s, pos %lld, "
        "len %d\n", inode_info->i_name, pos, retval);

	if (retval > 0)
		*ppos = pos + retval;

    file_accessed(filp);

    /* This function is not supported, data will not write to the device */

    return retval;
}

//-----------------------------------------------------------------------------
// Regular File Operation
//-----------------------------------------------------------------------------
ssize_t rawfs_reg_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
//    struct address_space *mapping=filp->f_mapping;
    struct inode *inode = filp->f_mapping->host;
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);

    ssize_t retval;
	size_t count;
	loff_t *ppos = &iocb->ki_pos;
	loff_t size;
    unsigned int curr_file_pos = pos;
    unsigned int curr_buf_pos = 0;
    int remain_buf_size;
    const struct iovec *iv = &iov[0];  // TODO: Process all io vectors

	retval = generic_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read %s, segment check "
        "result %d\n", inode_info->i_name, retval);

	if (retval)
		return retval;

    mutex_lock(&rawfs_sb->rawfs_lock);

    retval=iov_length(iov, nr_segs);

    size = i_size_read(inode);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read %s, pos %lld, len %d, "
        "filesize: %lld\n", inode_info->i_name, pos, retval, size);

    if ((retval + pos) >= size)
        retval = size - pos;

    if (pos < size) {
        /* Read File */
        {
            int preceding_pages, rear_pages;
            struct rawfs_page *page_buf = NULL;
            int i;

            // Prepare page buffer
            page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

            if (page_buf == NULL) {
                retval = 0;
                goto out;
            }

            preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
            rear_pages = CEILING((unsigned)pos + retval,
                rawfs_sb->page_data_size);
            remain_buf_size = retval;

            RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read %s, "
                "preceding_pages %d, rear_pages %d, remain_buf_size %d\n",
                inode_info->i_name, preceding_pages, rear_pages,
                remain_buf_size);

            // Step 1: Copy preceding pages, if starting pos is not 0.
            for (i=preceding_pages;i<rear_pages;i++)
            {
                __u32 crc;

                // Read page
                rawfs_sb->dev.read_page(sb,
                    inode_info->i_location_block,
                    inode_info->i_location_page+i,
                    page_buf);

                // TODO: skip this page, if unrecoverable error occurs

                /* CRC error should not happen,
                   since we have already check them at bootup */
                crc = rawfs_page_crc_data(sb, page_buf);

                if (crc != page_buf->i_crc)
                    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read: "
                        "%s @ %X, crc fail %X, expected %X\n",
                        page_buf->i_info.i_file_info.i_name,
                        page_buf->i_info.i_file_info.i_parent_folder_id, crc,
                        page_buf->i_crc);
                else
                    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read: "
                        "%s @ %X, crc %X\n",
                        page_buf->i_info.i_file_info.i_name,
                        page_buf->i_info.i_file_info.i_parent_folder_id,
                        page_buf->i_crc);

                /* Copy requried parts */
                {
                    int start_in_buf;
                    int copy_len;

                    start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
                    copy_len =  ((start_in_buf + remain_buf_size) >
                        rawfs_sb->page_data_size) ?
                        (rawfs_sb->page_data_size - start_in_buf) :
                        remain_buf_size;

                    if (copy_to_user((char*)iv->iov_base + curr_buf_pos,
                        &page_buf->i_data[0] + start_in_buf, copy_len))
                    {
                        retval = -EFAULT;
                        goto out2;
                    }

                    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_read %s, "
                        "%d, curr_buf_pos %d, remain_buf_size %d "
                        "start_in_buf %d copy_len %d starting pattern %X\n",
                        inode_info->i_name, i, curr_buf_pos, remain_buf_size,
                        start_in_buf, copy_len,
                        *(unsigned int*)(&page_buf->i_data[0] + start_in_buf));

                    curr_buf_pos    += copy_len;
                    remain_buf_size -= copy_len;
                }
            }

out2:
            if (page_buf)
                kfree(page_buf);
        }

        if (retval > 0)
            *ppos = pos + retval;

        if (retval < 0 || *ppos >= size) {
            file_accessed(filp);
            goto out;
        }
    }
    else
        retval = 0;

out:
    // Release Lock
    mutex_unlock(&rawfs_sb->rawfs_lock);

    return retval;
}

void rawfs_fill_fileinfo_by_dentry(struct dentry *dentry,
    struct rawfs_file_info *file_info)
{
    rawfs_fill_file_info(dentry->d_inode, file_info);
    file_info->i_parent_folder_id = RAWFS_I(dentry->d_parent->d_inode)->i_id;

    /* Dentry only has 32 bytes, file it with iname */
    strncpy(file_info->i_name, dentry->d_name.name, RAWFS_MAX_FILENAME_LEN+4);
}

void rawfs_fill_file_info(struct inode *inode,
    struct rawfs_file_info *file_info)
{
    int name_len;
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);

    name_len=strlen(inode_info->i_name);
    file_info->i_atime = inode->i_atime;
    file_info->i_mtime = inode->i_mtime;
    file_info->i_ctime = inode->i_ctime;
    file_info->i_uid = inode->i_uid;
    file_info->i_gid = inode->i_gid;
    file_info->i_mode = inode->i_mode;
    file_info->i_size = inode->i_size;
    file_info->i_id = inode_info->i_id;
    file_info->i_parent_folder_id = RAWFS_ROOT_DIR_ID;

//    inode->i_ino = rawfs_file_unique_ino(file_info->i_name, name_len);
//    inode->i_fop =  &rawfs_file_operations;
    strncpy(file_info->i_name, inode_info->i_name, name_len+1);
//    inode_info->i_location_block = block_no;
//    inode_info->i_location_page = page_no;
//    inode_info->i_location_page_count = file_info->i_chunk_total;
}

ssize_t rawfs_reg_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
//    struct address_space *mapping=filp->f_mapping;
    struct inode *inode = filp->f_mapping->host;
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);

    ssize_t retval;
//	size_t count;
	loff_t *ppos = &iocb->ki_pos;
    struct rawfs_file_info *fi = NULL;
    unsigned int curr_file_pos = pos;
    unsigned int curr_buf_pos = 0;
    int starting_page, total_pages;
    int remain_buf_size;
    int result;
    struct rawfs_file_list_entry * entry;
    const struct iovec *iv = &iov[0];  // TODO: Process all io vectors

    // Always use direct I/O
    //	loff_t size;
    //  int block_no;

    // Get Lock
    mutex_lock(&rawfs_sb->rawfs_lock);

    retval=iov_length(iov, nr_segs);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_write %s, pos %lld, "
        "len %d\n", inode_info->i_name, pos, retval);

    fi = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

    if (!fi) {
        retval = 0;
        goto out;
    }

    /* rawfs_fill_file_info(inode , fi); */
    rawfs_fill_fileinfo_by_dentry(filp->f_path.dentry, fi);

    /* Update file_info */
    if ((pos+retval) > fi->i_size)
        fi->i_size = pos+retval;

    fi->i_chunk_index = 1;
    fi->i_chunk_total = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
        rawfs_sb->page_data_size);

    /* do GC, if the required space is not enough */
    result = rawfs_reserve_space(sb, fi->i_chunk_total);
    if (result<0) {
        retval=result;
            goto out;
        }

    // get entry from file list
    entry = rawfs_file_list_get(sb, fi->i_name, fi->i_parent_folder_id);

    if (!entry) {
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_write: %s file list "
            "entry missing\n", fi->i_name);
        dump_stack();
        goto out;
    }

    // Write File
    {
        int preceding_pages, rear_pages;
        struct rawfs_page *page_buf = NULL;
        int i;

        // Prepare page buffer
        page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

        if (page_buf == NULL) {
            retval = 0;
            goto out;
        }

        starting_page = rawfs_sb->data_block_free_page_index;
        preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
        total_pages = CEILING((unsigned)fi->i_size, rawfs_sb->page_data_size);
        rear_pages = CEILING((unsigned)pos + iv->iov_len,
            rawfs_sb->page_data_size);
        remain_buf_size = iv->iov_len;

        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_write %s, "
            "preceding_pages %d, rear_pages %d, total_pages %d, "
            "remain_buf_size %d\n",
             inode_info->i_name, preceding_pages, rear_pages, total_pages,
             remain_buf_size);


        // Step 1: Copy preceding pages, if starting pos is not 0.
        for (i=0;i<total_pages;i++)
        {
            // Read, if necessary, (no need for new files)
            if ( (i<=preceding_pages) || (i>=rear_pages))
            rawfs_sb->dev.read_page(sb,
                entry->i_location_block,
                entry->i_location_page+i,
                page_buf);

            // Update info
            memcpy(&page_buf->i_info.i_file_info, fi,
                sizeof(struct rawfs_file_info));

            // Within Modify Range: Copy Modify
            if ((i>=preceding_pages) && (i<=rear_pages))
            {
                int start_in_buf;
                int copy_len;

                start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
                copy_len =  ((start_in_buf + remain_buf_size) >
                            rawfs_sb->page_data_size) ?
                            (rawfs_sb->page_data_size - start_in_buf) :
                            remain_buf_size;

                if (copy_from_user(&page_buf->i_data[0] + start_in_buf,
                    (char*)iv->iov_base + curr_buf_pos, copy_len))
                {
                    retval = -EFAULT;
                    goto out2;
                }

                curr_buf_pos    += copy_len;
                remain_buf_size -= copy_len;

                RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_aio_write %s, %d, "
                    "curr_buf_pos %d, remain_buf_size %d start_in_buf %d "
                    "copy_len %d starting pattern %X\n",
                inode_info->i_name, i, curr_buf_pos, remain_buf_size,
                start_in_buf, copy_len,
                *(unsigned int*)(&page_buf->i_data[0] + start_in_buf));
            }

            rawfs_page_signature(sb, page_buf);

            // Write
            rawfs_sb->dev.write_page(sb,
                rawfs_sb->data_block,
                rawfs_sb->data_block_free_page_index,
                page_buf);

            rawfs_sb->data_block_free_page_index++;
            fi->i_chunk_index++;
        }

out2:
        if (page_buf)
            kfree(page_buf);
    }

	if (retval > 0) {
		*ppos = pos + retval;
	}

    file_accessed(filp);

    // Update Inode: file size, block, page
    i_size_write(inode, fi->i_size);
    // TODO: Get inode lock when update
    inode_info->i_location_block = rawfs_sb->data_block;
    inode_info->i_location_page = starting_page;
    inode_info->i_location_page_count = total_pages;

    // update location
    entry->i_location_block = rawfs_sb->data_block;
    entry->i_location_page = starting_page;
    entry->i_location_page_count = total_pages;

out:
    if (fi)
        kfree(fi);

    // Release Lock
    mutex_unlock(&rawfs_sb->rawfs_lock);

    return retval;
}


//-----------------------------------------------------------------------------
// Internal Functions
//-----------------------------------------------------------------------------
int rawfs_reg_file_create(struct inode *dir, struct dentry *dentry,
    umode_t mode, struct nameidata *nd)
{
    int result=0;
    struct super_block *sb = dentry->d_sb;
    struct inode *inode = dentry->d_inode;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
    struct rawfs_page *page_buf = NULL;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_create: i_name=%s\n",
        inode_info->i_name);

    page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

    if (page_buf == NULL) {
        result = -ENOMEM;
        goto out;
    }

    result = rawfs_reserve_space(sb, 1);
    if (result<0)
            goto out;

    // rawfs_fill_file_info(inode, &page_buf->i_info.i_file_info);
    rawfs_fill_fileinfo_by_dentry(dentry, &page_buf->i_info.i_file_info);

    page_buf->i_info.i_file_info.i_mode = mode;
    page_buf->i_info.i_file_info.i_size = 0;
    page_buf->i_info.i_file_info.i_chunk_total = 1;
    page_buf->i_info.i_file_info.i_chunk_index = 1;

    rawfs_page_signature(sb, page_buf);

    rawfs_sb->dev.write_page(sb,
        rawfs_sb->data_block,
        rawfs_sb->data_block_free_page_index,
        page_buf);


    // Update inode_info
    inode_info->i_location_block = rawfs_sb->data_block;
    inode_info->i_location_page = rawfs_sb->data_block_free_page_index;
    inode->i_size = 0;

    rawfs_sb->data_block_free_page_index++;


out:
    if (page_buf)
        kfree(page_buf);

    return result;

}

int rawfs_reg_file_delete(struct inode *dir, struct dentry *dentry)
{
    int result=0;
    struct super_block *sb = dentry->d_sb;
    struct inode *inode = dentry->d_inode;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
    struct rawfs_page *page_buf = NULL;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_delete %s\n",
        dentry->d_name.name);

    page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

    if (page_buf == NULL) {
        result = -ENOMEM;
        goto out;
    }

    /* Do GC, if there's no free_pages */
    if ((rawfs_sb->data_block_free_page_index+1) > rawfs_sb->pages_per_block) {
        int reclaimed_size;
        reclaimed_size = rawfs_dev_garbage_collection(sb);
        if (reclaimed_size<=0) {
            RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_delete: "
                "Disk full: %d\n", reclaimed_size);
            result = -ENOSPC;
            goto out;
        }
    }

    /* Read existing data */
    rawfs_sb->dev.read_page(sb,
        inode_info->i_location_block,
        inode_info->i_location_page,
        page_buf);

    /* Set i_chun_total as zero to mark delete. */
    page_buf->i_info.i_file_info.i_chunk_total = -1;
    page_buf->i_info.i_file_info.i_chunk_index = -1;
    page_buf->i_info.i_file_info.i_size         = 0;

    rawfs_page_signature(sb, page_buf);

    rawfs_sb->dev.write_page(sb,
        rawfs_sb->data_block,
        rawfs_sb->data_block_free_page_index,
        page_buf);

    rawfs_sb->data_block_free_page_index++;

out:
    if (page_buf)
        kfree(page_buf);

    return result;
}

/* This function is used by rawfs_dir_rename() & rawfs_setattr()
   This function will put the new copeid file into file list. */
int rawfs_reg_file_copy(struct inode *src_dir, struct dentry *src_dentry,
    struct inode *dest_dir, struct dentry *dest_dentry)
{
    int result = 0;
    int starting_page;
    struct inode *src_inode = src_dentry->d_inode;
    struct super_block *sb = src_dentry->d_sb;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_file_info *fi = NULL;
    struct rawfs_file_list_entry *src_info;

    if ((dest_dir == NULL) && (dest_dentry == NULL)) {
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy (setattr) %s \n",
            src_dentry->d_name.name);
        dest_dir = src_dir;
        dest_dentry = src_dentry;
    } else {
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy (rename) %s -> %s\n",
            src_dentry->d_name.name, dest_dentry->d_name.name);
    }

    fi = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

    if (!fi) {
        result = -ENOMEM;
        goto out;
    }

    rawfs_fill_file_info(src_inode , fi);
    fi->i_parent_folder_id = RAWFS_I(dest_dir)->i_id;

    strncpy(fi->i_name, dest_dentry->d_name.name,
        strlen(dest_dentry->d_name.name)+1);

    fi->i_chunk_index = 1;
    fi->i_chunk_total = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
            rawfs_sb->page_data_size);

    /* do GC, if the required space is not enough */
    result = rawfs_reserve_space(sb, fi->i_chunk_total);
    if (result<0)
            goto out;

    /* Get soruce location & starting page again,
       it might be change to new location after GC */
    src_info=rawfs_file_list_get(sb,
        src_dentry->d_name.name,
        RAWFS_I(src_dir)->i_id);

    starting_page = rawfs_sb->data_block_free_page_index;

    if (!src_info) {
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy: src file %s @ %X "
            "missing after GC\n", src_dentry->d_name.name,
            RAWFS_I(src_dir)->i_id);
        goto out;
    }

    {
        int total_pages;
        struct rawfs_page *page_buf = NULL;
        int i;

        page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

        if (page_buf == NULL) {
            result = -ENOMEM;
            goto out;
        }

        total_pages = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
            rawfs_sb->page_data_size);

        for (i=0;i<total_pages;i++)
        {
            RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy copy %d,%d to "
                "%d,%d\n",
                src_info->i_location_block,
                src_info->i_location_page+i,
                rawfs_sb->data_block,
                rawfs_sb->data_block_free_page_index);

            rawfs_sb->dev.read_page(sb,
                src_info->i_location_block,
                src_info->i_location_page+i,
                page_buf);

            /* Update info */
            memcpy(&page_buf->i_info.i_file_info, fi,
                sizeof(struct rawfs_file_info));

            rawfs_page_signature(sb, page_buf);

            rawfs_sb->dev.write_page(sb,
                rawfs_sb->data_block,
                rawfs_sb->data_block_free_page_index,
                page_buf);

            rawfs_sb->data_block_free_page_index++;
            fi->i_chunk_index++;
        }

        if (page_buf)
            kfree(page_buf);
    }

    /* Add to file list, so that rawf_dir_rename can get the new
        location of the file from list. */
    rawfs_file_list_add(sb, fi, rawfs_sb->data_block, starting_page);

out:
    if (fi)
        kfree(fi);

    return result;

}

/* Read page, by pass page cache, always read from device. */
static int rawfs_readpage_nolock(struct file *filp, struct page *page)
{
    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct inode *inode = filp->f_path.dentry->d_inode;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
    loff_t pos;
    unsigned int curr_file_pos;
    unsigned int curr_buf_pos = 0;
    int remain_buf_size;
    unsigned size;
    int retval;
	unsigned char *pg_buf;

//	pg_buf = kmap_atomic(page);
	pg_buf = kmap(page);

    // TODO: check pg_buf

    size = i_size_read(inode);
    curr_file_pos = pos = page->index << PAGE_CACHE_SHIFT;
    retval = PAGE_CACHE_SIZE;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_readpage %s @ folder %X, "
        "page_index %ld, pos %lld, len %d, filesize: %d, pg_buf %X\n",
        inode_info->i_name,
        inode_info->i_parent_folder_id,
        page->index,
        pos, retval, size, (unsigned)pg_buf);

    if ((retval + pos) >= size)
        retval = size - pos;

    if (pos < size) {
        {
            int preceding_pages, rear_pages;
            struct rawfs_page *page_buf = NULL;
            int i;

            // Prepare page buffer
            page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

            if (page_buf == NULL)
            {
                retval = 0;
                goto out;
            }

            preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
            rear_pages = CEILING((unsigned)pos + retval,
                rawfs_sb->page_data_size);
            remain_buf_size = retval;

            RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_readpage %s, "
                "preceding_pages %d, rear_pages %d, remain_buf_size %d ",
                inode_info->i_name, preceding_pages, rear_pages,
                remain_buf_size);

            /* Step 1: Copy preceding pages, if starting pos is not 0. */
            for (i=preceding_pages;i<rear_pages;i++)
            {
                // Read page
                rawfs_sb->dev.read_page(sb,
                    inode_info->i_location_block,
                    inode_info->i_location_page+i,
                    page_buf);

                /* Copy requried parts */
                {
                    int start_in_buf;
                    int copy_len;

                    start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
                    copy_len =  ((start_in_buf + remain_buf_size) >
                                  rawfs_sb->page_data_size) ?
                                (rawfs_sb->page_data_size - start_in_buf) :
                                remain_buf_size;

                    memcpy(pg_buf + curr_buf_pos,
                        &page_buf->i_data[0] + start_in_buf, copy_len);

                    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_readpage %s, %d, "
                        "curr_buf_pos %d, remain_buf_size %d start_in_buf %d "
                        "copy_len %d starting pattern %X",
                        inode_info->i_name, i, curr_buf_pos, remain_buf_size,
                        start_in_buf, copy_len,
                        *(unsigned int*)(&page_buf->i_data[0] + start_in_buf));

                    curr_buf_pos    += copy_len;
                    remain_buf_size -= copy_len;
                }
            }

            if (page_buf)
                kfree(page_buf);
        }
    }
    else
        retval = 0;

out:

    SetPageUptodate(page);
    ClearPageError(page);
	flush_dcache_page(page);

	//kunmap_atomic(pg_buf);
	kunmap(page);
    unlock_page(page);

    return 0;
}


static int rawfs_readpage(struct file *filp, struct page *page)
{
    int result;

    result = rawfs_readpage_nolock(filp, page);
    unlock_page(page);

    return result;
}

static int rawfs_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_begin: unexpected call !");
    BUG();
    return simple_write_begin(filp, mapping, pos, len, flags, pagep, fsdata);
}

static int rawfs_write_end(struct file *filp, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *pg, void *fsdata)
{
    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_end: unexpected call !");
    BUG();
    return simple_write_end(filp, mapping, pos, len, copied, pg, fsdata);
}

#if 0
static int rawfs_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
//    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct inode *inode = filp->f_path.dentry->d_inode;
//    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	pgoff_t index;
    struct page *pg = NULL;
    int result = 0;

	index = pos >> PAGE_CACHE_SHIFT;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_begin: %s @ folder %X, page_index "
        "%d, len %d, page_size %ld\n",
        inode_info->i_name,
        inode_info->i_parent_folder_id,
        (unsigned)index, len, PAGE_CACHE_SIZE);

	/* Get a page */
	pg = grab_cache_page_write_begin(mapping, index, flags);

	*pagep = pg;
	if (!pg) {
		result = -ENOMEM;
		goto out;
	}

	if (!page_uptodate(pg))
		result = rawfs_readpage_nolock(filp, pg);

	if (result)
		goto out;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_begin: Success\n");

    return 0;

out:
    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_begin: %s @ folder %X => "
        "failed %d\n",
        inode_info->i_name,
        inode_info->i_parent_folder_id,
        result);

    if (pg) {
        unlock_page(pg);
        page_cache_release(pg);
    }
    return result;
}

static int rawfs_write_file(struct inode *inode, const char *buf, size_t n,
    loff_t pos)
{
    ssize_t retval;
//	size_t count;
//	loff_t *ppos = &iocb->ki_pos;
//    struct super_block *sb = filp->f_path.dentry->d_sb;
//    struct inode *inode = filp->f_path.dentry->d_inode;
    struct super_block *sb = inode->i_sb;
    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
    struct rawfs_file_info *fi = NULL;
    unsigned int curr_file_pos = pos;
    unsigned int curr_buf_pos = 0;
    int starting_page, total_pages;
    int remain_buf_size;
    struct rawfs_file_list_entry * entry;
//    const struct iovec *iv = &iov[0];  // TODO: Process all io vectors

    // Always use direct I/O
    //	loff_t size;
    //	struct address_space *mapping;
    //	struct inode *inode;
    //  int block_no;

    // Get inode ID
    // filp->f_path.dentry->d_inode->i_ino;

    // Get Lock
    mutex_lock(&rawfs_sb->rawfs_lock);

    retval = n;
    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_file %s, pos %lld, len %d\n",
        inode_info->i_name, pos, retval);

    fi = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

    if (!fi) {
        retval = 0;
        goto out;
    }

    rawfs_fill_file_info(inode , fi);
    // rawfs_fill_fileinfo_by_dentry(filp->f_path.dentry, fi);

    // Update file_info
    if ((pos+retval) > fi->i_size)
        fi->i_size = pos+retval;

    fi->i_chunk_index = 1;
    fi->i_chunk_total = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
        rawfs_sb->page_data_size);

    // Check file size,
    // do GC, if the required space is not enough
    if (fi->i_size > rawfs_dev_free_space(sb)) {
        int reclaimed_size;
        reclaimed_size = rawfs_dev_garbage_collection(sb);
        if (fi->i_size > reclaimed_size) {
            RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_file: disk full, "
                "recalimed size=%d, requried size=%lld\n", reclaimed_size,
                fi->i_size);
            retval = -ENOSPC;
            goto out;
        }
    }

    if (fi->i_size > rawfs_dev_free_space(sb)) {
        // Disk Full.
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_file(): Disk Full !\n");
        retval = -ENOSPC;
        goto out;
    }

    // Write File
    {
        int preceding_pages, rear_pages;
        struct rawfs_page *page_buf = NULL;
        int i;

        // Prepare page buffer
        page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

        if (page_buf == NULL)
        {
            retval = 0;
            goto out;
        }

        starting_page = rawfs_sb->data_block_free_page_index;
        preceding_pages = CEILING((unsigned)pos, rawfs_sb->page_data_size);
        total_pages = CEILING((unsigned)fi->i_size, rawfs_sb->page_data_size);
        rear_pages = CEILING((unsigned)pos + n, rawfs_sb->page_data_size);
        remain_buf_size = n;

        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_file %s, preceding_pages %d, "
            "rear_pages %d, total_pages %d, remain_buf_size %d ",
             inode_info->i_name, preceding_pages, rear_pages, total_pages,
             remain_buf_size);


        // Step 1: Copy preceding pages, if starting pos is not 0.
        for (i=0;i<total_pages;i++)
        {
            // Read, if necessary, (no need for new files)
            if ( (i<preceding_pages) || (i>rear_pages))
            rawfs_sb->dev.read_page(sb,
                inode_info->i_location_block,
                inode_info->i_location_page+i,
                page_buf);

            // Update info
            memcpy(&page_buf->i_info.i_file_info, fi,
            sizeof(struct rawfs_file_info));

            // Copy Modify
            if ((i>=preceding_pages) || (i<=rear_pages))
            {
                int start_in_buf;
                int copy_len;
//                int remain;

                start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
                copy_len =  ((start_in_buf + remain_buf_size) >
                            rawfs_sb->page_data_size) ?
                            (rawfs_sb->page_data_size - start_in_buf) :
                            remain_buf_size;

                memcpy(&page_buf->i_data[0] + start_in_buf, buf + curr_buf_pos,
                    copy_len);

                curr_buf_pos    += copy_len;
                remain_buf_size -= copy_len;

                RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_file %s, %d, "
                    "curr_buf_pos %d, remain_buf_size %d start_in_buf %d "
                    "copy_len %d starting pattern %X",
                    inode_info->i_name, i, curr_buf_pos, remain_buf_size,
                    start_in_buf, copy_len,
                    *(unsigned int*)(&page_buf->i_data[0] + start_in_buf));
            }

            rawfs_page_signature(sb, page_buf);

            // Write
            rawfs_sb->dev.write_page(sb,
                rawfs_sb->data_block,
                rawfs_sb->data_block_free_page_index,
                page_buf);

            rawfs_sb->data_block_free_page_index++;
            fi->i_chunk_index++;
        }

        if (page_buf)
            kfree(page_buf);
    }

    pos = pos + retval;

    // Update Inode: file size, block, page
    i_size_write(inode, fi->i_size);
    // TODO: Get inode lock when update
    inode_info->i_location_block = rawfs_sb->data_block;
    inode_info->i_location_page = starting_page;
    inode_info->i_location_page_count = total_pages;

    // update file list
    entry = rawfs_file_list_get(sb, fi->i_name, fi->i_parent_folder_id);
    entry->i_location_block = rawfs_sb->data_block;
    entry->i_location_page = starting_page;
    entry->i_location_page_count = total_pages;

out:
    if (fi)
        kfree(fi);

    // Release Lock
    mutex_unlock(&rawfs_sb->rawfs_lock);

    return retval;
}

static int rawfs_write_end(struct file *filp, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *pg, void *fsdadata)
{
//    struct super_block *sb = filp->f_path.dentry->d_sb;
    struct inode *inode = filp->f_path.dentry->d_inode;
//    struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
    struct rawfs_inode_info *inode_info = RAWFS_I(inode);
    void *addr, *kva;
    int result = 0;
    uint32_t offset_into_page = pos & (PAGE_CACHE_SIZE - 1);
	pgoff_t index;
//    loff_t last_pos = pos + copied;

	index = pos >> PAGE_CACHE_SHIFT;

	kva = kmap(pg);
	addr = kva + offset_into_page;

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_end: %s @ folder %X, "
        "page_index %d, len %d, page_size %ld, copied %d\n",
        inode_info->i_name,
        inode_info->i_parent_folder_id,
        (unsigned)index, len, PAGE_CACHE_SIZE, copied);

    result = rawfs_write_file(inode, addr, copied, pos);

    RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_end: %s @ folder %X, result %d, "
        "expected %d\n",
        inode_info->i_name,
        inode_info->i_parent_folder_id,
        result, copied);

    /*
	if (result != copied) {
		SetPageError(pg);
        RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_end: SetPageError, "
            "written %d\n", result);
	}
    else
    {
    */

    if (!PageUptodate(pg))
    	SetPageUptodate(pg);

	set_page_dirty(pg);

	kunmap(pg);
	unlock_page(pg);
	page_cache_release(pg);

	return copied;
}

static int rawfs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct address_space *mapping = page->mapping;
	struct inode *inode;
	unsigned long end_index;
	char *buffer;
	int n_written = 0;
	unsigned n_bytes;
	loff_t i_size;

	if (!mapping)
		BUG();
	inode = mapping->host;
	if (!inode)
		BUG();
	i_size = i_size_read(inode);

	end_index = i_size >> PAGE_CACHE_SHIFT;

	if (page->index < end_index)
		n_bytes = PAGE_CACHE_SIZE;
	else {
		n_bytes = i_size & (PAGE_CACHE_SIZE - 1);

		if (page->index > end_index || !n_bytes) {
			zero_user_segment(page, 0, PAGE_CACHE_SIZE);
			set_page_writeback(page);
			unlock_page(page);
			end_page_writeback(page);
			return 0;
		}
	}

	if (n_bytes != PAGE_CACHE_SIZE)
		zero_user_segment(page, n_bytes, PAGE_CACHE_SIZE);

	get_page(page);

	buffer = kmap(page);

    /*	n_written = yaffs_wr_file(obj, buffer,
				  page->index << PAGE_CACHE_SHIFT, n_bytes, 0);
    */

    n_written = rawfs_write_file(inode, buffer, n_bytes,
        page->index << PAGE_CACHE_SHIFT);

	kunmap(page);
	set_page_writeback(page);
	unlock_page(page);
	end_page_writeback(page);
	put_page(page);

	return (n_written == n_bytes) ? 0 : -ENOSPC;
}

#endif

// Address Space Operations: Block file & Normal file
struct address_space_operations rawfs_aops = {
	.readpage       = rawfs_readpage,  // simple_readpage,
    .write_begin    = rawfs_write_begin,
    .write_end      = rawfs_write_end,
//    .writepage      = rawfs_writepage,
};

// Normal file operation
struct file_operations rawfs_file_operations = {
	.read           = do_sync_read,
	.write          = do_sync_write,
	.mmap           = generic_file_mmap,
	.llseek         = generic_file_llseek,
	.aio_read	    = rawfs_reg_file_aio_read,
    .aio_write	    = rawfs_reg_file_aio_write,
//	.aio_read	    = generic_file_aio_read,
//	.aio_write	    = generic_file_aio_write,
//  .fsync          = simple_sync_file,
//  .sendfile       = generic_file_sendfile,
};

// Block File Operation
struct file_operations rawfs_block_file_operations = {
	.read           = do_sync_read,
	.aio_read       = rawfs_block_file_aio_read,
	.write          = do_sync_write,
	.aio_write      = rawfs_block_file_aio_write,
	.mmap           = generic_file_mmap,
	.llseek         = generic_file_llseek,
};


