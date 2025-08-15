## sqlite_header_vfs

sqlite 的 vfs 扩展，在读写数据库的情况下，忽略前 1024 个字节。（通过 `HEADER_SIZE` 设置）

注意：这个目前处于实验阶段，没有经过验证，建议只读的形式打开数据库

## 环境

* macOS Sequoia 15.5 x86_64
* SQLCipher 4.6.0 community

## 使用

```bash
sqlcipher

.load /path/to/headervfs.so

.open file:/path/to/your.db?mode=ro&vfs=headervfs

.tables
```