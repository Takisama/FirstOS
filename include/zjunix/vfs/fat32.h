#ifndef _ZJUNIX_VFS_FAT32_H
#define _ZJUNIX_VFS_FAT32_H

#include <zjunix/type.h>


#define SECTOR_SIZE 512
#define CLUSTER_SIZE 4096

struct __attribute__((__packed__)) BPB_attr 
{
    u8      BS_jump_code[3];
    u8      BS_oem_name[8];
    u16     BPB_sector_size;
    u8      BPB_sectors_per_cluster;
    u16     BPB_reserved_sector_count;
    u8      BPB_number_of_fats;
    u16     BPB_root_entries_count;
    u16     BPB_total_sector16;
    u8      BPB_media_descriptor;
    u16     BPB_fat_size16;
    u16     BPB_sectors_per_track;
    u16     BPB_num_of_heads;
    u32     BPB_num_of_hidden_sectors;
    u32     BPB_total_sector32;

    // 只有FAT32的项
    u32     BPB_num_of_sectors_per_fat32;
    u16     BPB_extended_flags;
    u16     BPB_FAT32_version;
    u32     BPB_cluster_number_of_root_dir;
    u16     BPB_total_sector_of_fs_info;
    u16     BPB_sector_number_of_backup_boot;
    u8      BPB_reserved_for_extend[12];
    u8      BS_logical_drive_number;
    u8      BS_reserved_for_NT;
    u8      BS_extended_signature;
    u32     BS_volume_id;
    u8      BS_volume_Lab[11];
    u8      BS_fat_name[8];
    u8      exec_code[420];
    u8      boot_record_signature[2];
};

struct DBR_Info 
{
    u32     base_addr;                              // 基地址
    u16     reserved_sec;                           // 保留扇区数
    u32     total_sec_cnt;                          // 总扇区数
    u32     fat_sec_cnt;                            // FAT表扇区数
    u32     root_clu_num;                           // 根目录簇号
    u8      DBR_data[SECTOR_SIZE];                  // DBR数据
};

struct __attribute__((__packed__)) FSI_attr 
{
    u32     FSI_lead_signature;
    u8      FSI_reserved_for_extend1[480];
    u32     FSI_struct_signature;
    u32     FSI_free_clustor_count;
    u32     FSI_next_free_clustor;
    u8      FSI_reserved_for_extend2[12];
    u32     FSI_trail_signature;
};

struct FSI_Info
{
    u32     free_clu_cnt;                           // 空闲簇总数
    u32     next_free_clu;                          // 下一个可用簇号
    u8      FSI_data[SECTOR_SIZE];                  // FSI数据
};

#define FST_DEFAULT_VAL 0xF8FFFF0F                  // 第一个FAT项的默认值
#define SEC_DEGAULT_VAL 0xFFFFFF0F                  // 第二个FAT项的默认值
#define UNUSED_CLUSTER  0x00000000                  // 空闲的FAT项
#define END_CLUSTER     0x0FFFFFFF                  // 文件最后一个FAT项的标志
#define BAD_CLUSTER     0xFFFFFFF7                  // 坏簇的FAT项标志
struct File_alloc_table 
{
    u32     FAT_Sec_cnt;
    u32*    FAT_data;
};

#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ACHIEVE     0x20
#define ATTR_LONG_NAME   0x0F
struct __attribute__((__packed__)) FAT_dir_struct 
{
    u8      DIR_name[11];
    u8      DIR_attr;
    u8      DIR_reserved_for_NT;
    u8      DIR_created_time_ms;
    u16     DIR_created_time;
    u16     DIR_created_date;
    u16     DIR_last_acc_date;
    u16     DIR_fst_clus_high;
    u16     DIR_last_write_time;
    u16     DIR_last_write_date;
    u16     DIR_fst_clus_low;
    u16     DIR_file_size;
};

struct __attribute__((__packed__)) FAT_long_struct 
{
    u8      LDIR_order;
    u8      LDIR_name1[10];
    u8      LDIR_attr;
    u8      LDIR_type;
    u8      LDIR_checksum;
    u8      LDIR_name2[12];
    u16     LDIR_fst_clus;
    u8      LDIR_name3[4];
};

struct fat_entry
{
    u8*     name;                                   // 文件名
    u8      FAT_struct_cnt;                         // 对应目录项个数（短文件名则为1）
    u8      clu_num;                                // 文件首部簇号
    u8      attr;                                   // 文件属性
    u16     last_acc_date;                          // 最后访问日期
    u16     last_write_time;                        // 最后写时间
    u16     last_write_date;                        // 最后写日期
    u16     file_size;                              // 文件大小
};