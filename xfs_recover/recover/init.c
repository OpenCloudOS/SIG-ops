/*
 * Copyright (c) Tencent, Inc.
 * All Rights Reserved.
 * Create add Edit by zorrozou 2020/03/30.
 * update by curuwang 2022/01/18
 * add recover dir by jindazhong 2022/04/12
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include "libxfs.h"
#include "libxlog.h"
#include "init.h"
#include "bmap.h"
#define RECOVER_DIR "./RECOVER"
#define FILE_DIR_HASH_SIZE 10000
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
// path /a/b/c/d max size 
#define FILE_PATH_MAX 1024

char *fsdevice;
static int recover_fd, device_fd;
int blkbb;
int exitcode;
int expert_mode;
static int force;
static struct xfs_mount xmount;
struct xfs_mount *mp;
static struct xlog xlog;
xfs_agnumber_t cur_agno = NULLAGNUMBER;
xfs_dinode_t *dinode;
struct xfs_sb *sbp;

struct inode_file_list file_list;
struct hlist_head  file_dir_htable[FILE_DIR_HASH_SIZE];


typedef struct inode_file_list{
    __u64 inumber;
    struct list_head list;
} inode_file_list;

typedef struct file_dir_hash_node {
    __u64 inumber;
    uint8_t file_type;
    uint8_t file_name_len;
    char* file_name;
    __u64 parent_inumber;
    struct hlist_node list;
} file_dir_hash_node;

static int recover_block_to_file(int devfd, int inofd, __u64 block, __u64 len, __u64 start)
{
    off_t offset_dev, offset_ino, offset;
    ssize_t size, ret;
    char buf[sbp->sb_blocksize];
    int i;

    offset_dev = lseek(devfd, start * sbp->sb_blocksize, SEEK_SET);
    if (offset_dev < 0) {
        perror("lseek(devfd)");
        return 0;
    }

    offset_ino = lseek(inofd, block * sbp->sb_blocksize, SEEK_SET);
    if (offset_ino < 0) {
        perror("lseek(inofd)");
        return 0;
    }

    for (i = 0; i < len; i++) {
        offset = 0;
        size = 0;
        while (offset < sbp->sb_blocksize) {
            size = read(devfd, buf + offset, sbp->sb_blocksize - offset);
            if (size < 0) {
                lseek(devfd, offset_dev + sbp->sb_blocksize, SEEK_SET);
                perror("read(dev_fd)");
                continue;
            }
            offset += size;
        }
        //printf("before write offset: %llu\n", offset);
        //printf("before write size: %llu\n", size);
        while (offset != 0) {
            ret = write(inofd, buf + (sbp->sb_blocksize - offset), offset);
            if (ret < 0) {
                lseek(inofd, offset_ino + sbp->sb_blocksize, SEEK_SET);
                perror("write(inofd)");
                continue;
            }
            offset -= ret;
            //printf("ret: %llu\n", ret);
        }
        //printf("after write offset: %llu\n", offset);
        //printf("after write size: %llu\n", size);
    }

    offset_dev = lseek(devfd, 0, SEEK_SET);
    if (offset_dev < 0) {
        perror("lseek(devfd)");
        return 0;
    }

    offset_ino = lseek(inofd, 0, SEEK_SET);
    if (offset_ino < 0) {
        perror("lseek(inofd)");
        return 0;
    }
    return 1;
}

static char *xfs_get_block(xfs_filblks_t blknum, uint32_t bsize)
{
    char *buf;
    ssize_t size;
    int fd;
    off_t offset;

    fd = libxfs_device_to_fd(mp->m_ddev_targp->dev);

    buf = malloc(bsize);
    if (buf == NULL) {
        fprintf(stderr, "xfs_get_block: malloc() error!\n");
        return NULL;
    }

    offset = lseek(fd, blknum * bsize, SEEK_SET);
    if (offset < 0) {
        perror("xfs_get_block: lseek()");
        return NULL;
    }

    size = read(fd, buf, bsize);
    if (size < 0) {
        perror("xfs_get_block: read()");
        return NULL;
    }

    return buf;
}


static int is_inode_used(xfs_dinode_t *dinode)
{
    //if (dinode->di_atime.t_sec == 0 ||
      //  dinode->di_atime.t_nsec == 0) {
        //	dinode->di_nlink > 0) {
     if (dinode->di_size != 0){
        return 1;
    }
    return 0;
}

static int btree_block_travel(struct xfs_btree_block *block)
{
    int count, state, num, retval;
    char *buf;
    xfs_bmbt_rec_t *rec;
    xfs_fileoff_t startoff;
    xfs_fsblock_t startblock;
    xfs_filblks_t blknum;
    xfs_filblks_t blockcount;
    xfs_bmbt_ptr_t *pp;

    if (be32_to_cpu(block->bb_magic) != XFS_BMAP_CRC_MAGIC) {
        printf("block->bb_magic: %x, block not a bmap!\n", be32_to_cpu(block->bb_magic));
        return 0;
    }

    if (be16_to_cpu(block->bb_level) == 0) {
        /* leaf */
        num = be16_to_cpu(block->bb_numrecs);
        printf("block->bb_numrecs: %d\n", num);
        /*
        buf = block;
        for (i=1; i <= 4096;i++) {
            printf("%.2x ", buf[i-1]);
            if (i % 16 == 0) {
                printf("\n");
            }
        }
        */
        rec = (xfs_bmbt_rec_t * )((struct xfs_btree_block *) block + 1);
        for (count = 0; count < num; count++) {
            convert_extent(rec + count, &startoff, &startblock, &blockcount, &state);

            /*
            printf("rec l0: %16llx\t", (rec+count)->l0);
            printf("rec l1: %16llx\n", (rec+count)->l1);
            printf("addr rec: %p\n", rec+count);
            */

            printf("%lu %lu %lu\n", startoff, startblock, blockcount);
            retval = recover_block_to_file(device_fd, recover_fd, startoff, blockcount, startblock);
            if (!retval) {
                fprintf(stderr, "recover_block_to_file(btree_block_travel)\n");
                return 0;
            }
        }
    } else if (be16_to_cpu(block->bb_level) > 0 &&
               be16_to_cpu(block->bb_level) <= 5) {
        /* middle */
        pp = XFS_BMBT_PTR_ADDR(mp, block, 1, mp->m_bmap_dmxr[0]);
        for (count = 0; count < be16_to_cpu(block->bb_numrecs); count++) {
            blknum = cpu_to_be64(*pp + count);
            printf("blknum: %lu\n", blknum);
            buf = xfs_get_block(blknum, sbp->sb_blocksize);
            if (buf == NULL) {
                return 0;
            }
            btree_block_travel((struct xfs_btree_block *) buf);
            free(buf);
        }
    } else {
        /* do nothing! */
    }

    return 1;
}


