int vbfs_readdir(struct inode_vbfs *dir_v_inode, off_t *pos, fuse_fill_dir_t filler, void *buf);

__u32 vbfs_inode_lookup_by_name(struct inode_vbfs *v_inode, const char *name, int *err);

struct inode_vbfs *vbfs_pathname_to_inode(const char *pathname);

int vbfs_fuse_update_times(struct inode_vbfs *v_inode, time_update_flags mask);

int vbfs_inode_close(struct inode_vbfs *v_inode);

int vbfs_mkdir(struct inode_vbfs *i_vbfs_p, const char *path);
