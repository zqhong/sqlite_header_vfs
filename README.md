## sqlite_header_vfs

sqlite 的 vfs 扩展，在读写数据库的情况下，忽略前 1024 个字节。（通过 `HEADER_SIZE` 设置）

注意：这个目前处于实验阶段，没有经过验证，建议只读的形式打开数据库

## 环境

* macOS Sequoia 15.5 x86_64
* SQLCipher 4.6.0 community
  * `sqlite3ext.h` 由 SQLCipher v4.6.0 版本提供的

## 已知问题

### 1、当多个进程同时访问一个数据库的情况下，会出现错误

```bash
# 备注：
# 1、这里使用只读模式打开
# 2、在关闭其他进程后，可以正常读写
sqlite> select * from table_name limit 10;
2025-08-15 20:07:35.844: sqlcipher_page_cipher: hmac check failed for pgno=1
2025-08-15 20:07:35.845: sqlite3Codec: error decrypting page 1 data: 1
2025-08-15 20:07:35.845: sqlcipher_codec_ctx_set_error 1
Runtime error: database disk image is malformed (11)
```


## 编译

```bash
cd build

cmake ..

make

## 【可选】测试
make test

## macOS 系统。如果是 Linux，则是 libheadervfs.so
file libheadervfs.dylib
libheadervfs.dylib: Mach-O 64-bit dynamically linked shared library x86_64
```

## 使用

```bash
sqlcipher

.load /path/to/libheadervfs.dylib

.open file:/path/to/your.db?mode=ro&vfs=headervfs

.tables
```