static int inode_extent_tree_travel(xfs_bmdr_block_t *rblock, int iflag)
{
    int state, fsize, i, retval;
    char *buf;
    xfs_fileoff_t startoff;
    xfs_fsblock_t startblock;
    xfs_filblks_t blockcount, blknum;
    xfs_bmbt_rec_t *rec;
    xfs_bmbt_ptr_t *pp;
    xfs_buf_t *bp;
    xfs_agnumber_t agno;


    if (rblock == NULL) {
        return 0;
    }

    if (cpu_to_be16(rblock->bb_level) == 0) {
        /* extent fmt or leaf */
        if (iflag == 1) {
            /* extent */
            rec = (xfs_bmbt_rec_t *) rblock;
        } else {
            /* leaf */
            rec = (xfs_bmbt_rec_t * )(rblock + 1);
        }
        convert_extent(rec, &startoff, &startblock, &blockcount, &state);
        while (1) {
            convert_extent(rec, &startoff, &startblock, &blockcount, &state);
            if (blockcount == 0) {
                break;
            }

            agno = XFS_FSB_TO_AGNO(mp, startblock);
            startblock = XFS_FSB_TO_DADDR(mp, startblock) >> mp->m_blkbb_log;
            printf("startoff:%-10lu\tstartblock:%-10lu\tblockcount:%-10lu\tag:%u\n",
                   startoff, startblock, blockcount, agno);
            retval = recover_block_to_file(device_fd, recover_fd, startoff, blockcount, startblock);
            if (!retval) {
                fprintf(stderr, "recover_block_to_file(inode_extent_tree_travel)\n");
                return 0;
            }
            rec++;
        }
    } else {
        /* btree fmt: root */
        if (cpu_to_be16(rblock->bb_level) > 0 && cpu_to_be16(rblock->bb_numrecs) < 10) {
            /* root */
            bp = malloc(sizeof(xfs_buf_t));
            fsize = XFS_DFORK_SIZE(dinode, mp, XFS_DATA_FORK);
            if (fsize > sbp->sb_inodesize) {
                return 0;
            }
            printf("root inode fsize: %u\n", fsize);
            printf("bb_numrecs: %u\n", cpu_to_be16(rblock->bb_numrecs));
            pp = XFS_BMDR_PTR_ADDR(rblock, 1, libxfs_bmdr_maxrecs(fsize, 0));
            for (i = 0; i < cpu_to_be16(rblock->bb_numrecs); i++) {
                blknum = cpu_to_be64(*pp + i);
                printf("blknum: %lu\n", blknum);
                buf = xfs_get_block(blknum, sbp->sb_blocksize);
                if (buf == NULL) {
                    fprintf(stderr, "inode_extent_tree_travel: btree_block_travel() error!\n");
                    return 0;
                }
                btree_block_travel((struct xfs_btree_block *) buf);
                /*
                libxfs_readbufr(mp->m_ddev_targp, blknum, bp, sbp->sb_blocksize, 0);
                inode_extent_tree_travel(bp->b_addr, 0);
                */
                free(buf);
            }
            free(bp);
        } else {
            /* do nothing! */
        }
    }

    return 1;
}

