# loadavg_tracer
a simple Linux loadavg tracer

## How to build
Note: the main branch support only TencentOS.
for CentOS7 support please go to CentOS7 branch.

just install kernel dev package and run `make`

## How to use
```bash
insmod loadavg_tracer.ko [load_threshold=10] [dump_interval=5]
# eg:
# dump task name and stack every 5 seconds when 1 minute loadavg is higher than 20
insmod loadavg_tracer.ko load_threshold=20 dump_interval=5
dmest -w
```
sample output:
```
[ 4367.497942] high loadavg deteced: load1 4.39 >= 3
[ 4367.498606] R [002] sh               9422
[ 4367.499226]
 [<ffffffffb9d90516>] retint_careful+0x14/0x32
 [<ffffffffffffffff>] 0xffffffffffffffff

[ 4367.500953] R [002] sh               9429
[ 4367.501451]
 [<ffffffffb9d90516>] retint_careful+0x14/0x32
 [<ffffffffffffffff>] 0xffffffffffffffff

[ 4367.503148] R [002] sh               9430
[ 4367.503638]
 [<ffffffffb9d90516>] retint_careful+0x14/0x32
 [<ffffffffffffffff>] 0xffffffffffffffff

[ 4367.505312] R [002] sh               9482
[ 4367.505835]
 [<ffffffffffffffff>] 0xffffffffffffffff

[ 4367.506977] R [001] dd              16176
[ 4367.507480]
 [<ffffffffb97bd431>] wait_on_page_bit+0x81/0xa0
 [<ffffffffb97bd561>] __filemap_fdatawait_range+0x111/0x190
 [<ffffffffb97bd5f4>] filemap_fdatawait_range+0x14/0x30
 [<ffffffffb97bfff6>] filemap_write_and_wait_range+0x56/0x90
 [<ffffffffc02c89fa>] ext4_sync_file+0xba/0x320 [ext4]
 [<ffffffffb98840bf>] generic_write_sync+0x4f/0x70
 [<ffffffffb97c0bf7>] generic_file_aio_write+0x77/0xa0
 [<ffffffffc02c85c8>] ext4_file_write+0x348/0x600 [ext4]
 [<ffffffffb984da43>] do_sync_write+0x93/0xe0
 [<ffffffffb984e4d0>]
```
