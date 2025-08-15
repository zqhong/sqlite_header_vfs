#!/bin/bash

# --- 配置 ---
DB_FILE="./test.db"
VFS_NAME="headervfs"

case "$(uname)" in
    Darwin)
        EXTENSION_PATH="./libheadervfs.dylib"
        ;;
    Linux)
        EXTENSION_PATH="./libheadervfs.so"
        ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        EXTENSION_PATH="./headervfs.dll"
        ;;
    *)
        echo "Unsupported OS: $(uname)"
        exit 1
        ;;
esac

echo "EXTENSION_PATH is set to: $EXTENSION_PATH"

# --- 准备工作 ---
set -e
rm -f "$DB_FILE"
if [ ! -f "$EXTENSION_PATH" ]; then
    echo "[错误] 扩展库 '$EXTENSION_PATH' 不存在。"
    exit 1
fi

# --- 执行操作 ---
sqlcipher "$DB_FILE" <<EOF
.load '${EXTENSION_PATH}'
PRAGMA vfs_name = '${VFS_NAME}';

CREATE TABLE IF NOT EXISTS t(x INT);
INSERT INTO t VALUES(22);
INSERT INTO t VALUES(33);

.headers on
.mode column

SELECT x FROM t;

.exit
EOF

echo "All tests succeeded!"
exit 0