static inline int hlist_empty(const struct hlist_head *h){
	return !h->first;
}

static struct file_dir_hash_node* search_file_dir_htable(__u64 inumber){
    struct file_dir_hash_node *data_node = NULL;
    struct hlist_node *hlist;
    __u64 key;

    key = inumber % FILE_DIR_HASH_SIZE;
    if(hlist_empty(&file_dir_htable[key]))
         return NULL;
    else{
        hlist_for_each_entry(data_node, hlist, &file_dir_htable[key], list){
            if(data_node->inumber == inumber){
                return data_node;
            }
                
        }
    }
    return NULL;  
}

static int insert_file_dir_htable(__u64 inumber, char *name, uint8_t namelen, uint8_t dir_ftype, __u64 parent_inumber){
    __u64 key;
    struct file_dir_hash_node *data_node;
    data_node = malloc(sizeof(struct file_dir_hash_node));
    if(!data_node){
        perror("malloc error struct file_dir_hash_node ");
        return -1;
    }

    data_node->inumber = inumber;
    data_node->file_name = malloc(namelen+1);
    memcpy(data_node->file_name, name, namelen);
    data_node->file_name[namelen] = '\0';
    data_node->file_name_len = namelen;
    data_node->file_type = dir_ftype;
    data_node->parent_inumber = parent_inumber;
    INIT_HLIST_NODE(&data_node->list);
   
    key = data_node->inumber % FILE_DIR_HASH_SIZE;
    hlist_add_head(&data_node->list, &file_dir_htable[key]);
    return 0;
}

