#!/bin/bash

set -eou pipefail

make --no-print-directory -C tools/rpcgen

rpcgen=tools/rpcgen/obj/rpcgen

echo "[RPC] kernel_kernel_task.rpc"
$rpcgen \
    kernel/kernel_task.rpc \
    common/kernel_task_common.{h,c} \
    kernel/kernel_task_server.{h,c} \
    userland/runtime/kernel_task_client.{h,c}

for program in logger vfs blk
do
    rpc_file="userland/${program}/${program}.rpc"
    
    echo "[RPC] $rpc_file"
    $rpcgen \
        "$rpc_file" \
        "userland/runtime/${program}_common".{h,c} \
        "userland/${program}/${program}_server".{h,c} \
        "userland/runtime/${program}_client".{h,c}
done