static int dir_travel(char * buf,  uint16_t isize, uint16_t ipblock){
    struct xfs_dir2_data_hdr *block;
    xfs_dir2_block_tail_t   *btp = NULL;
    xfs_dir2_leaf_entry_t   *lep = NULL;
    int                     i,j;
    struct xfs_dir2_data_hdr *data;
    xfs_dir2_data_entry_t   *dep;
    char                    *ptr;
    char                    *endptr;
    xfs_ino_t               lino;
    struct xfs_dir2_sf_hdr  *sf;
    xfs_dir2_sf_entry_t     *sfe;
    uint8_t dir_ftype;
    __u64 parent_inumber = 0;

    data = (struct xfs_dir2_data_hdr*) buf;
    block = (struct xfs_dir2_data_hdr*) buf;
    //recover block dir
    if (be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC ||
        be32_to_cpu(data->magic) == XFS_DIR2_DATA_MAGIC ||
        be32_to_cpu(block->magic) == XFS_DIR3_BLOCK_MAGIC ||
        be32_to_cpu(data->magic) == XFS_DIR3_DATA_MAGIC) {

        ptr = (char *)data + mp->m_dir_geo->data_entry_offset;
        if (be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC ||
            be32_to_cpu(block->magic) == XFS_DIR3_BLOCK_MAGIC) {
            btp = xfs_dir2_block_tail_p(mp->m_dir_geo, block);
            lep = xfs_dir2_block_leaf_p(btp);
            endptr = (char *)lep;
        } else{
            endptr = (char *)data + mp->m_dir_geo->blksize;
        }
        while (ptr < endptr) {
            dep = (xfs_dir2_data_entry_t *)ptr;
            dir_ftype = xfs_dir2_data_get_ftype(mp, dep);
            if ( dep->namelen != 0 && dir_ftype != 0 && be64_to_cpu(dep->inumber) != 0 ) {
                if (parent_inumber == 0 && dep->name[0] == '.' && dep->namelen == 1){
                    parent_inumber =  be64_to_cpu(dep->inumber);
		    ptr += libxfs_dir2_data_entsize(mp, dep->namelen);
		    continue;
                }
                if (dep->name[0] == '.' && dep->name[1] == '.' && dep->namelen == 2){
                    ptr += libxfs_dir2_data_entsize(mp, dep->namelen);
		    continue;
                }
                //printf("name:%s, namelen:%d,inode:%u,filetype:%u, parent_inumber:%u\n", dep->name, dep->namelen, be64_to_cpu(dep->inumber), dir_ftype, parent_inumber);
                insert_file_dir_htable(be64_to_cpu(dep->inumber), (char*)dep->name, dep->namelen, dir_ftype, parent_inumber);
            }
            ptr += libxfs_dir2_data_entsize(mp, dep->namelen);
        }
    }

    //recover inode dir
    dinode = (xfs_dinode_t *) buf;
    for (i = 0; i < ipblock; i++) {
        dinode = (xfs_dinode_t * ) & buf[isize * i];
        if (cpu_to_be16(dinode->di_magic) != XFS_DINODE_MAGIC) {
            continue;
        }
        //if (dinode->di_format == XFS_DINODE_FMT_LOCAL){
        if ( 1==1 ){
             //if (be64_to_cpu(dinode->di_ino) == 67){
            sf = (struct xfs_dir2_sf_hdr *)XFS_DFORK_DPTR(dinode);
            sfe = xfs_dir2_sf_firstentry(sf);
            //sf->count rewrite  0? 
            for (j = sf->count - 1; j >= -256; j--) {
                lino = libxfs_dir2_sf_get_ino(mp, sf, sfe);
                dir_ftype = libxfs_dir2_sf_get_ftype(mp, sfe);
                if ( dir_ftype == 0 || lino == 0 ){
                    break;
                }
                //printf("file:%s, inode:%u,dir_ftype:%u,parent_inode:%u\n",sfe->name, lino, dir_ftype, be64_to_cpu(dinode->di_ino));
                insert_file_dir_htable(lino, (char*)sfe->name, sfe->namelen, dir_ftype, be64_to_cpu(dinode->di_ino));
                sfe = libxfs_dir2_sf_nextentry(mp, sf, sfe);
            }
         }
    }
    return 0;
}

static int _mkdir(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/'){
        tmp[len - 1] = 0;
    }
    for(p = tmp + 1; *p; p++){
        if(*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
   return mkdir(tmp, 0777);
}

static int mv_or_cp_file(__u64 inumber, char *file_path, char *file_name){
    int retval;
    char recover_file_path[FILE_PATH_MAX+10];
    char recover_file[FILE_PATH_MAX+256];
    char inode_file[128];
    snprintf(inode_file, 128, "%s/%llu_file", RECOVER_DIR, inumber);
    snprintf(recover_file_path, FILE_PATH_MAX+10, "%s/%s", RECOVER_DIR, file_path);
    snprintf(recover_file, FILE_PATH_MAX+256, "%s/%s", recover_file_path, file_name);
    if ( strlen(file_name) == 0 ){
        return -1;
    }
    if (access(recover_file_path, 0) != 0){
        retval = _mkdir(recover_file_path);
        if (retval < 0 && errno != EEXIST) {
            perror("mkdir() recover_file_path");
        }    
    }
    //mv 
    if (access(inode_file, 0) != 0){
        perror("inode_file is not exist");
        return -1;
    }
  
    //printf("inode_file:%s, recover_file:%s\n",inode_file, recover_file);
    if (rename(inode_file, recover_file) != 0){
        perror("rename error");
        return -1;
    } 

    //cp todo 
    return 0;
}

static void recover_dir(){
    struct inode_file_list *file_list_node;
    struct file_dir_hash_node *data_node,*tmp_node;
    char *file_path, *file_path_tmp;
    file_path = malloc(FILE_PATH_MAX);
    file_path_tmp = malloc(FILE_PATH_MAX);
    list_for_each_entry(file_list_node, &file_list.list, list){
        file_path = memset(file_path, '\0', FILE_PATH_MAX);
        file_path_tmp = memset(file_path_tmp, '\0', FILE_PATH_MAX);
        data_node = search_file_dir_htable(file_list_node->inumber);
        if (!data_node){
            continue;
        }
        if (data_node->file_type == XFS_DIR3_FT_DIR){
	       continue;
	    }
        //printf("inode :%u, file:%s\n", file_list_node->inumber,data_node->file_name);
        tmp_node = data_node;
        while ( tmp_node->parent_inumber != sbp->sb_rootino && tmp_node->parent_inumber != 0 ){
            tmp_node = search_file_dir_htable(tmp_node->parent_inumber);
            if (!tmp_node){
                break;
            }
            if (tmp_node->file_type !=XFS_DIR3_FT_DIR){
                break;
            }
            snprintf(file_path_tmp, FILE_PATH_MAX, "%s/%s",tmp_node->file_name, file_path);
	        memcpy(file_path, file_path_tmp, strlen(file_path_tmp));	
        }
        printf("inode:%llu, path:%s, file_name:%s\n",data_node->inumber, file_path,data_node->file_name);
        mv_or_cp_file(data_node->inumber, file_path, data_node->file_name);
    }
}

static int disk_traverse(char *device, uint32_t bsize, uint16_t isize, uint16_t ipblock)
{
    int i, fd;
    char buf[bsize];
    ssize_t size, offset;
    xfs_bmdr_block_t *rblock;
    struct inode_file_list *file_list_node;
    char filename[BUFSIZ];

    fd = open(device, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
        perror("open(device)");
        return 0;
    }

    offset = -1;
    while ((size = read(fd, buf, bsize)) == bsize) {
        dir_travel(buf, isize, ipblock);
        offset++;
        dinode = (xfs_dinode_t *) buf;
        /*if (cpu_to_be16(dinode->di_magic) != XFS_DINODE_MAGIC) {
            continue;
        }*/
        for (i = 0; i < ipblock; i++) {
            dinode = (xfs_dinode_t * ) & buf[isize * i];
            if (cpu_to_be16(dinode->di_magic) != XFS_DINODE_MAGIC) {
                continue;
            }
            if (is_inode_used(dinode)) {
                continue;
            }
            //printf("magic: %d\n", dinode->di_magic);
           // printf("inode: %llu\n", be64_to_cpu(dinode->di_ino));
           // printf("inode offset: %lu:%d\n", offset, i);

            //snprintf(filename, BUFSIZ, "%s/%lu_%d_file", RECOVER_DIR, offset, i);
            snprintf(filename, BUFSIZ, "%s/%llu_file", RECOVER_DIR, be64_to_cpu(dinode->di_ino));
            file_list_node = (struct inode_file_list *)malloc(sizeof(struct inode_file_list));
            file_list_node->inumber = be64_to_cpu(dinode->di_ino);
            list_add(&(file_list_node->list), &(file_list.list));

            recover_fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE, 0644);
            if (recover_fd < 0) {
                perror("open(inode)");
                continue;
            }
            rblock = (xfs_bmdr_block_t *) XFS_DFORK_PTR(dinode, XFS_DATA_FORK);
            //printf("rblock addr - inode addr: %lu\n", (uint64_t)rblock - (uint64_t)dinode);
            inode_extent_tree_travel(rblock, 1);
            close(recover_fd);
        }
    }

    if (size != bsize && size != 0) {
        fprintf(stderr, "read(): read device error!\n");
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

int main(int argc, char **argv)
{
    struct xfs_buf *bp;
    unsigned int agcount;
    int retval,i;

    setlinebuf(stdout);

    progname = basename(argv[0]);

    if (argc != 2) {
        fprintf(stderr, "argument error!\n");
        exit(1);
    }

    fsdevice = argv[1];
    if (!x.disfile)
        x.volname = fsdevice;
    else
        x.dname = fsdevice;

    x.bcache_flags = CACHE_MISCOMPARE_PURGE;
    if (!libxfs_init(&x)) {
        fputs(_("\nfatal error -- couldn't initialize XFS library\n"),
              stderr);
        exit(1);
    }
    /*
     * Read the superblock, but don't validate it - we are a diagnostic
     * tool and so need to be able to mount busted filesystems.
     */
    memset(&xmount, 0, sizeof(struct xfs_mount));
    libxfs_buftarg_init(&xmount, x.ddev, x.logdev, x.rtdev);
    retval = -libxfs_buf_read_uncached(xmount.m_ddev_targp, XFS_SB_DADDR,
                                       1 << (XFS_MAX_SECTORSIZE_LOG - BBSHIFT), 0, &bp, NULL);
    if (retval) {
        fprintf(stderr, _("%s: %s is invalid (cannot read first 512 "
                          "bytes)\n"), progname, fsdevice);
        exit(1);
    }

    /* copy SB from buffer to in-core, converting architecture as we go */
    libxfs_sb_from_disk(&xmount.m_sb, bp->b_addr);

    sbp = &xmount.m_sb;
    if (sbp->sb_magicnum != XFS_SB_MAGIC) {
        fprintf(stderr, _("%s: %s is not a valid XFS filesystem (unexpected SB magic number 0x%08x)\n"),
                progname, fsdevice, sbp->sb_magicnum);
        if (!force) {
            fprintf(stderr, _("Use -F to force a read attempt.\n"));
            exit(EXIT_FAILURE);
        }
    }

    agcount = sbp->sb_agcount;
    mp = libxfs_mount(&xmount, sbp, x.ddev, x.logdev, x.rtdev,
                      LIBXFS_MOUNT_DEBUGGER);
    if (!mp) {
        fprintf(stderr,
                _("%s: device %s unusable (not an XFS filesystem?)\n"),
                progname, fsdevice);
        exit(1);
    }
    mp->m_log = &xlog;


	/*
	 * xfs_check needs corrected incore superblock values
	 */
    if (sbp->sb_rootino != NULLFSINO &&
        xfs_sb_version_haslazysbcount(&mp->m_sb)) {
        int error = -libxfs_initialize_perag_data(mp, sbp->sb_agcount);
        if (error) {
            fprintf(stderr,
                    _("%s: cannot init perag data (%d). Continuing anyway.\n"),
                    progname, error);
        }
    }

    printf("agcount: %u\n", agcount);
    printf("isize: %u\n", sbp->sb_inodesize);
    printf("bsize: %u\n", sbp->sb_blocksize);
    printf("rootinode: %lu\n", sbp->sb_rootino);
    printf("inopblock: %u\n", sbp->sb_inopblock);
    printf("sb_icount: %lu\n", sbp->sb_icount);
    device_fd = open(fsdevice, O_RDONLY);
    if (device_fd < 0) {
        perror("open(device)");
        exit(1);
    }

    retval = mkdir(RECOVER_DIR, 0777);
    if (retval < 0 && errno != EEXIST) {
        perror("mkdir()");
        exit(1);
    }

    INIT_LIST_HEAD(&file_list.list);
    for(i = 0; i < FILE_DIR_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&file_dir_htable[i]);
    }
    disk_traverse(fsdevice, sbp->sb_blocksize, sbp->sb_inodesize, sbp->sb_inopblock);

    recover_dir();
    close(device_fd);
    exit(0);
}
